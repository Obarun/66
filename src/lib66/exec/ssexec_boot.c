/*
 * ssexec_boot.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/reboot.h>

#ifdef __linux__
    #include <linux/kd.h>
    #include <skalibs/sig.h>
#endif

#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>
#include <oblibs/sastr.h>
#include <oblibs/stack.h>
#include <oblibs/io.h>

#include <skalibs/sgetopt.h>
#include <skalibs/djbunix.h>
#include <skalibs/stralloc.h>
#include <skalibs/types.h>
#include <skalibs/exec.h>
#include <skalibs/cspawn.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/ssexec.h>

static mode_t mask = SS_BOOT_UMASK ;
static unsigned int rescan = SS_BOOT_RESCAN ;
static unsigned int container = SS_BOOT_CONTAINER ;
static unsigned int catch_log = SS_BOOT_CATCH_LOG ;
static char const *skel = SS_SKEL_DIR ;
static char const *live = SS_LIVE ;
static char const *path = SS_BOOT_PATH ;
static char const *tree = SS_BOOT_TREE ;
static char const *rcinit = SS_SKEL_DIR SS_BOOT_RCINIT ;
static char const *rcinit_container = SS_SKEL_DIR SS_BOOT_RCINIT_CONTAINER ;
static char const *banner = "\n[Starts stage1 process...]" ;
static char const *slashdev = 0 ;
static char const *envdir = 0 ;
static char const *fifo = 0 ;
static char const *log_user = SS_LOGGER_RUNNER ;
static char const *cver = 0 ;
static char tpath[SS_MAX_PATH_LEN + 1] ;
static char trcinit[SS_MAX_PATH_LEN + 1] ;
static char trcinit_container[SS_MAX_PATH_LEN + 1] ;
static char tlive[SS_MAX_PATH_LEN + 1] ;
static char ttree[SS_MAX_PATH_LEN + 1] ;
static char confile[SS_MAX_PATH_LEN + 1 + SS_BOOT_CONF_LEN + 1] ;
static int notifpipe[2] ;

static void sulogin(char const *msg,char const *arg)
{
    static char const *const newarg[2] = { SS_EXTBINPREFIX "sulogin" , 0 } ;
    pid_t pid ;
    int wstat ;
    if (msg) log_warnusys(msg,arg) ;
    pid = child_spawn0(newarg[0],newarg,(char const *const *)environ) ;
    if (waitpid_nointr(pid,&wstat, 0) < 0)
        log_dieusys(LOG_EXIT_SYS,"wait for sulogin -- you are on your own") ;
    if (close(0) < 0)
        log_dieusys(LOG_EXIT_SYS,"close stdin -- you are on your own") ;
}

void read_cmdline(stack *stk, size_t len)
{
    log_flow() ;

    // Open /proc/cmdline for reading
    ssize_t r ;
    unsigned int n = 0 ;

    int fd = io_open("/proc/cmdline", O_RDONLY) ;
    if (fd == -1)
        sulogin("open: ", "/proc/cmdline") ;

    for(;;) {
        r = read(fd,stk->s + n,len - n);
        if (r == -1) {
            if (errno == EINTR) continue ;
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

    if (close(fd) < 0)
        sulogin("close fd", "") ;

    stk->len = n ;
    stk->s[n] = 0 ;
}

static inline void string_to_table(char *table,char const **pointer,char const *str, uint8_t empty)
{
    log_flow() ;

    if (!empty) {
        auto_strings(table,str) ;
        *pointer = table ;
    }
}

static inline uint8_t string_to_uint(char const *str, unsigned int *ui, uint8_t empty)
{
    log_flow() ;

    if (!empty)
        if (!uint0_oscan(str,ui))
            return 0 ;

    return 1 ;
}

static int get_value(stralloc *val,char const *key)
{
    log_flow() ;

    _alloc_stk_(stk, val->len + 1) ;
    if (!environ_search_value(&stk, val->s, key))
        return 0 ;
    val->len = 0 ;
    if (!stralloc_copyb(val, stk.s, stk.len) ||
        !stralloc_0(val))
            sulogin("stralloc in get_value","") ;
    val->len-- ;
    return 1 ;
}

static int read_kernel_parameters(stralloc *kernel, const char *file)
{
    log_flow() ;
    size_t len = 2048 ; // should be sufficient for a kernel command line
    _alloc_stk_(f, len) ;
    _alloc_stk_(trim, len + 1) ;
    _alloc_stk_(res, len + 1) ;
    size_t pos = 0 ;

    read_cmdline(&f, len) ;

    if (!stack_string_clean(&trim, f.s))
        sulogin("clean file: ", file) ;

    FOREACH_STK(&trim, pos) {
        ssize_t r = get_len_until(trim.s + pos, '=') ;
        if (r >= 0) {
            len = strlen(trim.s + pos) ;
            if (!stack_add(&res, trim.s + pos, len + 1))
                sulogin("stack overflow", "") ;
        }
    }

    if (!stack_close(&res))
        sulogin("stack overflow", "") ;

    if (!stralloc_copyb(kernel, res.s, res.len) ||
        !stralloc_0(kernel))
            sulogin("stralloc", "") ;

    kernel->len-- ;

    return 1 ;
}

static void parse_conf(stralloc *env)
{
    log_flow() ;

    static char const *valid[] =
    { "VERBOSITY", "PATH", "LIVE", "TREE", "RCINIT", "UMASK", "RESCAN", "CONTAINER", "CATCHLOG", "RCINIT_CONTAINER", 0 } ;

    unsigned int j = 0 ;
    uint8_t empty = 0 ;
    _alloc_sa_(kernel) ;
    _alloc_sa_(val) ;
    _alloc_sa_(copy) ;
    char *kfile = "/proc/cmdline" ;

    if (!read_kernel_parameters(&kernel, kfile))
        sulogin("read kernel parameters: ", kfile) ;

    // kernel key=value pair take precedence
    if (!environ_merge_environ(env, &kernel))
        sulogin("merge kernel parameters", "") ;

    if (!stralloc_copyb(&copy, env->s, env->len))
        sulogin("copy environment", "") ;

    if (!environ_rebuild(&copy))
        sulogin("rebuild environment", "") ;

    for (char const *const *p = valid; *p; p++, j++) {

        empty = 0 ;
        val.len = 0 ;
        if (!stralloc_copys(&val, copy.s))
            sulogin("copy stralloc", "") ;

        switch (j) {

            case 0:

                if (!get_value(&val, "VERBOSITY"))
                    empty = 1 ;

                if (!string_to_uint(val.s,&VERBOSITY,empty))
                    sulogin("parse VERBOSITY value: ",val.s) ;

                break ;

            case 1:

                if (!get_value(&val, "PATH"))
                    empty = 1 ;

                string_to_table(tpath,&path,val.s,empty) ;

                break ;

            case 2:

                if (!get_value(&val, "LIVE"))
                    empty = 1 ;

                string_to_table(tlive,&live,val.s,empty) ;

                if (live[0] != '/')
                    sulogin ("LIVE must be an absolute path",live) ;

                break ;

            case 3:

                if (!get_value(&val, "TREE"))
                    empty = 1 ;

                string_to_table(ttree,&tree,val.s,empty) ;

                break ;

            case 4:

                if (!get_value(&val, "RCINIT"))
                    empty = 1 ;

                string_to_table(trcinit,&rcinit,val.s,empty) ;

                if (rcinit[0] != '/')
                    sulogin ("RCINIT must be an absolute path: ",rcinit) ;

                break ;

            case 5:

                if (!get_value(&val, "UMASK"))
                    empty = 1 ;

                if (!string_to_uint(val.s,&mask,empty))
                    sulogin("invalid UMASK value: ",val.s) ;

                break ;

            case 6:

                if (!get_value(&val, "RESCAN"))
                    empty = 1 ;

                if (!string_to_uint(val.s,&rescan,empty))
                    sulogin("invalid RESCAN value: ",val.s) ;

                break ;

            case 7:

                if (!get_value(&val, "CONTAINER"))
                    empty = 1 ;

                if (!string_to_uint(val.s,&container,empty))
                    sulogin("invalid CONTAINER value: ",val.s) ;

                break ;

            case 8:

                if (!get_value(&val, "CATCHLOG"))
                    empty = 1 ;

                if (!string_to_uint(val.s,&catch_log,empty))
                    sulogin("invalid CATCHLOG value: ",val.s) ;

                break ;

            case 9:

                if (!get_value(&val, "RCINIT_CONTAINER"))
                    empty = 1 ;

                string_to_table(trcinit_container,&rcinit_container,val.s,empty) ;

                if (rcinit_container[0] != '/')
                    sulogin ("RCINIT_CONTAINER must be an absolute path: ",rcinit_container) ;

                break ;

            default: break ;
        }

    }
    if (container) {
        if (!auto_stra(env,"CONTAINER_HALTCMD=",live,SS_BOOT_CONTAINER_DIR,"/0/",SS_BOOT_CONTAINER_HALTCMD,"\n")) {
            char tmp[strlen(live) + SS_BOOT_CONTAINER_DIR_LEN + 1 + SS_BOOT_CONTAINER_HALTCMD_LEN +1] ;
            auto_strings(tmp,live,SS_BOOT_CONTAINER_DIR,"/",SS_BOOT_CONTAINER_HALTCMD) ;
            sulogin("append environment stralloc with key: CONTAINER_HALTCMD=",tmp) ;
        }
    }
}

static inline void wait_for_notif (int fd)
{
    log_flow() ;

    char buf[16] ;
    for (;;) {

        ssize_t r = read(fd, buf, 16) ;
        if (r < 0)
            sulogin("read from notification pipe","") ;

        if (!r) {
          log_warn("s6-svscan failed to send a notification byte!") ;
          break ;
        }

        if (memchr(buf, '\n', r))
            break ;
    }

    close(fd) ;
}

static int is_mnt(char const *str)
{
    log_flow() ;

    struct stat st;
    size_t slen = strlen(str) ;
    int is_not_mnt = 0 ;
    if (lstat(str,&st) < 0) sulogin("lstat: ",str) ;
    if (S_ISDIR(st.st_mode))
    {
        dev_t st_dev = st.st_dev ; ino_t st_ino = st.st_ino ;
        char p[slen+4] ;
        memcpy(p,str,slen) ;
        memcpy(p + slen,"/..",3) ;
        p[slen+3] = 0 ;
        if (!stat(p,&st))
            is_not_mnt = (st_dev == st.st_dev) && (st_ino != st.st_ino) ;
    }else return 0 ;
    return is_not_mnt ? 0 : 1 ;
}

static void split_tmpfs(char *dst,char const *str)
{
    log_flow() ;

    size_t len = get_len_until(str+1,'/') ;
    len++ ;
    memcpy(dst,str,len) ;
    dst[len] = 0 ;
}

static inline void run_stage2 (stralloc *env)
{
    log_flow() ;

    char const *newargv[3] ;

    if (container) {
        newargv[0]= rcinit_container ;

    } else {

        newargv[0] = rcinit ;
    }

    newargv[1] = confile ;
    newargv[2] = 0 ;

    setsid() ;

    if (!catch_log) {

        close(notifpipe[1]) ;
        wait_for_notif(notifpipe[0]) ;

    } else {

        close(1) ;
        if (open(fifo, O_WRONLY) != 1)  /* blocks until catch-all logger is up */
            sulogin("open for writing fifo: ",fifo) ;
        if (fd_copy(2, 1) == -1)
            sulogin("copy stderr to stdout","") ;
    }

    size_t tlen = env->len ;
    char t[tlen + 1] ;
    memcpy(t,env->s,tlen) ;
    t[tlen] = 0 ;
    stralloc_free(env) ;

    xmexec_m(newargv, t, tlen) ;
}

