/*
 * 66-shutdownd.c
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
 * This file is a modified copy of s6-linux-init-shutdownd.c file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>

#include <oblibs/environ.h>
#include <oblibs/files.h>
#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>
#include <oblibs/types.h>
#include <oblibs/clock.h>
#include <oblibs/fd.h>
#include <oblibs/stream.h>
#include <oblibs/io.h>
#include <oblibs/spawn.h>

#include <skalibs/posixplz.h>
#include <skalibs/sig.h>
#include <skalibs/tai.h>
#include <skalibs/direntry.h>
#include <skalibs/djbunix.h>
#include <skalibs/iopause.h>

#include <execline/config.h>

#include <s6/supervise.h>
#include <s6/config.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/svc.h>

#define STAGE4_FILE "stage4"
#define DOTPREFIX ".66-shutdownd:"
#define DOTPREFIXLEN (sizeof(DOTPREFIX) - 1)
#define DOTSUFFIX ":XXXXXX"
#define DOTSUFFIXLEN (sizeof(DOTSUFFIX) - 1)
#define SHUTDOWND_FIFO "fifo"
static char const *conf = SS_SKEL_DIR ;
static char const *live = 0 ;
static int inns = 0 ;
static int nologger = 0 ;

static opt_t const opts[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",      .arg = OPT_NONE,                                .help = "print this help" },
    { .id = 'l',         .shortname = 'l', .longname = "live",      .arg = OPT_REQUIRED, .argname = "path",         .help = "live directory" },
    { .id = 's',         .shortname = 's', .longname = "skeleton",  .arg = OPT_REQUIRED, .argname = "path",         .help = "skeleton directory" },
    { .id = 'g',         .shortname = 'g', .longname = "grace-time",.arg = OPT_REQUIRED, .argname = "milliseconds", .help = "grace time between the SIGTERM and the SIGKILL" },
    { .id = 'B',         .shortname = 'B', .longname = "container", .arg = OPT_NONE,                                .help = "the system is running inside a container" },
    { .id = 'c',         .shortname = 'c', .longname = "no-logger", .arg = OPT_NONE,                                .help = "the catch-all logger do not exist" },
} ;

static opt_cmd_t const cmd = {
    .name = "66-shutdownd",
    .opts = opts,
    .nopts = OPT_COUNT(opts),
} ;

static void restore_console (void)
{
    log_flow() ;

    close_fd(1) ;
    if (io_open("/dev/console", O_WRONLY) != 1 && io_open("/dev/null", O_WRONLY) != 1)
        log_warnusys("open /dev/console for writing") ;
    else if (copy_fd(2, 1) < 0)
        log_warnusys("copy_fd") ;
}

struct at_s
{
    int fd ;
    char const *name ;
} ;

static int renametemp (char const *s, mode_t mode, void *data)
{
    log_flow() ;

    struct at_s *at = data ;
    (void)mode ;
    return renameat(at->fd, at->name, at->fd, s) ;
}

static int mkrenametemp (int fd, char const *src, char *dst)
{
    log_flow() ;

    struct at_s at = { .fd = fd, .name = src } ;
    return mkfiletemp(dst, &renametemp, 0700, &at) ;
}

ssize_t file_get_size(const char* filename)
{
    log_flow() ;

    struct stat st;
    errno = 0 ;
    if (stat(filename, &st) == -1) return -1 ;
    return st.st_size;
}

static inline void auto_conf(char *confile,size_t conflen)
{
    log_flow() ;

    memcpy(confile,conf,conflen) ;
    confile[conflen] = '/' ;
    memcpy(confile + conflen + 1, SS_BOOT_CONF, SS_BOOT_CONF_LEN) ;
    confile[conflen + 1 + SS_BOOT_CONF_LEN] = 0 ;
}

static void parse_conf(char const *confile,char *rcshut,char const *key)
{
    log_flow() ;

    size_t filesize = file_get_size(confile) ;
    _alloc_strbuf_(stk, filesize + 1) ;
    _alloc_sbl_(val, filesize + 1) ;
    if (!strbuf_read_file(&stk, confile))
        log_dieusys(LOG_EXIT_SYS,"read file: ",confile) ;
    if (environ_search_value(&val, stk.s, key)) {
        memcpy(rcshut,val.s,val.len) ;
        rcshut[val.len] = 0 ;
    }
}

static inline void run_rcshut (void)
{
    log_flow() ;

    pid_t pid ;
    size_t conflen = strlen(conf) ;
    char rcshut[4096] ;
    char confile[conflen + 1 + SS_BOOT_CONF_LEN] ;
    auto_conf(confile,conflen) ;
    parse_conf(confile,rcshut,"RCSHUTDOWN") ;
    char const *rcshut_argv[3] = { rcshut, confile, 0 } ;
    pid = spawn_path(rcshut_argv[0], rcshut_argv, (char const *const *)environ) ;
    if (pid)
    {
        int wstat ;
        if (wait_pid(pid, &wstat) == -1) log_dieusys(LOG_EXIT_SYS, "waitpid") ;
        if (WIFSIGNALED(wstat))
            flog_warn(rcshut, " was killed by signal %d", WTERMSIG(wstat)) ;
        else if (WEXITSTATUS(wstat))
            flog_warn("%s exited %d", rcshut, WEXITSTATUS(wstat)) ;
    }
    else log_warnusys("spawn ", rcshut) ;
}

static inline void prepare_shutdown (istream *b, tain *deadline, unsigned int *grace_time)
{
    log_flow() ;

    uint32_t u ;
    char pack[CLOCK_PACK + 4] ;
    size_t w = 0 ;
    int r = istream_getall(b, pack, CLOCK_PACK + 4, &w) ;
    if (r < 0 && errno != EPIPE) log_dieusys(LOG_EXIT_SYS, "read from pipe") ;
    if (r != 1) log_dieusys(LOG_EXIT_SYS, "bad shutdown protocol") ;
    struct timespec rel ;
    clock_unpack(pack, &rel) ;
    tain trel = { .sec = { .x = (uint64_t)rel.tv_sec }, .nano = (uint32_t)rel.tv_nsec } ;
    tain_add_g(deadline, &trel) ;   /* relative (wire) -> absolute monotonic for iopause_g */
    u32_unpack_big(pack + CLOCK_PACK, &u) ;
    if (u && u <= 300000) *grace_time = u ;
}

