/*
 * ssexec_init.c
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
#include <oblibs/graph.h>

#include <skalibs/stralloc.h>
#include <skalibs/sgetopt.h>

#include <66/constants.h>
#include <66/config.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/svc.h>
#include <66/ssexec.h>
#include <66/state.h>
#include <66/graph.h>
#include <66/sanitize.h>
#include <66/utils.h>

static void doit(stralloc *sa, ssexec_t *info, uint8_t earlier)
{
    log_flow() ;

    uint32_t flag = 0 ;
    graph_t graph = GRAPH_ZERO ;

    unsigned int areslen = 0, list[SS_MAX_SERVICE + 1], visit[SS_MAX_SERVICE + 1], nservice = 0, n = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE + 1] ;

    memset(&visit, 0, SS_MAX_SERVICE + 1 * sizeof(unsigned int)) ;

    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_WANTUP) ;

    if (earlier)
        FLAGS_SET(flag, STATE_FLAGS_ISEARLIER) ;

    /** build the graph of the entire system */
    graph_build_service(&graph, ares, &areslen, info, flag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not available -- try first to install the corresponding frontend file") ;

    FOREACH_SASTR(sa, n) {

        int aresid = service_resolve_array_search(ares, areslen, sa->s + n) ;
        if (aresid < 0) {

            if (earlier) {
                log_trace("ignoring none earlier service: ", sa->s + n) ;
                continue ;
            }
            log_die(LOG_EXIT_USER, "service: ", sa->s + n, " not available -- please execute \"66 parse ", sa->s + n,"\" command first") ;
        }

        unsigned int l[graph.mlen], c = 0, pos = 0, idx = 0 ;

        idx = graph_hash_vertex_get_id(&graph, sa->s + n) ;

        if (!visit[idx]) {

            if (earlier) {

                if (ares[aresid].earlier) {

                    list[nservice++] = idx ;
                    visit[idx] = 1 ;
                }

            } else {

                if (ares[aresid].enabled) {

                    list[nservice++] = idx ;
                    visit[idx] = 1 ;

                } else {

                    log_trace("ignoring not enabled service: ", ares[aresid].sa.s + ares[aresid].name) ;

                }
            }

        }

        /** find dependencies of the service from the graph, do it recursively */
        c = graph_matrix_get_edge_g_list(l, &graph, sa->s + n, 0, 1) ;

        /** append to the list to deal with */
        for (; pos < c ; pos++) {

            if (!visit[l[pos]]) {

                char *name = graph.data.s + genalloc_s(graph_hash_t,&graph.hash)[l[pos]].vertex ;

                aresid = service_resolve_array_search(ares, areslen, name) ;
                if (aresid < 0)
                    log_die(LOG_EXIT_USER, "service: ", name, " not available -- did you parse it?") ;

                if (earlier) {

                    if (ares[aresid].earlier) {

                        list[nservice++] = l[pos] ;
                        visit[l[pos]] = 1 ;
                    }

                } else {

                    if (ares[aresid].enabled) {

                        list[nservice++] = l[pos] ;
                        visit[l[pos]] = 1 ;

                    }
                }
            }
        }
    }

    sanitize_init(list, nservice, &graph, ares, areslen) ;

    service_resolve_array_free(ares, areslen) ;
    graph_free_all(&graph) ;
}

int ssexec_init(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r ;
    uint8_t earlier = 0 ;
    char const *treename = 0 ;

    stralloc sa = STRALLOC_ZERO ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_INIT, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                default :
                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (!argc)
        log_usage(info->usage, "\n", info->help) ;

    treename = argv[0] ;

    if (!tree_isvalid(info->base.s, treename))
        log_diesys(LOG_EXIT_USER, "invalid tree name: ", treename) ;

    if (!tree_get_permissions(info->base.s, treename))
        log_die(LOG_EXIT_USER, "You're not allowed to use the tree: ", treename) ;

    r = scan_mode(info->scandir.s, S_IFDIR) ;
    if (r < 0) log_die(LOG_EXIT_SYS,info->scandir.s, " conflicted format") ;
    if (!r) log_die(LOG_EXIT_USER,"scandir: ", info->scandir.s, " doesn't exist -- please execute \"66 scandir create\" command first") ;

    r = svc_scandir_ok(info->scandir.s) ;
    if (r != 1) earlier = 1 ;

    if (!resolve_get_field_tosa_g(&sa, info->base.s, treename, DATA_TREE, E_RESOLVE_TREE_CONTENTS))
        log_dieu(LOG_EXIT_SYS, "get services list from tree: ", treename) ;

    if (sa.len) {

         doit(&sa, info, earlier) ;

    } else {

        log_info("Report: no services to initiate at tree: ", treename) ;
    }

    stralloc_free(&sa) ;
    return 0 ;
}