static inline void make_cmdline(char const *prog,char const **add,int len,char const *msg,char const *arg, stralloc *env)
{
    log_flow() ;

    size_t elen = sastr_nelement(env) ;
    char const *e[elen+1] ;

    if (!env_make(e, elen, env->s, env->len))
        sulogin("make environment", "") ;
    e[elen] = 0 ;

    pid_t pid ;
    int wstat ;
    int m = 7 + len, i = 0, n = 0 ;
    char const *newargv[m] ;

    newargv[n++] = "66" ;
    newargv[n++] = "-v" ;
    newargv[n++] = cver ;
    newargv[n++] = "-l" ;
    newargv[n++] = live ;
    newargv[n++] = prog ;

    for (;i<len;i++)
        newargv[n++] = add[i] ;

    newargv[n] = 0 ;

    pid = child_spawn0(newargv[0], newargv, e) ;

    if (waitpid_nointr(pid, &wstat, 0) < 0)
        sulogin("wait for: ", newargv[0]) ;

    if (wstat)
        sulogin(msg, arg) ;

}

static void cad(void)
{
    log_flow() ;

    if (container)
        return ;

#ifdef __linux__
    int fd ;
    fd = open("/dev/tty0", O_RDONLY | O_NOCTTY) ;
    if (fd < 0) {
        log_warnusys("open /dev/tty0 (kbrequest will not be handled)") ;
    }
    else {

        if (ioctl(fd, KDSIGACCEPT, SIGWINCH) < 0)
            log_warnusys("ioctl KDSIGACCEPT on tty0 (kbrequest will not be handled)") ;
        close(fd) ;
    }

    sig_block(SIGINT) ; /* don't panic on early cad before s6-svscan catches it */
#endif

    if (reboot(RB_DISABLE_CAD) == -1)
        log_warnusys("trap ctrl-alt-del") ;

}

