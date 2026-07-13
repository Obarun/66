/*
 * ssexec_emit.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>

#include <66/ssexec.h>
#include <66/svc.h>
#include <66/constants.h>
#include <66/config.h>

static opt_t const opts_emit[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
} ;

opt_cmd_t const cmd_emit = {
    .name = "66 emit",
    .help = "emit a user event to the event daemon",
    .operands = "name",
    .opts = opts_emit,
    .nopts = OPT_COUNT(opts_emit),
    .fn = &ssexec_emit,
} ;

int ssexec_emit(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    if (argc != 1)
        return opt_emit_usage(cmd_emit.name, &cmd_emit) ;

    char const *name = argv[0] ;
    size_t namelen = strlen(name) ;
    if (!namelen || namelen > SS_MAX_SERVICE_NAME)
        log_die(LOG_EXIT_USER, "invalid event name: ", name) ;

    char eventddir[info->scandir.len + 1 + SS_EVENTD_LEN + 1] ;
    auto_strings(eventddir, info->scandir.s, "/", SS_EVENTD) ;

    if (!svcd_notify(eventddir, 'e', name))
        log_dieusys(LOG_EXIT_SYS, "emit event to the event daemon") ;

    return 0 ;
}
