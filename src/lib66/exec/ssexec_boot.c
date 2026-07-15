/*
 * ssexec_boot.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <signal.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <linux/kd.h>

#include <oblibs/log.h>
#include <oblibs/exec.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/io.h>
#include <oblibs/types.h>
#include <oblibs/fd.h>
#include <oblibs/spawn.h>
#include <oblibs/process.h>
#include <oblibs/files.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/ssexec.h>
#include <66/tree.h>
#include <66/utils.h>

static unsigned int mask = SS_BOOT_UMASK ;
static unsigned int container = SS_BOOT_CONTAINER ;
static unsigned int catch_log = SS_BOOT_CATCH_LOG ;

static char const *skel = SS_SKEL_DIR ;
static char const *banner = "\n[Starts stage1 process...]" ;
static char const *slashdev = 0 ;
static char const *envdir = 0 ;
static char const *fifo = 0 ;
static char const *haltfile = 0 ;
static char const *log_user = SS_LOGGER_RUNNER ;
static char const *cver = 0 ;

/* init.conf / cmdline overridable values, pre-seeded with their default so they
 * are never empty even when the key is absent from the configuration */
static char path[SS_MAX_PATH_LEN + 1] = SS_BOOT_PATH ;
static char live[SS_MAX_PATH_LEN + 1] = SS_LIVE ;
static char tree[SS_MAX_PATH_LEN + 1] = SS_BOOT_TREE ;
static char confile[SS_MAX_PATH_LEN + 1 + SS_BOOT_CONF_LEN + 1] ;
static int notifpipe[2] ;

/**
 * @brief Last-resort boot recovery: log the error, hand the admin a rescue
 * shell, then RETURN so the boot resumes best-effort.
 *
 * This is deliberately not a log_die: the contract is that callers regain
 * control once the admin leaves the shell. Sites that read data produced by the
 * failed operation must therefore guarantee a defined state after the call.
 */
static void sulogin(char const *msg,char const *arg)
{
    static char const *const newarg[2] = { SS_EXTBINPREFIX "sulogin" , 0 } ;
    pid_t pid ;
    int wstat ;
    if (msg) log_warnusys(msg,arg) ;
    pid = spawn_path(newarg[0],newarg,(char const *const *)environ) ;
    if (!pid)
        log_dieusys(LOG_EXIT_SYS,"spawn sulogin -- you are on your own") ;
    if (process_wait(pid,&wstat) < 0)
        log_dieusys(LOG_EXIT_SYS,"wait for sulogin -- you are on your own") ;
    close_fd(0) ;
}

static void read_cmdline(strbuf *stk, size_t len)
{
    log_flow() ;

    int fd = io_open("/proc/cmdline", O_RDONLY) ;
    if (fd == -1) {
        // sulogin returns: leave the cmdline empty so init.conf defaults apply
        sulogin("open: ", "/proc/cmdline") ;
        stk->len = 0 ;
        stk->s[0] = 0 ;
        return ;
    }

    /* /proc/cmdline is procfs: stat() reports st_size 0, so a slurp would read
     * nothing. io_allread reads into our own sized buffer and stops at EOF;
     * len - 1 keeps room for the terminating NUL. */
    size_t n = io_allread(fd, stk->s, len - 1) ;

    close_fd(fd) ;

    stk->len = n ;
    stk->s[n] = 0 ;
}

static int get_value(strbuf *out, char const *env, char const *key)
{
    log_flow() ;

    _alloc_strbuf_(stk, strlen(env) + 1) ;
    if (!environ_search_value(&stk, env, key))
        return 0 ;
    out->len = 0 ;
    if (!strbuf_copyb(out, stk.s, stk.len) ||
        !strbuf_uncounted(out))
            sulogin("strbuf in get_value","") ;

    return 1 ;
}

static int read_kernel_parameters(strbuf *kernel, const char *file)
{
    log_flow() ;
    size_t len = 2048 ; // should be sufficient for a kernel command line
    _alloc_sbl_(f, len) ;
    _alloc_sbl_(trim, len + 1) ;
    _alloc_sbl_(res, len + 1) ;
    size_t pos = 0 ;

    read_cmdline(&f, len) ;

    if (!sbl_clean_string(&trim, f.s))
        sulogin("clean file: ", file) ;

    FOREACH_SBL(&trim, pos) {
        // keep only key=value tokens
        if (get_len_until(trim.s + pos, '=') >= 0) {
            size_t toklen = strlen(trim.s + pos) ;
            if (!sbl_addb(&res, trim.s + pos, toklen))
                sulogin("append kernel parameter: ", trim.s + pos) ;
        }
    }

    if (!strbuf_copyb(kernel, res.s, res.len) ||
        !strbuf_terminate(kernel))
            sulogin("strbuf", "") ;

    kernel->len-- ;

    return 1 ;
}