int ssexec_boot(int argc, char const *const *argv, ssexec_t *info)
{
	log_flow() ;

    VERBOSITY = 0 ;
    stralloc env = STRALLOC_ZERO ;
    unsigned int r , tmpfs = 0, opened = 0 ;
    size_t bannerlen, livelen ;
    pid_t pid ;
    char verbo[UINT_FMT] ;
    cver = verbo ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_BOOT, &l) ;
            if (opt == -1) break ;

            switch (opt)
            {
                case 'h' : VERBOSITY = 1 ; info_help(info->help, info->usage) ; return 0 ;
                case 'm' : tmpfs = 1 ; break ;
                case 's' : skel = l.arg ; break ;
                case 'e' : envdir = l.arg ; break ;
                case 'd' : slashdev = l.arg ; break ;
                case 'b' : banner = l.arg ; break ;
                case 'l' : log_user = l.arg ; break ;
                default :  log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (geteuid()) {
        errno = EPERM ;
        log_diesys(LOG_EXIT_USER, "nice try, but missing root privileges") ;
    }

    {
        if (skel[0] != '/')
            sulogin("skeleton directory must be an aboslute path: ",skel) ;

        auto_strings(confile, skel, "/", SS_BOOT_CONF) ;

        // init.conf
        if (!environ_merge_file(&env, confile))
            sulogin("merge environment file: ", confile) ;

        // envp
        if (!environ_import_arguments(&env, (char const *const *)environ, env_len((char const *const *)environ)))
            sulogin("import environment", "") ;

        // SS_ENVIRONMENT_ADMDIR
        if (!environ_merge_dir(&env, info->environment.s))
            sulogin("merge environment directory: ", info->environment.s) ;

        if (envdir) {

            if (envdir[0] != '/')
                sulogin("environment directory must be absolute: ", envdir) ;

            if (!environ_merge_dir(&env, envdir))
                sulogin("merge environment directory", envdir) ;
        }
    }

    parse_conf(&env) ;

    verbo[uint_fmt(verbo, VERBOSITY)] = 0 ;
    bannerlen = strlen(banner) ;
    livelen = strlen(live) ;
    char tfifo[livelen + 1 + SS_BOOT_LOGFIFO_LEN + 1] ;
    auto_strings(tfifo,live,"/",SS_BOOT_LOGFIFO) ;
    fifo = tfifo ;

    if (container) {
        /* If there's a Docker synchronization pipe, wait on it */
        char c ;
        ssize_t r = read(3, &c, 1) ;
        if (r < 0) {

          if (errno != EBADF)
            sulogin("read from fd 3","") ;

        } else {

          if (r)
            log_warn("parent wrote to fd 3!") ;

          close(3) ;
        }

    } else {

        allwrite(1, banner, bannerlen) ;
        allwrite(1, "\n", 2) ;
    }

    if (chdir("/") == -1) sulogin("chdir to ","/") ;
    umask(mask) ;
    setpgid(0, 0) ;
    close(0) ;

    if (container && slashdev)
        log_1_warn("-d options asked for a boot inside a container; are you sure your container does not come with a pre-mounted /dev?") ;

    if (slashdev)
    {
        log_info("Mount: ",slashdev) ;
        close(1) ;
        close(2) ;
        if (mount("dev", slashdev, "devtmpfs", MS_NOSUID | MS_NOEXEC, "") == -1)
            { opened++ ; sulogin ("mount: ", slashdev) ; }

        if (open("/dev/console", O_WRONLY) ||
        fd_move(2, 0) == -1 ||
        fd_copy(1, 2) == -1) return 111 ;
    }

    if (!opened) {

        if (open("/dev/null", O_RDONLY)) {

            /* ghetto /dev/null to the rescue */
            int p[2] ;
            log_1_warnusys("open /dev/null") ;

            if (pipe(p) < 0)
                sulogin("pipe for /dev/null","") ;

            close(p[1]) ;

            if (fd_move(0, p[0]) < 0)
                sulogin("fd_move to stdin","") ;
        }
    }

    char fs[livelen + 1] ;
    split_tmpfs(fs,live) ;
    r = is_mnt(fs) ;

    if (!r || tmpfs) {

        if (!r) {

            log_info("Mount: ",fs) ;
            if (mount("tmpfs", fs, "tmpfs", MS_NODEV | MS_NOSUID, "mode=0755") == -1)
                sulogin("mount: ",fs) ;

        } else if (tmpfs) {

            log_info("Remount: ",fs) ;
            if (mount("tmpfs", fs, "tmpfs", MS_REMOUNT | MS_NODEV | MS_NOSUID, "mode=0755") == -1)
                sulogin("mount: ",fs) ;
        }
    }

    /** create scandir */
    {
        size_t ncatch = !catch_log ? 1 : 0 ;
        size_t nargc = 6 + ncatch ;
        unsigned int m = 0 ;

        char const *t[nargc] ;

        t[m++] = "create" ;
        if (container) {
            t[m++] = "-B" ;
        } else {
            t[m++] = "-b" ;
        }

        if (!catch_log)
            t[m++] = "-c" ;

        t[m++] = "-s" ;
        t[m++] = skel ;
        t[m++] = "-L" ;
        t[m++] = log_user ;

        log_info("Create live scandir at: ",live) ;

        make_cmdline("scandir", t, nargc, "create live scandir at: ", live, &env) ;
    }

    /** initiate earlier service */
    {
        char const *t[] = { "init", tree } ;
        log_info("Initiate earlier service of tree: ",tree) ;
        make_cmdline("tree", t, 2, "initiate earlier service of tree: ", tree, &env) ;
    }

    if (catch_log)
    {
        log_info("Starts boot logger at: ",live,"/log/0") ;
        int fdr = open_read(fifo) ;
        if (fdr == -1) sulogin("open fifo: ",fifo) ;
        fd_close(1) ;
        if (open(fifo, O_WRONLY) != 1) sulogin("open fifo: ",fifo) ;
        fd_close(fdr) ;
    }

    /** fork and starts scandir */
    {
        char fmtfd[2 + UINT_FMT] = "-" ;

        size_t m = 0 ;
        static char const *newargv[8] ;
        newargv[m++] = "66" ;
        newargv[m++] = "-v0" ;
        newargv[m++] = "-l" ;
        newargv[m++] = live ;
        newargv[m++] = "scandir" ;
        newargv[m++] = "start" ;
        if (!catch_log)
            newargv[m++] = fmtfd ;
        newargv[m++] = 0 ;

        if (!catch_log && pipe(notifpipe) < 0)
            sulogin("pipe","") ;

        pid = fork() ;

        if (pid == -1)
            sulogin("fork: ",container ? rcinit_container : rcinit) ;

        if (!pid)
            run_stage2(&env) ;

        if (!catch_log) {

            close(notifpipe[0]) ;
            fmtfd[1] = 'd' ;
            fmtfd[2 + uint_fmt(fmtfd + 2, notifpipe[1])] = 0 ;
            cad() ;

        } else {

            cad() ;
            if (fd_copy(2, 1) == -1)
                sulogin("copy stderr to stdout","") ;
        }

        // it merge char const *const *environ with env.s where env.s take precedence
        xmexec_m(newargv, env.s, env.len) ;

    }
}
