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
#include <sys/un.h>
#include <signal.h>
#include <sys/ioctl.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>
#include <oblibs/sastr.h>
#include <oblibs/stack.h>
#include <oblibs/directory.h>
#include <oblibs/io.h>

#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/exec.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/constants.h>
#include <66/utils.h>
#include <66/enum.h>

#include <s6/fdholder.h>

#include <execline/execline.h>

#define EXECUTE_START 0
#define EXECUTE_STOP 1
uint8_t action = EXECUTE_START ; // start by default

#define USAGE "66-execute [ -h ] [ -v verbosity ] start|stop servicename"

static inline void help_execute (void)
{
    DEFAULT_MSG = 0 ;

    static char const *help =
"\n"
"options :\n"
"   -h: print this help\n"
"   -v: increase/decrease verbosity\n"
;

    log_info(USAGE,"\n", help) ;
}

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

    setup_uidgid(&uid, &gid, res, res->islog ? res->logger.execute.run.runas : 0) ;

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

    size_t len =  strlen(res->sa.s + res->live.fdholderdir) + 2 ;
    _alloc_stk_(sock, len + 1) ;
    auto_strings(sock.s, res->sa.s + res->live.fdholderdir, "/s") ;
    sock.len = len ;

    int fdhold ;
    s6_fdholder_t a = S6_FDHOLDER_ZERO ;
    tain deadline = tain_infinite_relative ;
    size_t namelen = strlen(name) ;

    char *prefix = !reader ? SS_FDHOLDER_PIPENAME "r-" : SS_FDHOLDER_PIPENAME "w-" ;
    size_t prefixlen = SS_FDHOLDER_PIPENAME_LEN + 2 ;
    _alloc_stk_(identifier, prefixlen + namelen + 1) ;

    auto_strings(identifier.s, prefix, name) ;
    identifier.len = prefixlen + namelen ;

    tain_now_set_stopwatch_g() ;
    tain_add_g(&deadline, &deadline) ;

    if (!s6_fdholder_start_g(&a, sock.s, &deadline))
        log_dieusys(LOG_EXIT_SYS, "connect to socket: ", sock.s) ;

    fdhold = s6_fdholder_retrieve_g(&a, identifier.s, &deadline) ;
    if (fdhold < 0)
        log_dieusys(LOG_EXIT_SYS, "retrieve fd for id ", identifier.s) ;

    s6_fdholder_end(&a) ;

    if (fd_move(fd, fdhold) < 0)
        log_dieusys(LOG_EXIT_SYS, "move fd") ;

    if (!fdhold)
        if (uncoe(fd) < 0)
            log_dieusys(LOG_EXIT_SYS, "uncoe fd") ;
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

    fdest = open3(destination, flags, 0666) ;

    if ((fdest == -1) && (errno == ENXIO)) {

        fdr = open_read(destination) ;
        if (fdr == -1)
            log_dieusys(LOG_EXIT_SYS, "open for reading ", destination) ;

        fdest = open3(destination, flags, 0666) ;

        fd_close(fdr) ;
    }

    if (fdest == -1)
        log_dieusys(LOG_EXIT_SYS, "open ", destination) ;

    if (fd_move(fd, fdest) == -1)
        log_dieusys(LOG_EXIT_SYS, "redirect io to: ", destination) ;

    if (uncoe(fd) < 0)
        log_dieusys(LOG_EXIT_SYS, "remove close-on-exec from fd") ;
}

static void io_open_destination(int fd, const char *destination, int flags, mode_t mode, uint8_t cloexec)
{
    log_flow() ;

    // open with O_CLOEXEC and remove it after the copy
    int fdo = io_open(destination, flags | O_CLOEXEC) ;
    if (fdo < 0)
        log_dieusys(LOG_EXIT_SYS, "open: ", destination) ;
    if (fd_move(fd, fdo) < 0)
        log_dieusys(LOG_EXIT_SYS, "move stdin to: ", destination) ;

    if (cloexec) {

        if (coe(fd) < 0)
            log_dieusys(LOG_EXIT_SYS, "make fd close-on-exec") ;

    } else {
        // be paranoid: fd_move use dup2 which disable O_CLOEXEC flag to the new fd.
        if (uncoe(fd) < 0)
            log_dieusys(LOG_EXIT_SYS, "remove close-on-exec from fd") ;
    }
}

