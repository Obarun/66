/*
 * ssexec_runstate.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/status.h>
#include <66/info.h>
#include <66/svc.h>
#include <66/state.h>
#include <66/ssexec.h>

static char const *opt_field = 0 ;
static uint8_t opt_noname = 0 ;

static opt_t const opts_runstate[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",   .arg = OPT_NONE,                             .help = "print this help" },
    { .id = 'f',         .shortname = 'f', .longname = "field",  .arg = OPT_REQUIRED, .argname = "field,...", .help = "display only these comma-separated fields" },
    { .id = 'n',         .shortname = 'n', .longname = "no-name", .arg = OPT_NONE,                            .help = "display only the value, not the field name" },
} ;

static int on_runstate(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {

        case 'f' :

            opt_field = arg ;
            break ;

        case 'n' :

            opt_noname = 1 ;
            break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_runstate = {
    .name = "66 runstate",
    .help = "display runtime status of services",
    .operands = "service",
    .opts = opts_runstate,
    .nopts = OPT_COUNT(opts_runstate),
    .on_option = &on_runstate,
    .fn = &ssexec_runstate,
    .epilog =
        "field:\n"
        "    state    result       who\n"
        "    pid      code         ndeaths\n"
        "    stamp    readystamp   window_start",
} ;

int ssexec_runstate(int argc, char const *const *argv, void *data)
{
    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy */
    char const *field = opt_field ;
    uint8_t noname = opt_noname ;
    opt_field = 0 ;
    opt_noname = 0 ;

    int r = -1 ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    service_status_t st = STATUS_ZERO ;
    char const *svname = 0 ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    svname = *argv ;

    if (svname[0] == '/') {

        if (access(svname, F_OK) < 0)
            log_dieusys(LOG_EXIT_SYS, "access status file: ", svname) ;

        r = status_read(&st, svname) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "read status file: ", svname) ;
        else if (!r)
            log_die(LOG_EXIT_USER, "invalid status file: ", svname) ;

    } else {

        r = service_is_g(svname, STATE_FLAGS_ISPARSED) ;
        if (r == -1)
            log_dieusys(LOG_EXIT_SYS, "get information of service: ", svname, " -- please a bug report") ;
        else if (!r || r == STATE_FLAGS_FALSE)
            log_die(LOG_EXIT_USER, "service: ", svname, " is not parsed -- try to parse it using '66 parse ", svname, "'") ;

        r = resolve_read(wres, info->base.s, svname) ;
        if (r <= 0)
            log_dieu(LOG_EXIT_SYS, "read resolve file: ", svname) ;

        if (svc_status(&res, &st) < 0)
            log_dieusys(LOG_EXIT_SYS, "read status of: ", svname) ;
    }

    info_status_display(&st, field, noname) ;

    resolve_free(wres) ;

    return 0 ;
}
