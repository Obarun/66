/*
 * ssexec_reload.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <oblibs/types.h>
#include <oblibs/bits.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/config.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/svc.h>
#include <66/service.h>
#include <66/command.h>

static uint8_t opt_nopropagate = 0 ;

static opt_t const opts_reload[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",         .arg = OPT_NONE, .help = "print this help" },
    { .id = 'P',         .shortname = 'P', .longname = "no-propagate", .arg = OPT_NONE, .help = "do not propagate signal to its dependencies" },
} ;

static int on_reload(int id, char const *arg, void *data)
{
    (void)arg ;
    (void)data ;

    switch (id) {

        case 'P' :

            opt_nopropagate = 1 ;
            break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_reload = {
    .name = "66 reload",
    .help = "convenient command to send a SIGHUP signal to services",
    .operands = "service...",
    .opts = opts_reload,
    .nopts = OPT_COUNT(opts_reload),
    .on_option = &on_reload,
    .fn = &ssexec_reload,
} ;

static int select_back(selection_service_t *back, selection_service_t const *run, command_graph_t *d, ssexec_t *info, strbuf *frontends)
{
    uint32_t index = 0 ;
    selection_service_t up = SELECTION_SERVICE_ZERO, uprun = SELECTION_SERVICE_ZERO ;
    selection_service_t down = SELECTION_SERVICE_ZERO ;
    selection_service_policy_t policy = command_policy(SELECTION_SERVICE_PROPAGATE_DEPENDS, false, !info->opt_react, false) ;

    selection_service_propagate(&up, &d->g, &run->member, &policy) ;

    if (!command_resolve_load(d, &up))
        return 0 ;

    command_refuse_selection(&up, frontends, SELECTION_SERVICE_REFUSE_MET_ABSENT | SELECTION_SERVICE_REFUSE_MET_INVALID) ;

    if (selection_svc_filter(&uprun, &up, &d->store, SERVICE_READ_EVENT, svc_isrunning) < 0)
        log_dieusys(LOG_EXIT_SYS, "read the state of the selection") ;

    down = up ;

    FOREACH_SELECTION_MEMBER(&uprun, index)
        selection_service_remove(&down, index) ;

    if (svc_lot(back, &down, &d->store, SVC_LOT_COMMAND) < 0)
        log_dieusys(LOG_EXIT_SYS, "select the services to bring back up") ;

    return command_refuse_conflict(back, d, info, SVC_CONFLICT_RUNNING) ;
}

static int select_lot(selection_service_t *lot, selection_service_t *back, command_graph_t *d, char const *const *argv, int argc, ssexec_t *info, strbuf *frontends, bool nopropagate)
{
    uint32_t pos = 0, index = 0 ;
    selection_service_t sel, sup = SELECTION_SERVICE_ZERO, run = SELECTION_SERVICE_ZERO ;
    selection_service_policy_t policy = command_policy(SELECTION_SERVICE_PROPAGATE_REQUIREDBY, nopropagate, false, false) ;

    *back = (selection_service_t)SELECTION_SERVICE_ZERO ;

    if (!command_argv_to_select(&sel, d, argv, argc, info, &policy, frontends, SELECTION_SERVICE_REFUSE_NAMED_ABSENT | SELECTION_SERVICE_REFUSE_MET_ABSENT | SELECTION_SERVICE_REFUSE_NAMED_INVALID | SELECTION_SERVICE_REFUSE_MET_INVALID))
        return 0 ;

    if (svc_lot(lot, &sel, &d->store, SVC_LOT_COMMAND) < 0)
        log_dieusys(LOG_EXIT_SYS, "select the services to reload") ;

    command_refuse_unsupervised(&sup, lot, &d->store) ;

    if (selection_svc_filter(&run, &sup, &d->store, SERVICE_READ_EVENT, svc_isrunning_supervised) < 0)
        log_dieusys(LOG_EXIT_SYS, "read the state of the selection") ;

    FOREACH_SELECTION(lot, pos, index) {

        if (bitset32_isvalid(&lot->named, index) && !bitset32_isvalid(&run.member, index))
            log_die(LOG_EXIT_USER, "service: ", service_graph_name(&d->g, index), " is not running -- start it first") ;
    }

    if (!nopropagate && !select_back(back, &run, d, info, frontends))
        return 0 ;

    if (service_graph_warn_invalid(&d->g, frontends, &sel.member) < 0)
        log_dieusys(LOG_EXIT_SYS, "warn about the invalid services") ;

    return 1 ;
}

int ssexec_reload(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    bool nopropagate = opt_nopropagate ;
    opt_nopropagate = 0 ;

    int r, e ;
    bitset32_t reached ;
    command_graph_t d = COMMAND_GRAPH_ZERO ;
    selection_service_t lot = SELECTION_SERVICE_ZERO, back = SELECTION_SERVICE_ZERO ;
    strbuf frontends = STRBUF_ZERO ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    command_refuse_scandir(info) ;

    if (service_graph_load(&d.g, info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "load the graph of the system") ;

    while (!select_lot(&lot, &back, &d, argv, argc, info, &frontends, nopropagate))
        command_graph_reload(&d, info, true) ;

    strbuf_free(&frontends) ;

    e = command_lot_start(&back, &d.store, info, SVC_TARGET_READY, SVC_LOT_COMMAND, &reached) ;

    command_drop_waiters(&lot, &back, &reached) ;

    if (lot.member.count) {

        svc_request_t req = command_request(info, SVC_ACTION_SIGNAL, SVC_TARGET_KEEP, "-l") ;

        req.verb = "reload" ;

        r = svc_send(&lot, &d.store, &req, 0) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "send the reload to the services of the selection") ;

        if (!r)
            e = LOG_EXIT_SYS ;
    }

    service_graph_resolve_free(&d.store) ;
    service_graph_free(&d.g) ;

    return e ;
}
