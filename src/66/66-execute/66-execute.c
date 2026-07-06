/*
 * 66-execute.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
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
#include <fcntl.h> // O_WRONLY,...
#include <sys/socket.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/resource.h> // limit
#include <stdint.h>
#include <stdlib.h> // free
#include <sys/prctl.h>

#include <oblibs/log.h>
#include <oblibs/exec.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/environ.h>
#include <oblibs/sbl.h>
#include <oblibs/subst.h>
#include <oblibs/directory.h>
#include <oblibs/io.h>
#include <oblibs/socket.h>
#include <oblibs/types.h>
#include <oblibs/fd.h>
#include <oblibs/files.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/constants.h>
#include <66/utils.h>
#include <66/caps.h>

#include <66/fdholder.h>

#define EXECUTE_START 0
#define EXECUTE_STOP 1
uint8_t action = EXECUTE_START ; // start by default

static opt_t const opts[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",    .arg = OPT_NONE,                          .help = "print this help" },
    { .id = 'v',         .shortname = 'v', .longname = "verbose", .arg = OPT_REQUIRED, .argname = "number", .help = "increase/decrease verbosity" },
} ;

static opt_cmd_t const cmd = {
    .name = "66-execute",
    .operands = "start|stop service",
    .opts = opts,
    .nopts = OPT_COUNT(opts),
} ;

static void setup_uidgid(uid_t *uid, gid_t *gid, resolve_service_t *res, uint32_t element)
{
    log_flow() ;

    if (!element) {
        (*uid) = res->owner ;
    } else {
        if (!youruid(uid, res->sa.s + element))
            log_dieusys(LOG_EXIT_SYS, "get uid of account: ", res->sa.s + element) ;
    }

    if (!yourgid(gid, (*uid)))
        log_dieusys(LOG_EXIT_SYS, "get gid") ;
}

static void execute_setup_destination(resolve_service_t *res, const char *dest)
{
    log_flow() ;

    /** stdout/stderr directory destination.It may no exists
     * on tmpfs filesystem i.e. boot time */
    uid_t uid ;
    gid_t gid ;

    // root by default as !res->logger.execute.run.runas if not set
    setup_uidgid(&uid, &gid, res, res->logger.execute.run.runas) ;

    log_trace("check logger destination directory: ", dest) ;
    if (!dir_create_parent(dest, 0755))
        log_dieusys(LOG_EXIT_SYS, "create logger directory: ", dest) ;

    if (!getuid())
        if (chown(dest, uid, gid) < 0)
            log_dieusys(LOG_EXIT_SYS, "chown: ", dest) ;
}

// !reader -> search for reader else writer
static void io_fdholder_retrieve(resolve_service_t *res, int fd, const char *name, uint8_t reader)
{
    log_flow() ;

    size_t len = strlen(res->sa.s + res->live.fdholderdir) + 2 ;
    char sock[len + 1] ;
    auto_strings(sock, res->sa.s + res->live.fdholderdir, "/s") ;

    fdholder_client_t c ;
    if (!fdholder_client_init(&c, sock))
        log_dieusys(LOG_EXIT_SYS, "connect to socket: ", sock) ;

    /* reader 0 -> read end, 1 -> write end ; the named pipe pair is created
     * atomically by the daemon on first request (no pre-seeding needed) */
    if (!fdholder_pipe(&c, name, reader, -1)) {
        uint8_t status = c.status ;
        fdholder_client_end(&c) ;
        log_die(LOG_EXIT_SYS, "get pipe flow '", name, "': ", fdholder_status_str(status)) ;
    }

    int got = c.received_fd ;
    fdholder_client_end(&c) ;

    if (move_fd(fd, got) < 0)
        log_dieusys(LOG_EXIT_SYS, "move fd") ;

    if (!fd)
        if (uncloexec_fd(fd) < 0)
            log_dieusys(LOG_EXIT_SYS, "uncloexec_fd fd") ;
}

