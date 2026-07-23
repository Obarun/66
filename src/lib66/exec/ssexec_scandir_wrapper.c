/*
 * ssexec_scandir_wrapper.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/types.h>

#include <66/ssexec.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/utils.h>

/* The dispatch tree lives here: each sub-command's option table is defined in
 * this file (so OPT_COUNT is a constant), while the leaves export only their
 * option applier (on_*) and their handler (ssexec_*). The signal sub-commands
 * all share ssexec_scandir_signal through the leaf's do_scandir_* helpers
 * (which set the signal name). */

extern opt_on_option_fn on_scandir_create ;
extern opt_on_option_fn on_scandir_signal ;

extern opt_cmd_fn do_scandir_start ;
extern opt_cmd_fn do_scandir_stop ;
extern opt_cmd_fn do_scandir_reconfigure ;
extern opt_cmd_fn do_scandir_check ;
extern opt_cmd_fn do_scandir_quit ;
extern opt_cmd_fn do_scandir_abort ;
extern opt_cmd_fn do_scandir_nuke ;
extern opt_cmd_fn do_scandir_annihilate ;
extern opt_cmd_fn do_scandir_zombies ;

static int on_scandir_wrapper(int id, char const *arg, void *data)
{
    ssexec_t *info = data ;

    switch (id) {

        case 'o' :

            if (MYUID)
                log_die(LOG_EXIT_USER, "only root can use -o option") ;

            uid_t owner = -1 ;
            if (!youruid(&owner, arg))
                log_dieusys(LOG_EXIT_SYS, "get uid of: ", arg) ;

            info->owner = owner ;
            info->ownerlen = uid_format(info->ownerstr, info->owner) ;
            info->ownerstr[info->ownerlen] = 0 ;

            info->scandir.len = 0 ;
            if (!auto_strbuf(&info->scandir, info->live.s, SS_SCANDIR, "/", info->ownerstr))
                log_die_nomem("strbuf") ;

            break ;
    }

    return 0 ;
}

static opt_t const opts_help[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
} ;

static opt_t const opts_scandir_wrapper[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",  .arg = OPT_NONE,                         .help = "print this help" },
    { .id = 'o',         .shortname = 'o', .longname = "owner", .arg = OPT_REQUIRED, .argname = "owner", .help = "handles scandir of owner" },
} ;

static opt_t const opts_scandir_create[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE,                            .help = "print this help" },
    { .id = 'b', .shortname = 'b', .longname = "boot",         .arg = OPT_NONE,                            .help = "create scandir for a boot process" },
    { .id = 'B', .shortname = 'B', .longname = "container",    .arg = OPT_NONE,                            .help = "create scandir for a boot process inside a container" },
    { .id = 'c', .shortname = 'c', .longname = "no-logger",    .arg = OPT_NONE,                            .help = "do not catch logs" },
    { .id = 'L', .shortname = 'L', .longname = "log-user",     .arg = OPT_REQUIRED, .argname = "username", .help = "run catch-all logger as username user" },
} ;

static opt_t const opts_scandir_signal[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE,                               .help = "print this help" },
    { .id = 'd', .shortname = 'd', .longname = "notify",       .arg = OPT_REQUIRED, .argname = "number",      .help = "notify readiness on file descriptor number" },
    { .id = 's', .shortname = 's', .longname = "rescan",       .arg = OPT_REQUIRED, .argname = "milliseconds",.help = "scan scandir every milliseconds milliseconds" },
    { .id = 'e', .shortname = 'e', .longname = "environment",  .arg = OPT_REQUIRED, .argname = "path",        .help = "use path as environment directory" },
    { .id = 'b', .shortname = 'b', .longname = "boot",         .arg = OPT_NONE,                               .help = "create scandir (if it doesn't exist yet) for a boot process" },
    { .id = 'B', .shortname = 'B', .longname = "container",    .arg = OPT_NONE,                               .help = "create scandir (if it doesn't exist yet) for a boot process inside a container" },
} ;

static opt_cmd_t const scandir_sub[] = {
    { .name = "create",      .help = "create a scandir",                      .opts = opts_scandir_create, .nopts = OPT_COUNT(opts_scandir_create), .on_option = &on_scandir_create, .fn = &ssexec_scandir_create },
    { .name = "remove",      .help = "remove a scandir",                      .opts = opts_help,           .nopts = OPT_COUNT(opts_help),                                            .fn = &ssexec_scandir_remove },
    { .name = "start",       .help = "start a scandir",                       .opts = opts_scandir_signal, .nopts = OPT_COUNT(opts_scandir_signal), .on_option = &on_scandir_signal, .fn = &do_scandir_start },
    { .name = "stop",        .help = "stop a running scandir",                .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_scandir_stop },
    { .name = "reconfigure", .help = "reconfigure a running scandir",         .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_scandir_reconfigure },
    { .name = "check",       .help = "check a running scandir",               .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_scandir_check },
    { .name = "quit",        .help = "quit a running scandir",                .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_scandir_quit },
    { .name = "abort",       .help = "abort a running scandir",               .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_scandir_abort },
    { .name = "nuke",        .help = "nuke a running scandir",                .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_scandir_nuke },
    { .name = "annihilate",  .help = "annihilate a running scandir",          .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_scandir_annihilate },
    { .name = "zombies",     .help = "destroy zombies from a running scandir",.opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_scandir_zombies },
} ;

opt_cmd_t const cmd_scandir = {
    .name = "66 scandir",
    .help = "main subcommands to manage scandir",
    .opts = opts_scandir_wrapper,
    .nopts = OPT_COUNT(opts_scandir_wrapper),
    .on_option = &on_scandir_wrapper,
    .sub = scandir_sub,
    .nsub = OPT_COUNT(scandir_sub),
} ;
