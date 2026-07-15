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
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/types.h>
#include <oblibs/hash.h>

#include <66/ssexec.h>
#include <66/config.h>
#include <66/graph.h>
#include <66/svc.h>
#include <66/service.h>
#include <66/enum_parser.h>

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

int ssexec_reload(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    uint8_t nopropagate = opt_nopropagate ;
    opt_nopropagate = 0 ;

    int r, nargc = 0 ;
    char const *nargv[argc] ;
    vertex_t *c, *tmp ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_WANT_SUPERVISED|GRAPH_WANT_DEPENDS, nservice = 0 ;

    unsigned int m = 0 ;

    if (nopropagate)
        FLAGS_CLEAR(flag, GRAPH_WANT_DEPENDS) ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    if ((svc_scandir_ok(info->scandir.s)) !=  1 )
        log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s, " is not running") ;

    if (!service_graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the service graph") ;

    nservice = service_graph_build_arguments(&graph, argv, argc, info, flag) ;

    if (!nservice) {
        if (errno == EINVAL)
            log_dieusys(LOG_EXIT_SYS, "build the graph") ;
        log_warn_return(LOG_EXIT_ZERO, "service selection is not supervised -- try to start it first") ;
    }

    r = svc_send(argv, argc, info, "-l", "-w ", 0, nopropagate ? 0 : 1) ;
    if (r) {
        service_graph_destroy(&graph) ;
        return r ;
    }

    /** the supervisor does not deal with oneshot services:
     * the previous send command will bring it down but
     * the supervisor will not bring it up automatically.
     * Well, do it manually */

    HASH_FOREACH(&graph.g.vertexes, c, tmp) {

        struct resolve_hash_s *h = NULL ;
        h = resolve_hash_search(&graph.hres, c->name) ;
        if (h == NULL)
            log_dieusys(LOG_EXIT_SYS, "find service: ", c->name, " -- please make a bug report") ;

        if (h->res.type == E_PARSER_TYPE_ONESHOT) {
            nargc++ ;
            nargv[m++] = c->name ;
        }

    }

    if (nargc) {

        nargv[m] = 0 ;
        int verbo = VERBOSITY ;
        VERBOSITY = 0 ;
        r = svc_send(nargv, nargc, info, "-u", "-wU", 1, nopropagate ? 0 : 1) ;
        VERBOSITY = verbo ;
    }

    service_graph_destroy(&graph) ;

    return r ;
}
