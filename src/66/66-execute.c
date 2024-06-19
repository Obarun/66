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

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>
#include <oblibs/sastr.h>
#include <oblibs/stack.h>
#include <oblibs/directory.h>

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

static inline ssize_t is_log(const char *name)
{
    return get_rstrlen_until(name,SS_LOG_SUFFIX) ;
}

static void execute_get_uidgid(uid_t *uid, gid_t *gid, resolve_service_t *res, uint32_t element)
{
    if (!element) {
        (*uid) = res->owner ;
    } else {
        if (!youruid(uid, res->sa.s + element))
            log_dieusys(LOG_EXIT_SYS, "get uid of account: ", res->sa.s + element) ;
    }

    if (!yourgid(gid, (*uid)))
        log_dieusys(LOG_EXIT_SYS, "get gid") ;
}

static void execute_create_log_destination(resolve_service_t *res)
{
    if (!res->logger.want)
        return ;

    /** Creation of the logger destination. logger directory
     * may no exists on tmpfs filesystem i.e. boot time*/
    uid_t uid ;
    gid_t gid ;

    char *dst = res->sa.s + res->logger.destination ;

    execute_get_uidgid(&uid, &gid, res, res->logger.execute.run.runas) ;

    log_trace("check logger destination directory: ", dst) ;
    if (!dir_create_parent(dst, 0755))
        log_dieusys(LOG_EXIT_SYS, "create logger directory: ", dst) ;

    if (!getuid())
        if (chown(dst, uid, gid) < 0)
            log_dieusys(LOG_EXIT_SYS, "chown: ", dst) ;
}