static void io_open_file(resolve_service_t *res,  int fd, char const *destination)
{
    log_flow() ;

    int fdest, fdr ;
    int flags = O_WRONLY | O_CREAT | O_APPEND ; // add
    flags &= ~(O_TRUNC|O_EXCL) ; // drop O_TRUNC and O_EXCL

    // retrieve first the dirname of the file to create.
    char dir[strlen(destination) + 1] ;
    if (!ob_dirname(dir, destination))
        log_dieusys(LOG_EXIT_SYS, "get dirname of: ", destination) ;

    execute_setup_destination(res, dir) ;

    fdest = io_open_mode(destination, flags, 0666) ;

    if ((fdest == -1) && (errno == ENXIO)) {

        fdr = io_open(destination, O_RDONLY|O_NONBLOCK) ;
        if (fdr == -1)
            log_dieusys(LOG_EXIT_SYS, "open for reading ", destination) ;

        fdest = io_open_mode(destination, flags, 0666) ;

        close_fd(fdr) ;
    }

    if (fdest == -1)
        log_dieusys(LOG_EXIT_SYS, "open ", destination) ;

    if (move_fd(fd, fdest) == -1)
        log_dieusys(LOG_EXIT_SYS, "redirect io to: ", destination) ;

    if (uncloexec_fd(fd) < 0)
        log_dieusys(LOG_EXIT_SYS, "remove close-on-exec from fd") ;
}

static void io_open_destination(int fd, const char *destination, int flags, uint8_t cloexec)
{
    log_flow() ;

    // open with O_CLOEXEC and remove it after the copy
    int fdo = io_open(destination, flags | O_CLOEXEC) ;
    if (fdo < 0)
        log_dieusys(LOG_EXIT_SYS, "open: ", destination) ;
    if (move_fd(fd, fdo) < 0)
        log_dieusys(LOG_EXIT_SYS, "move stdin to: ", destination) ;

    if (cloexec) {

        if (cloexec_fd(fd) < 0)
            log_dieusys(LOG_EXIT_SYS, "make fd close-on-exec") ;

    } else {
        // be paranoid: move_fd use dup2 which disable O_CLOEXEC flag to the new fd.
        if (uncloexec_fd(fd) < 0)
            log_dieusys(LOG_EXIT_SYS, "remove close-on-exec from fd") ;
    }
}

static void io_open_terminal(int fd, const char *destination, int flags)
{
    log_flow() ;

    int e = errno ;
    io_open_destination(fd, destination, flags, 0) ;
    errno = 0 ;
    if (!isatty(fd) && errno != EBADF)
        log_dieusys(LOG_EXIT_ZERO, "associate fd to: ", destination) ;
    errno = e ;
}

static void io_open_terminal_ncontrol(int fd, const char *destination, int flags)
{
    log_flow() ;

    int e = errno ;
    errno = 0 ;
    struct sigaction sold, shug, snew ;

    snew.sa_handler = SIG_IGN ;
    sigemptyset(&snew.sa_mask) ;
    snew.sa_flags = 0 ;

    io_open_terminal(fd, destination, flags) ;

    if (sigaction(SIGTTOU, &snew, &sold) < 0)
        log_dieusys(LOG_EXIT_SYS, "ignore SIGTTOU") ;
    if (sigaction(SIGHUP, &snew, &shug) < 0)
        log_dieusys(LOG_EXIT_SYS, "ignore SIGHUP") ;

    // try to take control of the terminal
    errno = 0 ;
    if (ioctl(fd, TIOCSCTTY) < 0) {
        if (errno == EPERM)
            log_warnusys("tty is controlled by another process") ;
        else
            log_dieusys(LOG_EXIT_SYS, "tcsetpgrp") ;
    }

    if (sigaction(SIGTTOU, &sold, NULL) < 0 ||
        sigaction(SIGHUP, &shug, NULL) < 0)
            log_warnusys("restore sigaction") ;

    errno = e ;
}

