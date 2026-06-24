/*
 * ssexec_sleep_wrapper.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
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
#include <oblibs/exec.h>
#include <oblibs/environ.h>

#include <66/ssexec.h>
#include <66/config.h>

static opt_t const opts_sleep[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
} ;

static int sleep_run(char const *action, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    char const *newargv[5] ;
    unsigned int m = 0 ;
    newargv[m++] = SS_BINPREFIX "66-hpr" ;
    newargv[m++] = action ;
    newargv[m++] = "-l" ;
    newargv[m++] = info->live.s ;
    newargv[m] = 0 ;

    exec_path_die(newargv[0], newargv, (char const *const *) environ) ;
}

static int do_suspend(int argc, char const *const *argv, void *data)
{
    (void)argc ;
    (void)argv ;
    return sleep_run("-s", data) ;
}

static int do_hibernate(int argc, char const *const *argv, void *data)
{
    (void)argc ;
    (void)argv ;
    return sleep_run("-i", data) ;
}

opt_cmd_t const cmd_suspend = {
    .name = "66 suspend",
    .help = "suspend the system to RAM",
    .opts = opts_sleep,
    .nopts = OPT_COUNT(opts_sleep),
    .fn = &do_suspend,
} ;

opt_cmd_t const cmd_hibernate = {
    .name = "66 hibernate",
    .help = "hibernate the system to disk",
    .opts = opts_sleep,
    .nopts = OPT_COUNT(opts_sleep),
    .fn = &do_hibernate,
} ;
