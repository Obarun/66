/*
 * ssexec_stop.c
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
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
#include <stdbool.h>

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/hash.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/config.h>

static uint8_t opt_unsupervise = 0 ;
static uint8_t opt_nopropagate = 0 ;

static opt_t const opts_stop[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",         .arg = OPT_NONE, .help = "print this help" },
    { .id = 'u',         .shortname = 'u',                             .arg = OPT_NONE, .help = "unsupervise the service", .hidden = true },
    { .id = 'P',         .shortname = 'P', .longname = "no-propagate", .arg = OPT_NONE, .help = "do not propagate signal to its requiredby" },
} ;

static int on_stop(int id, char const *arg, void *data)
{
    (void)arg ;
    (void)data ;

    switch (id) {

        case 'u' :

            opt_unsupervise = 1 ;
            break ;

        case 'P' :

            opt_nopropagate = 1 ;
            break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_stop = {
    .name = "66 stop",
    .help = "bring down services",
    .operands = "service...",
    .opts = opts_stop,
    .nopts = OPT_COUNT(opts_stop),
    .on_option = &on_stop,
    .fn = &ssexec_stop,
} ;

int ssexec_stop(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    uint8_t unsupervise = opt_unsupervise, nopropagate = opt_nopropagate ;
    opt_unsupervise = 0 ;
    opt_nopropagate = 0 ;

    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    vertex_t *c, *tmp ;
    uint8_t propagate = 3 ;
    int e = 0 ;
    uint32_t flag = GRAPH_WANT_SUPERVISED|GRAPH_WANT_REQUIREDBY, nservice = 0 ;

    if (nopropagate) {
        FLAGS_CLEAR(flag, GRAPH_WANT_REQUIREDBY) ;
        propagate++ ;
    }

    if (unsupervise)
        FLAGS_SET(flag, GRAPH_WANT_LOGGER) ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    if ((svc_scandir_ok(info->scandir.s)) != 1)
        log_diesys(LOG_EXIT_SYS,"scandir: ", info->scandir.s," is not running") ;

    if (!graph_new(&graph, (uint32_t)SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "allocate the graph") ;

    nservice = service_graph_build_arguments(&graph, argv, argc, info, flag) ;
    if (!nservice && errno == EINVAL)
        log_die(LOG_EXIT_USER, "services selection is not available -- did you start it first?") ;

    if (!graph.g.nsort)
        log_warn_return(e,"no services found to handle") ;

    char *sig[propagate] ;
    if (propagate > 3) {

        sig[0] = "-P" ;
        sig[1] = "-wD" ;
        sig[2] = "-d" ;
        sig[3] = 0 ;

    } else {

        sig[0] = "-wD" ;
        sig[1] = "-d" ;
        sig[2] = 0 ;
    }

    char const *nargv[nservice + 1] ;
    nservice = 0 ;
    HASH_ITER(hh, graph.g.vertexes, c, tmp)
        nargv[nservice++] = c->name ;

    nargv[nservice] = 0 ;

    e = svc_send_wait(nargv, nservice, sig, propagate, info) ;

    if (e)
        return e ;

    if (unsupervise)
        svc_unsupervise(&graph) ;

    service_graph_destroy(&graph) ;

    return e ;
}

/* "66 free" is "66 stop -u": bring services down and unsupervise them. It is a
 * distinct command (own help) that re-enters the dispatcher on the stop node
 * with the -u flag injected. */

static opt_t const opts_free[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
} ;

static int do_free(int argc, char const *const *argv, void *data)
{
    int m = 0, i = 0 ;
    char const *nargv[argc + 3] ;

    nargv[m++] = "free" ;
    nargv[m++] = "-u" ;
    for (; i < argc ; i++)
        nargv[m++] = argv[i] ;
    nargv[m] = 0 ;

    return opt_dispatch(m, nargv, &cmd_stop, data) ;
}

opt_cmd_t const cmd_free = {
    .name = "66 free",
    .help = "bring down services and remove them from the scandir",
    .operands = "service...",
    .opts = opts_free,
    .nopts = OPT_COUNT(opts_free),
    .fn = &do_free,
} ;