static void io_open_active_console(int fd)
{
    log_flow() ;

    char *path = "/sys/class/tty/tty0/active" ;
    int e = errno ;
    errno = 0 ;
    size_t len = 1024 ; //sysfs type here, 1024 should be large enough
    char stk[5 + len + 1] ;

    auto_strings(stk, "/dev/") ;
    ssize_t r = file_read(path, stk + 5, len) ;
    if (r == -1)
        log_dieusys(LOG_EXIT_SYS, "read: ", path) ;
    if (r && stk[5 + r - 1] == '\n')
        r-- ; // drop the trailing newline
    stk[5 + r] = 0 ;

    io_open_terminal(fd, stk, O_WRONLY | O_NOCTTY) ;
    errno = e ;
}

static void io_open_syslog(int fd)
{
    log_flow() ;

    int sock = -1, socktype = SOCK_DGRAM, e = errno ;
    errno = 0 ;

    int l = 0 ;
    while(sock < 0 && l < 2) {

        sock = socket_private(AF_UNIX, socktype, 0, O_CLOEXEC) ;
        if (sock < 0)
            log_dieusys(LOG_EXIT_SYS, "create socket") ;

        if (socketunix_connect(sock, "/dev/log") < 0) {
            close_fd(sock) ;
            if (errno == EPROTOTYPE) {
                socktype = SOCK_STREAM ;
                sock = -1 ;
                l++ ;
                continue ;
            }
            log_dieusys(LOG_EXIT_SYS, "connect to /dev/log") ;
        }
    }

    if (shutdown(sock, SHUT_RD) < 0)
        log_dieusys(LOG_EXIT_SYS, "close reading part of socket") ;

    if (move_fd(fd, sock) < 0)
        log_dieusys(LOG_EXIT_SYS, "move fd to socket") ;

    if (uncloexec_fd(fd) < 0)
        log_dieusys(LOG_EXIT_SYS, "remove close-on-exec from fd") ;

    errno = e ;
}

static void io_setup_stdin(resolve_service_t *res, resolve_service_addon_io_t *io)
{
    log_flow() ;

    switch(io->fdin.type) {

        case E_PARSER_IO_TYPE_TTY:
            io_open_terminal_ncontrol(0, io->sa.s + io->fdin.destination, O_RDWR | O_NOCTTY) ;
            break ;

        case E_PARSER_IO_TYPE_NULL:
            io_open_destination(0, res->sa.s +  io->fdin.destination, O_RDONLY | O_NOCTTY, 0) ;
            break ;

        case E_PARSER_IO_TYPE_CLOSE:
            close_fd(0) ;
            break ;

        case E_PARSER_IO_TYPE_66LOG:
            if (res->type == E_PARSER_TYPE_CLASSIC && res->islog)
                io_fdholder_retrieve(res, 0, res->sa.s + res->name, 0) ;
            break ;

        case E_PARSER_IO_TYPE_PARENT:
            break ;

        case E_PARSER_IO_TYPE_CONSOLE:
        case E_PARSER_IO_TYPE_FILE:
        case E_PARSER_IO_TYPE_SYSLOG:
        case E_PARSER_IO_TYPE_INHERIT:
            break ;
        default:
            log_warn("unknown StdIn type -- applying default") ;
            break ;
    }
}

static void io_setup_stdout(resolve_service_t *res, resolve_service_addon_io_t *io)
{
    log_flow() ;

    switch(io->fdout.type) {

        case E_PARSER_IO_TYPE_CONSOLE:
            io_open_active_console(1) ;
            break ;

        case E_PARSER_IO_TYPE_66LOG:

            if (res->type == E_PARSER_TYPE_CLASSIC && !res->islog) {

                io_fdholder_retrieve(res, 1, res->sa.s + res->logger.name, 1) ;

            } else if (res->type == E_PARSER_TYPE_ONESHOT) {

                char stk[strlen(io->sa.s + io->fdout.destination) + SS_CURRENT_LEN + 2] ;
                auto_strings(stk, io->sa.s + io->fdout.destination, "/", SS_CURRENT) ;
                stk[strlen(io->sa.s + io->fdout.destination) + 1 + SS_CURRENT_LEN] = 0 ;

                io_open_file(res, 1, stk) ;
            }
            break ;

        case E_PARSER_IO_TYPE_TTY:
            io_open_terminal(1, io->sa.s + io->fdout.destination, O_WRONLY | O_NOCTTY) ;
            break ;

        case E_PARSER_IO_TYPE_NULL:
            io_open_destination(1, io->sa.s + io->fdout.destination, O_WRONLY, 0) ;
            break ;

        case E_PARSER_IO_TYPE_FILE:
            io_open_file(res, 1, io->sa.s + io->fdout.destination) ;
            break ;

        case E_PARSER_IO_TYPE_SYSLOG:
            io_open_syslog(1) ;
            break ;

        case E_PARSER_IO_TYPE_CLOSE:
            close_fd(1) ;
            break ;

        case E_PARSER_IO_TYPE_PARENT:
            break ;

        case E_PARSER_IO_TYPE_INHERIT:
            if (copy_fd(1, 0) < 0)
                log_dieusys(LOG_EXIT_SYS, "copy stdout to stderr") ;
            break ;

        default:
            log_warn("unknown StdOut type -- applying default") ;
            break ;
    }
}

