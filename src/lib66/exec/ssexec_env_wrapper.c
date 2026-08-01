/*
 * ssexec_env_wrapper.c
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

#include <oblibs/opt.h>

#include <66/ssexec.h>

/* The dispatch tree lives here: each sub-command's option table is defined in
 * this file (so OPT_COUNT is a constant), while the leaves export only their
 * handler (ssexec_env_*). */

static opt_t const opts_help[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
} ;

static char const env_epilog[] =
    "a published variable overrides the frontend, the service configuration and\n"
    "the ImportFile files of every service, and reaches it verbatim: a ${...} in\n"
    "the value stays literal, and a value starting with '!' is refused rather\n"
    "than passed on with the unexport marker inside it.\n" ;

static opt_cmd_t const env_sub[] = {
    { .name = "import", .help = "publish variables from the environment of the caller", .operands = "variable...",       .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &ssexec_env_import },
    { .name = "set",    .help = "publish variables with the given values",              .operands = "variable=value...", .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &ssexec_env_set },
    { .name = "unset",  .help = "withdraw published variables",                         .operands = "variable...",       .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &ssexec_env_unset },
    { .name = "list",   .help = "list the published variables",                                                          .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &ssexec_env_list },
} ;

opt_cmd_t const cmd_env = {
    .name = "66 env",
    .help = "main subcommands to manage the runtime environment",
    .epilog = env_epilog,
    .opts = opts_help,
    .nopts = OPT_COUNT(opts_help),
    .sub = env_sub,
    .nsub = OPT_COUNT(env_sub),
} ;
