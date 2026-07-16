/*
 * ssexec_shutdown_wrapper.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 *
 * The planned-shutdown logic (time parsing, access control, wall) is a modified
 * copy of s6-linux-init-shutdown.c coming from skarnet software at
 * https://skarnet.org/software/s6-linux-init.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <utmpx.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/opt.h>
#include <oblibs/exec.h>
#include <oblibs/environ.h>
#include <oblibs/types.h>
#include <oblibs/clock.h>
#include <oblibs/fd.h>
#include <oblibs/io.h>

#include <66/ssexec.h>
#include <66/config.h>
#include <66/hpr.h>

#ifndef UT_NAMESIZE
#define UT_NAMESIZE 32
#endif

#define AC_FILE SS_SKEL_DIR "shutdown.allow"
#define AC_BUFSIZE 4096
#define AC_MAX 64

static opt_t const opts_shutdown[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",         .arg = OPT_NONE,                           .help = "print this help" },
    { .id = 'a',         .shortname = 'a', .longname = "access",       .arg = OPT_NONE,                           .help = "use access control" },
    { .id = 'c',         .shortname = 'c', .longname = "cancel",       .arg = OPT_NONE,                           .help = "cancel a planned shutdown" },
    { .id = 'f',         .shortname = 'f', .longname = "force",        .arg = OPT_NONE,                           .help = "sync filesytem and immediately stop the system" },
    { .id = 'F',         .shortname = 'F', .longname = "force-nosync", .arg = OPT_NONE,                           .help = "do not sync filesytem and immediately stop the system" },
    { .id = 'm',         .shortname = 'm', .longname = "message",      .arg = OPT_REQUIRED, .argname = "message", .help = "replace the default message by message" },
    { .id = 't',         .shortname = 't', .longname = "timeout",      .arg = OPT_REQUIRED, .argname = "seconds", .help = "grace time between the SIGTERM and the SIGKILL" },
    { .id = 'W',         .shortname = 'W', .longname = "no-wall",      .arg = OPT_NONE,                           .help = "do not send a wall message to users" },
} ;

static uint8_t opt_acl = 0, opt_cancel = 0, opt_force = 0, opt_nowall = 0 ;
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

        case 'c' :
            opt_cancel++ ;
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

static inline void add_one_day(struct tm *tm)
{
    tm->tm_isdst = -1 ;
    if (tm->tm_mday++ < 31) return ;
    tm->tm_mday = 1 ;
    if (tm->tm_mon++ < 11) return ;
    tm->tm_mon = 0 ;
    tm->tm_year++ ;
}

static inline void parse_hourmin(struct timespec *when, struct timespec const *now, char const *s)
{
    struct timespec thents ;
    struct tm tmthen ;
    unsigned int hour, minute ;
    size_t len = u32_scan(s, &hour) ;

    if (!len || len > 2 || s[len] != ':' || hour > 23)
        log_die(LOG_EXIT_USER, "invalid time format") ;
    s += len + 1 ;
    len = u32_scan_strict(s, &minute) ;

    if (!len || len != 2 || minute > 59)
        log_die(LOG_EXIT_USER, "invalid time format") ;

    if (!clock_to_localtm(&tmthen, now))
        log_dieusys(LOG_EXIT_SYS, "break down current time into struct tm") ;

    tmthen.tm_hour = hour ;
    tmthen.tm_min = minute ;
    tmthen.tm_sec = 0 ;

    if (!clock_from_localtm(&thents, &tmthen))
        log_dieusys(LOG_EXIT_SYS, "assemble broken-down time into timespec") ;

    if (clock_cmp(&thents, now) < 0) {

        add_one_day(&tmthen) ;

        if (!clock_from_localtm(&thents, &tmthen))
            log_dieusys(LOG_EXIT_SYS, "assemble broken-down time into timespec") ;
    }
    *when = thents ;
}

static void parse_mins(struct timespec *when, struct timespec const *now, char const *s)
{
    unsigned int mins ;
    if (!u32_scan_strict(s, &mins))
        log_die(LOG_EXIT_USER, "invalid time format") ;
    clock_addsec(when, now, (int64_t)mins * 60) ;
}

static inline void parse_time(struct timespec *when, struct timespec const *now, char const *s)
{
    if (!strcmp(s, "now")) *when = *now ;
    else if (s[0] == '+') parse_mins(when, now, s + 1) ;
    else if (strchr(s, ':')) parse_hourmin(when, now, s) ;
    else parse_mins(when, now, s) ;
}

static inline unsigned char cclass(unsigned char c)
{
    switch (c) {
        case 0 : return 0 ;
        case '\n' : return 1 ;
        case '#' : return 2 ;
        default : return 3 ;
    }
}

static inline unsigned int parse_authorized_users(char *buf, char const **users, unsigned int max)
{
    static unsigned char const table[3][4] =
    {
        { 0x03, 0x00, 0x01, 0x12 },
        { 0x03, 0x00, 0x01, 0x01 },
        { 0x23, 0x20, 0x02, 0x02 }
    } ;
    size_t mark = 0 ;
    unsigned int n = 0 ;
    unsigned int state = 0 ;
    for (size_t pos = 0 ; state < 3 ; pos++) {
        unsigned char what = table[state][cclass(buf[pos])] ;
        state = what & 3 ;
        if (what & 0x10) mark = pos ;
        if (what & 0x20) {
            if (n >= max) {
                flog_warn(AC_FILE " lists more than %d authorized users - ignoring the extra ones", AC_MAX) ;
                break ;
            }
            buf[pos] = 0 ;
            users[n++] = buf + mark ;
        }
    }
    return n ;
}

static inline int match_users_with_utmp(char const *const *users, unsigned int n)
{
    setutxent() ;
    for (;;) {
        struct utmpx *utx ;
        errno = 0 ;
        utx = getutxent() ;
        if (!utx)
            break ;
        if (utx->ut_type != USER_PROCESS)
            continue ;
        for (unsigned int i = 0 ; i < n ; i++) {

            if (!strncmp(utx->ut_user, users[i], UT_NAMESIZE)) {
                endutxent() ;
                return 1 ;
            }
        }
    }
    endutxent() ;
    return 0 ;
}

static inline void access_control(void)
{
    char buf[AC_BUFSIZE] ;
    char const *users[AC_MAX] ;
    unsigned int n ;
    struct stat st ;

    int fd = io_open(AC_FILE, O_RDONLY | O_NONBLOCK) ;

    if (fd >= 0 && !io_set_block(fd)) {
        close_fd(fd) ;
        fd = -1 ;
    }

    if (fd == -1) {
        if (errno == ENOENT)
            return ;
        log_dieusys(LOG_EXIT_SYS, "open ", AC_FILE) ;
    }

    if (fstat(fd, &st) == -1)
        log_dieusys(LOG_EXIT_SYS, "stat ", AC_FILE) ;

    if (st.st_size >= AC_BUFSIZE)
        flog_die(LOG_EXIT_ONE, "%s is too big: it needs to be %d bytes or less", AC_FILE, AC_BUFSIZE) ;

    if (io_allread(fd, buf, st.st_size) < (size_t)st.st_size)
        log_dieusys(LOG_EXIT_SYS, "read ", AC_FILE) ;

    close_fd(fd) ;

    buf[st.st_size] = 0 ;
    n = parse_authorized_users(buf, users, AC_MAX) ;

    if (!n || !match_users_with_utmp(users, n))
        log_die(LOG_EXIT_ONE, "no authorized users logged in") ;
}

static void wall_message(char const *msg)
{
    size_t len = strlen(msg) ;
    char m[sizeof(HPR_WALL_BANNER) + 1 + len] ;

    auto_strings(m, HPR_WALL_BANNER "\n", msg) ;
    hpr_wall(m) ;
}

static int shutdown_run(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy. */
    uint8_t acl = opt_acl, cancel = opt_cancel, force = opt_force, nowall = opt_nowall ;
    char const *msg = opt_msg, *timeout = opt_time, *command = hpr_command ;
    opt_acl = 0 ;
    opt_cancel = 0 ;
    opt_force = 0 ;
    opt_nowall = 0 ;
    opt_msg = 0 ;
    opt_time = 0 ;
    hpr_command = 0 ;

    char const *when = "now" ;
    if (argc && argv[0])
        when = argv[0] ;

    /* -f/-F: sync and stop the hardware right now, through 66-hpr */
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

    char const *live = info->live.s ;
    size_t livelen = strlen(live) ;
    char tlive[livelen + INITCTL_LEN + 1] ;
    auto_strings(tlive, live, INITCTL) ;

    if (geteuid()) {
        errno = EPERM ;
        log_dieusys(LOG_EXIT_SYS, "shutdown") ;
    }

    if (acl)
        access_control() ;

    if (cancel) {
        // planned shutdown / cancel: talk to the 66-shutdownd fifo in-process
        if (msg && !nowall)
            hpr_wall(msg) ;
        if (!hpr_cancel(tlive))
            goto err ;
        return 0 ;
    }

    struct timespec now, whents ;
    if (!clock_now(&now))
        log_warnsys("get current time") ;

    parse_time(&whents, &now, when) ;
    clock_sub(&whents, &whents, &now) ;

    if (msg && !nowall)
        wall_message(msg) ;

    unsigned int gracetime = 0 ;

    if (timeout && !u32_scan_strict(timeout, &gracetime))
        log_die(LOG_EXIT_USER, "invalid timeout: ", timeout) ;

    if (gracetime > 300) {
        gracetime = 300 ;
        log_warn("delay between SIGTERM and SIGKILL is capped to 300 seconds") ;
    }

    int what = !strcmp(command, "-p") ? 2 : !strcmp(command, "-r") ? 3 : 1 ; /* -h */
    if (!hpr_shutdown(tlive, (unsigned int)what, &whents, gracetime * 1000))
        goto err ;

    return 0 ;

    err:
        log_dieusys(LOG_EXIT_SYS, "write to ", tlive) ;
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
