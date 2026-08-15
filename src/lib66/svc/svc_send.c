/*
 * svc_send.c
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/types.h>

#include <66/svc.h>
#include <66/graph.h>
#include <66/ssexec.h>
#include <66/config.h>

int svc_send(char const *const *argv, int argc, ssexec_t *info, uint8_t target, char const *signal, char const *wsignal, uint8_t woption, uint8_t propagate)
{
    log_flow() ;

    int r ;
    char *cmdmsg = 0 ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_SKIP_MODULECONTENTS, nservice = 0 ;

    uint8_t requiredby = svc_target_starts(target) ? 0 : 1 ;

    if (signal[1] == 'r')
        cmdmsg = "restart" ;
    else if (signal[1] == 'l')
        cmdmsg = "reload" ;

    if (propagate) {
        if (requiredby) {
            FLAGS_SET(flag, GRAPH_WANT_REQUIREDBY) ;
        } else FLAGS_SET(flag, GRAPH_WANT_DEPENDS) ;
    }

    if ((svc_scandir_ok(info->scandir.s)) != 1)
        log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;

    if (!service_graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the graph") ;

    nservice = service_graph_build_arguments(&graph, argv, argc, info, flag) ;

    if (!nservice) {
        if (errno == EINVAL)
            log_dieusys(LOG_EXIT_USER, "build the graph") ;

        log_die(LOG_EXIT_USER, "services selection is not supervised -- initiate its first") ;
    }

    svc_ctx_t asvc[graph.g.nsort] ;

    svc_init_ctx(asvc, &graph, requiredby, flag, target) ;

    r = svc_launch(asvc, graph.g.nsort, target, info, wsignal, woption, signal, cmdmsg, propagate) ;

    service_graph_destroy(&graph) ;

    return r ;
}
