/*
 * ssexec_main.c
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <66/constants.h>
#include <string.h>
#include <unistd.h> // getuid, isatty, access
#include <errno.h>


#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/fd.h>
#include <oblibs/opt.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/sanitize.h>
#include <66/shutdown.h>
#include <66/config.h>

static opt_t const opts_main[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",      .arg = OPT_NONE,                                .help = "print this help" },
    { .id = 'v',         .shortname = 'v', .longname = "verbosity", .arg = OPT_REQUIRED, .argname = "number",       .help = "increase/decrease verbosity" },
    { .id = 'l',         .shortname = 'l', .longname = "live",      .arg = OPT_REQUIRED, .argname = "path",         .help = "an absolute path to the live directory" },
    { .id = 't',         .shortname = 't', .longname = "tree",      .arg = OPT_REQUIRED, .argname = "treename",     .help = "the tree to use" },
    { .id = 'T',         .shortname = 'T', .longname = "timeout",   .arg = OPT_REQUIRED, .argname = "milliseconds", .help = "a timeout in milliseconds" },
    { .id = 'z',         .shortname = 'z', .longname = "color",     .arg = OPT_NONE,                                .help = "enable colorization of the output" },
} ;

static int on_global(int id, char const *arg, void *data)
{
    ssexec_t *info = data ;

    switch (id) {

        case 'v' :

            if (!u32_scan_strict(arg, &VERBOSITY))
                log_die(LOG_EXIT_USER, "invalid verbosity level: ", arg) ;
            info->opt_verbo = 1 ;
            break ;

        case 'l' :

            if (strlen(arg) > SS_MAX_PATH)
                flog_die(LOG_EXIT_USER, "live path is too long -- it can not exceed %d", SS_MAX_PATH) ;
            info->live.len = 0 ;
            if (!auto_strbuf(&info->live, arg))
                log_die_nomem("strbuf") ;
            info->opt_live = 1 ;
            break ;

        case 't' :

            if (strlen(arg) > SS_MAX_TREENAME)
                flog_die(LOG_EXIT_USER, "tree name is too long -- it can not exceed %d", SS_MAX_TREENAME) ;
            info->treename.len = 0 ;
            if (!auto_strbuf(&info->treename, arg))
                log_die_nomem("strbuf") ;
            info->opt_tree = 1 ;
            break ;

        case 'T' :

            if (!u64_scan_strict(arg, &info->timeout))
                log_die(LOG_EXIT_USER, "invalid timeout: ", arg) ;
            info->opt_timeout = 1 ;
            break ;

        case 'z' :

            log_color = !isatty(1) ? &log_color_disable : &log_color_enable ;
            info->opt_color = 1 ;
            break ;
    }

    return 0 ;
}

static opt_t const opts_help[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help", .arg = OPT_NONE, .help = "print this help" },
} ;

static int do_version(int argc, char const *const *argv, void *data)
{
    (void)argc ;
    (void)argv ;
    (void)data ;

    log_info(SS_VERSION) ;
    return 0 ;
}

static int do_wall(int argc, char const *const *argv, void *data)
{
    (void)data ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing message to send") ;

    /** message needs to be double quoted. we don't check that here */
    hpr_wall(argv[0]) ;
    return 0 ;
}

static opt_cmd_t const cmd_wall = {
    .name = "wall",
    .help = "send a message to all logged-in users",
    .operands = "message",
    .opts = opts_help,
    .nopts = OPT_COUNT(opts_help),
    .fn = &do_wall,
} ;

static opt_cmd_t const cmd_version = {
    .name = "version",
    .help = "display the 66 version",
    .opts = opts_help,
    .nopts = OPT_COUNT(opts_help),
    .fn = &do_version,
} ;

/* A static aggregate initialiser cannot copy command nodes defined in other
 * translation units (a struct copy is not a constant expression across a TU
 * boundary) -- but their addresses are. This registry maps each node to the
 * short name it must carry inside the tree; cmd_build() copies the nodes into
 * cmd_sub at startup, overriding the name, so opt_dispatch builds the
 * "66 <cmd>" / "66 <cmd> <sub>" paths itself. */
static struct { opt_cmd_t const *node ; char const *name ; } const cmd_reg[] = {
    { &cmd_boot,        "boot" },
    { &cmd_enable,      "enable" },
    { &cmd_disable,     "disable" },
    { &cmd_start,       "start" },
    { &cmd_stop,        "stop" },
    { &cmd_configure,   "configure" },
    { &cmd_parse,       "parse" },
    { &cmd_reconfigure, "reconfigure" },
    { &cmd_reload,      "reload" },
    { &cmd_restart,     "restart" },
    { &cmd_free,        "free" },
    { &cmd_signal,      "signal" },
    { &cmd_emit,        "emit" },
    { &cmd_status,      "status" },
    { &cmd_log,         "log" },
    { &cmd_resolve,     "resolve" },
    { &cmd_state,       "state" },
    { &cmd_runstate,    "runstate" },
    { &cmd_remove,      "remove" },
    { &cmd_tree,        "tree" },
    { &cmd_snapshot,    "snapshot" },
    { &cmd_scandir,     "scandir" },
    { &cmd_env,         "env" },
    { &cmd_fdholder,    "fdholder" },
    { &cmd_poweroff,    "poweroff" },
    { &cmd_reboot,      "reboot" },
    { &cmd_halt,        "halt" },
    { &cmd_suspend,     "suspend" },
    { &cmd_hibernate,   "hibernate" },
    { &cmd_wall,        "wall" },
    { &cmd_version,     "version" },
} ;

