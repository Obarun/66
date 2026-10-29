/*
 * ssexec_state.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <oblibs/string.h>
#include <stddef.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>

#include <66/info.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/constants.h>
#include <66/state.h>
#include <66/config.h>
#include <66/ssexec.h>
#include <66/command.h>

/* One row per state flag, in display order -- the key is what -f selects. */

static info_field_t const fields_state[] = {
    { "toinit",        INFO_FIELD_FLAG, offsetof(ss_state_t, toinit),        0 },
    { "toreload",      INFO_FIELD_FLAG, offsetof(ss_state_t, toreload),      0 },
    { "tounsupervise", INFO_FIELD_FLAG, offsetof(ss_state_t, tounsupervise), 0 },
    { "issupervised",  INFO_FIELD_FLAG, offsetof(ss_state_t, issupervised),  0 },
} ;

/* option state, set by on_state, drained at the top of ssexec_state */
static char const *opt_field = 0 ;
static uint8_t opt_noname = 0 ;

static opt_t const opts_state[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",   .arg = OPT_NONE,                             .help = "print this help" },
    { .id = 'f',         .shortname = 'f', .longname = "field",  .arg = OPT_REQUIRED, .argname = "field,...", .help = "display only these comma-separated fields" },
    { .id = 'n',         .shortname = 'n', .longname = "no-name", .arg = OPT_NONE,                            .help = "display only the value, not the field name" },
} ;

static int on_state(int id, char const *arg, void *data)
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

opt_cmd_t const cmd_state = {
    .name = "66 state",
    .help = "display state files contents of services",
    .operands = "service",
    .opts = opts_state,
    .nopts = OPT_COUNT(opts_state),
    .on_option = &on_state,
    .fn = &ssexec_state,
    .epilog =
        "field:\n"
        "    toinit          toreload        tounsupervise\n"
        "    issupervised",
} ;

int ssexec_state(int argc, char const *const *argv, void *data)
{
    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy */
    char const *field = opt_field ;
    uint8_t noname = opt_noname ;
    opt_field = 0 ;
    opt_noname = 0 ;

    int r = -1 ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_SERVICE, &res) ;

    ss_state_t sta = STATE_ZERO ;
    char svname[SS_MAX_SERVICE_NAME + 1] ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    if (*argv[0] == '/') {

        if (state_read_file(&sta, *argv) <= 0)
            log_dieusys(LOG_EXIT_SYS, "read status file") ;

    } else {

        r = service_resolve_provide(svname, *argv, 0, info->base.s) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "resolve provide alias: ", *argv) ;

        char const *sv = r ? svname : *argv ;

        r = resolve_read(&wres, info->base.s, sv) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", sv) ;

        if (!r)
            command_refuse_unknown(sv, info->base.s, info->owner, false) ;

        if (state_read(&sta, &res) <= 0)
            log_dieusys(111,"read state file of: ", sv) ;
    }

    info_resolve_display(&sta, 0, fields_state, OPT_COUNT(fields_state), field, noname, 0, 0) ;

    resolve_free_obj(&wres) ;

    return 0 ;
}
