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
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>
#include <stdbool.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/hash.h>
#include <oblibs/types.h>
#include <oblibs/environ.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/config.h>

static opt_t const opts_disable[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
    { .id = 'S',         .shortname = 'S', .longname = "stop", .arg = OPT_NONE, .help = "also stop the service immediately" },
    { .id = 'P',         .shortname = 'P',                     .arg = OPT_NONE, .help = "do not propagate to its requiredby", .hidden = true },
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

int ssexec_disable(int argc, char const *const *argv, void *data)
{
    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    uint8_t stop_opt = opt_stop, nopropagate = opt_nopropagate ;
    opt_stop = 0 ;
    opt_nopropagate = 0 ;

    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    bool stop = false, propagate = true, action = false ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    vertex_t *c, *tmp ;
    int e = 1 ;
    uint32_t flag = GRAPH_WANT_REQUIREDBY, nservice = 0 ;

    if (stop_opt)
        stop = true ;

    if (nopropagate) {
        FLAGS_CLEAR(flag, GRAPH_WANT_REQUIREDBY) ;
        propagate = false ;
    }

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    if (!graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the service graph") ;

    if (!environ_import_arguments(&sa, argv, argc))
        log_dieusys(LOG_EXIT_SYS, "import arguments") ;

    nservice = service_graph_build_list(&graph, sa.s, sa.len, info, flag) ;

    if (!nservice)
        log_die(LOG_EXIT_USER, "services selection is not available -- try to parse it first") ;

    hash_reset_visit(graph.hres) ;

    nservice = 0 ;
    FOREACH_GRAPH_SORT(service_graph_t, &graph, nservice) {

        uint32_t index = graph.g.sort[nservice] ;
        vertex_t *v = graph.g.sindex[index] ;
        char *name = v->name ;
        struct resolve_hash_s *hash = hash_search(&graph.hres, name) ;

        if (hash == NULL)
            log_die(LOG_EXIT_SYS, "get information of service: ", name, " -- please make a bug report") ;

        if (!hash->visit)
            service_enable_disable(&graph, hash, action, propagate, info, &sa) ;
    }

    e = 0 ;

    if (stop && graph.g.nvertexes) {

        int nargc = 3 + graph.g.nvertexes ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        newargv[m++] = "stop" ;
        newargv[m++] = "-u" ;
        HASH_ITER(hh, graph.g.vertexes, c, tmp)
            newargv[m++] = c->name ;
        newargv[m] = 0 ;

        PROG = "stop" ;
        e = opt_dispatch(m, newargv, &cmd_stop, info) ;
        PROG = prog ;
    }

    service_graph_destroy(&graph) ;

    return e ;
}