static void set_env(strbuf *env, const char *key, const char *value)
{
    char tmp[strlen(key) + 1 + strlen(value) + 1] ;
    auto_strings(tmp, key, "=", value) ;
    if (!sbl_add(env, tmp))
        sulogin("append environment variable: ", tmp) ;

}

typedef enum conf_type_e conf_type_e ;
enum conf_type_e { CONF_UINT, CONF_STR } ;

typedef struct conf_entry_s conf_entry_t ;
struct conf_entry_s {
    char const *key ;
    conf_type_e type ;
    void *target ;      // unsigned int* (CONF_UINT) | buffer char* (CONF_STR)
    uint8_t absolute ;  // require an absolute path
} ;

static void parse_conf(const char *conf)
{
    log_flow() ;

    static conf_entry_t const conf_table[] = {
        { "VERBOSITY",        CONF_UINT, &VERBOSITY,       0 },
        { "PATH",             CONF_STR,  path,             0 },
        { "LIVE",             CONF_STR,  live,             1 },
        { "TREE",             CONF_STR,  tree,             0 },
        { "UMASK",            CONF_UINT, &mask,            0 },
        { "CATCHLOG",         CONF_UINT, &catch_log,       0 },
        { 0, 0, 0, 0 }
    } ;

    _cleanup_strbuf_ strbuf kernel = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf env = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf text = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf val = STRBUF_ZERO ;
    char *kfile = "/proc/cmdline" ;

    // init.conf
    if (!environ_merge_file(&env, conf))
        sulogin("merge environment file: ", conf) ;

    if (!read_kernel_parameters(&kernel, kfile))
        sulogin("read kernel parameters: ", kfile) ;

    // kernel key=value pair take precedence
    if (!environ_merge_environ(&env, &kernel))
        sulogin("merge kernel parameters", "") ;

    if (!environ_untrim(&text, &env))
        sulogin("rebuild environment", "") ;

    for (conf_entry_t const *e = conf_table ; e->key ; e++) {

        if (!get_value(&val, text.s, e->key))
            continue ; // key absent: keep the static default

        if (e->type == CONF_UINT) {

            if (!u32_scan_strict_base(val.s, e->target, 8))
                sulogin("invalid value for: ", e->key) ;

        } else {

            auto_strings(e->target, val.s) ;

            if (e->absolute && *(char const *)e->target != '/')
                sulogin("must be an absolute path: ", e->target) ;
        }
    }
}

/* fd idiom used throughout boot: io_open returns the fd it got.
 * `if (io_open(...))` works because the caller guarantees fd 0 is the lowest
 * free descriptor, so a success returns 0 (falsy) and only a -1 failure is
 * truthy. Its counterpart `io_open(...) != 1` is used where fd 1 is expected. */
static void opendevnull (void)
{
    if (io_open("/dev/null", O_RDONLY)) {
        /* ghetto /dev/null to the rescue */
        int p[2] ;
        log_warnusys("open /dev/null") ;
        if (pipe(p) < 0)
            sulogin("pipe", "") ;
        close_fd(p[1]) ;
        if (move_fd(0, p[0]) < 0)
            sulogin("move_fd to stdin", "") ;
    }
}

static void reset_stdin (void)
{
    close_fd(0) ;
    opendevnull() ;
}

static inline void wait_for_notif (int fd)
{
    log_flow() ;

    char buf[16] ;
    for (;;) {

        ssize_t r = io_read(fd, buf, 16) ;
        if (r < 0)
            sulogin("read from notification pipe","") ;

        if (!r) {
          log_warn("66-scandir failed to send a notification byte!") ;
          break ;
        }

        if (memchr(buf, '\n', r))
            break ;
    }

    close_fd(fd) ;
}

