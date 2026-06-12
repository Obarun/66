/*
 * 66-shutdown.c
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
 *
 * This file is a modified copy of s6-linux-init-shutdown.c file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <utmpx.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/types.h>
#include <oblibs/clock.h>
#include <oblibs/fd.h>
#include <oblibs/io.h>

#include <skalibs/sig.h>
#include <skalibs/buffer.h>

#include <66/config.h>
#include <66/hpr.h>

#ifndef UT_NAMESIZE
#define UT_NAMESIZE 32
#endif

#define AC_FILE SS_SKEL_DIR "shutdown.allow"
#define AC_BUFSIZE 4096
#define AC_MAX 64

static char const *live = 0 ;

static opt_t const opts[] = {
    { .id = OPT_ID_HELP, .shortname = 'H', .longname = "help",          .arg = OPT_NONE,                            .help = "print this help" },
    { .id = 'v',         .shortname = 'v', .longname = "verbose",       .arg = OPT_REQUIRED, .argname = "verbosity",.help = "increase/decrease verbosity" },
    { .id = 'l',         .shortname = 'l', .longname = "live",          .arg = OPT_REQUIRED, .argname = "live",     .help = "live directory" },
    { .id = 'h',         .shortname = 'h', .longname = "halt",          .arg = OPT_NONE,                            .help = "halt the system" },
    { .id = 'p',         .shortname = 'p', .longname = "poweroff",      .arg = OPT_NONE,                            .help = "poweroff the system" },
    { .id = 'r',         .shortname = 'r', .longname = "reboot",        .arg = OPT_NONE,                            .help = "reboot the system" },
    { .id = 'k',         .shortname = 'k', .longname = "warn-users",    .arg = OPT_NONE,                            .help = "only warn users" },
    { .id = 'f',         .shortname = 'f', .longname = 0,               .arg = OPT_NONE,                            .help = "ignored (compatibility option)" },
    { .id = 'F',         .shortname = 'F', .longname = 0,               .arg = OPT_NONE,                            .help = "ignored (compatibility option)" },
    { .id = 'a',         .shortname = 'a', .longname = "access-control",.arg = OPT_NONE,                            .help = "check users access control" },
    { .id = 'c',         .shortname = 'c', .longname = "cancel",        .arg = OPT_NONE,                            .help = "cancel planned shutdown" },
    { .id = 't',         .shortname = 't', .longname = "grace-time",    .arg = OPT_REQUIRED, .argname = "seconds",  .help = "grace time between the SIGTERM and the SIGKILL" },
} ;

static opt_cmd_t const cmd = {
    .name = "66-shutdown",
    .operands = "time [ message ] or 66-shutdown -c [ message ]",
    .opts = opts,
    .nopts = OPT_COUNT(opts),
} ;

/* shutdown 01:23: date/time format parsing */

static inline void add_one_day (struct tm *tm)
{
    log_flow() ;

    tm->tm_isdst = -1 ;
    if (tm->tm_mday++ < 31) return ;
    tm->tm_mday = 1 ;
    if (tm->tm_mon++ < 11) return ;
    tm->tm_mon = 0 ;
    tm->tm_year++ ;
}

static inline void parse_hourmin (struct timespec *when, struct timespec const *now, char const *s)
{
    log_flow() ;

    struct timespec thents ;
    struct tm tmthen ;
    unsigned int hour, minute ;
    size_t len = u32_scan(s, &hour) ;
    if (!len || len > 2 || s[len] != ':' || hour > 23)
        log_die(LOG_EXIT_USER, "invalid time format") ;
    s += len+1 ;
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
    if (clock_cmp(&thents, now) < 0)
    {
        add_one_day(&tmthen) ;
        if (!clock_from_localtm(&thents, &tmthen))
            log_dieusys(LOG_EXIT_SYS, "assemble broken-down time into timespec") ;
    }
    *when = thents ;
}

static void parse_mins (struct timespec *when, struct timespec const *now, char const *s)
{
    log_flow() ;

    unsigned int mins ;
    if (!u32_scan_strict(s, &mins)) _exit(opt_emit_usage(cmd.name, &cmd)) ;
    clock_addsec(when, now, (int64_t)mins * 60) ;
}

static inline void parse_time (struct timespec *when, struct timespec const *now, char const *s)
{
    log_flow() ;

    if (!strcmp(s, "now")) *when = *now ;
    else if (s[0] == '+') parse_mins(when, now, s+1) ;
    else if (strchr(s, ':')) parse_hourmin(when, now, s) ;
    else parse_mins(when, now, s) ;
}


 /* shutdown -a: access control */

static inline unsigned char cclass (unsigned char c)
{
    log_flow() ;

    switch (c)
    {
        case 0 : return 0 ;
        case '\n' : return 1 ;
        case '#' : return 2 ;
        default : return 3 ;
    }
}

