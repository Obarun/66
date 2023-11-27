/*
 * ssexec_boot.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
static char const *const *genv = 0 ;
static int fdin ;
static stralloc sacmdline = STRALLOC_ZERO ;
static int notifpipe[2] ;

#define MAXBUF 1024*64*2

static void sulogin(char const *msg,char const *arg)
{
    static char const *const newarg[2] = { SS_EXTBINPREFIX "sulogin" , 0 } ;
    pid_t pid ;
    int wstat ;
    close(0) ;
    if (dup2(fdin,0) == -1) log_dieu(LOG_EXIT_SYS,"duplicate stdin -- you are on your own") ;
    close(fdin) ;
    if (*msg) log_warnu(msg,arg) ;
    pid = child_spawn0(newarg[0],newarg,genv) ;
    if (waitpid_nointr(pid,&wstat, 0) < 0)
        log_dieusys(LOG_EXIT_SYS,"wait for sulogin -- you are on your own") ;
    fdin=dup(0) ;
    if (fdin == -1) log_dieu(LOG_EXIT_SYS,"duplicate stdin -- you are on your own") ;
    close(0) ;
    if (open("/dev/null",O_WRONLY)) log_dieu(LOG_EXIT_SYS,"open /dev/null -- you are on your own") ;
}

static int get_value(stralloc *val,char const *key)
{
    if (!environ_get_val_of_key(val,key)) return 0 ;
    /** value may be empty, in this case we use the default one */
    if (!sastr_clean_element(val))
    {
        log_warnu("get value of: ",key," -- keeps the default") ;
        return 0 ;
    }
    if (!sastr_rebuild_in_oneline(val)) sulogin("rebuild line of value: ",val->s) ;

    return 1 ;
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