#define CMD_NSUB OPT_COUNT(cmd_reg)

static opt_cmd_t cmd_sub[CMD_NSUB] ;

static opt_cmd_t const global_cmd = {
    .name = "66",
    .help = "init a system, control and manage services",
    .opts = opts_main,
    .nopts = OPT_COUNT(opts_main),
    .sub = cmd_sub,
    .nsub = CMD_NSUB,
    .operands = "service...|tree",
} ;

static void cmd_build(void)
{
    for (size_t i = 0 ; i < CMD_NSUB ; i++) {
        cmd_sub[i] = *cmd_reg[i].node ;
        cmd_sub[i].name = cmd_reg[i].name ;
    }
}

static uint8_t cmd_skips_sanitize(char const *cmd)
{
    static char const *const skip[] = { "boot", "snapshot", "poweroff", "reboot", "halt", "suspend", "hibernate", 0 } ;
    for (size_t i = 0 ; skip[i] ; i++)
        if (!strcmp(cmd, skip[i]))
            return 1 ;
    return 0 ;
}

static uint8_t cmd_skips_tree(char const *cmd)
{
    static char const *const skip[] = { "boot", "snapshot", "poweroff", "reboot", "halt", "suspend", "hibernate", "env", 0 } ;
    for (size_t i = 0 ; skip[i] ; i++)
        if (!strcmp(cmd, skip[i]))
            return 1 ;
    return 0 ;
}

static opt_cmd_t const *cmd_find(char const *name)
{
    if (!name)
        return 0 ;
    for (size_t i = 0 ; i < CMD_NSUB ; i++)
        if (!strcmp(name, cmd_sub[i].name))
            return &cmd_sub[i] ;
    return 0 ;
}

static void info_clean(ssexec_t *info)
{
    info->base.len = 0 ;
    info->live.len = 0 ;
    info->scandir.len = 0 ;
    info->treename.len = 0 ;
    info->environment.len = 0 ;
}

int ssexec_main(int argc, char const *const *argv, ssexec_t *info)
{
    PROG = "66" ;
    log_color = &log_color_disable ;

    cmd_build() ;
    info_clean(info) ;

    info->owner = getuid() ;
    info->ownerlen = uid_format(info->ownerstr, info->owner) ;
    info->ownerstr[info->ownerlen] = 0 ;

    if (!set_ownersysdir(&info->base, info->owner))
        log_dieusys(LOG_EXIT_SYS, "set owner directory") ;

    /* First pass: apply the global options to info and locate the command, so
     * the sanitation step below can decide what to do. opt_dispatch re-scans
     * the same global options (on_global is idempotent: it resets then sets). */
    char const *cmd = 0 ;
    {
        opt_scan_t st = OPT_SCAN_ZERO ;

        for (;;) {

            int o = opt_scan(argc, argv, opts_main, OPT_COUNT(opts_main), &st) ;
            if (o == OPT_END || o == OPT_ID_HELP || o == OPT_UNKNOWN || o == OPT_MISSARG)
                break ;
            on_global(o, st.arg, info) ;
        }

        if (st.ind < argc)
            cmd = argv[st.ind] ;
    }

    /* log lines are prefixed with the invoked command, as before. */
    if (cmd)
        PROG = cmd ;

    if (!ensure_stdfds())
        log_dieusys(LOG_EXIT_SYS, "sanitize stdin/stdout/stderr") ;

    if (cmd_find(cmd) && strcmp(cmd, "wall") && strcmp(cmd, "version")) {

        if (cmd_skips_tree(cmd))
            info->skip_opt_tree = 1 ;

        if (!cmd_skips_sanitize(cmd))
            sanitize_system(info) ;

        if (strcmp(cmd, "snapshot")) {

            // migration process
            char dst[info->base.len + SS_SYSTEM_LEN + 9 + 1] ;
            auto_strings(dst, info->base.s, SS_SYSTEM, "/.version") ;

            if (access(dst, F_OK) < 0) {

                if (errno != ENOENT)
                    log_dieusys(LOG_EXIT_SYS, "access system version file: ", dst) ;

                /* Best-effort: a container boot skips sanitize, so the system
                 * directory may not exist yet (it is created a bit later by the
                 * spawned scandir/tree commands, or was never writable). Do not
                 * abort here -- the marker is written once the state is set up. */
                log_trace("initialize system version file with version: ", SS_VERSION) ;
                if (!file_write(dst, SS_VERSION, strlen(SS_VERSION)))
                    log_1_warnusys("write system version file: ", dst) ;

            } else {

                ssize_t len = file_get_size(dst) ;
                _alloc_strbuf_(file, len + 1) ;
                if (!strbuf_read_file(&file, dst))
                    log_dieu(LOG_EXIT_SYS, "read system version file: ", dst) ;

                if (sanitize_migrate(info, file.s)) {
                    log_trace("write system version file with version: ", SS_VERSION) ;
                    if (!file_write(dst, SS_VERSION, strlen(SS_VERSION)))
                        log_dieusys(LOG_EXIT_SYS, "write system version file: ", dst) ;
                }
            }
        }

        set_info(info) ;
    }

    return opt_dispatch(argc, argv, &global_cmd, info) ;
}
