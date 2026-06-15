/*
 * ssexec_snapshot_wrapper.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/opt.h>

#include <66/ssexec.h>

/* The dispatch tree lives here: each sub-command's option table is defined in
 * this file (so OPT_COUNT is a constant), while the leaves export only their
 * option applier (on_*) and their handler (ssexec_*). */

extern opt_on_option_fn on_snapshot_create ;

static opt_t const opts_help[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
} ;

static opt_t const opts_snapshot_create[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
    { .id = 's',         .shortname = 's',                     .arg = OPT_NONE, .help = "allow the reserved system@ prefix (injected internally by version migration)", .hidden = true },
} ;

static opt_cmd_t const snapshot_sub[] = {
    { .name = "create",  .help = "create a snapshot of the entire 66 system", .operands = "name", .opts = opts_snapshot_create, .nopts = OPT_COUNT(opts_snapshot_create), .on_option = &on_snapshot_create, .fn = &ssexec_snapshot_create },
    { .name = "restore", .help = "restore a snapshot",                        .operands = "name", .opts = opts_help,            .nopts = OPT_COUNT(opts_help),                                              .fn = &ssexec_snapshot_restore },
    { .name = "remove",  .help = "remove a snapshot",                         .operands = "name", .opts = opts_help,            .nopts = OPT_COUNT(opts_help),                                              .fn = &ssexec_snapshot_remove },
    { .name = "list",    .help = "list available snapshots",                                      .opts = opts_help,            .nopts = OPT_COUNT(opts_help),                                              .fn = &ssexec_snapshot_list },
} ;

opt_cmd_t const cmd_snapshot = {
    .name = "66 snapshot",
    .help = "main subcommands to manage snapshot",
    .opts = opts_help,
    .nopts = OPT_COUNT(opts_help),
    .sub = snapshot_sub,
    .nsub = OPT_COUNT(snapshot_sub),
} ;