static void parse_conf(void)
{
    log_flow() ;

    static char const *valid[] =
    { "VERBOSITY", "PATH", "LIVE", "TREE", "RCINIT", "UMASK", "RESCAN", "CONTAINER", "CATCHLOG", "RCINIT_CONTAINER", 0 } ;
    int r ;
    unsigned int j = 0 ;
    uint8_t empty = 0 ;
    size_t filesize = 0 ;
    char u[UINT_FMT] ;
    char *gvalue = 0 ;
    stralloc src = STRALLOC_ZERO ;
    stralloc val = STRALLOC_ZERO ;

    if (skel[0] != '/')
        sulogin("skeleton directory must be an aboslute path: ",skel) ;

    auto_strings(confile, skel, "/", SS_BOOT_CONF) ;

    filesize = file_get_size(confile) ;
    /** skeleton file */
    r = openreadfileclose(confile, &src, filesize) ;
    if(!r)
        sulogin("open configuration file: ",confile) ;
    if (!stralloc_0(&src))
        sulogin("append stralloc of file: ",confile) ;

    for (char const *const *p = valid; *p; p++, j++)
    {
        empty = 0 ;
        /** try first to read from kernel environment.
         * If the key is not found, try to read the skeleton file.
         * Finally keep the default value if we cannot get a correct
         * key=value pair */
        gvalue = getenv(*p) ;
        if (gvalue) {

            if (!auto_stra(&val, *p, "=", gvalue))
                sulogin("copy value of key: ", *p) ;

        } else {

            if (!stralloc_copy(&val, &src))
                sulogin("copy stralloc of file: ",confile) ;

            if (!get_value(&val,*p))
                empty = 1 ;

        }

        switch (j)
        {
            case 0:

                if (!string_to_uint(val.s,&VERBOSITY,empty))
                    sulogin("parse VERBOSITY value: ",val.s) ;

                u[uint_fmt(u, VERBOSITY)] = 0 ;
                if (!auto_stra(&sacmdline,"VERBOSITY=",u,"\n"))
                    sulogin("append environment stralloc with key: VERBOSITY=",u) ;
                break ;

            case 1:

                string_to_table(tpath,&path,val.s,empty) ;

                if (!auto_stra(&sacmdline,"PATH=",path,"\n"))
                    sulogin("append environment stralloc with key: PATH=",path) ;
                break ;

            case 2:

                string_to_table(tlive,&live,val.s,empty) ;

                if (live[0] != '/')
                    sulogin ("LIVE must be an absolute path",live) ;

                if (!auto_stra(&sacmdline,"LIVE=",live,"\n"))
                    sulogin("append environment stralloc with key: LIVE=",live) ;
                break ;

            case 3:

                string_to_table(ttree,&tree,val.s,empty) ;

                if (!auto_stra(&sacmdline,"TREE=",tree,"\n"))
                    sulogin("append environment stralloc with key: TREE=",tree) ;
                break ;

            case 4:

                string_to_table(trcinit,&rcinit,val.s,empty) ;

                if (rcinit[0] != '/')
                    sulogin ("RCINIT must be an absolute path: ",rcinit) ;

                if (!auto_stra(&sacmdline,"RCINIT=",rcinit,"\n"))
                    sulogin("append environment stralloc with key: RCINIT=",rcinit) ;
                break ;

            case 5:

                if (!string_to_uint(val.s,&mask,empty))
                    sulogin("invalid UMASK value: ",val.s) ;

                u[uint_fmt(u, mask)] = 0 ;
                if (!auto_stra(&sacmdline,"UMASK=",u,"\n"))
                    sulogin("append environment stralloc with key: UMASK=",u) ;
                break ;

            case 6:

                if (!string_to_uint(val.s,&rescan,empty))
                    sulogin("invalid RESCAN value: ",val.s) ;

                u[uint_fmt(u, rescan)] = 0 ;
                if (!auto_stra(&sacmdline,"RESCAN=",u,"\n"))
                    sulogin("append environment stralloc with key: RESCAN=",u) ;
                break ;

            case 7:

                if (!string_to_uint(val.s,&container,empty))
                    sulogin("invalid CONTAINER value: ",val.s) ;

                u[uint_fmt(u,container)] = 0 ;
                if (!auto_stra(&sacmdline,"CONTAINER=",u,"\n"))
                    sulogin("append environment stralloc with key: CONTAINER=",u) ;

                break ;

            case 8:

                if (!string_to_uint(val.s,&catch_log,empty))
                    sulogin("invalid CATCHLOG value: ",val.s) ;

                u[uint_fmt(u,catch_log)] = 0 ;
                if (!auto_stra(&sacmdline,"CATCHLOG=",u,"\n"))
                    sulogin("append environment stralloc with key: CATCHLOG=",u) ;
                break ;

            case 9:

                string_to_table(trcinit_container,&rcinit_container,val.s,empty) ;

                if (rcinit_container[0] != '/')
                    sulogin ("RCINIT_CONTAINER must be an absolute path: ",rcinit_container) ;

                if (!auto_stra(&sacmdline,"RCINIT_CONTAINER=",rcinit_container,"\n"))
                    sulogin("append environment stralloc with key: RCINIT_CONTAINER=",rcinit_container) ;
                break ;

            default: break ;
        }

    }
    if (container) {
        if (!auto_stra(&sacmdline,"CONTAINER_HALTCMD=",live,SS_BOOT_CONTAINER_DIR,"/0/",SS_BOOT_CONTAINER_HALTCMD,"\n")) {
            char tmp[strlen(live) + SS_BOOT_CONTAINER_DIR_LEN + 1 + SS_BOOT_CONTAINER_HALTCMD_LEN +1] ;
            auto_strings(tmp,live,SS_BOOT_CONTAINER_DIR,"/",SS_BOOT_CONTAINER_HALTCMD) ;
            sulogin("append environment stralloc with key: CONTAINER_HALTCMD=",tmp) ;
        }
    }

    if (!sastr_split_string_in_nline(&sacmdline)) sulogin("split string: ",sacmdline.s) ;

    stralloc_free(&val) ;
    stralloc_free(&src) ;
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

static inline void run_stage2 (char const *const *envp, size_t envlen, char const *modifs, size_t modiflen)
{
    log_flow() ;

    size_t pos = 0 ;
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
    }
    else {

        close(1) ;
        if (open(fifo, O_WRONLY) != 1)  /* blocks until catch-all logger is up */
            sulogin("open for writing fifo: ",fifo) ;
        if (fd_copy(2, 1) == -1)
            sulogin("copy stderr to stdout","") ;
    }

    close(fdin) ;

    for (;pos < modiflen ; pos += strlen(modifs + pos) +1)
        if (!stralloc_catb(&sacmdline,modifs + pos, strlen(modifs + pos) + 1))
            sulogin("append environment stralloc with value: ",modifs + pos) ;

    size_t tlen = sacmdline.len ;
    char t[tlen + 1] ;
    memcpy(t,sacmdline.s,tlen) ;
    t[tlen] = 0 ;
    stralloc_free(&sacmdline) ;

    xmexec_fm(newargv, envp, envlen, t, tlen) ;
}