static inline void handle_fifo (istream *b, char *what, tain *deadline, unsigned int *grace_time)
{
    log_flow() ;

    for (;;)
    {
        char c ;
        size_t w = 0 ;
        if (istream_getall(b, &c, 1, &w) < 0 && errno != EPIPE)
            log_dieusys(LOG_EXIT_SYS, "read from pipe") ;
        if (!w) break ;   // would-block or EOF: nothing more -> back to iopause
        switch (c)
        {
            case 'S' :
            case 'h' :
            case 'p' :
            case 'r' :
                *what = c ;
                prepare_shutdown(b, deadline, grace_time) ;
                break ;
            case 'c' :
                *what = 'S' ;
                tain_add_g(deadline, &tain_infinite_relative) ;
                break ;
            default :
                {
                    char s[2] = { c, 0 } ;
                    log_warn("unknown command: ", s) ;
                }
                break ;
        }
    }
}

static inline void prepare_stage4 (char what)
{
    log_flow() ;

    ostream b ;
    int fd ;
    char buf[512] ;
    char shutfinal[4096] ; //huge path allowed
    size_t conflen = strlen(conf) ;
    char confile[conflen + 1 + SS_BOOT_CONF_LEN] ;
    auto_conf(confile,conflen) ;
    parse_conf(confile,shutfinal,"RCSHUTDOWNFINAL") ;

    if (inns) {

        char s[2] = { what, '\n' } ;
        _alloc_strbuf_(stk, 30) ;
        char ownerstr[UID_FMT] ;
        size_t olen = uid_format(ownerstr, getuid()), livelen = strlen(live) ;
        char tmp[livelen + SS_BOOT_CONTAINER_DIR_LEN + 1 + olen + 1 + SS_BOOT_CONTAINER_HALTFILE_LEN + 1] ;
        ownerstr[olen] = 0 ;

        auto_strings(tmp, live, SS_BOOT_CONTAINER_DIR, "/", ownerstr, "/", SS_BOOT_CONTAINER_HALTFILE) ;

        auto_strings(stk.s, "HALTCODE=", s, "\nEXITCODE=0\n") ;
        stk.len = 22 ;

        if (!file_write(tmp, stk.s, stk.len))
            log_dieusys(LOG_EXIT_SYS, "write file: ", tmp) ;
    }

    unlink_void(STAGE4_FILE ".new") ;
    fd = io_open_mode(STAGE4_FILE ".new", O_WRONLY|O_CREAT|O_EXCL|O_NONBLOCK, 0666) ;
    if (fd == -1) log_dieusys(LOG_EXIT_SYS, "open ", STAGE4_FILE ".new", " for writing") ;
    ostream_init(&b, fd, buf, 512) ;

    if (inns) {

        if (!ostream_puts(&b,
            "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n\n"
            EXECLINE_EXTBINPREFIX "foreground { "
            S6_EXTBINPREFIX "s6-svc -0x -- . }\n"
            EXECLINE_EXTBINPREFIX "background\n{\n  ")

            || (!nologger && !ostream_puts(&b,
            EXECLINE_EXTBINPREFIX "foreground { "
            S6_EXTBINPREFIX "s6-svc -0xc -- ")
            || !ostream_puts(&b,live)
            || !ostream_puts(&b,SS_BOOT_LOG " }\n  "))

            || !ostream_puts(&b, S6_EXTBINPREFIX "66 -l ")
            || !ostream_puts(&b, live)
            || !ostream_puts(&b, " scandir abort\n}\n"))
            log_dieusys(LOG_EXIT_SYS, "write to ", STAGE4_FILE ".new") ;
    }
    else
    {
        if (!ostream_puts(&b,
            "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n\n"
            EXECLINE_EXTBINPREFIX "foreground { "
            SS_BINPREFIX "66-umountall }\n"
            EXECLINE_EXTBINPREFIX "foreground { tryexec { ")
            || !ostream_put(&b,shutfinal,strlen(shutfinal))
            || !ostream_puts(&b," } }\n"
            SS_BINPREFIX "66-hpr -f -")
            || !ostream_put(&b, &what, 1)
            || !ostream_putflush(&b, "\n", 1)) log_dieusys(LOG_EXIT_SYS, "write to ", STAGE4_FILE ".new") ;
    }
    if (fchmod(fd, S_IRWXU) == -1) log_dieusys(LOG_EXIT_SYS, "fchmod ", STAGE4_FILE ".new") ;
    close_fd(fd) ;
    if (rename(STAGE4_FILE ".new", STAGE4_FILE) == -1)
        log_dieusys(LOG_EXIT_SYS, "rename ", STAGE4_FILE ".new", " to ", STAGE4_FILE) ;
}

