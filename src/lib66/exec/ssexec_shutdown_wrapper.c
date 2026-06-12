/*
 * ssexec_shutdown_wrapper.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/exec.h>
#include <oblibs/environ.h>

#include <66/ssexec.h>
#include <66/config.h>

static opt_t const opts_shutdown[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",         .arg = OPT_NONE,                           .help = "print this help" },
    { .id = 'a',         .shortname = 'a', .longname = "access",       .arg = OPT_NONE,                           .help = "use access control" },
    { .id = 'f',         .shortname = 'f', .longname = "force",        .arg = OPT_NONE,                           .help = "sync filesytem and immediately stop the system" },
    { .id = 'F',         .shortname = 'F', .longname = "force-nosync", .arg = OPT_NONE,                           .help = "do not sync filesytem and immediately stop the system" },
    { .id = 'm',         .shortname = 'm', .longname = "message",      .arg = OPT_REQUIRED, .argname = "message", .help = "replace the default message by message" },
    { .id = 't',         .shortname = 't', .longname = "timeout",      .arg = OPT_REQUIRED, .argname = "seconds", .help = "grace time between the SIGTERM and the SIGKILL" },
    { .id = 'W',         .shortname = 'W', .longname = "no-wall",      .arg = OPT_NONE,                           .help = "do not send a wall message to users" },
} ;

static uint8_t opt_acl = 0, opt_force = 0, opt_nowall = 0 ;
static char const *opt_msg = 0 ;
static char const *opt_time = 0 ;
static char const *hpr_command = 0 ;

static int on_shutdown(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {

        case 'f' :
            opt_force = 1 ;
            break ;

        case 'F' :
            opt_force = 2 ;
            break ;

        case 'm' :
            opt_msg = arg ;
            break ;

        case 'a' :
            opt_acl++ ;
            break ;

        case 't' :
            opt_time = arg ;
            break ;

        case 'W' :
            opt_nowall++ ;
            break ;
    }

    return 0 ;
}

static int shutdown_run(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    uint8_t acl = opt_acl, force = opt_force, nowall = opt_nowall ;
    char const *msg = opt_msg, *time = opt_time, *command = hpr_command ;
    opt_acl = 0 ;
    opt_force = 0 ;
    opt_nowall = 0 ;
    opt_msg = 0 ;
    opt_time = 0 ;
    hpr_command = 0 ;

    char *when = "now" ;

    if (argc && argv[0])
        when = (char *) argv[0] ;

    if (force) {

        unsigned int nargc = 5 + (force > 1 ? 1 : 0) + (nowall ? 1 : 0) ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;
        newargv[m++] = SS_BINPREFIX "66-hpr" ;
        newargv[m++] = "-f" ;
        if (force > 1)
            newargv[m++] = "-n" ;
        if (nowall)
            newargv[m++] = "-W" ;
        newargv[m++] = command ;
        newargv[m++] = "-l" ;
        newargv[m++] = info->live.s ;
        newargv[m] = 0 ;

        exec_path_die(newargv[0], newargv, (char const *const *) environ) ;
    }

    unsigned int nargc = 5 + (time ? 2 : 0) + (acl ? 1 : 0) + (msg ? 1 : 0) ;
    char const *newargv[nargc] ;
    unsigned int m = 0 ;

    newargv[m++] = SS_BINPREFIX "66-shutdown" ;
    newargv[m++] = command ;
    if (time) {
        newargv[m++] = "-t" ;
        newargv[m++] = time ;
    }
    if (acl)
        newargv[m++] = "-a" ;
    newargv[m++] = "-l" ;
    newargv[m++] = info->live.s ;
    newargv[m++] = when ;
    if (msg)
        newargv[m++] = msg ;
    newargv[m] = 0 ;

    exec_path_die(newargv[0], newargv, (char const *const *) environ) ;
}

static int do_poweroff(int argc, char const *const *argv, void *data)
{
    hpr_command = "-p" ;
    return shutdown_run(argc, argv, data) ;
}

static int do_reboot(int argc, char const *const *argv, void *data)
{
    hpr_command = "-r" ;
    return shutdown_run(argc, argv, data) ;
}

static int do_halt(int argc, char const *const *argv, void *data)
{
    hpr_command = "-h" ;
    return shutdown_run(argc, argv, data) ;
}

opt_cmd_t const cmd_poweroff = {
    .name = "66 poweroff",
    .help = "poweroff the system",
    .operands = "when",
    .opts = opts_shutdown,
    .nopts = OPT_COUNT(opts_shutdown),
    .on_option = &on_shutdown,
    .fn = &do_poweroff,
} ;

opt_cmd_t const cmd_reboot = {
    .name = "66 reboot",
    .help = "reboot the system",
    .operands = "when",
    .opts = opts_shutdown,
    .nopts = OPT_COUNT(opts_shutdown),
    .on_option = &on_shutdown,
    .fn = &do_reboot,
} ;

opt_cmd_t const cmd_halt = {
    .name = "66 halt",
    .help = "halt the system",
    .operands = "when",
    .opts = opts_shutdown,
    .nopts = OPT_COUNT(opts_shutdown),
    .on_option = &on_shutdown,
    .fn = &do_halt,
} ;