static inline void make_cmdline(char const *prog,char const **add,int len,char const *msg,char const *arg,char const *const *envp)
{
    log_flow() ;

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

    pid = child_spawn0(newargv[0], newargv, envp) ;

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
    VERBOSITY = 0 ;
    unsigned int r , tmpfs = 0, opened = 0 ;
    size_t bannerlen, livelen ;
    pid_t pid ;
    char verbo[UINT_FMT] ;
    cver = verbo ;
    stralloc envmodifs = STRALLOC_ZERO ;
    genv = (char const *const *)environ ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_BOOT, &l) ;
            if (opt == -1) break ;

            switch (opt)
            {
                case 'h' : info_help(info->help, info->usage) ; return 0 ;
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
    if (geteuid())
    {
        errno = EPERM ;
        log_diesys(LOG_EXIT_USER, "nice try, peon") ;
    }
    fdin=dup(0) ;
    parse_conf() ;
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

    if (!r || tmpfs)
    {
        if (r && tmpfs)
        {
            log_info("Umount: ",fs) ;
            if (umount(fs) == -1) sulogin ("umount: ",fs ) ;
        }
        log_info("Mount: ",fs) ;
        if (mount("tmpfs", fs, "tmpfs", MS_NODEV | MS_NOSUID, "mode=0755") == -1)
            sulogin("mount: ",fs) ;
    }
    /** respect the path before run API*/
    if (setenv("PATH", path, 1) == -1) sulogin("set initial PATH: ",path) ;
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

        make_cmdline("scandir", t, nargc, "create live scandir at: ", live, genv) ;
    }
    /** initiate earlier service */
    {
        char const *t[] = { tree } ;
        log_info("Initiate earlier service of tree: ",tree) ;
        make_cmdline("init", t, 1, "initiate earlier service of tree: ", tree, genv) ;
    }

    if (envdir) {
        if (!environ_clean_envfile_unexport(&envmodifs,envdir))
            sulogin("prepare environment from: ",envdir) ;
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
        size_t pathlen = strlen(path) ;
        char const *newenvp[2] = { 0, 0 } ;
        char pathvar[6 + pathlen] ;
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

        memcpy(pathvar, "PATH=", 5) ;
        memcpy(pathvar + 5, path, pathlen + 1) ;

        newenvp[0] = pathvar ;

        if (!catch_log && pipe(notifpipe) < 0)
            sulogin("pipe","") ;

        pid = fork() ;

        if (pid == -1)
            sulogin("fork: ",container ? rcinit_container : rcinit) ;

        if (!pid)
            run_stage2(newenvp, 1, envmodifs.s,envmodifs.len) ;

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

        close(fdin) ;
        xmexec_fm(newargv, newenvp, 1, envmodifs.s, envmodifs.len) ;

    }
}
