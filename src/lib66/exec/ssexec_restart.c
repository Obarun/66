/*
 * ssexec_restart.c
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
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/parse.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/command.h>

static uint8_t opt_nopropagate = 0 ;

static opt_t const opts_restart[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",         .arg = OPT_NONE, .help = "print this help" },
    { .id = 'P',         .shortname = 'P', .longname = "no-propagate", .arg = OPT_NONE, .help = "do not propagate signal to its dependencies" },
} ;

static int on_restart(int id, char const *arg, void *data)
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

opt_cmd_t const cmd_restart = {
    .name = "66 restart",
    .help = "convenient command to bring down then bring up services in one pass",
    .operands = "service...",
    .opts = opts_restart,
    .nopts = OPT_COUNT(opts_restart),
    .on_option = &on_restart,
    .fn = &ssexec_restart,
} ;

#define SELECT_STALE 0 // the graph was stale: load it again
#define SELECT_DONE 1 // the lots are drawn
#define SELECT_PARSED 2 // the parse on the fly ran: the graph is a new one, draw again

static void prepare(bitset32_t const *seeds, command_graph_t *d, ssexec_t *info)
{
    _cleanup_sbl_ strbuf sa = SBL_ZERO ;
    uint32_t n = 0 ;
    size_t pos = 0 ;

    for (uint32_t index = 0 ; index < service_graph_nvertex(&d->g) ; index++) {

        if (bitset32_isvalid(seeds, index) && !sbl_add(&sa, service_graph_name(&d->g, index)))
            log_die_nomem("strbuf") ;
    }

    char const *names[sbl_count(&sa) + 1] ;

    FOREACH_SBL(&sa, pos)
        names[n++] = sa.s + pos ;

    names[n] = 0 ;

    service_graph_resolve_free(&d->store) ;

    command_parse_missing(&d->g, names, (int)n, info, info->opt_react ? PARSE_NOFROM : 0) ;
}

static void split_reactors(selection_service_t *up, selection_service_t *reactors, service_graph_resolve_t *store, bitset32_t const *named)
{
    uint32_t pos = 0, index = 0 ;
    selection_service_t lot = *up ;

    *reactors = *up ;

    FOREACH_SELECTION(&lot, pos, index) {

        struct resolve_hash_s *h ;
        int r = 0 ;

        if (bitset32_isvalid(named, index)) {

            if (service_graph_resolve_get(store, index, SERVICE_READ_EVENT, &h) <= 0)
                log_dieusys(LOG_EXIT_SYS, "read the services of the selection") ;

            r = svc_reactor_armed_idle(h) ;
            if (r < 0)
                log_dieusys(LOG_EXIT_SYS, "read the event of: ", service_graph_name(lot.g, index)) ;
        }

        selection_service_remove(r ? up : reactors, index) ;
    }
}

static int select_lots(selection_service_t *down, selection_service_t *up, selection_service_t *reactors, command_graph_t *d, char const *const *argv, int argc, ssexec_t *info, strbuf *frontends, bool nopropagate, bool *prepared)
{
    bitset32_t seeds ;
    uint32_t index = 0 ;
    service_graph_resolve_t *store = &d->store ;
    selection_service_t sel = SELECTION_SERVICE_ZERO, sup = SELECTION_SERVICE_ZERO, run = SELECTION_SERVICE_ZERO, back = SELECTION_SERVICE_ZERO ;
    selection_service_policy_t pdown = command_policy(SELECTION_SERVICE_PROPAGATE_REQUIREDBY, nopropagate, false, false) ;
    selection_service_policy_t pup = command_policy(SELECTION_SERVICE_PROPAGATE_DEPENDS, nopropagate, !info->opt_react, false) ;

    if (!command_argv_to_select(&sel, d, argv, argc, info, &pdown, frontends, SELECTION_SERVICE_REFUSE_NAMED_ABSENT | SELECTION_SERVICE_REFUSE_NAMED_INVALID | SELECTION_SERVICE_REFUSE_MET_INVALID))
        return SELECT_STALE ;

    if (svc_lot(down, &sel, store, SVC_LOT_COMMAND) < 0)
        log_dieusys(LOG_EXIT_SYS, "select the services to restart") ;

    command_refuse_unsupervised(&sup, down, store) ;

    if (selection_svc_filter(&run, &sup, store, SERVICE_READ_EVENT, svc_isrunning_supervised) < 0)
        log_dieusys(LOG_EXIT_SYS, "read the state of the selection") ;

    seeds = sel.named ;

    FOREACH_SELECTION_MEMBER(&run, index)
        bitset32_set(&seeds, index) ;

    if (!nopropagate && !*prepared) {
        *prepared = true ;
        prepare(&seeds, d, info) ;
        return SELECT_PARSED ;
    }

    selection_service_propagate(&back, &d->g, &seeds, &pup) ;

    if (!command_resolve_load(d, &back))
        return SELECT_STALE ;

    command_refuse_selection(&back, frontends, SELECTION_SERVICE_REFUSE_MET_ABSENT | SELECTION_SERVICE_REFUSE_NAMED_INVALID | SELECTION_SERVICE_REFUSE_MET_INVALID) ;

    if (svc_lot(up, &back, store, SVC_LOT_COMMAND) < 0)
        log_dieusys(LOG_EXIT_SYS, "select the services to bring back up") ;

    if (!command_refuse_conflict(up, d, info, SVC_CONFLICT_RUNNING))
        return SELECT_STALE ;

    split_reactors(up, reactors, store, &sel.named) ;

    if (service_graph_warn_invalid(&d->g, frontends, &sel.member) < 0)
        log_dieusys(LOG_EXIT_SYS, "warn about the invalid services") ;

    return SELECT_DONE ;
}

int ssexec_restart(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    bool nopropagate = opt_nopropagate ;
    opt_nopropagate = 0 ;

    int r, e ;
    bitset32_t reached ;
    bool prepared = false ;
    command_graph_t d = COMMAND_GRAPH_ZERO ;
    selection_service_t down = SELECTION_SERVICE_ZERO, up = SELECTION_SERVICE_ZERO, reactors = SELECTION_SERVICE_ZERO ;
    strbuf frontends = STRBUF_ZERO ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    command_refuse_scandir(info) ;

    if (service_graph_load(&d.g, info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "load the graph of the system") ;

    while ((r = select_lots(&down, &up, &reactors, &d, argv, argc, info, &frontends, nopropagate, &prepared)) != SELECT_DONE) {

        if (r == SELECT_PARSED)
            continue ;

        command_graph_reload(&d, info, true) ;
    }

    strbuf_free(&frontends) ;

    /** a stop that fails is the code of the command, whatever the start gives
     * after it; what comes back up comes back ready: a reactor armed
     * above the restarted service is armed again, it does not run */
    e = command_lot_stop(&down, &d.store, info, SVC_TARGET_DOWN, "-D", 0, 0) ;

    r = command_lot_start(&up, &d.store, info, SVC_TARGET_READY, SVC_LOT_COMMAND, &reached) ;

    if (!r)
        r = command_lot_start(&reactors, &d.store, info, SVC_TARGET_UP, SVC_LOT_COMMAND, 0) ;
    else {
        command_drop_waiters(&reactors, &up, &reached) ;
        command_lot_start(&reactors, &d.store, info, SVC_TARGET_READY, SVC_LOT_COMMAND, 0) ;
    }

    if (!e)
        e = r ;

    service_graph_resolve_free(&d.store) ;
    service_graph_free(&d.g) ;

    return e ;
}
