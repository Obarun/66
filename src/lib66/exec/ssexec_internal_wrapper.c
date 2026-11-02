/*
 * ssexec_internal_wrapper.c
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

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/types.h>

#include <66/ssexec.h>
#include <66/constants.h>
#include <66/utils.h>

extern void internal_signal_set_name(char const *name) ;

static int do_internal_signal(char const *name, int argc, char const *const *argv, void *data)
{
    internal_signal_set_name(name) ;
    return ssexec_internal_signal(argc, argv, data) ;
}

static int do_internal_start(int argc, char const *const *argv, void *data)
{
    return do_internal_signal("start", argc, argv, data) ;
}

static int do_internal_stop(int argc, char const *const *argv, void *data)
{
    return do_internal_signal("stop", argc, argv, data) ;
}

static int do_internal_restart(int argc, char const *const *argv, void *data)
{
    return do_internal_signal("restart", argc, argv, data) ;
}

static int on_internal_wrapper(int id, char const *arg, void *data)
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

static opt_t const opts_internal_wrapper[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",  .arg = OPT_NONE,                         .help = "print this help" },
    { .id = 'o',         .shortname = 'o', .longname = "owner", .arg = OPT_REQUIRED, .argname = "owner", .help = "operate on the internal services of owner" },
} ;

static opt_t const opts_help[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
} ;

static opt_cmd_t const internal_sub[] = {
    { .name = "start",   .help = "start an internal service of the scandir",   .operands = "fdholderd|oneshotd|eventd", .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_internal_start },
    { .name = "stop",    .help = "stop an internal service of the scandir",    .operands = "fdholderd|oneshotd|eventd", .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_internal_stop },
    { .name = "restart", .help = "restart an internal service of the scandir", .operands = "fdholderd|oneshotd|eventd", .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_internal_restart },
} ;

opt_cmd_t const cmd_internal = {
    .name = "66 internal",
    .help = "control the internal services of the scandir",
    .opts = opts_internal_wrapper,
    .nopts = OPT_COUNT(opts_internal_wrapper),
    .on_option = &on_internal_wrapper,
    .sub = internal_sub,
    .nsub = OPT_COUNT(internal_sub),
} ;