static inline unsigned int parse_authorized_users (char *buf, char const **users, unsigned int max)
{
    log_flow() ;

    static unsigned char const table[3][4] =
    {
        { 0x03, 0x00, 0x01, 0x12 },
        { 0x03, 0x00, 0x01, 0x01 },
        { 0x23, 0x20, 0x02, 0x02 }
    } ;
    size_t pos = 0 ;
    size_t mark = 0 ;
    unsigned int n = 0 ;
    unsigned int state = 0 ;
    for (; state < 3 ; pos++)
    {
        unsigned char what = table[state][cclass(buf[pos])] ;
        state = what & 3 ;
        if (what & 0x10) mark = pos ;
        if (what & 0x20)
        {
            if (n >= max)
            {
                flog_warn(AC_FILE, " lists more than %d authorized users - ignoring the extra ones", AC_MAX) ;
                break ;
            }
            buf[pos] = 0 ;
            users[n++] = buf + mark ;
        }
    }
    return n ;
}

static inline int match_users_with_utmp (char const *const *users, unsigned int n)
{
    log_flow() ;

    setutxent() ;
    for (;;)
    {
        struct utmpx *utx ;
        errno = 0 ;
        utx = getutxent() ;
        if (!utx) break ;
        if (utx->ut_type != USER_PROCESS) continue ;
        for (unsigned int i = 0 ; i < n ; i++)
            if (!strncmp(utx->ut_user, users[i], UT_NAMESIZE)) goto yes ;
    }
    endutxent() ;
    return 0 ;
    yes:
        endutxent() ;
        return 1 ;
}

static inline void access_control (void)
{
    log_flow() ;

    char buf[AC_BUFSIZE] ;
    char const *users[AC_MAX] ;
    unsigned int n ;
    struct stat st ;
    int fd = io_open(AC_FILE, O_RDONLY | O_NONBLOCK) ;
    if (fd >= 0 && !io_set_block(fd)) { close_fd(fd) ; fd = -1 ; }
    if (fd == -1)
    {
        if (errno == ENOENT) return ;
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


 /* main */

int main (int argc, char const *const *argv)
{
    unsigned int gracetime = 0 ;
    int what = 0 ;
    int doactl = 0 ;
    int docancel = 0 ;
    struct timespec when ;
    struct timespec now ;

    PROG = "66-shutdown" ;
    {
        opt_scan_t st = OPT_SCAN_ZERO ;

        for (;;)
        {
            int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
            if (o == OPT_END) break ;
            switch (o) {
                case OPT_ID_HELP : return opt_emit_help(cmd.name, &cmd) ;
                case 'v' : if (!u32_scan_strict(st.arg, &VERBOSITY)) return opt_emit_usage(cmd.name, &cmd) ; break ;
                case 'l' : live = st.arg ; break ;
                case 'h' : what = 1 ; break ;
                case 'p' : what = 2 ; break ;
                case 'r' : what = 3 ; break ;
                case 'k' : what = 4 ; break ;
                case 'a' : doactl = 1 ; break ;
                case 'f' : /* talk to the hand */ break ;
                case 'F' : /* no, the other hand */ break ;
                case 'c' : docancel = 1 ; break ;
                case 't' : if (!u32_scan_strict(st.arg, &gracetime)) return opt_emit_usage(cmd.name, &cmd) ; break ;
                default : return opt_emit_error(cmd.name, &cmd, o, &st) ;
            }
        }
        argc -= st.ind ; argv += st.ind ;
    }
    if (live && live[0] != '/') log_die(LOG_EXIT_USER,"live: ",live," must be an absolute path") ;
    else live = SS_LIVE ;

    size_t livelen = strlen(live) ;
    char tlive[livelen + INITCTL_LEN + 1] ;
    memcpy(tlive,live,livelen) ;
    memcpy(tlive + livelen,INITCTL,INITCTL_LEN) ;
    tlive[livelen + INITCTL_LEN] = 0 ;

    if (geteuid())
    {
        errno = EPERM ;
        log_dieusys(LOG_EXIT_SYS, "shutdown") ;
    }
    if (doactl) access_control() ;
    if (!clock_now(&now)) log_warnsys("get current time") ;
    if (docancel)
    {
        if (argv[0]) hpr_wall(argv[0]) ;
        if (!hpr_cancel(tlive)) goto err ;
        return 0 ;
    }
    if (!argc) return opt_emit_usage(cmd.name, &cmd) ;
    parse_time(&when, &now, argv[0]) ;
    clock_sub(&when, &when, &now) ;
    if (argv[1])
    {
        size_t len = strlen(argv[1]) ;
        char msg[sizeof(HPR_WALL_BANNER) + 1 + len] ;
        memcpy(msg, HPR_WALL_BANNER, sizeof(HPR_WALL_BANNER) - 1) ;
        msg[sizeof(HPR_WALL_BANNER) - 1] = '\n' ;
        memcpy(msg + sizeof(HPR_WALL_BANNER), argv[1], len + 1) ;
        hpr_wall(msg) ;
    }
    if (what < 4)
    {
        if (gracetime > 300)
        {
            gracetime = 300 ;
            log_warn("delay between SIGTERM and SIGKILL is capped to 300 seconds") ;
        }
        if (!hpr_shutdown(tlive,what, &when, gracetime * 1000)) goto err ;
    }
    return 0 ;
    err:
        log_dieusys(LOG_EXIT_SYS, "write to ", tlive) ;
}