static void io_setup_stderr(resolve_service_t *res, resolve_service_addon_io_t *io)
{
    log_flow() ;

    switch(io->fderr.type) {

        case E_PARSER_IO_TYPE_CONSOLE:
            io_open_active_console(2) ;
            break ;

        case E_PARSER_IO_TYPE_TTY:
            io_open_terminal(2, io->sa.s + io->fderr.destination, O_WRONLY | O_NOCTTY) ;
            break ;

        case E_PARSER_IO_TYPE_NULL:
            io_open_destination(2, io->sa.s + io->fdout.destination, O_WRONLY, 0) ;
            break ;

        case E_PARSER_IO_TYPE_FILE:
            io_open_file(res, 2, io->sa.s + io->fderr.destination) ;
            break ;

        case E_PARSER_IO_TYPE_SYSLOG:
            io_open_syslog(2) ;
            break ;

        case E_PARSER_IO_TYPE_CLOSE:
            close_fd(2) ;
            break ;

        case E_PARSER_IO_TYPE_INHERIT:
            if (copy_fd(2, 1) < 0)
                log_dieusys(LOG_EXIT_SYS, "copy stderr to stdout") ;
            break ;

        case E_PARSER_IO_TYPE_PARENT:
            break ;

        default:
            log_warn("unknown StdErr type -- applying default") ;
            break ;
    }
}

static void execute_environment(char const **nenvp, char const *const *env, strbuf *eram, subst_t *info, resolve_service_t *res)
{
    log_flow() ;

    resolve_service_addon_environ_t e = RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
    resolve_wrapper_t_ref we = resolve_set_struct(DATA_SERVICE_ENVIRON, &e) ;
    uint8_t loaded = res->has_environ && resolve_read(we, res->sa.s + res->path.home, res->sa.s + res->name) > 0 ;

    if (loaded && e.env > 0) {

        _alloc_strbuf_(path, strlen(e.sa.s + e.envdir) + SS_SYM_VERSION_LEN + 1) ;

        if (!auto_strbuf(&path, e.sa.s + e.envdir, SS_SYM_VERSION))
            log_die_nomem("stack") ;

        if (!environ_merge_dir(eram, path.s))
            log_dieusys(LOG_EXIT_SYS, "merge environment directory: ", path.s) ;

        if (e.nimportfile) {

            _alloc_sbl_(stk, strlen(e.sa.s + e.importfile)) ;
            size_t pos = 0 ;

            if (!sbl_clean_string(&stk, e.sa.s + e.importfile))
                log_dieusys(LOG_EXIT_SYS, "clean string") ;

            FOREACH_SBL(&stk, pos) {

                if (!environ_merge_file(eram, stk.s + pos))
                    log_dieusys(LOG_EXIT_SYS, "merge environment file: ", stk.s + pos) ;
            }
        }

        if (!environ_substitute(eram, info))
            log_dieusys(LOG_EXIT_SYS, "substitue environment variables") ;

        if (!environ_clean_unexport(eram))
            log_dieusys(LOG_EXIT_SYS, "remove exclamation mark from environment") ;
    }

    resolve_free(we) ;

    if (!environ_create_environ(nenvp, env, eram))
        log_dieusys(LOG_EXIT_SYS, "create environment") ;
}

