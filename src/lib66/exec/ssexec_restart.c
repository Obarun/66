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
#include <string.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/types.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/svc.h>
#include <66/config.h>
#include <66/sanitize.h>

static uint8_t opt_nopropagate = 0 ;

static opt_t const opts_restart[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",         .arg = OPT_NONE, .help = "print this help" },
    { .id = 'P',         .shortname = 'P', .longname = "no-propagate", .arg = OPT_NONE, .help = "do not propagate signal to its dependencies" },
} ;

static int on_restart(int id, char const *arg, void *data)
{
    (void)arg ;
    (void)data ;

    switch (id) {

        case 'P' :

            opt_nopropagate = 1 ;
            break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_restart = {
    .name = "66 restart",
    .help = "convenient command to bring down then bring up services in one pass",
    .operands = "service...",
    .opts = opts_restart,
    .nopts = OPT_COUNT(opts_restart),
    .on_option = &on_restart,
    .fn = &ssexec_restart,
} ;

int ssexec_restart(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    uint8_t nopropagate = opt_nopropagate ;
    opt_nopropagate = 0 ;

    int r ;
    unsigned int m = 0 ;
    uint8_t propagate = 3 ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_WANT_REQUIREDBY|GRAPH_WANT_SUPERVISED, nservice = 0, pos = 0 ;

    if (nopropagate) {
        FLAGS_CLEAR(flag, GRAPH_WANT_REQUIREDBY) ;
        propagate++ ;
    }

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    if ((svc_scandir_ok(info->scandir.s)) !=  1 )
        log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s, " is not running") ;

    if (!graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the service graph") ;

    nservice = service_graph_build_arguments(&graph, argv, argc, info, flag) ;

    if (!nservice) {
        if (errno == EINVAL)
            log_dieusys(LOG_EXIT_SYS, "unable to build service selection graph") ;
        log_die(LOG_EXIT_SYS, "service selection is not supervised -- try to start it first") ;
    }

    sanitize_init(&graph, flag) ;

    char *sig[propagate] ;
    sig[0] = "-wD" ;
    sig[1] = "-D" ;

    if (propagate > 3) {

        sig[2] = "-P" ;
        sig[3] = 0 ;

    } else sig[2] = 0 ;

    r = svc_send_wait(argv, argc, sig, propagate, info) ;

    if (r)
        log_warnusys("stop service selection") ;

    sig[0] = "-wU" ;
    sig[1] = "-U" ;

    {
        /** use ssexec_start here to handle freed
         * services before calling ssexec_signal.
         * For instance, 66 free -P sA, 66 start sB,
         * where sB depends on sA */
        int nargc = 2 + nservice + propagate ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;

        newargv[m++] = "start" ;
        if (propagate > 3)
            newargv[m++] = "-P" ;

        pos = 0 ;
        FOREACH_GRAPH_SORT(service_graph_t, &graph, pos) {

            uint32_t index = graph.g.sort[pos] ;
            char *name = graph.g.sindex[index]->name ;
            newargv[m++] = name ;
        }

        newargv[m] = 0 ;

        PROG = "start" ;
        r = opt_dispatch(m, newargv, &cmd_start, info) ;
        PROG = prog ;
    }

    service_graph_destroy(&graph) ;

    return r ;
}



