/*
 * ssexec_restart.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <oblibs/types.h>
#include <oblibs/graph.h>


#include <skalibs/sgetopt.h>

#include <66/ssexec.h>
#include <66/config.h>
#include <66/graph.h>
#include <66/state.h>
#include <66/svc.h>
#include <66/sanitize.h>
#include <66/service.h>
#include <66/enum.h>

int ssexec_restart(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r, n = 0 ;
    uint32_t flag = 0 ;
    uint8_t siglen = 3 ;
    graph_t graph = GRAPH_ZERO ;
    struct resolve_hash_s *hres = NULL ;
    ss_state_t sta = STATE_ZERO ;
    unsigned int list[SS_MAX_SERVICE + 1], visit[SS_MAX_SERVICE + 1], nservice = 0 ;
    const char *const *pargv = 0 ;

    memset(list, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    memset(visit, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_TORESTART|STATE_FLAGS_WANTUP|STATE_FLAGS_WANTDOWN) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = subgetopt_r(argc, argv, OPTS_SUBSTART, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 'P' :

                    FLAGS_CLEAR(flag, STATE_FLAGS_TOPROPAGATE) ;
                    siglen++ ;
                    break ;

                default :

                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(info->usage, "\n", info->help) ;

    if ((svc_scandir_ok(info->scandir.s)) !=  1 )
        log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s, " is not running") ;


    graph_build_arguments(&graph, argv, argc, &hres, info, flag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- have you already parsed a service?") ;

    for (n = 0 ; n < argc ; n++) {

        struct resolve_hash_s *hash = hash_search(&hres, argv[n]) ;
        if (hash == NULL)
            log_dieu(LOG_EXIT_USER, "find service: ", argv[n], " -- did you parse it?") ;

        if (!state_read(&sta, &hash->res))
            log_dieu(LOG_EXIT_SYS, "read state file of: ", argv[n]) ;

        if (sta.issupervised == STATE_FLAGS_FALSE)
            /** nothing to do */
            log_warn_return(LOG_EXIT_ZERO, "service: ", argv[n], " is not supervised -- try to start it first using '66 start ", argv[n], "'") ;

        /** be sure to retrieve the exact same service selection state
         * with its own dependencies after bringing it down.
         * ssexec_signal will not be aware about its requiredby dependencies at up time.
         * The service selection list is therefore explicitly passed.
        */
        if (siglen <= 3)
            graph_compute_visit(*hash, visit, list, &graph, &nservice, 1) ;
        else
            nservice++ ;
    }

    char const *newargv[nservice + 1] ;

    memset(newargv, 0, (nservice + 1) * sizeof(char)) ;

    char *sig[siglen] ;
    sig[0] = "-wD" ;
    sig[1] = "-D" ;

    if (siglen > 3) {

        sig[2] = "-P" ;
        sig[3] = 0 ;

        pargv = argv ;
        nservice = argc ;

    } else {

        sig[2] = 0 ;
        unsigned int m = 0 ;
        for (n = 0 ; n < nservice ; n++) {

            char *name = graph.data.s + genalloc_s(graph_hash_t,&graph.hash)[list[n]].vertex ;

            r = service_is_g(name, STATE_FLAGS_ISSUPERVISED) ;
            if (r < 0)
                log_warnusys("get information of service: ", name, " -- please make a bug report") ;

            if (r || r == STATE_FLAGS_TRUE)
                newargv[m++] = name ;

        }
        newargv[m] = 0 ;
        pargv = newargv ;
    }

    r = svc_send_wait(argv, argc, sig, siglen, info) ;

    if (r)
        log_warnusys("stop service selection") ;

    sig[0] = "-wU" ;
    sig[1] = "-U" ;

    r = svc_send_wait(pargv, nservice, sig, siglen, info) ;

    hash_free(&hres) ;
    graph_free_all(&graph) ;

    return r ;
}
