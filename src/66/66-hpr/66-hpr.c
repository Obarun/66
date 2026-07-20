/*
 * 66-hpr.c
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
 *
 * This file is a modified copy of s6-linux-init-hpr.c file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <utmpx.h>
#include <sys/reboot.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/opt.h>
#include <oblibs/clock.h>
#include <oblibs/io.h>
#include <oblibs/files.h> // macro hpr_send

#include <66/shutdown.h>
#include <66/config.h>

#define HPR_POWER_STATE "/sys/power/state"

#ifndef UT_NAMESIZE
#define UT_NAMESIZE 32
#endif

#ifndef UT_HOSTSIZE
#define UT_HOSTSIZE 256
#endif

#ifndef _PATH_WTMP
#define _PATH_WTMP "/dev/null/wtmp"
#ifdef WTMPX_FILE
#define _PATH_WTMP WTMPX_FILE
#else
#define _PATH_WTMP "/var/log/wtmp"
#endif
#endif

char const *banner = 0 ;
char const *live = 0 ;

static opt_t const opts[] = {
    { .id = OPT_ID_HELP, .shortname = 'H', .longname = "help",     .arg = OPT_NONE,                          .help = "print this help" },
    { .id = 'l',         .shortname = 'l', .longname = "live",     .arg = OPT_REQUIRED, .argname = "path",   .help = "live directory" },
    { .id = 'b',         .shortname = 'b', .longname = "banner",   .arg = OPT_REQUIRED, .argname = "message",.help = "end banner to display" },
    { .id = 'f',         .shortname = 'f', .longname = "force",    .arg = OPT_NONE,                          .help = "force" },
    { .id = 'h',         .shortname = 'h', .longname = "halt",     .arg = OPT_NONE,                          .help = "halt the system" },
    { .id = 'p',         .shortname = 'p', .longname = "poweroff", .arg = OPT_NONE,                          .help = "poweroff the system" },
    { .id = 'r',         .shortname = 'r', .longname = "reboot",   .arg = OPT_NONE,                          .help = "reboot the system" },
    { .id = 's',         .shortname = 's', .longname = "suspend",  .arg = OPT_NONE,                          .help = "suspend the system to RAM" },
    { .id = 'i',         .shortname = 'i', .longname = "hibernate",.arg = OPT_NONE,                          .help = "hibernate the system to disk" },
    { .id = 'n',         .shortname = 'n', .longname = "no-sync",  .arg = OPT_NONE,                          .help = "do not sync" },
    { .id = 'd',         .shortname = 'd', .longname = "no-wtmp",  .arg = OPT_NONE,                          .help = "do not write wtmp shutdown entry" },
    { .id = 'w',         .shortname = 'w', .longname = "wtmp-only", .arg = OPT_NONE,                         .help = "only write wtmp shutdown entry" },
    { .id = 'W',         .shortname = 'W', .longname = "no-wall",  .arg = OPT_NONE,                          .help = "do not send a wall message" },
} ;

static opt_cmd_t const cmd = {
    .name = "66-hpr",
    .opts = opts,
    .nopts = OPT_COUNT(opts),
} ;

int main (int argc, char const *const *argv)
{
    int what = 0 ;
    int force = 0 ;
    int dowtmp = 1 ;
    int dowall = 1 ;
    int dosync = 1 ;
    struct timespec now ;

    PROG = "66-hpr" ;
    {
        opt_scan_t st = OPT_SCAN_ZERO ;

        for (;;)
        {
            int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
            if (o == OPT_END) break ;
            switch (o) {
                case OPT_ID_HELP : return opt_emit_help(cmd.name, &cmd) ;
                case 'l' : live = st.arg ; break ;
                case 'h' : what = 1 ; break ;
                case 'p' : what = 2 ; break ;
                case 'r' : what = 3 ; break ;
                case 's' : what = 4 ; break ;
                case 'i' : what = 5 ; break ;
                case 'f' : force = 1 ; break ;
                case 'd' : dowtmp = 0 ; break ;
                case 'w' : dowtmp = 2 ; break ;
                case 'W' : dowall = 0 ; break ;
                case 'n' : dosync = 0 ; break ;
                case 'b' : banner = st.arg ; break ;
                default : return opt_emit_error(cmd.name, &cmd, o, &st) ;
            }
        }
        argc -= st.ind ; argv += st.ind ;
    }
    if (!banner) banner = HPR_WALL_BANNER ;
    if (live && live[0] != '/') log_die(LOG_EXIT_USER,"live: ",live," must be an absolute path") ;
    else live = SS_LIVE ;
    if (!what)
        log_die(LOG_EXIT_USER, "one of the -h, -p, -r, -s or -i options must be given") ;

    if (geteuid())
    {
        errno = EPERM ;
        log_diesys(LOG_EXIT_USER, "nice try, but you need to be root") ;
    }

    if (what >= 4)
    {
        char const *state = what == 5 ? "disk\n" : "mem\n" ;
        if (dosync) sync() ;
        // io_writenclose blocks until the system wakes up, then closes the fd
        if (!io_writenclose(io_open(HPR_POWER_STATE, O_WRONLY | O_CLOEXEC), state, strlen(state)))
            log_dieusys(LOG_EXIT_SYS, "write to ", HPR_POWER_STATE) ;
        return 0 ;
    }

    if (force)
    {
        if (dosync) sync() ;
        reboot(what == 3 ? RB_AUTOBOOT : what == 2 ? RB_POWER_OFF : RB_HALT_SYSTEM) ;
            log_dieusys(LOG_EXIT_SYS, "reboot()") ;
    }

    if (!clock_now(&now)) log_warnsys("get current time") ;

    size_t livelen = strlen(live) ;
    char tlive[livelen + INITCTL_LEN + 1] ;

    auto_strings(tlive, live, INITCTL) ;

    if (!hpr_send(tlive, "", 0)) {
        errno = EPERM ;
        log_dieusys(LOG_EXIT_SYS, "talk to shutdownd") ;
    }

    if (dowtmp)
    {
        struct utmpx utx =
        {
            .ut_type = RUN_LVL,
            .ut_pid = getpid(),
            .ut_line = "~",
            .ut_id = "",
            .ut_session = getsid(0)
        } ;
        strncpy(utx.ut_user, what == 3 ? "reboot" : "shutdown", UT_NAMESIZE) ;
        if (gethostname(utx.ut_host, UT_HOSTSIZE) < 0)
        {
            utx.ut_host[0] = 0 ;
            log_warnusys("gethostname") ;
        }
        else utx.ut_host[UT_HOSTSIZE - 1] = 0 ;

/* glibc multilib can go fuck itself */
#ifdef  __WORDSIZE_TIME64_COMPAT32
    {
        struct timeval tv ;
        clock_to_timeval(&tv, &now) ;
        utx.ut_tv.tv_sec = tv.tv_sec ;
        utx.ut_tv.tv_usec = tv.tv_usec ;
    }
#else
    clock_to_timeval(&utx.ut_tv, &now) ;
#endif

        updwtmpx(_PATH_WTMP, &utx) ;
    }
    if (dowall) hpr_wall(banner) ;
    if (dowtmp < 2)
    {
        size_t livelen = strlen(live) ;
        char tlive[livelen + INITCTL_LEN + 1] ;

        auto_strings(tlive, live, INITCTL) ;

        if (!hpr_shutdown(tlive,what, &(struct timespec){0,0}, 0))
            log_dieusys(LOG_EXIT_SYS, "notify 66-shutdownd") ;
    }
    return 0 ;
}