static void io_read_file(stack *stk, const char *file, size_t len)
{
    log_flow() ;

    ssize_t r ;
    unsigned int n = 0 ;
    errno = 0 ;
    int fd = io_open(file, O_RDONLY) ;
    if (fd == -1)
        log_dieusys(LOG_EXIT_SYS, "open: ", file) ;

    for(;;) {
        r = read(fd,stk->s + n,len - n);
        if (r == -1) {
            if (errno == EINTR)
                continue ;
            break ;
        }
        n += r ;
        // buffer is full
        if (n == len) {
            --n ;
            break ;
        }
        // end of file
        if (r == 0) break ;
    }

    close(fd) ;
    stk->len = n ;
    stk->s[n] = 0 ;
}

static void io_open_terminal(resolve_service_t *res, int fd, const char *destination, int flags)
{
    log_flow() ;

    int e = errno ;
    io_open_destination(fd, destination, flags, 0, 0) ;
    errno = 0 ;
    if (!isatty(fd) && errno != EBADF)
        log_dieusys(LOG_EXIT_ZERO, "associate fd to: ", destination) ;
    errno = e ;
}

static void io_open_terminal_ncontrol(resolve_service_t *res, int fd, const char *destination, int flags)
{
    log_flow() ;

    int e = errno ;
    errno = 0 ;
    struct sigaction sold, shug, snew ;

    snew.sa_handler = SIG_IGN ;
    sigemptyset(&snew.sa_mask) ;
    snew.sa_flags = 0 ;

    io_open_terminal(res, fd, destination, flags) ;

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

static void io_open_active_console(int fd, resolve_service_t *res)
{
    log_flow() ;

    char *path = "/sys/class/tty/tty0/active" ;
    int e = errno ;
    errno = 0 ;
    size_t len = 1024 ; //sysfs type here, 1024 should be large enough
    _alloc_stk_(stk, 4 + len + 1) ;

    io_read_file(&stk, path, len) ;
    stk.len = strlen(stk.s) ;

    if (!stack_insert(&stk, 0, "/dev/"))
        log_dieusys(LOG_EXIT_SYS, "invert prefix path") ;

    stk.s[stk.len - 1] = 0 ; // remove the last '\n'

    io_open_terminal(res, fd, stk.s, O_WRONLY | O_NOCTTY) ;
    errno = e ;
}

static void io_open_syslog(int fd)
{
    log_flow() ;

    int sock = -1, socktype= SOCK_DGRAM, e = errno ;
    errno = 0 ;
    struct sockaddr_un addr ;
    memset(&addr, 0, sizeof(addr)) ;

    int l = 0 ;
    while(sock < 0 && l < 2) {

        sock = socket(AF_UNIX, socktype | SOCK_CLOEXEC, 0) ;
        if (sock < 0)
            log_dieusys(LOG_EXIT_SYS, "create socket") ;

        addr.sun_family = AF_UNIX ;
        strcpy(addr.sun_path, "/dev/log");
        int r = connect(sock, (struct sockaddr *)&addr, sizeof (addr)) ;
        if (r < 0) {
            fd_close(sock) ;
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

    if (fd_move(fd, sock) < 0)
        log_dieusys(LOG_EXIT_SYS, "move fd to socket") ;

    if (uncoe(fd) < 0)
        log_dieusys(LOG_EXIT_SYS, "remove close-on-exec from fd") ;

    errno = e ;
}

static void io_setup_stdin(resolve_service_t *res)
{
    log_flow() ;

    switch(res->io.fdin.type) {

        case IO_TYPE_TTY:
            io_open_terminal_ncontrol(res, 0, res->sa.s + res->io.fdin.destination, O_RDWR | O_NOCTTY) ;
            break ;

        case IO_TYPE_NULL:
            io_open_destination(0, res->sa.s +  res->io.fdin.destination, O_RDONLY | O_NOCTTY, 0, 0) ;
            break ;

        case IO_TYPE_CLOSE:
            fd_close(0) ;
            break ;

        case IO_TYPE_S6LOG:
            if (res->type == TYPE_CLASSIC && res->islog)
                io_fdholder_retrieve(res, 0, res->sa.s + res->name, 0) ;
            break ;

        case IO_TYPE_PARENT:
            break ;

        case IO_TYPE_CONSOLE:
        case IO_TYPE_FILE:
        case IO_TYPE_SYSLOG:
        case IO_TYPE_INHERIT:
            break ;
        default:
            log_warn("unknown StdIn type -- applying default") ;
            break ;
    }
}

static void io_setup_stdout(resolve_service_t *res)
{
    log_flow() ;

    switch(res->io.fdout.type) {

        case IO_TYPE_CONSOLE:
            io_open_active_console(1, res) ;
            break ;

        case IO_TYPE_S6LOG:

            if (res->type == TYPE_CLASSIC && !res->islog) {

                io_fdholder_retrieve(res, 1, res->sa.s + res->logger.name, 1) ;

            } else if (res->type == TYPE_ONESHOT) {

                _alloc_stk_(stk, strlen(res->sa.s + res->logger.destination) + SS_TREE_CURRENT_LEN + 2) ;
                auto_strings(stk.s, res->sa.s + res->logger.destination, "/", SS_TREE_CURRENT) ;
                stk.s[strlen(res->sa.s + res->logger.destination) + 1 + SS_TREE_CURRENT_LEN] = 0 ;

                io_open_file(res, 1, stk.s) ;
            }
            break ;

        case IO_TYPE_TTY:
            io_open_terminal(res, 1, res->sa.s + res->io.fdout.destination, O_WRONLY | O_NOCTTY) ;
            break ;

        case IO_TYPE_NULL:
            io_open_destination(1, res->sa.s + res->io.fdout.destination, O_WRONLY, 0, 0) ;
            break ;

        case IO_TYPE_FILE:
            io_open_file(res, 1, res->sa.s + res->io.fdout.destination) ;
            break ;

        case IO_TYPE_SYSLOG:
            io_open_syslog(1) ;
            break ;

        case IO_TYPE_CLOSE:
            fd_close(1) ;
            break ;

        case IO_TYPE_PARENT:
            break ;

        case IO_TYPE_INHERIT:
            if (fd_copy(1, 0) < 0)
                log_dieusys(LOG_EXIT_SYS, "copy stdout to stderr") ;
            break ;

        default:
            log_warn("unknown StdOut type -- applying default") ;
            break ;
    }
}

static void io_setup_stderr(resolve_service_t *res)
{
    log_flow() ;

    switch(res->io.fderr.type) {

        case IO_TYPE_CONSOLE:
            io_open_active_console(2, res) ;
            break ;

        case IO_TYPE_TTY:
            io_open_terminal(res, 2, res->sa.s + res->io.fderr.destination, O_WRONLY | O_NOCTTY) ;
            break ;

        case IO_TYPE_NULL:
            io_open_destination(2, res->sa.s + res->io.fdout.destination, O_WRONLY, 0, 0) ;
            break ;

        case IO_TYPE_FILE:
            io_open_file(res, 2, res->sa.s + res->io.fderr.destination) ;
            break ;

        case IO_TYPE_SYSLOG:
            io_open_syslog(2) ;
            break ;

        case IO_TYPE_CLOSE:
            fd_close(2) ;
            break ;

        case IO_TYPE_INHERIT:
            if (fd_copy(2, 1) < 0)
                log_dieusys(LOG_EXIT_SYS, "copy stderr to stdout") ;
            break ;

        case IO_TYPE_PARENT:
            break ;

        default:
            log_warn("unknown StdErr type -- applying default") ;
            break ;
    }
}

static void execute_environment(stralloc *env, exlsn_t *info, resolve_service_t *res)
{
    log_flow() ;

    _alloc_sa_(sa) ;
    _alloc_stk_(path, strlen(res->sa.s + res->environ.envdir) + SS_SYM_VERSION_LEN + 1) ;

    if (!environ_import_arguments(env, (char const *const *)environ, env_len((char const *const *)environ)))
        log_dieusys(LOG_EXIT_SYS, "merge system environment") ;

    if (res->environ.env > 0) {

        if (!stack_add(&path, res->sa.s + res->environ.envdir, strlen(res->sa.s + res->environ.envdir)) ||
            !stack_add(&path, SS_SYM_VERSION, SS_SYM_VERSION_LEN) ||
            !stack_close(&path))
                log_die_nomem("stack") ;

        if (!environ_merge_dir(env, path.s))
            log_dieusys(LOG_EXIT_SYS, "merge environment directory: ", path.s) ;

        if (!environ_substitute(env, info))
            log_dieusys(LOG_EXIT_SYS, "substitue environment variables") ;

        if (!environ_clean_unexport(env))
            log_dieusys(LOG_EXIT_SYS, "remove exclamation mark from environment") ;
    }
}

static void execute_script(const char *runuser, resolve_service_t *res, exlsn_t *info)
{
    log_flow() ;

    int r = 0 ;
    _alloc_sa_(sa) ;
    uid_t owner = getuid() ;
    uint32_t want = (action == EXECUTE_START) ? res->execute.run.build : res->execute.finish.build ;
    short build = !strcmp(res->sa.s + want, "custom") ? BUILD_CUSTOM : BUILD_AUTO ;
    char *script = res->sa.s + (action == EXECUTE_START ? res->execute.run.run_user : res->execute.finish.run_user) ;
    size_t scriptlen = strlen(script) ;

    if (!build) {

        r = el_substitute(&sa, script, scriptlen, info->vars.s, info->values.s,
            genalloc_s(elsubst_t const, &info->data), genalloc_len(elsubst_t const, &info->data)) ;

        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "el_substitute") ;
        if (!r) {
            /** No other arguments
            * This should never happen. At least we have the shebang */
            log_dieusys(LOG_EXIT_SYS, "get arguments of run.user script -- please make a bug report") ;
        }

    } else {

        char *s = res->sa.s + ((action == EXECUTE_START) ? res->execute.run.run_user : res->execute.finish.run_user) ;
        if (!auto_stra(&sa, s))
            log_die_nomem("stralloc") ;
    }

    log_trace("write file: ", runuser) ;
    if (!openwritenclose_unsafe(runuser, sa.s, strlen(sa.s)))
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

    exlsn_free(info) ;
}

static void execute_io(resolve_service_t *res)
{
    log_flow() ;

    io_setup_stdin(res) ;
    io_setup_stdout(res) ;
    io_setup_stderr(res) ;
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

            if (!uid0_scan(as, &uid))
                log_dieusys(LOG_EXIT_SYS,  "get uid of: ", as) ;

            if (!gid0_scan(colon + 1, &gid))
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

int main(int argc, char const *const *argv, char const *const *envp)
{
    log_flow() ;

    char const *service = 0 ;
    _alloc_stk_(base, SS_MAX_PATH + 1) ;
    _alloc_sa_(env) ;
    char *run = 0 ;
    char *runuser = 0 ;

    exlsn_t info = EXLSN_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    PROG = "66-execute" ;
    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = subgetopt_r(argc,argv, "hv:", &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    help_execute() ;
                    return 0 ;

                case 'v' :

                    if (!uint0_scan(l.arg, &VERBOSITY))
                        log_usage(USAGE) ;

                    break ;

                default :

                    log_usage(USAGE) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 2)
        log_usage(USAGE) ;

    if (argv[0][0] != 's')
        log_die(LOG_EXIT_USER, "invalid command argument -- please use start|stop") ;

    if (argv[0][2] == 'o')
        action = EXECUTE_STOP ;

    service = argv[1] ;

    log_trace("processing service: ", service) ;

    if (chdir(".") < 0)
        log_dieusys(LOG_EXIT_ZERO, "chdir") ;

    if (!set_ownersysdir_stack(base.s, getuid()))
        log_dieusys(LOG_EXIT_SYS, "set owner directory") ;

    if (!resolve_read_g(wres, base.s, service))
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

    execute_environment(&env, &info, &res) ;

    execute_script(brunuser, &res, &info) ;

    execute_io(&res) ;

    execute_uidgid(&res) ;

    /** We can now send message to a eventd handler socket.
     * For now, just send a simple message */
    log_info(action == EXECUTE_START ? "Starting" : "Stopping", " service: ", service) ;

    xmexec_m(newargv, env.s, env.len) ;

    return 0 ;
}