static void execute_script(const char *runuser, resolve_service_t *res, subst_t *info)
{
    log_flow() ;

    int r = 0 ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    uid_t owner = getuid() ;
    uint32_t want = (action == EXECUTE_START) ? res->execute.run.build : res->execute.finish.build ;
    short build = !strcmp(res->sa.s + want, "custom") ? E_PARSER_BUILD_CUSTOM : E_PARSER_BUILD_AUTO ;
    char *script = res->sa.s + (action == EXECUTE_START ? res->execute.run.run_user : res->execute.finish.run_user) ;
    size_t scriptlen = strlen(script) ;

    if (!build) {

        r = subst(&sa, script, scriptlen, info) ;

        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "el_substitute") ;
        if (!r) {
            /** No other arguments
            * This should never happen except in case of empty value. At least we have the shebang */
            log_dieusys(LOG_EXIT_SYS, "get arguments of run.user script") ;
        }

    } else {

        char *s = res->sa.s + ((action == EXECUTE_START) ? res->execute.run.run_user : res->execute.finish.run_user) ;
        if (!auto_strbuf(&sa, s))
            log_die_nomem("strbuf") ;
    }

    log_trace("write file: ", runuser) ;
    if (!file_write(runuser, sa.s, strlen(sa.s)))
        log_dieusys(LOG_EXIT_SYS,"create and write file: ", runuser) ;

    if (chmod(runuser, 0755) < 0)
        log_dieusys(LOG_EXIT_SYS,"chmod: ", runuser) ;

    if (!owner) {

        uid_t uid ;
        gid_t gid ;

        setup_uidgid(&uid, &gid, res, ((action == EXECUTE_START) ? res->execute.run.runas : res->execute.finish.runas)) ;

        if (uid)
            if (chown(runuser, uid, gid) < 0)
                log_dieusys(LOG_EXIT_SYS, "chown: ", runuser) ;
    }
}

static void execute_io(resolve_service_t *res)
{
    log_flow() ;

    resolve_service_addon_io_t io = RESOLVE_SERVICE_ADDON_IO_ZERO ;
    resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE_IO, &io) ;
    if (!res->has_io || resolve_read(w, res->sa.s + res->path.home, res->sa.s + res->name) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read io addon of: ", res->sa.s + res->name) ;

    io_setup_stdin(res, &io) ;
    io_setup_stdout(res, &io) ;
    io_setup_stderr(res, &io) ;

    resolve_free(w) ;
}

static void execute_uidgid(resolve_service_t *res)
{
    log_flow() ;

    /*only root can control that*/
    if (geteuid())
        return ;

    uid_t uid = - 1 ;
    gid_t gid = - 1 ;
    uint32_t want = (action == EXECUTE_START) ? (res->islog) ? res->logger.execute.run.runas : res->execute.run.runas : res->execute.finish.runas ;
    char *as = res->sa.s + want ;

    if (want) {

        char *colon = strchr(as,':') ;

        if (colon) {

            if (!uid_parse_strict(as, &uid))
                log_dieusys(LOG_EXIT_SYS,  "get uid of: ", as) ;

            if (!gid_parse_strict(colon + 1, &gid))
                log_dieusys(LOG_EXIT_SYS, "get gid of: ", as) ;

        } else {

            if (!youruid(&uid, as) ||
                !yourgid(&gid, uid))
                log_dieusys(LOG_EXIT_SYS, "get uid and gid of: ", as) ;
        }

        if (setgid(gid) < 0)
            log_dieusys(LOG_EXIT_SYS, "setgid") ;

        if (setuid(uid) < 0)
            log_dieusys(LOG_EXIT_SYS, "setgid") ;

        if (uid && (geteuid() == 0 || getegid() == 0))
            log_dieusys(LOG_EXIT_SYS, "drop root privileges") ;
    }
}

