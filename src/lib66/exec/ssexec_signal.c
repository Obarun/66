/*
 * ssexec_signal.c
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

#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/graph.h>
#include <oblibs/types.h>

#include <skalibs/sgetopt.h>
#include <skalibs/nsig.h>
#include <skalibs/sig.h>

#include <66/svc.h>
#include <66/config.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/service.h>
#include <66/state.h>
#include <66/enum.h>

static char const cmdsig[NSIG] = {

    [SIGALRM] = 'a',
    [SIGABRT] = 'b',
    [SIGQUIT] = 'q',
    [SIGHUP] = 'h',
    [SIGKILL] = 'k',
    [SIGTERM] = 't',
    [SIGINT] = 'i',
    [SIGUSR1] = '1',
    [SIGUSR2] = '2',
    [SIGSTOP] = 'p',
    [SIGCONT] = 'c',
    [SIGWINCH] = 'y'
} ;

int ssexec_signal(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r ;
    uint8_t what = 1, requiredby = 0, propagate = 1, opt_updown = 0, reloadmsg = 0 ;
    graph_t graph = GRAPH_ZERO ;
    unsigned int napid = 0, pos = 0 ;
    char updown[4] = "-w \0" ;
    char data[DATASIZE + 1] = "-" ;
    unsigned int datalen = 1 ;
    struct resolve_hash_s *hres = NULL ;
    unsigned int list[SS_MAX_SERVICE + 1], visit[SS_MAX_SERVICE + 1] ;

    /*
     * STATE_FLAGS_TOPROPAGATE = 0
     * do not send signal to the depends/requiredby of the service.
     *
     * STATE_FLAGS_TOPROPAGATE = 1
     * send signal to the depends/requiredby of the service
     *
     * When we come from 66 start/stop command we always want to
     * propagate the signal. But we may need/want to send a e.g. SIGHUP signal
     * to a specific service without interfering on its depends/requiredby services
     *
     * Also, we only deal with already supervised service. This command is the signal sender,
     * it not intended to sanitize the state of the services.
     *
     * */
    uint32_t gflag = STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_ISSUPERVISED|STATE_FLAGS_WANTUP ;

    memset(list, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;
    memset(visit, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc,argv, OPTS_SIGNAL, &l) ;
            if (opt == -1) break ;

            switch (opt) {
                case 'h' : info_help(info->help, info->usage) ; return 0 ;
                case 's' :
                    {
                        int sig ;
                        if (!sig0_scan(l.arg, &sig))
                            log_die(LOG_EXIT_USER, "invalid signal: ", l.arg) ;
                        if (!cmdsig[sig])
                            log_die(LOG_EXIT_USER, l.arg, " is not in the list of user-available signals") ;
                        opt = cmdsig[sig] ;
                    }
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
    graph_build_service(&graph, &hres, info, gflag) ;

    if (!graph.mlen)
        log_die(LOG_EXIT_USER, "services selection is not supervised -- initiate its first") ;

    for (; pos < argc ; pos++) {

        struct resolve_hash_s *hash = hash_search(&hres, argv[pos]) ;
        /** The service may not be supervised, for example serviceB depends on
         * serviceA and serviceB was unsupervised by the user. So it will be ignored
         * by the function graph_build_service. In this case, the service does not
         * exist at array.
         *
         * At stop process, just ignore it as it already down anyway */
        if (hash == NULL) {
            if (what && data[1] != 'r' || data[1] != 'h') {
                log_warn("service: ", argv[pos], " is already stopped or unsupervised -- ignoring it") ;
                continue ;
            } else {
                log_die(LOG_EXIT_USER, "service: ", argv[pos], " not available -- did you parse it?") ;
            }
        }
        graph_compute_visit(*hash, visit, list, &graph, &napid, requiredby) ;
    }

    if (!napid)
        log_dieu(LOG_EXIT_USER, "find service: ", argv[0], " -- not currently in use") ;

    pidservice_t apids[napid] ;

    svc_init_array(list, napid, apids, &graph, &hres, info, requiredby, gflag) ;

    r = svc_launch(apids, napid, what, &graph, &hres, info, updown, opt_updown, reloadmsg, data, propagate) ;

    graph_free_all(&graph) ;
    hash_free(&hres) ;

    return r ;
}
