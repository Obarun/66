/*
 * ssexec_disable.c
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>
#include <stdbool.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/bits.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/command.h>

static opt_t const opts_disable[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
    { .id = 'S',         .shortname = 'S', .longname = "stop", .arg = OPT_NONE, .help = "also stop the service immediately" },
    { .id = 'P',         .shortname = 'P',                     .arg = OPT_NONE, .help = "do not propagate to its requiredby" },
} ;

static uint8_t opt_stop = 0 ;
static uint8_t opt_nopropagate = 0 ;

static int on_disable(int id, char const *arg, void *data)
{
    (void)arg ;
    (void)data ;

    switch (id) {

        case 'S' : opt_stop = 1 ; break ;
        case 'P' : opt_nopropagate = 1 ; break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_disable = {
    .name = "66 disable",
    .help = "deactivate services at the next boot",
    .operands = "service...",
    .opts = opts_disable,
    .nopts = OPT_COUNT(opts_disable),
    .on_option = &on_disable,
    .fn = &ssexec_disable,
} ;

static int select_disable(selection_service_t *sel, command_graph_t *d, char const *const *argv, int argc, ssexec_t *info, strbuf *frontends, bool nopropagate)
{
    selection_service_policy_t policy = command_policy(SELECTION_SERVICE_PROPAGATE_REQUIREDBY, nopropagate, false, !nopropagate) ;

    if (!command_argv_to_select(sel, d, argv, argc, info, &policy, frontends, SELECTION_SERVICE_REFUSE_NAMED_ABSENT | SELECTION_SERVICE_REFUSE_NAMED_INVALID | SELECTION_SERVICE_REFUSE_MET_INVALID))
        return 0 ;

    if (service_graph_warn_invalid(&d->g, frontends, &sel->member) < 0)
        log_dieusys(LOG_EXIT_SYS, "warn about the invalid services") ;

    return 1 ;
}

int ssexec_disable(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    bool stop = opt_stop, nopropagate = opt_nopropagate ;
    opt_stop = 0 ;
    opt_nopropagate = 0 ;

    int e = 0 ;
    uint32_t supervised = 0 ;
    command_graph_t d = COMMAND_GRAPH_ZERO ;
    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    selection_service_t sup = SELECTION_SERVICE_ZERO ;
    strbuf frontends = STRBUF_ZERO ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    command_refuse_member(argv, argc, "disable") ;

    if (service_graph_load(&d.g, info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "load the graph of the system") ;

    while (!select_disable(&sel, &d, argv, argc, info, &frontends, nopropagate))
        command_graph_reload(&d, info, true) ;

    if (service_graph_write_begin(info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "lock the system") ;

    if (service_enable_selection(&sel, &d.store, false, 0) < 0)
        log_dieusys(LOG_EXIT_SYS, "disable the services of the selection") ;

    command_write_end(0) ;

    if (stop) {

        command_refuse_scandir(info) ;

        if (selection_svc_supervised(&sup, &sel, &d.store) < 0)
            log_dieusys(LOG_EXIT_SYS, "read the state of the selection") ;

        supervised = sup.member.count ;
    }

    service_graph_resolve_free(&d.store) ;
    service_graph_free(&d.g) ;

    if (stop && !supervised)
        log_info("no services found to handle") ;
    else if (stop)
        e = command_down(argv, argc, info, &frontends, nopropagate, true, false, false) ;

    strbuf_free(&frontends) ;

    return e ;
}