static void limit_setup(resolve_service_t *res, int resource, uint64_t rval)
{
    log_flow() ;

    if (!rval)
        return ;

    struct rlimit r ;
    uint64_t n = rval ;

    if (getrlimit(resource, &r) < 0)
        log_dieusys(LOG_EXIT_SYS, "get limit") ;

    // n == (uint64_t)(RLIM_INFINITY) is implied
    if (!r.rlim_max && !res->owner)
        r.rlim_max = n ;

    if (n > r.rlim_max)
        if (n == (uint64_t)(RLIM_INFINITY) && !res->owner)
            r.rlim_max = n ;
        else n = r.rlim_max ;

    r.rlim_cur = n ;

    if (setrlimit(resource, &r) < 0)
        log_dieusys(LOG_EXIT_SYS, "set limit") ;
}

static void execute_limit(resolve_service_t *res)
{
    log_flow() ;

    resolve_service_addon_limit_t la = RESOLVE_SERVICE_ADDON_LIMIT_ZERO ;
    resolve_wrapper_t_ref w = resolve_set_struct(DATA_SERVICE_LIMIT, &la) ;

    if (res->has_limit && resolve_read(w, res->sa.s + res->path.home, res->sa.s + res->name) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read limit addon of: ", res->sa.s + res->name) ;

    free(w) ;

#ifdef RLIMIT_AS
    limit_setup(res, RLIMIT_AS, la.limitas ? la.limitas : 0) ;
#endif
#ifdef RLIMIT_CORE
    limit_setup(res, RLIMIT_CORE, la.limitcore ? la.limitcore : 0) ;
#endif
#ifdef RLIMIT_CPU
    limit_setup(res, RLIMIT_CPU, la.limitcpu ? la.limitcpu : 0) ;
#endif
#ifdef RLIMIT_DATA
    limit_setup(res, RLIMIT_DATA, la.limitdata ? la.limitdata : 0) ;
#endif
#ifdef RLIMIT_FSIZE
    limit_setup(res, RLIMIT_FSIZE, la.limitfsize ? la.limitfsize : 0) ;
#endif
#ifdef RLIMIT_LOCKS
    limit_setup(res, RLIMIT_LOCKS, la.limitlocks ? la.limitlocks : 0) ;
#endif
#ifdef RLIMIT_MEMLOCK
    limit_setup(res, RLIMIT_MEMLOCK, la.limitmemlock ? la.limitmemlock : 0) ;
#endif
#ifdef RLIMIT_MSGQUEUE
    limit_setup(res, RLIMIT_MSGQUEUE, la.limitmsgqueue ? la.limitmsgqueue : 0) ;
#endif
#ifdef RLIMIT_NICE
    limit_setup(res, RLIMIT_NICE, la.limitnice ? la.limitnice : 0) ;
#endif
#ifdef RLIMIT_NOFILE
    limit_setup(res, RLIMIT_NOFILE, la.limitnofile ? la.limitnofile : 0) ;
#endif
#ifdef RLIMIT_NPROC
    limit_setup(res, RLIMIT_NPROC, la.limitnproc ? la.limitnproc : 0) ;
#endif
#ifdef RLIMIT_RTPRIO
    limit_setup(res, RLIMIT_RTPRIO, la.limitrtprio ? la.limitrtprio : 0) ;
#endif
#ifdef RLIMIT_RTTIME
    limit_setup(res, RLIMIT_RTTIME, la.limitrttime ? la.limitrttime : 0) ;
#endif
#ifdef RLIMIT_SIGPENDING
    limit_setup(res, RLIMIT_SIGPENDING, la.limitsigpending ? la.limitsigpending : 0) ;
#endif
#ifdef RLIMIT_STACK
    limit_setup(res, RLIMIT_STACK, la.limitstack ? la.limitstack : 0) ;
#endif

}

static void execute_privileges(resolve_service_t *res)
{
    log_flow() ;

    if (res->execute.blockprivileges)
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0)
            log_dieusys(LOG_EXIT_SYS, "set NO_NEW_PRIVILEGES") ;
}

static void execute_umask(resolve_service_t *res)
{
    log_flow() ;

    if (res->execute.want_umask)
        umask((mode_t)res->execute.umask) ;
}

