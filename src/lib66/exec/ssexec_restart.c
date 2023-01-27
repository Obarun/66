/*
 * ssexec_restart.c
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

    int r, nargc = 0, n = 0 ;
    uint32_t flag = 0 ;
    uint8_t siglen = 3 ;
    graph_t graph = GRAPH_ZERO ;

    unsigned int areslen = 0, m = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE] ;
    char atree[SS_MAX_TREENAME + 1] ;

    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_TORESTART|STATE_FLAGS_WANTUP) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = subgetopt_r(argc, argv, OPTS_SUBSTART, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'P' :

                    FLAGS_CLEAR(flag, STATE_FLAGS_TOPROPAGATE) ;
                    siglen++ ;
                    break ;

                default :

                    log_usage(usage_start) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(usage_start) ;

    if ((svc_scandir_ok(info->scandir.s)) !=  1 )
        log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s, " is not running") ;

    char const *nargv[argc] ;

    for (; n < argc ; n++) {

        r = service_is_g(atree, argv[n], STATE_FLAGS_ISPARSED) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "get information of service: ", argv[n], " -- please a bug report") ;

        if (!r) {
            /** nothing to do */
            log_warn(argv[n], " is not parsed -- try to start it first") ;
            return 0 ;
        }

        r = service_is_g(atree, argv[n], STATE_FLAGS_ISSUPERVISED) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "get information of service: ", argv[n], " -- please a bug report") ;

        if (!r) {
            /** nothing to do */
            log_warn(argv[n], " is not running -- try to start it first") ;
            return 0 ;
        }
    }

    /** build the graph of the entire system */
    graph_build_service(&graph, ares, &areslen, info, flag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- try first to install the corresponding frontend file") ;

    for (n = 0 ; n < argc ; n++) {

        int aresid = service_resolve_array_search(ares, areslen, argv[n]) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", *argv, " not available -- did you parsed it?") ;

        if (ares[aresid].type == TYPE_ONESHOT) {
            nargc++ ;
            nargv[m++] = ares[aresid].sa.s + ares[aresid].name ;
        }
    }

    if (nargc)
        nargv[m] = 0 ;

    char *sig[siglen] ;
    if (siglen > 3) {

        sig[0] = "-P" ;
        sig[1] = "-wR" ;
        sig[2] = "-ru" ;
        sig[3] = 0 ;

    } else {

        sig[0] = "-wR" ;
        sig[1] = "-ru" ;
        sig[2] = 0 ;
    }

    r = svc_send_wait(argv, argc, sig, siglen, info) ;
    if (r)
        goto err ;
    /** s6-supervise do not deal with oneshot service:
     * The previous send command will bring it down but
     * s6-supervise will not bring it up automatically.
     * Well, do it manually */

    if (nargc) {

        int verbo = VERBOSITY ;
        VERBOSITY = 0 ;

        if (siglen > 3) {

            sig[0] = "-P" ;
            sig[1] = "-wU" ;
            sig[2] = "-u" ;
            sig[3] = 0 ;

        } else {

            sig[0] = "-wU" ;
            sig[1] = "-u" ;
            sig[2] = 0 ;
        }

        r = svc_send_wait(nargv, nargc, sig, siglen, info) ;
        VERBOSITY = verbo ;
    }

    err:
        service_resolve_array_free(ares, areslen) ;
        graph_free_all(&graph) ;

        return r ;
}
