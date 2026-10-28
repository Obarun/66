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
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>
#include <stdbool.h>

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/command.h>

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
    bool unsupervise = opt_unsupervise, nopropagate = opt_nopropagate ;
    opt_unsupervise = 0 ;
    opt_nopropagate = 0 ;

    int e ;
    strbuf frontends = STRBUF_ZERO ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    e = command_down(argv, argc, info, &frontends, nopropagate, unsupervise, false, unsupervise) ;
    strbuf_free(&frontends) ;

    return e ;
}

/* "66 free" is "66 stop -u": bring services down and unsupervise them. It is a
 * distinct command (own help) that re-enters the dispatcher on the stop node
 * with the -u flag injected. */

static uint8_t opt_free_nopropagate = 0 ;

static opt_t const opts_free[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",         .arg = OPT_NONE, .help = "print this help" },
    { .id = 'P',         .shortname = 'P', .longname = "no-propagate", .arg = OPT_NONE, .help = "do not propagate to its requiredby" },
} ;

static int on_free(int id, char const *arg, void *data)
{
    (void)arg ;
    (void)data ;

    if (id == 'P')
        opt_free_nopropagate = 1 ;

    return 0 ;
}

static int do_free(int argc, char const *const *argv, void *data)
{
    uint8_t nopropagate = opt_free_nopropagate ;
    opt_free_nopropagate = 0 ;

    int m = 0, i = 0 ;
    char const *nargv[argc + 4] ;

    nargv[m++] = "free" ;
    nargv[m++] = "-u" ;
    if (nopropagate)
        nargv[m++] = "-P" ;
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
    .on_option = &on_free,
    .fn = &do_free,
} ;
