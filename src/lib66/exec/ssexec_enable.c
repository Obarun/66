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
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>
#include <stdbool.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/hash.h>
#include <oblibs/types.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/graph.h>
#include <oblibs/environ.h>

#include <66/ssexec.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/enum_parser.h>
#include <66/config.h>

static opt_t const opts_enable[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",  .arg = OPT_NONE, .help = "print this help" },
    { .id = 'S',         .shortname = 'S', .longname = "start", .arg = OPT_NONE, .help = "also start the service immediately" },
    { .id = 'P',         .shortname = 'P', .longname = 0,       .arg = OPT_NONE, .help = "do not propagate to dependencies", .hidden = true },
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

int ssexec_enable(int argc, char const *const *argv, void *data)
{
    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    uint8_t start_opt = opt_start, nopropagate = opt_nopropagate ;
    opt_start = 0 ;
    opt_nopropagate = 0 ;

    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    bool start = false, action = true ; /* action=true -> enable */
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    vertex_t *c, *tmp ;
    int e = 1 ;
    uint32_t flag = GRAPH_WANT_DEPENDS|GRAPH_COLLECT_PARSE, nservice = 0 ;

    if (start_opt)
        start = true ;

    if (nopropagate)
        FLAGS_CLEAR(flag, GRAPH_WANT_DEPENDS) ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    if (!service_graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the service graph") ;

    if (!environ_import_arguments(&sa, argv, argc))
        log_dieusys(LOG_EXIT_SYS, "import arguments") ;

    nservice = service_graph_build_list(&graph, sa.s, sa.len, info, flag) ;

    if (!nservice)
        log_die(LOG_EXIT_USER, "services selection is not available -- please make a bug report") ;

    resolve_hash_reset_visit(&graph.hres) ;
    nservice = 0 ;

    FOREACH_GRAPH_SORT(service_graph_t, &graph, nservice) {

        uint32_t index = graph.g.sort[nservice] ;
        vertex_t *v = graph.g.sindex[index] ;
        char *name = v->name ;
        struct resolve_hash_s *hash = resolve_hash_search(&graph.hres, name) ;

        if (hash == NULL)
            log_die(LOG_EXIT_SYS, "get information of service: ", name, " -- please make a bug report") ;

        if (!hash->visit)
            service_enable_disable(&graph, hash, action, info, &sa) ;

        /**
         * We only want the service asked by user. Doing '66 -t test enable sB'
         * where sB depends on sA should only move the associated tree for
         * service sB leaving sA at its initial state.*/
        if (info->opt_tree && ((hash->res.inns && sbl_search(&sa, hash->res.sa.s + hash->res.inns) >= 0) || sbl_search(&sa, name) >= 0)) {

            service_switch_tree(&hash->res, info->treename.s, info) ;

            if (hash->res.logger.want && hash->res.type == E_PARSER_TYPE_CLASSIC) {

                struct resolve_hash_s *log = resolve_hash_search(&graph.hres, hash->res.sa.s + hash->res.logger.name) ;
                if (log == NULL)
                    log_die(LOG_EXIT_USER, "service: ", hash->res.sa.s + hash->res.logger.name, " not available -- please make a bug report") ;

                service_switch_tree(&log->res, info->treename.s, info) ;
            }
        }
    }

    e = 0 ;

    if (start && graph.g.nvertexes) {

        int nargc = 2 + graph.g.nvertexes ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        newargv[m++] = "start" ;
        HASH_FOREACH(&graph.g.vertexes, c, tmp)
            newargv[m++] = c->name ;
        newargv[m] = 0 ;

        PROG = "start" ;
        e = opt_dispatch(m, newargv, &cmd_start, info) ;
        PROG = prog ;
    }

    service_graph_destroy(&graph) ;

    return e ;
}
