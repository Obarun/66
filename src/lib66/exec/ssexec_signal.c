/*
 * ssexec_signal.c
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

#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/types.h>

#include <skalibs/sgetopt.h>
#include <skalibs/nsig.h> // NSIG

#include <66/svc.h>
#include <66/graph.h>
#include <66/ssexec.h>
#include <66/config.h>

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

/**
 * This function assume that the list
 * of service are at least supervised.
 * */
int ssexec_signal(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r ;
    uint8_t requiredby = 1, propagate = 1, woption = 0 ;
    char *cmdmsg = 0 ;
    char wsignal[5] = "-w \0" ;
    char signal[DATASIZE + 1] = "-" ;
    unsigned int datalen = 1 ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_SKIP_MODULECONTENTS, nservice = 0 ;

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
                        if (!sig_parse(l.arg, &sig))
                            log_die(LOG_EXIT_USER, "invalid signal: ", l.arg) ;
                        if (!cmdsig[sig])
                            log_die(LOG_EXIT_USER, l.arg, " is not in the list of user-available signals") ;
                        opt = cmdsig[sig] ;
                    }
                    __attribute__((fallthrough)) ;
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

                    signal[datalen++] = opt == 'H' ? 'h' : opt ;
                    break ;

                case 'w' :

                    if (!memchr("dDuUrR", l.arg[0], 6))
                        log_usage(info->usage, "\n", info->help) ;

                    wsignal[2] = l.arg[0] ;
                    woption = 1 ;
                    break ;

                case 'P':
                    propagate = 0 ;
                    break ;

                default :
                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1 || datalen < 2)
        log_usage(info->usage, "\n", info->help) ;

    if (signal[1] == 'u' || signal[1] == 'U')
        requiredby = 0 ;

    if (signal[1] == 'r')
        cmdmsg = "restart" ;
    else if (signal[1] == 'h')
        cmdmsg = "reload" ;

    if (propagate) {
        if (requiredby) {
            FLAGS_SET(flag, GRAPH_WANT_REQUIREDBY) ;
        } else FLAGS_SET(flag, GRAPH_WANT_DEPENDS) ;
    }

    if ((svc_scandir_ok(info->scandir.s)) != 1)
        log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;

    if (!graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the graph") ;

    nservice = service_graph_build_arguments(&graph, argv, argc, info, flag) ;

    if (!nservice) {
        if (errno == EINVAL)
            log_dieusys(LOG_EXIT_USER, "build the graph") ;

        log_die(LOG_EXIT_USER, "services selection is not supervised -- initiate its first") ;
    }

    svc_ctx_t asvc[graph.g.nsort] ;

    svc_init_ctx(asvc, &graph, requiredby, flag) ;

    r = svc_launch(asvc, graph.g.nsort, requiredby, info, wsignal, woption, signal, cmdmsg, propagate) ;

    service_graph_destroy(&graph) ;

    return r ;
}