static void execute_nice(resolve_service_t *res)
{
    log_flow();

    if (res->execute.want_nice) {

        int64_t p = 20 - (int64_t)(res->execute.nice) ;

        errno = 0 ; // see: https://pubs.opengroup.org/onlinepubs/9699919799/
        if (setpriority(PRIO_PROCESS, 0, (int)p) < 0) {
            if (errno == EPERM)
                log_warnusys("setting nice value requires CAP_SYS_NICE or root") ;

            log_dieusys(LOG_EXIT_SYS, "set nice value");
        }
    }
}

static void execute_chdir(resolve_service_t *res)
{
    log_flow() ;

    if (res->execute.chdir) {

        if (chdir(res->sa.s + res->execute.chdir) < 0)
            log_dieusys(LOG_EXIT_ZERO, "chdir") ;

    }
}

int main(int argc, char const *const *argv, char const *const *envp)
{
    log_flow() ;

    char const *service = 0 ;
    char base[SS_MAX_PATH + 1] ;
    _cleanup_strbuf_ strbuf eram = STRBUF_ZERO ; // envrionment in memory
    char const *nenvp[MAXENV + 1] ;
    char *run = 0 ;
    char *runuser = 0 ;

    subst_t info = SUBST_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    PROG = "66-execute" ;
    {
        opt_scan_t st = OPT_SCAN_ZERO ;

        for (;;) {

            int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
            if (o == OPT_END) break ;

            switch (o) {
                case OPT_ID_HELP : return opt_emit_help(cmd.name, &cmd) ;
                case 'v' :
                    if (!u32_scan_strict(st.arg, &VERBOSITY))
                        return opt_emit_usage(cmd.name, &cmd) ;
                    break ;
                default : return opt_emit_error(cmd.name, &cmd, o, &st) ;
            }
        }
        argc -= st.ind ; argv += st.ind ;
    }

    if (argc < 2)
        return opt_emit_usage(cmd.name, &cmd) ;

    if (argv[0][0] != 's')
        log_die(LOG_EXIT_USER, "invalid command argument -- please use start|stop") ;

    if (argv[0][2] == 'o')
        action = EXECUTE_STOP ;

    service = argv[1] ;

    log_trace("processing service: ", service) ;

    if (chdir(".") < 0)
        log_dieusys(LOG_EXIT_ZERO, "chdir") ;

    if (!set_ownersysdir_stack(base, getuid()))
        log_dieusys(LOG_EXIT_SYS, "set owner directory") ;

    if (!resolve_read(wres, base, service))
        log_dieusys(LOG_EXIT_SYS,"read resolve file of: ", service) ;

    run = (action == EXECUTE_START) ? "/run" : "/finish" ;
    char brun[strlen(res.sa.s + res.live.servicedir) + strlen(run) + 1] ;
    auto_strings(brun, res.sa.s + res.live.servicedir, run) ;
    runuser = (action == EXECUTE_START) ? "/run.user" : "/finish.user" ;
    char brunuser[strlen(res.sa.s + res.live.servicedir) + strlen(runuser) + 1] ;
    auto_strings(brunuser, res.sa.s + res.live.servicedir, runuser) ;

    char const *newargv[2] = { brunuser, 0 } ;

    if (access(brun, F_OK) < 0) {
        if (action == EXECUTE_STOP) /* really nothing to do here */
            return 0 ;
        else /** should never happen*/
            log_dieusys(LOG_EXIT_SYS, "find script: ", brun, " -- please make a bug report") ;
    }

    execute_io(&res) ;

    /** We can now send message to a eventd handler socket.
     * For now, just send a simple message */
    log_info(action == EXECUTE_START ? "Starting" : "Stopping", " service: ", service) ;

    execute_environment(nenvp, envp, &eram, &info, &res) ;

    execute_script(brunuser, &res, &info) ;

    execute_limit(&res) ;

    execute_nice(&res) ;

    execute_privileges(&res) ;

    execute_caps(&res) ;

    execute_uidgid(&res) ;

    execute_umask(&res) ;

    execute_chdir(&res) ;

    exec_path_merge_die(newargv[0], newargv, nenvp, info.modifs.s, info.modifs.len) ;

    return 0 ;
}

