/*
 * ssexec_start.c
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

#include <stdbool.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/command.h>

static uint8_t opt_nopropagate = 0 ;

static opt_t const opts_start[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",         .arg = OPT_NONE, .help = "print this help" },
    { .id = 'P',         .shortname = 'P', .longname = "no-propagate", .arg = OPT_NONE, .help = "do not propagate signal to its dependencies" },
} ;

static int on_start(int id, char const *arg, void *data)
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

opt_cmd_t const cmd_start = {
    .name = "66 start",
    .help = "bring up services",
    .operands = "service...",
    .opts = opts_start,
    .nopts = OPT_COUNT(opts_start),
    .on_option = &on_start,
    .fn = &ssexec_start,
} ;

int ssexec_start(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    bool nopropagate = opt_nopropagate ;
    opt_nopropagate = 0 ;

    int e ;
    strbuf frontends = STRBUF_ZERO ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    e = command_up(argv, argc, info, &frontends, info->target, nopropagate, SVC_LOT_COMMAND, false, false) ;

    strbuf_free(&frontends) ;

    return e ;
}
