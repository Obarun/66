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
#include <signal.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/types.h>
#include <oblibs/attributes.h>

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
static opt_t const opts_signal[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",         .arg = OPT_NONE,                          .help = "print this help" },
    { .id = 'P',         .shortname = 'P', .longname = "no-propagate", .arg = OPT_NONE,                          .help = "do not propagate signal to its dependencies" },
    { .id = 'a',         .shortname = 'a', .longname = "alarm",        .arg = OPT_NONE,                          .help = "send a SIGALRM signal" },
    { .id = 'b',         .shortname = 'b', .longname = "abort",        .arg = OPT_NONE,                          .help = "send a SIGABRT signal" },
    { .id = 'q',         .shortname = 'q', .longname = "quit",         .arg = OPT_NONE,                          .help = "send a SIGQUIT signal" },
    { .id = 'H',         .shortname = 'H', .longname = "hangup",       .arg = OPT_NONE,                          .help = "send a SIGHUP signal" },
    { .id = 'k',         .shortname = 'k', .longname = "kill",         .arg = OPT_NONE,                          .help = "send a SIGKILL signal" },
    { .id = 't',         .shortname = 't', .longname = "term",         .arg = OPT_NONE,                          .help = "send a SIGTERM signal" },
    { .id = 'i',         .shortname = 'i', .longname = "interrupt",    .arg = OPT_NONE,                          .help = "send a SIGINT signal" },
    { .id = '1',         .shortname = '1', .longname = "usr1",         .arg = OPT_NONE,                          .help = "send a SIGUSR1 signal" },
    { .id = '2',         .shortname = '2', .longname = "usr2",         .arg = OPT_NONE,                          .help = "send a SIGUSR2 signal" },
    { .id = 'p',         .shortname = 'p', .longname = "stop",         .arg = OPT_NONE,                          .help = "send a SIGSTOP signal" },
    { .id = 'c',         .shortname = 'c', .longname = "cont",         .arg = OPT_NONE,                          .help = "send a SIGCONT signal" },
    { .id = 'y',         .shortname = 'y', .longname = "winch",        .arg = OPT_NONE,                          .help = "send a SIGWINCH signal" },
    { .id = 's',         .shortname = 's', .longname = "signal",       .arg = OPT_REQUIRED, .argname = "signal", .help = "send signal to the supervised process by signal name or its number" },
    { .id = 'r',         .shortname = 'r', .longname = "restart",      .arg = OPT_NONE,                          .help = "restart service by sending it a signal (default SIGTERM)" },
    { .id = 'o',         .shortname = 'o', .longname = "once",         .arg = OPT_NONE,                          .help = "once. Equivalent to '-uO'" },
    { .id = 'd',         .shortname = 'd', .longname = "down",         .arg = OPT_NONE,                          .help = "send a SIGTERM signal then a SIGCONT signal" },
    { .id = 'D',         .shortname = 'D', .longname = "down-keep",    .arg = OPT_NONE,                          .help = "bring down service and avoid to be bring it up automatically" },
    { .id = 'u',         .shortname = 'u', .longname = "up",           .arg = OPT_NONE,                          .help = "bring up service" },
    { .id = 'U',         .shortname = 'U', .longname = "up-restart",   .arg = OPT_NONE,                          .help = "bring up service and ensure that service can be restarted automatically" },
    { .id = 'x',         .shortname = 'x', .longname = "exit",         .arg = OPT_NONE,                          .help = "bring down the service and propagate to its supervisor" },
    { .id = 'O',         .shortname = 'O', .longname = "once-at-most", .arg = OPT_NONE,                          .help = "mark the service to run once at most" },
    { .id = 'Q',         .shortname = 'Q',                             .arg = OPT_NONE,                          .help = "synthetic accumulator option", .hidden = true },
    { .id = 'w',         .shortname = 'w', .longname = "wait",         .arg = OPT_REQUIRED, .argname = "uUdDrR", .help = "do not exit until the service reaches the wanted state" },
} ;

static char sig_signal[DATASIZE + 1] = "-" ;
static unsigned int sig_datalen = 1 ;
static char sig_wsignal[5] = "-w \0" ;
static uint8_t sig_woption = 0 ;
static uint8_t sig_propagate = 1 ;

static int on_signal(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {

        case 's' :
            {
                int sig ;
                if (!sig_parse(arg, &sig))
                    log_die(LOG_EXIT_USER, "invalid signal: ", arg) ;
                if (!cmdsig[sig])
                    log_die(LOG_EXIT_USER, arg, " is not in the list of user-available signals") ;
                id = cmdsig[sig] ;
            }
            attribute_fallthrough ;
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

            if (sig_datalen >= DATASIZE)
                log_die(LOG_EXIT_USER, "too many arguments") ;

            sig_signal[sig_datalen++] = id == 'H' ? 'h' : id ;
            break ;

        case 'w' :

            if (!memchr("dDuUrR", arg[0], 6))
                log_die(LOG_EXIT_USER, "invalid value for -w -- expected one of u/U/d/D/r/R") ;

            sig_wsignal[2] = arg[0] ;
            sig_woption = 1 ;
            break ;

        case 'P' :
            sig_propagate = 0 ;
            break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_signal = {
    .name = "66 signal",
    .help = "send a signal to services",
    .operands = "service...",
    .opts = opts_signal,
    .nopts = OPT_COUNT(opts_signal),
    .on_option = &on_signal,
    .fn = &ssexec_signal,
    .epilog =
        "values for -w (do not exit until the service reaches the wanted state):\n"
        "    u  the service is up\n"
        "    U  the service is up and ready and has notified readiness\n"
        "    d  the service is down\n"
        "    D  the service is down and ready to be brought up and has notified readiness\n"
        "    r  the service has been started or restarted\n"
        "    R  the service has been started or restarted and has notified readiness",
} ;

int ssexec_signal(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    int r ;
    uint8_t requiredby = 1, propagate = sig_propagate, woption = sig_woption ;
    char *cmdmsg = 0 ;
    char wsignal[5] ;
    char signal[DATASIZE + 1] ;
    unsigned int datalen = sig_datalen ;
    service_graph_t graph = GRAPH_SERVICE_ZERO ;
    uint32_t flag = GRAPH_SKIP_MODULECONTENTS, nservice = 0 ;

    memcpy(wsignal, sig_wsignal, sizeof wsignal) ;
    memcpy(signal, sig_signal, sizeof signal) ;

    /* the locals now hold the whole option state: reset the statics to their
     * initial values so a nested re-dispatch of "signal" starts clean. */
    sig_propagate = 1 ;
    sig_woption = 0 ;
    sig_datalen = 1 ;
    sig_signal[0] = '-' ;
    sig_wsignal[2] = ' ' ;

    if (argc < 1 || datalen < 2)
        return opt_emit_usage(cmd_signal.name, &cmd_signal) ;

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
