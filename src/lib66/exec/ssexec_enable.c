/*
 * ssexec_enable.c
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
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/parse.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/symlink.h>
#include <66/svc.h>
#include <66/command.h>

static opt_t const opts_enable[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",  .arg = OPT_NONE, .help = "print this help" },
    { .id = 'S',         .shortname = 'S', .longname = "start", .arg = OPT_NONE, .help = "also start the service immediately" },
    { .id = 'P',         .shortname = 'P', .longname = 0,       .arg = OPT_NONE, .help = "do not propagate to dependencies" },
} ;

static uint8_t opt_start = 0 ;
static uint8_t opt_nopropagate = 0 ;

static int on_enable(int id, char const *arg, void *data)
{
    (void)arg ;
    (void)data ;

    switch (id) {

        case 'S' : opt_start = 1 ; break ;
        case 'P' : opt_nopropagate = 1 ; break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_enable = {
    .name = "66 enable",
    .help = "activate services at the next boot",
    .operands = "service...",
    .opts = opts_enable,
    .nopts = OPT_COUNT(opts_enable),
    .on_option = &on_enable,
    .fn = &ssexec_enable,
} ;

static void refuse_held(char const *const *argv, int argc, ssexec_t const *info)
{
    _cleanup_strbuf_ strbuf msg = STRBUF_ZERO ;

    for (int i = 0 ; i < argc ; i++) {

        int r = symlink_provide_isavailable(info->base.s, argv[i], info->owner, &msg) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "check the provider of the name: ", argv[i]) ;

        if (!r)
            log_die(LOG_EXIT_USER, msg.s) ;
    }
}

static void take_back(char const *const *argv, int argc, ssexec_t const *info)
{
    bool locked = false ;
    char holder[SS_MAX_SERVICE_NAME + 1] ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_SERVICE, &res) ;

    for (int i = 0 ; i < argc ; i++) {

        int r = service_resolve_provide(holder, argv[i], 0, info->base.s) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "resolve provide alias: ", argv[i]) ;

        if (!r)
            continue ;

        r = resolve_check(&wres, info->base.s, argv[i]) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "check the resolve file of: ", argv[i]) ;

        if (!r)
            continue ;

        if (!locked && service_graph_write_begin(info->owner, info->base.s, info->live.s) < 0)
            log_dieusys(LOG_EXIT_SYS, "lock the system") ;

        locked = true ;

        if (symlink_provide_drop(info->base.s, argv[i]) < 0)
            log_dieusys(LOG_EXIT_SYS, "take the name back from: ", holder) ;

        log_trace("Name: ", argv[i], " taken back from: ", holder) ;
    }

    if (locked)
        command_write_end(0) ;
}

static void refuse_claims(selection_service_t const *sel, service_graph_resolve_t *store, char const *base)
{
    uint32_t index ;
    struct resolve_hash_s *h ;
    _cleanup_strbuf_ strbuf msg = STRBUF_ZERO ;

    FOREACH_SELECTION_MEMBER(sel, index) {

        size_t pos = 0 ;
        _cleanup_strbuf_ strbuf list = STRBUF_ZERO ;

        if (service_graph_resolve_get(store, index, SERVICE_READ_DEPENDENCIES, &h) <= 0)
            log_dieusys(LOG_EXIT_SYS, "read the services of the selection") ;

        resolve_service_addon_dependencies_t const *dep = &h->dependencies ;

        if (h->res.islog || !dep->nprovide)
            continue ;

        if (!sbl_clean_string(&list, dep->sa.s + dep->provide))
            log_die_nomem("sbl") ;

        FOREACH_SBL(&list, pos) {

            int r = symlink_provide_claim(sel->g, base, 0, h->name, list.s + pos, &msg) ;
            if (r < 0)
                log_dieusys(LOG_EXIT_SYS, "check the names provided by: ", h->name) ;

            if (!r)
                log_die(LOG_EXIT_USER, msg.s) ;
        }
    }
}

static int select_enable(selection_service_t *sel, command_graph_t *d, char const *const *argv, int argc, ssexec_t *info, strbuf *frontends, bool nopropagate)
{
    selection_service_policy_t policy = command_policy(SELECTION_SERVICE_PROPAGATE_DEPENDS, nopropagate, false, !nopropagate) ;

    if (!command_argv_to_select(sel, d, argv, argc, info, &policy, frontends, SELECTION_SERVICE_REFUSE_NAMED_ABSENT | SELECTION_SERVICE_REFUSE_MET_ABSENT | SELECTION_SERVICE_REFUSE_NAMED_INVALID | SELECTION_SERVICE_REFUSE_MET_INVALID))
        return 0 ;

    if (service_graph_warn_invalid(&d->g, frontends, &sel->member) < 0)
        log_dieusys(LOG_EXIT_SYS, "warn about the invalid services") ;

    refuse_claims(sel, &d->store, info->base.s) ;

    return command_refuse_conflict(sel, d, info, SVC_CONFLICT_ENABLED) ;
}

int ssexec_enable(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    bool start = opt_start, nopropagate = opt_nopropagate ;
    opt_start = 0 ;
    opt_nopropagate = 0 ;

    int e = 0 ;
    command_graph_t d = COMMAND_GRAPH_ZERO ;
    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    strbuf frontends = STRBUF_ZERO ;
    uint32_t flags = PARSE_FRONTEND | PARSE_NOFROM | (nopropagate ? PARSE_NODEPS : 0) ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    command_refuse_member(argv, argc, "enable") ;

    refuse_held(argv, argc, info) ;

    take_back(argv, argc, info) ;

    if (service_graph_load(&d.g, info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "load the graph of the system") ;

    command_parse_missing(&d.g, argv, argc, info, flags) ;

    while (!select_enable(&sel, &d, argv, argc, info, &frontends, nopropagate))
        command_graph_reload(&d, info, true) ;

    if (service_graph_write_begin(info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "lock the system") ;

    if (service_enable_selection(&sel, &d.store, true, info->opt_tree ? info->treename.s : 0) < 0)
        log_dieusys(LOG_EXIT_SYS, "enable the services of the selection") ;

    command_write_end(0) ;

    service_graph_resolve_free(&d.store) ;
    service_graph_free(&d.g) ;

    if (start)
        e = command_up(argv, argc, info, &frontends, info->target, nopropagate, SVC_LOT_COMMAND, false, true) ;

    strbuf_free(&frontends) ;

    return e ;
}