static inline void unsupervise_tree (void)
{
    log_flow() ;

    char const *except[5] =
    {
        SS_BOOT_SHUTDOWND,
        nologger ? 0 : SS_SCANDIR "-" SS_LOG,
        SS_ONESHOTD,
        SS_FDHOLDER,
        0
    } ;
    size_t livelen = strlen(live) ;
    size_t newlen ;
    char tmp[livelen + 1 + SS_SCANDIR_LEN + 3 + 1] ;
    memcpy(tmp,live,livelen) ;
    memcpy(tmp + livelen,"/" SS_SCANDIR "/0/",SS_SCANDIR_LEN + 4) ;
    tmp[livelen + SS_SCANDIR_LEN + 4] = 0 ;
    newlen = livelen + SS_SCANDIR_LEN + 4 ;
    DIR *dir = opendir(tmp) ;
    int fdd ;
    if (!dir) log_dieusys(LOG_EXIT_SYS, "opendir: ",tmp) ;
    fdd = dirfd(dir) ;
    if (fdd == -1) log_dieusys(LOG_EXIT_SYS, "dir_fd: ",tmp) ;
    for (;;)
    {
        char const *const *p = except ;
        direntry *d ;
        errno = 0 ;
        d = readdir(dir) ;
        if (!d) break ;
        if (d->d_name[0] == '.') continue ;
        for (; *p ; p++) if (!strcmp(*p, d->d_name)) break ;
        if (!*p)
        {
            size_t dlen = strlen(d->d_name) ;
            char fn[newlen + DOTPREFIXLEN + dlen + DOTSUFFIXLEN + 1] ;
            memcpy(fn, tmp,newlen) ;
            memcpy(fn + newlen,DOTPREFIX,DOTPREFIXLEN) ;
            memcpy(fn + newlen + DOTPREFIXLEN, d->d_name, dlen) ;
            memcpy(fn + newlen + DOTPREFIXLEN + dlen, DOTSUFFIX, DOTSUFFIXLEN + 1) ;
            if (mkrenametemp(fdd, d->d_name, fn + newlen) == -1)
            {
                log_warnusys("rename ",tmp, d->d_name, " to something based on ", fn) ;
                unlinkat(fdd, d->d_name, 0) ;
                /* if it still fails, too bad, it will restart in stage 4 and race */
            }
            else s6_svc_writectl(fn, S6_SUPERVISE_CTLDIR, "dx", 2) ;
        }
    }
    dir_close(dir) ;
    if (errno) log_dieusys(LOG_EXIT_SYS, "readdir: ",tmp) ;
    if (svc_scandir_send(tmp, "an") <= 0)
        log_warnu("reload scandir: ", tmp) ;
}

