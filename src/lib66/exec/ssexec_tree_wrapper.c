/*
 * ssexec_tree_wrapper.c
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

#include <oblibs/opt.h>

#include <66/ssexec.h>

/* The dispatch tree lives here: each sub-command's option table is defined in
 * this file (so OPT_COUNT is a constant), while the leaves export only their
 * option applier (on_*) and their handler (ssexec_*). The create/admin family
 * share ssexec_tree_admin; remove/enable/disable/current preset the action via
 * on_tree_admin. start/stop/free share ssexec_tree_signal through the leaf's
 * do_tree_* helpers (which set the signal name). */

extern opt_on_option_fn on_tree_admin ;
extern opt_on_option_fn on_tree_signal ;
extern opt_on_option_fn on_tree_status ;
extern opt_on_option_fn on_tree_resolve ;
extern opt_on_option_fn on_tree_init ;

extern opt_cmd_fn do_tree_start ;
extern opt_cmd_fn do_tree_stop ;
extern opt_cmd_fn do_tree_free ;

static int do_tree_remove(int argc, char const *const *argv, void *data)
{
    on_tree_admin('R', 0, data) ;
    return ssexec_tree_admin(argc, argv, data) ;
}

static int do_tree_enable(int argc, char const *const *argv, void *data)
{
    on_tree_admin('E', 0, data) ;
    return ssexec_tree_admin(argc, argv, data) ;
}

static int do_tree_disable(int argc, char const *const *argv, void *data)
{
    on_tree_admin('D', 0, data) ;
    return ssexec_tree_admin(argc, argv, data) ;
}

static int do_tree_current(int argc, char const *const *argv, void *data)
{
    on_tree_admin('c', 0, data) ;
    return ssexec_tree_admin(argc, argv, data) ;
}

static opt_t const opts_help[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
} ;

static opt_t const opts_tree_admin[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",    .arg = OPT_NONE,                             .help = "print this help" },
    { .id = 'o',         .shortname = 'o', .longname = "options", .arg = OPT_REQUIRED, .argname = "option:...", .help = "colon-separated tree options (see below)" },
    { .id = 'c',         .shortname = 'c',                        .arg = OPT_NONE,                             .help = "mark tree as current (injected by the tree wrapper)", .hidden = true },
    { .id = 'E',         .shortname = 'E',                        .arg = OPT_NONE,                             .help = "enable tree (injected by the tree wrapper)", .hidden = true },
    { .id = 'D',         .shortname = 'D',                        .arg = OPT_NONE,                             .help = "disable tree (injected by the tree wrapper)", .hidden = true },
    { .id = 'R',         .shortname = 'R',                        .arg = OPT_NONE,                             .help = "remove tree (injected by the tree wrapper)", .hidden = true },
} ;

static opt_t const opts_tree_resolve[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",    .arg = OPT_NONE,                             .help = "print this help" },
    { .id = 'f',         .shortname = 'f', .longname = "field",   .arg = OPT_REQUIRED, .argname = "field,...", .help = "display only these comma-separated fields" },
    { .id = 'n',         .shortname = 'n', .longname = "no-name", .arg = OPT_NONE,                             .help = "display only the value, not the field name" },
} ;

static opt_t const opts_tree_signal[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
    { .id = 'f',         .shortname = 'f', .longname = "fork", .arg = OPT_NONE, .help = "fork the process" },
} ;

static opt_t const opts_tree_init[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",   .arg = OPT_NONE,                            .help = "print this help" },
    { .id = 'g',         .shortname = 'g', .longname = "group", .arg = OPT_REQUIRED, .argname = "group",    .help = "initiate every tree of this group instead of a single tree" },
} ;

static opt_t const opts_tree_status[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",     .arg = OPT_NONE,                             .help = "print this help" },
    { .id = 'n',         .shortname = 'n', .longname = "no-name",  .arg = OPT_NONE,                             .help = "do not display the names of fields" },
    { .id = 'o',         .shortname = 'o', .longname = "options",  .arg = OPT_REQUIRED, .argname = "field,...", .help = "deprecated options, please use -f instead", .hidden = true },
    { .id = 'f',         .shortname = 'f', .longname = "field",    .arg = OPT_REQUIRED, .argname = "field,...", .help = "display only these comma-separated fields" },
    { .id = 'g',         .shortname = 'g', .longname = "graph",    .arg = OPT_NONE,                             .help = "displays the contents field as graph" },
    { .id = 'r',         .shortname = 'r', .longname = "reverse",  .arg = OPT_NONE,                             .help = "reverse the contents field" },
    { .id = 'd',         .shortname = 'd', .longname = "depth",    .arg = OPT_REQUIRED, .argname = "number",    .help = "limit the depth of the contents field recursion by depth" },
} ;

