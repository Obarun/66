/*
 * ssexec_enable.c
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
#include <oblibs/sastr.h>
#include <oblibs/string.h>

#include <skalibs/sgetopt.h>
#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/ssexec.h>
#include <66/graph.h>
#include <66/service.h>
#include <66/config.h>
#include <66/state.h>
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

static void parse_it(char const *name, uint8_t force, uint8_t conf, ssexec_t *info)
{
    int argc = 4 + (force ? 1 : 0) + (conf ? 1 : 0) ;
    int m = 0 ;
    char const *prog = PROG ;
    char const *newargv[argc] ;

    newargv[m++] = "parse" ;
    if (force)
        newargv[m++] = force == 2 ? "-F" : "-f" ;
    if (conf)
        newargv[m++] = "-I" ;
    newargv[m++] = name ;
    newargv[m++] = 0 ;

    PROG = "parse" ;
    if (ssexec_parse(argc, newargv, info))
        log_dieu(LOG_EXIT_SYS, "parse service: ", name) ;
    PROG = prog ;
}

int ssexec_enable(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    uint32_t flag = 0 ;
    uint8_t force = 0, conf = 0, start = 0 ;
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
            int opt = subgetopt_r(argc, argv, OPTS_ENABLE, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 'f' :

                    /** only rewrite the service itself */
                    if (force)
                        log_usage(info->usage, "\n", info->help) ;
                    force = 1 ;
                    break ;

                case 'F' :

                     /** force to rewrite it dependencies */
                    if (force)
                        log_usage(info->usage, "\n", info->help) ;
                    force = 2 ;
                    break ;

                case 'I' :

                    conf = 1 ;
                    break ;

                case 'S' :

                    start = 1 ;
                    break ;

                default :
                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(info->usage, "\n", info->help) ;

    for(; n < argc ; n++) {
        check_identifier(argv[n]) ;
        parse_it(argv[n], force, conf, info) ;
    }

    /** build the graph of the entire system */
    graph_build_service(&graph, ares, &areslen, info, flag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- try first to install the corresponding frontend file") ;

    for (n = 0 ; n < argc ; n++) {
        /**
         *
         *
         * need to check if the service is declared at the same tree
         * than asked:
         *    sA on tA
         *    ask sA on tB by the -t options
         *
         * in this case we need to remove sA from the resolve file of the tree tA
         *
         *
         * */
        service_enable_disable(&graph, info->base.s, argv[n], 1) ;

        if (!sastr_add_string(&sa, argv[n]))
            log_dieu(LOG_EXIT_SYS, "add string") ;
    }

    if (start && sa.len) {

        size_t len = sastr_nelement(&sa) ;
        int nargc = 1 + len ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        newargv[m++] = "start" ;
        FOREACH_SASTR(&sa, pos)
            newargv[m++] = sa.s + pos ;
        newargv[m] = 0 ;

        PROG = "start" ;
        e = ssexec_start(nargc, newargv, info) ;
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