static void execute_compute_environ(stralloc *env, exlsn_t *info, resolve_service_t *res)
{
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

static void execute_log_fdholder(resolve_service_t *res)
{
    int fd ;
    s6_fdholder_t a = S6_FDHOLDER_ZERO ;
    tain deadline = tain_infinite_relative ;

    char *socket = res->sa.s + res->live.fdholderdir ;
    size_t socketlen = strlen(socket) ;
    _alloc_stk_(sock, socketlen + 3) ;

    ssize_t islog = is_log(res->sa.s + res->name) ;
    char *baselog = islog > 0 ? res->sa.s + res->name : res->sa.s + res->logger.name ;
    size_t loglen = strlen(baselog) ;

    char *prefix = islog > 0 ? SS_FDHOLDER_PIPENAME "r-" : SS_FDHOLDER_PIPENAME "w-" ;
    size_t prefixlen = SS_FDHOLDER_PIPENAME_LEN + 2 ;
    _alloc_stk_(logname, prefixlen + loglen + 1) ;

    auto_strings(sock.s, socket, "/s") ;
    sock.s[socketlen + 2] = 0 ;
    sock.len = socketlen + 2 ;

    auto_strings(logname.s, prefix, baselog) ;
    logname.s[prefixlen + loglen] = 0 ;
    logname.len = prefixlen + loglen ;

    if (islog < 0 )
        if (fd_move(1, 0) < 0)
            log_dieusys(LOG_EXIT_SYS, "move stdin to stdout") ;

    tain_now_set_stopwatch_g() ;
    tain_add_g(&deadline, &deadline) ;

    if (!s6_fdholder_start_g(&a, sock.s, &deadline))
        log_dieusys(LOG_EXIT_SYS, "connect to socket: ", sock.s) ;

    fd = s6_fdholder_retrieve_g(&a, logname.s, &deadline) ;
    if (fd < 0)
        log_dieusys(LOG_EXIT_SYS, "retrieve fd for id ", logname.s) ;

    s6_fdholder_end(&a) ;

    if (!fd) {
        if (uncoe(0) < 0)
            log_dieusys(LOG_EXIT_SYS, "uncoe stdin") ;
    } else {
        if (fd_move(0, fd) < 0)
            log_dieusys(LOG_EXIT_SYS, "move fd") ;
    }

    if (islog < 0)
        if (fd_move2(0, 1, 1, 0) < 0)
            log_dieusys(LOG_EXIT_SYS, "swap fds") ;
}

static void execute_classic_start(resolve_service_t *res)
{
    if (res->logger.want || is_log(res->sa.s + res->name) >= 0)
        execute_log_fdholder(res) ;
}

static void execute_classic_stop(resolve_service_t *res)
{
    /** Be paranoid: No finish script exists for a logger.
     * This function should be never called for logger service.*/
    if (is_log(res->sa.s + res->name) > 0)
        return ;

    execute_log_fdholder(res) ;
}

static void execute_classic(resolve_service_t *res, uint8_t action)
{
    switch (action) {
        case EXECUTE_START:
            execute_classic_start(res) ;
            break ;

        case EXECUTE_STOP:
            execute_classic_stop(res) ;
            break ;

        default:
            break;
    }
}

void execute_log_file(resolve_service_t *res)
{
    int fd = 1, fdest, fdr ;
    int flags = O_WRONLY | O_CREAT | O_APPEND ; // add
    flags &= ~(O_TRUNC|O_EXCL) ; // drop O_TRUNC and O_EXCL

    _alloc_stk_(dest, strlen(res->sa.s + res->logger.destination) + SS_TREE_CURRENT_LEN + 2) ;
    auto_strings(dest.s, res->sa.s + res->logger.destination, "/", SS_TREE_CURRENT) ;
    dest.s[strlen(dest.s)] = 0 ;
    dest.len = strlen(dest.s) ;

    fdest = open3(dest.s, flags, 0666) ;

    if ((fdest == -1) && (errno == ENXIO)) {

        fdr = open_read(dest.s) ;
        if (fdr == -1)
            log_dieusys(LOG_EXIT_SYS, "open for reading ", dest.s) ;

        fdest = open3(dest.s, flags, 0666) ;

        fd_close(fdr) ;
    }

    if (fdest == -1)
        log_dieusys(LOG_EXIT_SYS, "open ", dest.s) ;

    if (fd_move(fd, fdest) == -1)
        log_dieusys(LOG_EXIT_SYS, "redirect stdout to: ", dest.s) ;

}

static void execute_oneshot_start(resolve_service_t *res)
{
    if (res->logger.want)
        execute_log_file(res) ;
}

static void execute_oneshot_stop(resolve_service_t *res)
{
    if (res->logger.want)
        execute_log_file(res) ;
}

static void execute_oneshot(resolve_service_t *res, uint8_t action)
{
    switch (action) {
        case EXECUTE_START:
            execute_oneshot_start(res) ;
            break ;
        case EXECUTE_STOP:
            execute_oneshot_stop(res) ;
            break ;
        default:
            break;
    }
}

static void get_runas_uidgid(uid_t *uid, gid_t *gid, const char *str)
{
    char *colon = strchr(str,':') ;

    if (colon) {

        if (!uid0_scan(str, uid))
            log_dieusys(LOG_EXIT_SYS,  "get uid of: ", str) ;

        if (!gid0_scan(colon + 1, gid))
            log_dieusys(LOG_EXIT_SYS, "get gid of: ", str) ;

    } else {

        if (!youruid(uid, str) ||
            !yourgid(gid, (*uid)))
            log_dieusys(LOG_EXIT_SYS, "get uid and gid of: ", str) ;
    }
}

static void setup_uidgid(resolve_service_t *res)
{
    /*only root can control that*/
    if (geteuid())
        return ;

    uid_t uid = - 1 ;
    gid_t gid = - 1 ;
    ssize_t islog = is_log(res->sa.s + res->name) ;
    uint32_t want = (action == EXECUTE_START) ? ((islog > 0) ? res->logger.execute.run.runas : res->execute.run.runas) : res->execute.finish.runas ;
    char *as = res->sa.s + want ;

    if (want) {

        get_runas_uidgid(&uid, &gid, as) ;

        if (setgid(gid) < 0)
            log_dieusys(LOG_EXIT_SYS, "setgid") ;

        if (setuid(uid) < 0)
            log_dieusys(LOG_EXIT_SYS, "setgid") ;

        if (uid && (geteuid() == 0 || getegid() == 0))
            log_dieusys(LOG_EXIT_SYS, "drop root privileges") ;
    }
}

static void execute_write_script(const char *runuser, resolve_service_t *res, exlsn_t *info)
{
    int r = 0 ;
    _alloc_sa_(sa) ;
    uid_t owner = getuid() ;
    uint32_t want = (action == EXECUTE_START) ? res->execute.run.build : res->execute.finish.build ;
    char *script = res->sa.s + (action == EXECUTE_START ? res->execute.run.run_user : res->execute.finish.run_user) ;
    size_t scriptlen = strlen(script) ;

    if (!want) {

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

    log_trace("create file: ", runuser) ;
    if (!openwritenclose_unsafe(runuser, sa.s, strlen(sa.s)))
        log_dieusys(LOG_EXIT_SYS,"write file: ./run.user") ;

    if (chmod(runuser, 0755) < 0)
        log_dieusys(LOG_EXIT_SYS,"chmod: ", runuser) ;

    if (!owner) {

        uid_t uid ;
        gid_t gid ;

        execute_get_uidgid(&uid, &gid, res, ((action == EXECUTE_START) ? res->execute.run.runas : res->execute.finish.runas)) ;

        if (uid)
            if (chown(runuser, uid, gid) < 0)
                log_dieusys(LOG_EXIT_SYS, "chown: ", runuser) ;
    }

    exlsn_free(info) ;
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

    execute_create_log_destination(&res) ;

    execute_compute_environ(&env, &info, &res) ;

    execute_write_script(brunuser, &res, &info) ;

    switch (res.type) {

        case TYPE_CLASSIC:
            execute_classic(&res, action) ;
            break;
        case TYPE_ONESHOT:
            execute_oneshot(&res, action) ;
            break;
        default:
            break;
    }

    if (fd_copy(2, 1) < 0)
        log_dieusys(LOG_EXIT_SYS, "copy stdout to stderr") ;

    setup_uidgid(&res) ;

    /** We can now send message to a eventd handler socket.
     * For now, just send a simple message to the logger */
    log_info(action == EXECUTE_START ? "starting" : "stopping", " service: ", service) ;

    xmexec_m(newargv, env.s, env.len) ;

    return 0 ;
}


