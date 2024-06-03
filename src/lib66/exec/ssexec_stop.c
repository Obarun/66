/*
 * ssexec_stop.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <oblibs/sastr.h>
#include <oblibs/string.h>

#include <skalibs/sgetopt.h>
#include <skalibs/djbunix.h>
#include <skalibs/genalloc.h>

#include <66/graph.h>
#include <66/config.h>
#include <66/ssexec.h>
#include <66/state.h>
#include <66/svc.h>
#include <66/service.h>
#include <66/enum.h>
#include <66/constants.h>

int ssexec_stop(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    uint32_t flag = 0 ;
    graph_t graph = GRAPH_ZERO ;
    uint8_t siglen = 3 ;
    int e = 0 ;
    struct resolve_hash_s *hres = NULL ;
    unsigned int list[SS_MAX_SERVICE + 1], visit[SS_MAX_SERVICE + 1], nservice = 0, pos = 0, idx = 0 ;

    memset(list, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    memset(visit, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_ISSUPERVISED|STATE_FLAGS_WANTDOWN) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = subgetopt_r(argc,argv, OPTS_STOP, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 'P' :

                    FLAGS_CLEAR(flag, STATE_FLAGS_TOPROPAGATE) ;
                    siglen++ ;
                    break ;

                case 'u' :

                    FLAGS_SET(flag, STATE_FLAGS_TOUNSUPERVISE|STATE_FLAGS_WANTUP) ;
                    break ;

                case 'X' :

                    log_1_warn("deprecated option -- use 66 signal -xd instead") ;
                    return 0 ;

                case 'K' :

                    log_1_warn("deprecated option -- use 66 signal -kd instead") ;
                    return 0 ;

                default :
                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(info->usage, "\n", info->help) ;

    if ((svc_scandir_ok(info->scandir.s)) != 1)
        log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;

    graph_build_arguments(&graph, argv, argc, &hres, info, flag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- did you start it first?") ;

    for (; pos < argc ; pos++) {

        /** The service may not be supervised, so it will be ignored by the
         * function graph_build_arguments. In this case, the service does not
         * exist at array.
         *
         * This the stop process, just ignore it as it already down anyway */
        struct resolve_hash_s *hash = hash_search(&hres, argv[pos]) ;
        if (hash == NULL) {
            log_warn("service: ", argv[pos], " is already stopped or unsupervised -- ignoring it") ;
            continue ;
        }
        graph_compute_visit(*hash, visit, list, &graph, &nservice, 1) ;
    }

    if (!nservice)
        log_dieu(LOG_EXIT_USER, "find service: ", argv[0], " -- not currently in use") ;

    char *sig[siglen] ;
    if (siglen > 3) {

        sig[0] = "-P" ;
        sig[1] = "-wD" ;
        sig[2] = "-d" ;
        sig[3] = 0 ;

    } else {

        sig[0] = "-wD" ;
        sig[1] = "-d" ;
        sig[2] = 0 ;
    }

    unsigned int flist[SS_MAX_SERVICE + 1], fvisit[SS_MAX_SERVICE + 1], fnservice = 0 ;
    char const *nargv[(nservice * 2) + 1] ; // nservice * 2 -> at worse one logger per service
    unsigned int nargc = 0 ;

    idx = 0 ;
    memset(flist, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    memset(fvisit, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;

    for (pos = 0 ; pos < nservice ; pos++) {

        char *name = graph.data.s + genalloc_s(graph_hash_t, &graph.hash)[list[pos]].vertex ;
        nargv[nargc++] = name ;

        idx = graph_hash_vertex_get_id(&graph, name) ;

        if (!fvisit[idx]) {
            flist[fnservice++] = idx ;
            fvisit[idx] = 1 ;
        }

        struct resolve_hash_s *hash = hash_search(&hres, name) ;
        if (hash == NULL)
            log_die(LOG_EXIT_USER, "service: ", name, " not available -- please make a bug report") ;

        /** the logger need to be stopped in case of unsupervise request */
        if (FLAGS_ISSET(flag, STATE_FLAGS_TOUNSUPERVISE)) {

            if (get_rstrlen_until(name, SS_LOG_SUFFIX) < 0 && hash->res.logger.want) {

                hash = hash_search(&hres, hash->res.sa.s + hash->res.logger.name) ;
                if (hash == NULL)
                    continue ;

                nargv[nargc++] = hash->res.sa.s + hash->res.logger.name ;
                idx = graph_hash_vertex_get_id(&graph, hash->res.sa.s + hash->res.logger.name) ;
                if (!fvisit[idx]) {
                    flist[fnservice++] = idx ;
                    fvisit[idx] = 1 ;
                }
            }
        }
    }

    nargv[nargc] = 0 ;

    e = svc_send_wait(nargv, nargc, sig, siglen, info) ;

    if (FLAGS_ISSET(flag, STATE_FLAGS_TOUNSUPERVISE))
        svc_unsupervise(flist, fnservice, &graph, &hres, info) ;

    hash_free(&hres) ;
    graph_free_all(&graph) ;

    return e ;
}
