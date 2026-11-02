/*
 * ssexec_fdholder_wrapper.c
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
 *
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

// leaf handlers (defined in ssexec_fdholder_{store,retrieve,list}.c)
extern opt_cmd_fn ssexec_fdholder_store ;
extern opt_cmd_fn ssexec_fdholder_retrieve ;
extern opt_cmd_fn ssexec_fdholder_list ;

// per-sub option appliers (defined alongside their handler), as scandir does
extern opt_on_option_fn on_fdholder_store ;
extern opt_on_option_fn on_fdholder_retrieve ;
extern opt_on_option_fn on_fdholder_list ;

/* wrapper-level options : pick the scandir owner whose fdholder we operate on,
 * exactly like the scandir wrapper */
static int on_fdholder_wrapper(int id, char const *arg, void *data)
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

static opt_t const opts_fdholder_wrapper[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",  .arg = OPT_NONE,                         .help = "print this help" },
    { .id = 'o',         .shortname = 'o', .longname = "owner", .arg = OPT_REQUIRED, .argname = "owner", .help = "operate on the fdholder of owner" },
} ;

static opt_t const opts_fdholder_store[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",    .arg = OPT_NONE,                                .help = "print this help" },
    { .id = 'T',         .shortname = 'T', .longname = "timeout", .arg = OPT_REQUIRED, .argname = "milliseconds", .help = "timeout for the operation" },
    { .id = 'd',         .shortname = 'd', .longname = "fd",      .arg = OPT_REQUIRED, .argname = "number",       .help = "file descriptor to store (default 0)" },
    { .id = 'e',         .shortname = 'e', .longname = "expire",  .arg = OPT_REQUIRED, .argname = "seconds",      .help = "expiry in seconds (0 = never)" },
} ;

static opt_t const opts_fdholder_retrieve[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",    .arg = OPT_NONE,                                .help = "print this help" },
    { .id = 'T',         .shortname = 'T', .longname = "timeout", .arg = OPT_REQUIRED, .argname = "milliseconds", .help = "timeout for the operation" },
    { .id = 'D',         .shortname = 'D', .longname = "delete",  .arg = OPT_NONE,                                .help = "delete the entry after retrieval" },
} ;

static opt_t const opts_fdholder_list[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",    .arg = OPT_NONE,                                .help = "print this help" },
    { .id = 'T',         .shortname = 'T', .longname = "timeout", .arg = OPT_REQUIRED, .argname = "milliseconds", .help = "timeout for the operation" },
} ;

static opt_cmd_t const fdholder_sub[] = {
    { .name = "store",    .help = "store a file descriptor under a name",  .operands = "id",         .opts = opts_fdholder_store,    .nopts = OPT_COUNT(opts_fdholder_store),    .on_option = &on_fdholder_store,    .fn = &ssexec_fdholder_store },
    { .name = "retrieve", .help = "retrieve a descriptor and exec prog",   .operands = "id prog...", .opts = opts_fdholder_retrieve, .nopts = OPT_COUNT(opts_fdholder_retrieve), .on_option = &on_fdholder_retrieve, .fn = &ssexec_fdholder_retrieve },
    { .name = "list",     .help = "list stored descriptor names",          .operands = NULL,         .opts = opts_fdholder_list,     .nopts = OPT_COUNT(opts_fdholder_list),     .on_option = &on_fdholder_list,     .fn = &ssexec_fdholder_list },
} ;

opt_cmd_t const cmd_fdholder = {
    .name = "66 fdholder",
    .help = "main subcommands to manage the scandir's fdholder",
    .opts = opts_fdholder_wrapper,
    .nopts = OPT_COUNT(opts_fdholder_wrapper),
    .on_option = &on_fdholder_wrapper,
    .sub = fdholder_sub,
    .nsub = OPT_COUNT(fdholder_sub),
} ;
