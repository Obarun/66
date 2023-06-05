/*
 * ssexec_signal.c
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
#include <oblibs/graph.h>
#include <oblibs/types.h>

#include <skalibs/sgetopt.h>

#include <66/svc.h>
#include <66/config.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/service.h>
#include <66/state.h>
#include <66/enum.h>

#include <s6/supervise.h>//s6_svstatus_t

static pidservice_t pidservice_init(unsigned int len)
{
    log_flow() ;

    pidservice_t pids = PIDSERVICE_ZERO ;

    if (len > SS_MAX_SERVICE)
        log_die(LOG_EXIT_SYS, "too many services") ;

    graph_array_init_single(pids.edge, len) ;

    return pids ;
}

static void pidservice_init_array(unsigned int *list, unsigned int listlen, pidservice_t *apids, graph_t *g, resolve_service_t *ares, unsigned int areslen, ssexec_t *info, uint8_t requiredby, uint32_t flag) {

    log_flow() ;

    int r = 0 ;
    unsigned int pos = 0 ;

    for (; pos < listlen ; pos++) {

        pidservice_t pids = pidservice_init(g->mlen) ;

        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[list[pos]].vertex ;

        pids.aresid = service_resolve_array_search(ares, areslen, name) ;

        if (pids.aresid < 0)
            log_dieu(LOG_EXIT_SYS,"find ares id of: ", name, " -- please make a bug reports") ;

        if (FLAGS_ISSET(flag, STATE_FLAGS_TOPROPAGATE)) {

            pids.nedge = graph_matrix_get_edge_g_sorted_list(pids.edge, g, name, requiredby, 1) ;

            if (pids.nedge < 0)
                log_dieu(LOG_EXIT_SYS,"get sorted ", requiredby ? "required by" : "dependency", " list of service: ", name) ;

            pids.nnotif = graph_matrix_get_edge_g_sorted_list(pids.notif, g, name, !requiredby, 1) ;

            if (pids.nnotif < 0)
                log_dieu(LOG_EXIT_SYS,"get sorted ", !requiredby ? "required by" : "dependency", " list of service: ", name) ;

        }

        pids.vertex = graph_hash_vertex_get_id(g, name) ;

        if (pids.vertex < 0)
            log_dieu(LOG_EXIT_SYS, "get vertex id -- please make a bug report") ;

        if (ares[pids.aresid].type != TYPE_CLASSIC) {

                ss_state_t sta = STATE_ZERO ;

                if (!state_read(&sta, &ares[pids.aresid]))
                    log_dieusys(LOG_EXIT_SYS, "read state file of: ", name) ;

                if (service_is(&sta, STATE_FLAGS_ISUP) == STATE_FLAGS_TRUE)
                    FLAGS_SET(pids.state, SVC_FLAGS_UP) ;
                else
                    FLAGS_SET(pids.state, SVC_FLAGS_DOWN) ;

        } else {

            s6_svstatus_t status ;

            r = s6_svstatus_read(ares[pids.aresid].sa.s + ares[pids.aresid].live.scandir, &status) ;

            pid_t pid = !r ? 0 : status.pid ;

            if (pid > 0) {

                FLAGS_SET(pids.state, SVC_FLAGS_UP) ;
            }
            else
                FLAGS_SET(pids.state, SVC_FLAGS_DOWN) ;
        }

        apids[pos] = pids ;
    }

}

int ssexec_signal(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r ;
    uint8_t what = 1, requiredby = 0, propagate = 1 ;
    graph_t graph = GRAPH_ZERO ;

    unsigned int napid = 0 ;

    char updown[4] = "-w \0" ;
    uint8_t opt_updown = 0 ;
    char data[DATASIZE + 1] = "-" ;
    unsigned int datalen = 1 ;
    uint8_t reloadmsg = 0 ;

    unsigned int areslen = 0, list[SS_MAX_SERVICE + 1], visit[SS_MAX_SERVICE + 1] ;
    resolve_service_t ares[SS_MAX_SERVICE + 1] ;

    /*
     * STATE_FLAGS_TOPROPAGATE = 0
     * do not send signal to the depends/requiredby of the service.
     *
     * STATE_FLAGS_TOPROPAGATE = 1
     * send signal to the depends/requiredby of the service
     *
     * When we come from 66 start/stop tool we always want to
     * propagate the signal. But we may need/want to send a e.g. SIGHUP signal
     * to a specific service without interfering on its depends/requiredby services
     *
     * Also, we only deal with already supervised service. This tool is the signal sender,
     * it not intended to sanitize the state of the services.
     *
     * */
    uint32_t gflag = STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_ISSUPERVISED|STATE_FLAGS_WANTUP ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc,argv, OPTS_SIGNAL, &l) ;
            if (opt == -1) break ;

            switch (opt) {
                case 'h' : info_help(info->help, info->usage) ; return 0 ;
                case 'a' :
                case 'b' :
                case 'q' :
                case 'H' :
                case 'k' :
                case 't' :
                case 'i' :
                case '1' :
                case '2' :
                case 'p' :
                case 'c' :
                case 'y' :
                case 'r' :
                case 'o' :
                case 'd' :
                case 'D' :
                case 'u' :
                case 'U' :
                case 'x' :
                case 'O' :
                case 'Q' :

                    if (datalen >= DATASIZE)
                        log_die(LOG_EXIT_USER, "too many arguments") ;

                    data[datalen++] = opt == 'H' ? 'h' : opt ;
                    break ;

                case 'w' :

                    if (!memchr("dDuUrR", l.arg[0], 6))
                        log_usage(info->usage, "\n", info->help) ;

                    updown[2] = l.arg[0] ;
                    opt_updown = 1 ;
                    break ;

                case 'P':
                    propagate = 0 ;
                    FLAGS_CLEAR(gflag, STATE_FLAGS_TOPROPAGATE) ;
                    break ;

                default :
                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1 || datalen < 2)
        log_usage(info->usage, "\n", info->help) ;

    if (data[1] == 'u' || data[1] == 'U')
        what = 0 ;

    if (data[1] == 'r')
        reloadmsg = 1 ;
    else if (data[1] == 'h')
        reloadmsg = 2 ;

    if (what) {
        requiredby = 1 ;
        FLAGS_SET(gflag, STATE_FLAGS_WANTDOWN) ;
        FLAGS_CLEAR(gflag, STATE_FLAGS_WANTUP) ;
    }

    if ((svc_scandir_ok(info->scandir.s)) != 1)
        log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;

    /** build the graph of the entire system.*/
    graph_build_service(&graph, ares, &areslen, info, gflag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not supervised -- initiate its first") ;

    graph_array_init_single(visit, SS_MAX_SERVICE) ;

    for (; *argv ; argv++) {

        int aresid = service_resolve_array_search(ares, areslen, *argv) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", *argv, " not available -- did you parse it?") ;

        graph_compute_visit(*argv, visit, list, &graph, &napid, requiredby) ;
    }

    pidservice_t apids[napid] ;

    pidservice_init_array(list, napid, apids, &graph, ares, areslen, info, requiredby, gflag) ;

    r = svc_launch(apids, napid, what, &graph, ares, info, updown, opt_updown, reloadmsg, data, propagate) ;

    graph_free_all(&graph) ;

    service_resolve_array_free(ares, areslen) ;

    return r ;
}