static char const tree_admin_epilog[] =
    "options for -o (colon-separated):\n"
    "    enable               enable the tree\n"
    "    noseed               do not apply a seed file\n"
    "    clone                clone an existing tree\n"
    "    depends=tree,...     trees this one depends on (or none)\n"
    "    requiredby=tree,...  trees required by this one (or none)\n"
    "    groups=group,...     boot, admin, user or none\n"
    "    allow=user,...       accounts allowed to use the tree\n"
    "    deny=user,...        accounts denied" ;

static char const tree_status_epilog[] =
    "field:\n"
    "    name          current       enabled\n"
    "    allowed       groups        depends\n"
    "    requiredby    contents" ;

static char const tree_resolve_epilog[] =
    "field:\n"
    "    name          enabled       depends\n"
    "    requiredby    allow         groups\n"
    "    contents      ndepends      nrequiredby\n"
    "    nallow        ngroups       ncontents\n"
    "    rversion" ;

static opt_cmd_t const tree_sub[] = {
    { .name = "create",  .help = "create a tree", .operands = "tree", .opts = opts_tree_admin, .nopts = OPT_COUNT(opts_tree_admin), .on_option = &on_tree_admin, .fn = &ssexec_tree_admin, .epilog = tree_admin_epilog },
    { .name = "admin",   .help = "administrate an existing tree", .operands = "tree", .opts = opts_tree_admin, .nopts = OPT_COUNT(opts_tree_admin), .on_option = &on_tree_admin, .fn = &ssexec_tree_admin, .epilog = tree_admin_epilog },
    { .name = "remove",  .help = "remove an existing tree", .operands = "tree", .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_tree_remove },
    { .name = "enable",  .help = "enable a tree", .operands = "tree", .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_tree_enable },
    { .name = "disable", .help = "disable a tree", .operands = "tree", .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_tree_disable },
    { .name = "current", .help = "mark a tree as the current one", .operands = "tree", .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_tree_current },
    { .name = "resolve", .help = "display the resolve files contents of tree", .operands = "tree", .opts = opts_tree_resolve, .nopts = OPT_COUNT(opts_tree_resolve), .on_option = &on_tree_resolve, .fn = &ssexec_tree_resolve, .epilog = tree_resolve_epilog },
    { .name = "status",  .help = "display information about tree", .operands = "tree", .opts = opts_tree_status, .nopts = OPT_COUNT(opts_tree_status), .on_option = &on_tree_status, .fn = &ssexec_tree_status, .epilog = tree_status_epilog },
    { .name = "init",    .help = "initiate all enabled services of a tree(s) to a scandir", .operands = "tree", .opts = opts_tree_init, .nopts = OPT_COUNT(opts_tree_init), .on_option = &on_tree_init, .fn = &ssexec_tree_init },
    { .name = "start",   .help = "bring up all services of a tree", .operands = "tree", .opts = opts_help, .nopts = OPT_COUNT(opts_help), .fn = &do_tree_start },
    { .name = "stop",    .help = "bring down all services of a tree", .operands = "tree", .opts = opts_tree_signal, .nopts = OPT_COUNT(opts_tree_signal), .on_option = &on_tree_signal, .fn = &do_tree_stop },
    { .name = "free",    .help = "bring down all services of a tree and unsupervise them", .operands = "tree", .opts = opts_tree_signal, .nopts = OPT_COUNT(opts_tree_signal), .on_option = &on_tree_signal, .fn = &do_tree_free },
} ;

opt_cmd_t const cmd_tree = {
    .name = "66 tree",
    .help = "main subcommands to manage trees",
    .opts = opts_help,
    .nopts = OPT_COUNT(opts_help),
    .sub = tree_sub,
    .nsub = OPT_COUNT(tree_sub),
} ;
