/*
 * ssexec_disable.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/sgetopt.h>
#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/ssexec.h>
#include <66/config.h>
#include <66/service.h>
#include <66/graph.h>
#include <66/resolve.h>

static void check_identifier(char const *name)
{
    log_flow() ;

    int logname = get_rstrlen_until(name,SS_LOG_SUFFIX) ;
    if (logname > 0) log_die(LOG_EXIT_USER,"service: ",name,": ends with reserved suffix -log") ;
    if (!memcmp(name,SS_MASTER+1,6)) log_die(LOG_EXIT_USER,"service: ",name,": starts with reserved prefix Master") ;
    if (!strcmp(name,SS_SERVICE)) log_die(LOG_EXIT_USER,"service as service name is a reserved name") ;
    if (!strcmp(name,"service@")) log_die(LOG_EXIT_USER,"service@ as service name is a reserved name") ;
}

int ssexec_disable(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    uint32_t flag = 0 ;
    uint8_t stop = 0 ;
    int n = 0, e = 1 ;
    size_t pos = 0 ;
    graph_t graph = GRAPH_ZERO ;
    stralloc sa = STRALLOC_ZERO ;

    unsigned int areslen = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE] ;

    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTUP) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_DISABLE, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 'S' :

                    stop = 1 ;
                    break ;

                case 'F' :

                    log_1_warn("options -F is dead -- skipping it") ;
                    break ;

                case 'R' :

                    log_1_warn("options -F is deprecated -- use instead 66 service remove <service>") ;
                    break ;

                default :
                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(info->usage, "\n", info->help) ;

    /** build the graph of the entire system */
    graph_build_service(&graph, ares, &areslen, info, flag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- try to parse it first") ;

    for (; n < argc ; n++) {
        check_identifier(argv[n]) ;
        service_enable_disable(&graph, info->base.s, argv[n], 0) ;

        if (!sastr_add_string(&sa, argv[n]))
            log_dieu(LOG_EXIT_SYS, "add string") ;
    }

    if (stop && sa.len) {

        size_t len = sastr_nelement(&sa) ;
        int nargc = 2 + len ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        newargv[m++] = "stop" ;
        newargv[m++] = "-u" ;
        FOREACH_SASTR(&sa, pos)
            newargv[m++] = sa.s + pos ;
        newargv[m] = 0 ;

        PROG = "stop" ;
        e = ssexec_stop(nargc, newargv, info) ;
        PROG = prog ;
        goto end ;
    }
    e = 0 ;

    end:
        stralloc_free(&sa) ;
        service_resolve_array_free(ares, areslen) ;
        graph_free_all(&graph) ;

    return e ;
}

