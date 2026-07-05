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
 * except according to the terms contained in the LICENSE file./
 */

#include <stddef.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/files.h>

#include <66/info.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/state.h>
#include <66/config.h>
#include <66/ssexec.h>

/* One row per state flag, in display order -- the key is what -f selects. */

static info_field_t const fields_state[] = {
    { "toinit",        INFO_FIELD_FLAG, offsetof(ss_state_t, toinit) },
    { "toreload",      INFO_FIELD_FLAG, offsetof(ss_state_t, toreload) },
    { "torestart",     INFO_FIELD_FLAG, offsetof(ss_state_t, torestart) },
    { "tounsupervise", INFO_FIELD_FLAG, offsetof(ss_state_t, tounsupervise) },
    { "toparse",       INFO_FIELD_FLAG, offsetof(ss_state_t, toparse) },
    { "isparsed",      INFO_FIELD_FLAG, offsetof(ss_state_t, isparsed) },
    { "issupervised",  INFO_FIELD_FLAG, offsetof(ss_state_t, issupervised) },
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
        "    toinit          toreload        torestart\n"
        "    tounsupervise   toparse         isparsed\n"
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
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    ss_state_t sta = STATE_ZERO ;
    char const *svname = 0 ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    svname = *argv ;

    if (svname[0] == '/') {

        char pack[STATE_STATE_SIZE] ;

        r = file_read(svname, pack, STATE_STATE_SIZE) ;
            if (r < STATE_STATE_SIZE || r < 0)
                log_dieusys(LOG_EXIT_SYS, "read status file") ;

        state_unpack(pack, &sta) ;

    } else {

        r = service_is_g(svname, STATE_FLAGS_ISPARSED) ;
        if (r == -1)
            log_dieusys(LOG_EXIT_SYS, "get information of service: ", svname, " -- please a bug report") ;
        else if (!r || r == STATE_FLAGS_FALSE)
            log_die(LOG_EXIT_USER, "service: ", svname, " is not parsed -- try to parse it using '66 parse ", svname, "'") ;

        r = resolve_read(wres, info->base.s, svname) ;
        if (r <= 0)
            log_dieu(LOG_EXIT_SYS, "read resolve file: ", svname) ;

        if (!state_read(&sta, &res))
            log_dieusys(111,"read state file of: ", svname) ;
    }

    info_resolve_display(&sta, 0, fields_state, OPT_COUNT(fields_state), field, noname, 0, 0) ;

    resolve_free(wres) ;

    return 0 ;
}