int main (int argc, char const *const *argv)
{
    unsigned int grace_time = 3000 ;
    tain deadline ;
    int fdr, fdw ;
    istream b ;
    char what = 'S' ;
    char buf[64] ;

    PROG = "66-shutdownd" ;
    {
        opt_scan_t st = OPT_SCAN_ZERO ;

        for (;;)
        {
            int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
            if (o == OPT_END) break ;
            switch (o)
            {
                case OPT_ID_HELP : return opt_emit_help(cmd.name, &cmd) ;
                case 'l' : live = st.arg ; break ;
                case 's' : conf = st.arg ; break ;
                case 'g' :
                    if (!u32_scan_strict(st.arg, &grace_time))
                        return opt_emit_usage(cmd.name, &cmd) ;
                    break ;
                case 'B' : inns = 1 ; break ;
                case 'c' : nologger = 1 ; break ;
                default : return opt_emit_error(cmd.name, &cmd, o, &st) ;
            }
        }
        argc -= st.ind ; argv += st.ind ;
    }
    if (conf[0] != '/') log_dieusys(LOG_EXIT_USER, "skeleton: ",conf," must be an absolute path") ;
    if (live && live[0] != '/') log_die(LOG_EXIT_USER,"live: ",live," must be an absolute path") ;
    else live = SS_LIVE ;
    if (grace_time > 300000) grace_time = 300000 ;

    /* if we're in stage 4, exec it immediately */
    {
        char const *stage4_argv[2] = { "./" STAGE4_FILE, 0 } ;

        if (!inns && !nologger) {

            int fd[2] ;
            int e ;
            fd[0] = fcntl(1, F_DUPFD_CLOEXEC, 0) ;

            if (fd[0] < 0)
                log_dieusys(LOG_EXIT_SYS, "dup stdout") ;

            fd[1] = fcntl(2, F_DUPFD_CLOEXEC, 0) ;

            if (fd[1] < 0)
                log_dieusys(LOG_EXIT_SYS, "dup stderr") ;

            restore_console() ;

            execv(stage4_argv[0], (char **)stage4_argv) ;

            e = errno ;
            if (remap_fds(1, fd[0], 2, fd[1]) < 0)
                log_warnusys("restore fds") ;
            errno = e ;

        } else {

            execv(stage4_argv[0], (char **)stage4_argv) ;
            if (errno != ENOENT)
                log_warnusys("exec ", stage4_argv[0]) ;
        }
    }

    fdr = io_open(SHUTDOWND_FIFO, O_RDONLY|O_NONBLOCK) ;
    if (fdr == -1 || cloexec_fd(fdr) == -1)
        log_dieusys(LOG_EXIT_SYS, "open ", SHUTDOWND_FIFO, " for reading") ;
    fdw = io_open(SHUTDOWND_FIFO, O_WRONLY|O_NONBLOCK) ;
    if (fdw == -1 || cloexec_fd(fdw) == -1)
        log_dieusys(LOG_EXIT_SYS, "open ", SHUTDOWND_FIFO, " for writing") ;
    if (!sig_ignore(SIGPIPE))
        log_dieusys(LOG_EXIT_SYS, "sig_ignore SIGPIPE") ;
    istream_init(&b, fdr, buf, 64) ;
    tain_now_set_stopwatch_g() ;
    tain_add_g(&deadline, &tain_infinite_relative) ;

    for (;;)
    {
        iopause_fd x = { .fd = fdr, .events = IOPAUSE_READ } ;
        int r = iopause_g(&x, 1, &deadline) ;
        if (r == -1) log_dieusys(LOG_EXIT_SYS, "iopause") ;
        if (!r)
        {
            run_rcshut() ;
            tain_now_g() ;
            if (what != 'S') break ;
            tain_add_g(&deadline, &tain_infinite_relative) ;
            continue ;
        }
        if (x.revents & IOPAUSE_READ)
            handle_fifo(&b, &what, &deadline, &grace_time) ;
    }

    close_fd(fdw) ;
    close_fd(fdr) ;

    if (!inns && !nologger)
        restore_console() ;

    /* The end is coming! */
    prepare_stage4(what) ;
    unsupervise_tree() ;

    if (!sig_ignore(SIGTERM)) log_warnusys("sig_ignore SIGTERM") ;

    if (!inns) {
        sync() ;
        log_info("Sending all processes the TERM signal...") ;
    }

    kill(-1, SIGTERM) ;
    kill(-1, SIGCONT) ;

    struct timespec gnow, gd ;
    clock_now_mono(&gnow) ;
    clock_from_ms(&gd, grace_time) ;
    clock_add(&gd, &gnow, &gd) ;
    clock_deepsleep(&gd) ;

    if (!inns) {
        sync() ;
        log_info("Sending all processes the KILL signal...") ;
    }

    kill(-1, SIGKILL) ;

    return 0 ;
}