static int is_mnt(char const *str)
{
    log_flow() ;

    struct stat st;
    size_t slen = strlen(str) ;
    int is_not_mnt = 0 ;
    if (lstat(str,&st) < 0) {
        // sulogin returns: report the path as a mount point so we don't lay a
        // fresh tmpfs over an unknown state
        sulogin("lstat: ",str) ;
        return 1 ;
    }
    if (S_ISDIR(st.st_mode)) {
        dev_t st_dev = st.st_dev ; ino_t st_ino = st.st_ino ;
        char p[slen+4] ;
        auto_strings(p, str, "/..") ;
        if (!stat(p,&st))
            is_not_mnt = (st_dev == st.st_dev) && (st_ino != st.st_ino) ;

    } else return 0 ;

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

static void set_container_exitcode(uint32_t code)
{
    char fmt[U32_FMT] ;
    fmt[u32_fmt(fmt, code)] = 0 ;

    char content[9 + U32_FMT + 11 + 1] ; // "EXITCODE=" <code> "\nHALTCODE=p\n"
    auto_strings(content, "EXITCODE=", fmt, "\nHALTCODE=p\n") ;

    if (!file_write(haltfile, content, strlen(content)))
        log_warnusys("write container halt file: ", haltfile) ;
}

static inline void run_stage2 (strbuf *env, const char *tty, ssexec_t *info)
{
    log_flow() ;

    if (setsid() < 0)
        sulogin("setsid to run stage2", "") ;

    if (tty) {

        close_fd(0) ;
        if (io_open(tty, O_RDONLY)) {
            log_warnusys("open ", tty) ;
            opendevnull() ;
        }
    }

    if (!catch_log) {

        close_fd(notifpipe[1]) ;
        wait_for_notif(notifpipe[0]) ;

    } else {

        close_fd(1) ;
        if (io_open(fifo, O_WRONLY) != 1)  // blocks until catch-all logger is up
            sulogin("open for writing fifo: ",fifo) ;
        if (copy_fd(2, 1) == -1)
            sulogin("copy stderr to stdout","") ;
    }

    info->live.len = 0 ;
    if (!auto_strbuf(&info->live, live) || set_livedir(&info->live) <= 0) {
        log_warnusys("set live directory: ", live) ;
        _exit(LOG_EXIT_SYS) ;
    }

    info->scandir.len = 0 ;
    if (!strbuf_copy(&info->scandir, &info->live) || !strbuf_uncounted(&info->scandir)
        || set_livescan(&info->scandir, info->owner) <= 0) {
        log_warnusys("set scandir directory: ", info->live.s) ;
        _exit(LOG_EXIT_SYS) ;
    }

    size_t modn = sbl_count(env), elen = environ_length((char const *const *)environ) ;
    char const *merged[elen + modn + 1] ;
    environ_merge(merged, elen + modn + 1, (char const *const *)environ, elen, env->s, env->len) ;
    environ = (char **)merged ;

    log_info("Starting services of tree: ", tree) ;

    int rc = tree_send(0, tree, 0, info) ;

    if (rc) {

        log_warnu("start services of tree: ", tree, " -- see log with '66 log system'") ;

    } else {

        log_info("Starting enabled trees") ;

        rc = tree_send(0, 0, 0, info) ;

        if (rc)
            log_warnu("start enabled trees -- see log with '66 log system'") ;
    }

    if (container && rc)
        set_container_exitcode(LOG_EXIT_SYS) ;

    /* TODO: End-of-boot event is emitted here -- boot-done on success, boot-failed otherwise */

    _exit(rc ? LOG_EXIT_SYS : 0) ;
}

static inline void make_cmdline(char const *prog,char const **add,int len,char const *msg,char const *arg, strbuf *env)
{
    log_flow() ;

    size_t elen = sbl_count(env) ;
    char const *e[elen+1] ;

    if (!environ_make(e, elen, env->s, env->len))
        sulogin("make environment", "") ;

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

    pid = spawn_path(newargv[0], newargv, e) ;
    if (!pid)
        sulogin("spawn: ", newargv[0]) ;

    if (process_wait(pid, &wstat) < 0)
        sulogin("wait for: ", newargv[0]) ;

    if (wstat)
        sulogin(msg, arg) ;
}

static void cad(void)
{
    log_flow() ;

    if (container)
        return ;

    int fd ;
    fd = io_open("/dev/tty0", O_RDONLY | O_NOCTTY) ;
    if (fd < 0) {

        if (errno == ENOENT)
            log_warn("headless system detected") ;
        else log_warnu("open tty0 (kbrequest will not be handled)") ;

    } else {

        if (ioctl(fd, KDSIGACCEPT, SIGWINCH) < 0)
            log_warnusys("ioctl KDSIGACCEPT on tty0 (kbrequest will not be handled)") ;

        close_fd(fd) ;
    }

    sigset_t ss ;
    sigemptyset(&ss) ;
    sigaddset(&ss, SIGINT) ; // don't panic on early cad before the scanner catches it
    sigprocmask(SIG_BLOCK, &ss, 0) ;

    if (reboot(RB_DISABLE_CAD) == -1)
        log_warnusys("trap ctrl-alt-del") ;

}

static opt_t const opts_boot[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",        .arg = OPT_NONE,                            .help = "print this help" },
    { .id = 'm',         .shortname = 'm', .longname = "mount",       .arg = OPT_NONE,                            .help = "mount parent live directory" },
    { .id = 's',         .shortname = 's', .longname = "skeleton",    .arg = OPT_REQUIRED, .argname = "path",     .help = "skeleton directory to use" },
    { .id = 'e',         .shortname = 'e', .longname = "environment", .arg = OPT_REQUIRED, .argname = "path",     .help = "environment directory or file to use" },
    { .id = 'd',         .shortname = 'd', .longname = "dev",         .arg = OPT_REQUIRED, .argname = "path",     .help = "mount dev directory" },
    { .id = 'b',         .shortname = 'b', .longname = "banner",      .arg = OPT_REQUIRED, .argname = "message",  .help = "print banner at the beginning of the init process" },
    { .id = 'l',         .shortname = 'l', .longname = "log-user",    .arg = OPT_REQUIRED, .argname = "username", .help = "run catch-all logger as log_user user" },
    { .id = 'c',         .shortname = 'c', .longname = "container",   .arg = OPT_NONE,                            .help = "boot a container" },
} ;

static uint8_t boot_tmpfs = 0 ;

static int on_boot(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {
        case 'm' : boot_tmpfs = 1 ; break ;
        case 's' : skel = arg ; break ;
        case 'e' : envdir = arg ; break ;
        case 'd' : slashdev = arg ; break ;
        case 'b' : banner = arg ; break ;
        case 'l' : log_user = arg ; break ;
        case 'c' : container = 1 ; break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_boot = {
    .name = "66 boot",
    .help = "boot a system with 66",
    .opts = opts_boot,
    .nopts = OPT_COUNT(opts_boot),
    .on_option = &on_boot,
    .fn = &ssexec_boot,
} ;

int ssexec_boot(int argc, char const *const *argv, void *data)
{
	log_flow() ;

    ssexec_t *info = data ;
    info->who = STATUS_WHO_BOOT ;

    (void)argc ;
    (void)argv ;

    strbuf env = STRBUF_ZERO ;
    // boot is one-shot: ssexec_boot always ends in exec/sulogin, never returns
    unsigned int r , tmpfs = boot_tmpfs, hasconsole = 1 ;
    size_t bannerlen, livelen ;
    pid_t pid ;
    char verbo[U32_FMT] ;
    cver = verbo ;
    char *tty = 0 ;

    if (geteuid()) {
        errno = EPERM ;
        log_diesys(LOG_EXIT_USER, "nice try, but missing root privileges") ;
    }

    // Configuration file init.conf
    {
        if (skel[0] != '/')
            sulogin("skeleton directory must be an absolute path: ",skel) ;

        auto_strings(confile, skel, "/", SS_BOOT_CONF) ;

        parse_conf(confile) ;
    }

    verbo[u32_fmt(verbo, VERBOSITY)] = 0 ;
    int ttyfd = catch_log ? 2 : 1 ; // the terminal is on stderr (logger) or stdout (no logger)
    bannerlen = strlen(banner) ;
    livelen = strlen(live) ;
    char tfifo[livelen + 1 + SS_BOOT_LOGFIFO_LEN + 1] ;
    auto_strings(tfifo, live, "/", SS_BOOT_LOGFIFO) ;
    fifo = tfifo ;

    // outlives the fork: run_stage2 writes it on a container boot failure
    char thalt[livelen + SS_BOOT_CONTAINER_DIR_LEN + 1 + info->ownerlen + 1 + SS_BOOT_CONTAINER_HALTFILE_LEN + 1] ;
    if (container) {
        auto_strings(thalt, live, SS_BOOT_CONTAINER_DIR, "/", info->ownerstr, "/", SS_BOOT_CONTAINER_HALTFILE) ;
        haltfile = thalt ;
    }

    if (fcntl(1, F_GETFD) < 0)
        hasconsole = 0 ;

    if (container) {
        // If there's a Docker synchronization pipe, wait on it
        char c ;
        ssize_t rd = io_read(3, &c, 1) ;
        if (rd < 0) {

          if (errno != EBADF)
            sulogin("read from fd 3","") ;

        } else {

          if (rd)
            log_warn("parent wrote to fd 3!") ;

          close_fd(3) ;
        }

        if (!slashdev && hasconsole && isatty(ttyfd)) {
            tty = ttyname(ttyfd) ;
            if (!tty)
                log_warnusys("ttyname std", (!catch_log) ? "err" : "out") ;
        }

    } else if (hasconsole) {

        io_allwrite(1, (char *)banner, bannerlen) ;
        io_allwrite(1, "\n", 1) ;
    }

    if (chdir("/") == -1)
        sulogin("chdir to ","/") ;

    umask(mask) ;

    if (container && slashdev)
        log_1_warn("-d options asked for a boot inside a container; are you sure your container does not come with a pre-mounted /dev?") ;

    if (slashdev) {

        int nope, e ;
        log_info("Mount: ",slashdev) ;
        close_fd(0) ;
        close_fd(1) ;
        close_fd(2) ;

        nope = mount("dev", slashdev, "devtmpfs", MS_NOSUID | MS_NOEXEC, "") == -1 ;
        e = errno ;
        if (io_open("/dev/console", O_WRONLY) && io_open("/dev/null", O_WRONLY))
            sulogin("open /dev/console or /dev/null", "") ;
        if (move_fd(2, 0) < 0)
            sulogin("move stderr to stdin", "") ;
        if (copy_fd(1, 2) < 0)
            sulogin("copy stdout to stderr", "") ;

        if (nope) {
            errno = e ;
            sulogin("mount a devtmpfs on /dev", "") ;
        }

        if (io_open("/dev/console", O_RDONLY))
            opendevnull() ;
    }

    if (!hasconsole) {

        if (!slashdev)
            reset_stdin() ;
        if (io_open("/dev/null", O_WRONLY) != 1 || copy_fd(2, 1) == -1)
            sulogin("open /dev/null or copy stderr to stdout", "") ;
    }

    char fs[livelen + 1] ;
    split_tmpfs(fs,live) ;
    r = is_mnt(fs) ;

    if (!r || tmpfs) {

        if (!r) {

            log_info("Mount: ",fs) ;
            if (mount("tmpfs", fs, "tmpfs", MS_NODEV | MS_NOSUID, "mode=0755") == -1)
                sulogin("mount: ",fs) ;

        } else {

            log_info("Remount: ",fs) ;
            if (mount("tmpfs", fs, "tmpfs", MS_REMOUNT | MS_NODEV | MS_NOSUID, "mode=0755") == -1)
                sulogin("mount: ",fs) ;
        }
    }

    set_env(&env, "PATH", path) ;

    // create scandir
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

    // initiate earlier service
    {
        char const *t[] = { "init", tree } ;
        log_info("Initiate earlier service of tree: ",tree) ;
        make_cmdline("tree", t, 2, "initiate earlier service of tree: ", tree, &env) ;
    }

    if (catch_log)
    {
        log_info("Starts boot logger at: ",live,"/log/0") ;
        int fdr = io_open(fifo, O_RDONLY|O_NONBLOCK) ;
        if (fdr == -1)
            sulogin("open fifo: ",fifo) ;
        close_fd(1) ;
        if (io_open(fifo, O_WRONLY) != 1)
            sulogin("open fifo: ",fifo) ;
        close_fd(fdr) ;
    }

    // environment
    {
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

    // fork and starts scandir
    {
        char fmtfd[2 + U32_FMT] = "-" ;

        size_t m = 0 ;
        char const *newargv[8] ;
        newargv[m++] = "66" ;
        newargv[m++] = "-v0" ;
        newargv[m++] = "-l" ;
        newargv[m++] = live ;
        newargv[m++] = "scandir" ;
        newargv[m++] = "start" ;
        if (!catch_log)
            newargv[m++] = fmtfd ; // contents filled in the parent branch below, once the pipe exists
        newargv[m++] = 0 ;

        if (!catch_log && pipe(notifpipe) < 0)
            sulogin("pipe","") ;

        if (tty && !slashdev && ioctl(ttyfd, TIOCNOTTY) == -1)
            log_warnusys("relinquish control terminal") ;

        pid = fork() ;

        if (pid == -1)
            sulogin("fork: stage2") ;

        if (!pid)
            run_stage2(&env, tty, info) ;

        reset_stdin() ;
        setsid() ;

        if (!catch_log) {

            close_fd(notifpipe[0]) ;
            /* format the readiness fd now that the pipe exists; notifpipe[1] is
             * >= 3 here (fds 0,1,2 are taken) as 66 scandir start requires it */
            fmtfd[1] = 'd' ;
            fmtfd[2 + u32_fmt(fmtfd + 2, notifpipe[1])] = 0 ;
            cad() ;

        } else {

            cad() ;
            if (copy_fd(2, 1) == -1)
                sulogin("copy stderr to stdout", "") ;
        }

        // it merge char const *const *environ with env.s where env.s take precedence
        exec_path_merge_die(newargv[0], newargv, (char const *const *)environ, env.s, env.len) ;
    }
}
