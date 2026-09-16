/*
 * ssexec_start.c
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/types.h>
#include <oblibs/hash.h>
#include <oblibs/sbl.h>
#include <oblibs/graph.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/svc.h>
#include <66/sanitize.h>
#include <66/config.h>
#include <66/service.h>
#include <66/constants.h>

static void ensure_no_conflict(service_graph_t *graph, char const *name)
{
    struct resolve_hash_s *hash = resolve_hash_search(&graph->hres, name) ;

    if (hash == NULL)
        log_die(LOG_EXIT_USER, "service: ", name, " not available -- please make a bug report") ;

    if (hash->dependencies.nconflict) {

        char sv[SS_MAX_SERVICE_NAME + 1] ;
        _alloc_sbl_(stk, strlen(hash->dependencies.sa.s + hash->dependencies.conflict)) ;
        size_t pos = 0 ;
        int r ;

        if (!sbl_clean_string(&stk, hash->dependencies.sa.s + hash->dependencies.conflict))
            log_dieu(LOG_EXIT_SYS, "clean string") ;

        FOREACH_SBL(&stk, pos) {

            if (!service_resolve_provide(sv, stk.s + pos, hash->res.sa.s + hash->res.path.home))
                log_dieusys(LOG_EXIT_SYS, "resolve provide alias: ", stk.s + pos) ;

            r = svc_is_up(sv) ;
            if (r > 0)
                log_die(LOG_EXIT_SYS, "conflicting service for '", hash->res.sa.s + hash->res.name, "' -- please stop the '", stk.s + pos, "' service first.") ;
        }
    }
}

static uint8_t opt_nopropagate = 0 ;

static opt_t const opts_start[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",         .arg = OPT_NONE, .help = "print this help" },
    { .id = 'P',         .shortname = 'P', .longname = "no-propagate", .arg = OPT_NONE, .help = "do not propagate signal to its dependencies" },
} ;

static int on_start(int id, char const *arg, void *data)
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

opt_cmd_t const cmd_start = {
    .name = "66 start",
    .help = "bring up services",
    .operands = "service...",
    .opts = opts_start,
    .nopts = OPT_COUNT(opts_start),
    .on_option = &on_start,
    .fn = &ssexec_start,
} ;

int ssexec_start(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    uint8_t nopropagate = opt_nopropagate ;
    opt_nopropagate = 0 ;

    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    vertex_t *c, *tmp ;
    uint32_t flag = GRAPH_WANT_DEPENDS|GRAPH_COLLECT_PARSE|/* sanitize_init */GRAPH_WANT_LOGGER, nservice = 0 ;
    int e = 0 ;

    if (nopropagate)
        FLAGS_CLEAR(flag, GRAPH_WANT_DEPENDS) ;

    /* an arm pulls a service/signal reactor's From sources (establishment); a
     * fire-time start from 66-eventd (opt_react) must not, or the reaction would
     * re-pull and revive its own trigger. */
    if (!info->opt_react)
        FLAGS_SET(flag, GRAPH_WANT_EVENTDEPS) ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    if ((svc_scandir_ok(info->scandir.s)) !=  1 )
        log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s, " is not running") ;

    if (!service_graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the graph") ;

    nservice = service_graph_build_arguments(&graph, argv, argc, info, flag) ;
    if (!nservice)
        log_dieusys(LOG_EXIT_SYS, "build the service selection graph") ;

    if (!graph.g.nsort)
        log_warn_return(e,"no services found to handle") ;

    char const *nargv[nservice + 1] ;
    nservice = 0 ;
    HASH_FOREACH(&graph.g.vertexes, c, tmp) {
        ensure_no_conflict(&graph, c->name) ;
        nargv[nservice++] = c->name ;
    }
    nargv[nservice] = 0 ;

    /** initiate services at the corresponding scandir */
    sanitize_init(&graph, flag, info->who) ;

    uint8_t target = info->target ? info->target : SVC_TARGET_READY ;

    e = svc_send(nargv, nservice, info, target, "-u", "-wU", 1, nopropagate ? 0 : 1) ;

    service_graph_destroy(&graph) ;

    return e ;
}
