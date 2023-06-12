/*
 * ssexec_scandir_create.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdarg.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>
#include <oblibs/files.h>

#include <skalibs/djbunix.h>
#include <skalibs/buffer.h>
#include <skalibs/sgetopt.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/config.h>
#include <66/ssexec.h>
#include <66/enum.h>

#include <execline/config.h>
#include <s6/config.h>

#define CRASH 0
#define FINISH 1
#define INT 2
#define QUIT 3
#define TERM 4
#define USR1 5
#define USR2 6
#define PWR 7
#define WINCH 8

#define AUTO_CRTE_CHW 1
#define AUTO_CRTE_CHW_CHM 2
#define PERM1777 S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO

static uid_t OWNER ;
static char *OWNERSTR ;
static gid_t GIDOWNER ;
static char GIDSTR[GID_FMT] ;

static char const *skel = SS_SKEL_DIR ;
static char const *log_user = SS_LOGGER_RUNNER ;
static unsigned int BOOT = 0 ;
static unsigned int CONTAINER = SS_BOOT_CONTAINER ;
static unsigned int CATCH_LOG = SS_BOOT_CATCH_LOG ;
static size_t compute_buf_size(char const *str,...) ;
static size_t CONFIG_STR_LEN = 0 ;
static int BUF_FD ; // general buffer fd


static void inline auto_chown(char const *str)
{
    log_flow() ;

    log_trace("chown directory: ",str," to: ",OWNERSTR,":",GIDSTR) ;
    if (chown(str,OWNER,GIDOWNER) < 0)
        log_dieusys(LOG_EXIT_SYS,"chown: ",str) ;
}

static void inline auto_dir(char const *str,mode_t mode)
{
    log_flow() ;

    log_trace("create directory: ",str) ;
    if (!dir_create_parent(str,mode))
        log_dieusys(LOG_EXIT_SYS,"create directory: ",str) ;

    size_t n = strlen(str), i = 0 ;
    char tmp[n + 1] ;
    for (; i < n ; i++) {

        if ((str[i] == '/') && i)
        {
            tmp[i] = 0 ;
            auto_chown(tmp) ;
        }
        tmp[i] = str[i] ;
    }

    auto_chown(str) ;
}

static void inline auto_chmod(char const *str,mode_t mode)
{
    log_flow() ;

    if (chmod(str,mode) < 0)
        log_dieusys(LOG_EXIT_SYS,"chmod: ",str) ;
}

static void inline auto_file(char const *dst,char const *file,char const *contents,size_t conlen)
{
    log_flow() ;

    char f[strlen(dst) + 1 + strlen(file) + 1] ;
    auto_strings(f, dst, "/", file) ;

    log_trace("write file: ", f) ;
    if (!file_write_unsafe(dst,file,contents,conlen))
        log_dieusys(LOG_EXIT_SYS,"write file: ",dst,"/",file) ;

    auto_chown(f) ;
}

static void inline auto_check(char const *str,mode_t type,mode_t perm,int what)
{
    log_flow() ;

    int r ;
    r = scan_mode(str,S_IFDIR) ;
    if (r < 0) { errno = EEXIST ; log_diesys(LOG_EXIT_SYS,"conflicting format of: ",str) ; }
    if (!r)
    {
        auto_dir(str,type) ;
        if (what > 0) auto_chown(str) ;
        if (what > 1) auto_chmod(str,perm) ;
    }
}

static void inline auto_fifo(char const *str)
{
    log_flow() ;

    int r ;
    r = scan_mode(str,S_IFIFO) ;
    if (r < 0) { errno = EEXIST ; log_diesys(LOG_EXIT_SYS,"conflicting format of: ",str) ; }
    if (!r)
    {
        log_trace("create fifo: ",str) ;
        if (mkfifo(str, 0600) < 0)
            log_dieusys(LOG_EXIT_SYS,"create fifo: ",str) ;
    }
}

static void inline auto_rm(char const *str)
{
    log_flow() ;

    int r ;
    r = scan_mode(str,S_IFDIR) ;
    if (r > 0)
    {
        log_info("removing: ",str,"...") ;
        if (!dir_rm_rf(str)) log_dieusys(LOG_EXIT_SYS,"remove: ",str) ;
    }
}

static void inline log_perm(char const *str,uid_t *uid,gid_t *gid)
{
    log_flow() ;

    if (!youruid(uid,str)) log_dieusys(LOG_EXIT_SYS,"set uid of: ",str) ;
    if (!yourgid(gid,*uid)) log_dieusys(LOG_EXIT_SYS,"set gid of: ",str) ;
}

void inline shebang(buffer *b, char const *opts)
{
    log_flow() ;

    if (!auto_buf(b, "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb ", opts, "\n"))
        log_die_nomem("buffer") ;
}

void append_shutdown(buffer *b, char const *live, char const *opts)
{
    log_flow() ;

    if (!auto_buf(b,SS_BINPREFIX "66-shutdown ",opts))
        log_die_nomem("buffer") ;

    if (!CONTAINER)
        if (!auto_buf(b," -a"))
            log_die_nomem("buffer") ;

    if (!auto_buf(b," -l ",live," -- now\n"))
        log_die_nomem("buffer") ;

}

static size_t compute_buf_size(char const *str,...)
{

    va_list alist ;
    va_start(alist,str) ;
    size_t len = 0 ;

    while (str != 0) {
        len += strlen(str) ;
        str = va_arg(alist, char const *) ;
    }
    va_end(alist) ;

    return len ;
}

static buffer init_buffer(char const *dst, char const *file, size_t len)
{
    log_flow() ;

    int fd ;
    buffer b ;

    size_t dstlen = strlen(dst), filen = strlen(file) ;
    char w[dstlen + 1 + filen + 1] ;
    char buf[len + 1] ;
    auto_strings(w, dst, "/", file) ;

    fd = open_trunc(w) ;

    if (fd < 0  || ndelay_off(fd) < 0)
        log_die(LOG_EXIT_SYS,"open trunc") ;

    buffer_init(&b,&fd_writev, fd, buf, len) ;

    return b ;
}

void write_to_bufnclose(buffer *b, char const *dst, char const *file)
{
    if (!buffer_flush(b))
        log_dieusys(LOG_EXIT_SYS, "write to: ", dst, "/", file) ;

    fd_close(BUF_FD) ;
}

void write_shutdownd(char const *live, char const *scandir)
{
    log_flow() ;

    buffer b ;
    size_t blen = compute_buf_size(live, skel, 0) ;
    blen += 500 + CONFIG_STR_LEN ;
    size_t scandirlen = strlen(scandir) ;
    char shut[scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN + 5 + 1] ;

    auto_strings(shut,scandir,"/",SS_BOOT_SHUTDOWND) ;

    auto_check(shut,0755,0755,AUTO_CRTE_CHW_CHM) ;

    auto_strings(shut + scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN,"/fifo") ;

    auto_fifo(shut) ;

    shut[scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN] = 0 ;

    b = init_buffer(shut, "run", blen) ;

    shebang(&b, "-P") ;
    if (!auto_buf(&b,
        SS_BINPREFIX "66-shutdownd -l ",
        live," -s ",skel," -g 3000"))
            log_die_nomem("buffer") ;

    if (CONTAINER)
        if (!auto_buf(&b," -B"))
            log_die_nomem("buffer") ;

    if (!CATCH_LOG)
        if (!auto_buf(&b," -c"))
            log_die_nomem("buffer") ;

    if (!auto_buf(&b,"\n"))
        log_die_nomem("buffer") ;

    write_to_bufnclose(&b, shut, "run") ;

    auto_strings(shut + scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN,"/run") ;

    auto_chmod(shut,0755) ;
}

void write_bootlog(char const *live, char const *scandir)
{
    log_flow() ;

    int r ;
    uid_t uid = -1 ;
    gid_t gid = -1 ;
    size_t livelen = strlen(live), scandirlen = strlen(scandir), ownerlen = uid_fmt(OWNERSTR,OWNER), loglen = 0, blen = 0 ;
    buffer b ;
    char path[livelen + 4 + ownerlen + 1] ;
    char logdir[scandirlen + SS_SCANDIR_LEN + SS_LOG_SUFFIX_LEN + 1 + 5 + 1] ;

    /** run/66/scandir/uid_name/scandir-log */
    auto_strings(logdir,scandir,"/" SS_SCANDIR SS_LOG_SUFFIX) ;

    loglen = scandirlen + SS_SCANDIR_LEN + SS_LOG_SUFFIX_LEN + 1 ;

    auto_check(logdir,0755,0,AUTO_CRTE_CHW) ;

    /** make the fifo*/
    auto_strings(logdir + loglen, "/fifo") ;

    auto_fifo(logdir) ;
    auto_chown(logdir) ;

    /** set the log path for the run file
     * /run/66/log*/
    auto_strings(path,live,"log/",OWNERSTR) ;

    log_trace("create directory: ",path) ;
    r = dir_create_parent(path,02750) ;
    if (!r)
        log_dieusys(LOG_EXIT_SYS,"create: ",path) ;

    log_perm(log_user,&uid,&gid) ;

    if (chown(path,uid,gid) < 0)
        log_dieusys(LOG_EXIT_SYS,"chown: ",path) ;

    auto_chmod(path,02755) ;

    logdir[loglen] = 0 ;

    blen = compute_buf_size(live, logdir, log_user, path, 0) ;
    blen += 500 + CONFIG_STR_LEN;
    b = init_buffer(logdir, "run", blen) ;

    /** make run file */
    shebang(&b,"-P") ;
    if (CONTAINER) {

        if (!auto_buf(&b,EXECLINE_BINPREFIX "fdmove -c 1 2\n"))
            log_die_nomem("buffer") ;

    } else {

        if (!auto_buf(&b,
            EXECLINE_BINPREFIX "redirfd -w 1 /dev/null\n"))
                log_die_nomem("buffer") ;
    }

    if (!auto_buf(&b,
            EXECLINE_BINPREFIX "redirfd -rnb 0 fifo\n" \
            S6_BINPREFIX "s6-setuidgid ",
            log_user,
            "\n" S6_BINPREFIX "s6-log -bpd3 -- 1"))
                log_die_nomem("buffer") ;

    if (SS_LOGGER_TIMESTAMP < TIME_NONE)
        if (!auto_buf(&b, SS_LOGGER_TIMESTAMP == TIME_ISO ? " T " : " t "))
            log_die_nomem("buffer") ;

    if (!auto_buf(&b,path,"\n"))
        log_die_nomem("buffer") ;

    write_to_bufnclose(&b, logdir, "run") ;

    auto_file(logdir, SS_NOTIFICATION, "3\n",2) ;

    auto_strings(logdir + loglen,"/run") ;

    auto_chmod(logdir,0755) ;
    auto_chown(logdir) ;
}

void write_control(char const *scandir,char const *live, char const *filename, int file)
{
    log_flow() ;

    buffer b ;
    size_t scandirlen = strlen(scandir), filen = strlen(filename), blen = 0 ;
    char mode[scandirlen + SS_SVSCAN_LOG_LEN + filen + 1] ;

    auto_strings(mode,scandir,SS_SVSCAN_LOG) ;

    blen = compute_buf_size(live, scandir, 0) ;
    blen += 500 + CONFIG_STR_LEN ;

    b = init_buffer(mode, filename + 1, blen) ;

    shebang(&b,"-P") ;

    if (file == FINISH)
    {
        if (CONTAINER) {

            if (!auto_buf(&b,
                SS_BINPREFIX "execl-envfile ",live, SS_BOOT_CONTAINER_DIR "/",OWNERSTR,"\n" \
                EXECLINE_BINPREFIX "fdclose 1\n" \
                EXECLINE_BINPREFIX "fdclose 2\n" \
                EXECLINE_BINPREFIX "wait { }\n" \
                EXECLINE_BINPREFIX "foreground {\n" \
                SS_BINPREFIX "66-hpr -f -n -${HALTCODE} -l ",live," \n}\n" \
                EXECLINE_BINPREFIX "exit ${EXITCODE}\n"))
                    log_die_nomem("buffer") ;

        } else if (BOOT) {

            if (!auto_buf(&b,
                EXECLINE_BINPREFIX "redirfd -w 2 /dev/console\n" \
                EXECLINE_BINPREFIX "fdmove -c 1 2\n" \
                EXECLINE_BINPREFIX "foreground { " SS_BINPREFIX "66-echo -- \"scandir ",
                scandir," exited. Rebooting.\" }\n" \
                SS_BINPREFIX "66-hpr -r -f -l ",
                live,"\n"))
                    log_die_nomem("buffer") ;

        } else {

            if (!auto_buf(&b,
                SS_BINPREFIX "66-echo -- \"scandir ",
                scandir," stopped...\"\n"))
                    log_die_nomem("buffer") ;
        }
        goto write ;
    }

    if (file == CRASH)
    {

        if (CONTAINER) {

            if (!auto_buf(&b,
                EXECLINE_BINPREFIX "foreground {\n" \
                EXECLINE_BINPREFIX "fdmove -c 1 2\n" \
                SS_BINPREFIX "66-echo \"scandir crashed. Killing everythings and exiting.\"\n}\n" \
                EXECLINE_BINPREFIX "foreground {\n" \
                EXECLINE_BINPREFIX "66-nuke\n}\n" \
                EXECLINE_BINPREFIX "wait { }\n" \
                SS_BINPREFIX "66-hpr -f -n -p -l ",live,"\n"))
                    log_die_nomem("buffer") ;
        }
        else {

            if (!auto_buf(&b,
                EXECLINE_BINPREFIX "redirfd -w 2 /dev/console\n" \
                EXECLINE_BINPREFIX "fdmove -c 1 2\n" \
                EXECLINE_BINPREFIX "foreground { " SS_BINPREFIX "66-echo -- \"scandir ",
                scandir, " crashed."))
                    log_die_nomem("buffer") ;

            if (BOOT) {

                if (!auto_buf(&b,
                    " Rebooting.\" }\n" \
                    SS_BINPREFIX "66-hpr -r -f -l ",
                    live,"\n"))
                        log_die_nomem("buffer") ;

            } else if (!auto_buf(&b,"\" }\n"))
                log_die_nomem("buffer") ;
        }

        goto write ;
    }
    if (!BOOT) {

        if (!auto_buf(&b,
            EXECLINE_BINPREFIX "foreground { " SS_BINPREFIX "66 -v3 -l ",
            live," tree stop }\n"))
                log_die_nomem("buffer") ;

    }

    switch(file)
    {
        case PWR:
        case USR1:

            if (BOOT)
                append_shutdown(&b,live,"-p") ;

            break ;
        case USR2:

            if (BOOT)
                append_shutdown(&b,live,"-h") ;

            break ;
        case TERM:
        case QUIT:
            break ;

        case INT:

            if (BOOT)
                append_shutdown(&b,live,"-r") ;

            break ;

        case WINCH:
            break ;

        default:
            break ;
    }

    write:

        write_to_bufnclose(&b, mode, filename + 1) ;

        auto_strings(mode + scandirlen + SS_SVSCAN_LOG_LEN, filename) ;

        auto_chmod(mode,0755) ;
        auto_chown(mode) ;
}

void auto_empty_file(char const *dst, char const *filename, char const *contents)
{
    size_t dstlen = strlen(dst), filen = strlen(filename) ;

    char tmp[dstlen + filen + 1] ;
    auto_strings(tmp, dst, filename) ;

    if (!file_write_unsafe_g(tmp, contents))
        log_dieusys(LOG_EXIT_SYS, "create file: ", tmp) ;

    auto_chown(tmp) ;
}

static void create_service_skel(char const *service, char const *target, char const *notif, ssexec_t *info)
{
    size_t targetlen = strlen(target) ;
    size_t servicelen = strlen(service) + 1 ;

    char dst[targetlen + 1 + servicelen + info->ownerlen + 21 + 1] ;
    auto_strings(dst, target, "/", service, "/data/rules/uid/", info->ownerstr) ;

    auto_dir(dst, 0755) ;
    auto_empty_file(dst, "/allow", "") ;

    char sym[targetlen + 1 + servicelen + 22 + 1] ;
    auto_strings(sym, target, "/", service, "/data/rules/uid/self") ;

    log_trace("point symlink: ", sym, " to ", info->ownerstr) ;
    if (symlink(info->ownerstr, sym) < 0)
        log_dieusys(LOG_EXIT_SYS, "symlink: ", sym) ;

    if (lchown(sym, OWNER, GIDOWNER) < 0)
        log_dieusys(LOG_EXIT_SYS, "chown: ", sym) ;

    auto_strings(dst, target, "/", service, "/data/rules/gid/", info->ownerstr) ;
    auto_dir(dst, 0755) ;

    auto_empty_file(dst, "/allow", "") ;

    auto_strings(dst, target, "/", service, "/") ;
    auto_file(dst, SS_NOTIFICATION, notif, strlen(notif)) ;
}

static void create_service_oneshot(char const *scandir, ssexec_t *info)
{
    size_t scandirlen = strlen(scandir) ;
    size_t fdlen = scandirlen + 1 + SS_ONESHOTD_LEN ;

    create_service_skel(SS_ONESHOTD, scandir, "3\n", info) ;
    size_t runlen = strlen(SS_EXECLINE_SHEBANGPREFIX) + strlen(SS_LIBEXECPREFIX) + 174 ;
    char run[runlen + 1] ;
    auto_strings(run,"#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n", \
                    "fdmove -c 2 1\n", \
                    "fdmove 1 3\n", \
                    "s6-ipcserver-socketbinder -- s\n", \
                    "s6-ipcserverd -1 --\n", \
                    "s6-ipcserver-access -v0 -E -l0 -i data/rules --\n", \
                    "s6-sudod -t 30000 --\n", \
                    SS_LIBEXECPREFIX "66-oneshot --\n") ;


    char dst[fdlen + 5] ;
    auto_strings(dst, scandir, "/", SS_ONESHOTD, "/run") ;

    // -1 openwritenclose_unsafe do not accept closed string
    if (!openwritenclose_unsafe(dst, run, runlen))
        log_dieusys(LOG_EXIT_SYS, "write: ", dst) ;

    if (chmod(dst, 0755) < 0)
        log_dieusys(LOG_EXIT_SYS, "chmod: ", dst) ;

    auto_chown(dst) ;
}

static void create_service_fdholder(char const *scandir, ssexec_t *info)
{
    size_t scandirlen = strlen(scandir) ;
    size_t fdlen = scandirlen + 1 + SS_FDHOLDER_LEN ;

    create_service_skel(SS_FDHOLDER, scandir, "1\n", info) ;

    char dst[fdlen + info->ownerlen + 20 + 1] ;
    auto_strings(dst, scandir, "/", SS_FDHOLDER, "/data/rules/uid/", info->ownerstr, "/env") ;

    auto_dir(dst, 0755) ;

    auto_empty_file(dst, "/S6_FDHOLDER_GETDUMP", "\n") ;
    auto_empty_file(dst, "/S6_FDHOLDER_LIST", "\n") ;
    auto_empty_file(dst, "/S6_FDHOLDER_SETDUMP", "\n") ;

    auto_strings(dst + fdlen + info->ownerlen + 20, "/S6_FDHOLDER_STORE_REGEX" ) ;

    if(!openwritenclose_unsafe(dst, "^" SS_FDHOLDER_PIPENAME "\n", SS_FDHOLDER_PIPENAME_LEN + 2))
        log_dieusys(LOG_EXIT_SYS, "write: ", dst) ;

    auto_chown(dst) ;

    char sym[fdlen + info->ownerlen + 47 + 1] ;
    auto_strings(sym, scandir, "/", SS_FDHOLDER, "/data/rules/uid/",info->ownerstr,"/env/S6_FDHOLDER_RETRIEVE_REGEX") ;

    auto_strings(dst, "S6_FDHOLDER_STORE_REGEX") ;

    log_trace("point symlink: ", sym, " to ", dst) ;
    if (symlink(dst, sym) < 0)
        log_dieusys(LOG_EXIT_SYS, "symlink: ", dst) ;

    if (lchown(sym, OWNER, GIDOWNER) < 0)
        log_dieusys(LOG_EXIT_SYS, "chown: ", sym) ;

    auto_strings(sym, scandir, "/", SS_FDHOLDER, "/data/rules/gid/", info->ownerstr, "/env") ;
    auto_strings(dst, "../../uid/", info->ownerstr, "/env") ;

    log_trace("point symlink: ", sym, " to ", dst) ;
    if (symlink(dst, sym) < 0)
        log_dieusys(LOG_EXIT_SYS, "symlink: ", dst) ;

    if (lchown(sym, OWNER, GIDOWNER) < 0)
        log_dieusys(LOG_EXIT_SYS, "chown: ", sym) ;

    size_t runlen = strlen(SS_EXECLINE_SHEBANGPREFIX) + strlen(SS_LIBEXECPREFIX) + 277 + 1 ;

    char run[runlen] ;
    auto_strings(run, "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n", \
                "pipeline -dw -- {\n",
                "   if -- {\n", \
                "       forstdin -x0 -- i\n", \
                "           exit 0\n", \
                "   }\n", \
                "   if -nt -- {\n", \
                "       redirfd -r 0 ./data/autofilled\n", \
                "       s6-ipcclient -l0 -- s\n", \
                "       ", SS_LIBEXECPREFIX "66-fdholder-filler -1 --\n", \
                "   }\n", \
                "   s6-svc -t .\n", \
                "}\n", \
                "s6-fdholder-daemon -1 -i data/rules -- s\n") ;

    auto_strings(dst, scandir, "/", SS_FDHOLDER, "/run") ;

    // -1 openwritenclose_unsafe do not accept closed string
    if (!openwritenclose_unsafe(dst, run, strlen(run) - 1))
        log_dieusys(LOG_EXIT_SYS, "write: ", dst) ;

    auto_chmod(dst, 0755) ;
    auto_chown(dst) ;

    auto_strings(dst, scandir, "/", SS_FDHOLDER, "/data/autofilled") ;

    // -1 openwritenclose_unsafe do not accept closed string
    if(!openwritenclose_unsafe(dst, "\n", 1))
        log_dieusys(LOG_EXIT_SYS, "write: ", dst) ;

    auto_chown(dst) ;
}

static void create_scandir(char const *live, char const *scandir, ssexec_t *info)
{
    log_flow() ;

    size_t scanlen = strlen(scandir) ;
    char tmp[scanlen + 11 + 1] ;

    /** run/66/scandir/<uid> */
    auto_strings(tmp,scandir) ;

    auto_check(tmp,0755,0,AUTO_CRTE_CHW) ;

    /** run/66/scandir/uid/.svscan */
    auto_strings(tmp + scanlen, SS_SVSCAN_LOG) ;

    auto_check(tmp,0755,0,AUTO_CRTE_CHW) ;

    char const *const file[] =
    {
        "/crash", "/finish", "/SIGINT",
        "/SIGQUIT", "/SIGTERM", "/SIGUSR1", "/SIGUSR2",
        "/SIGPWR", "/SIGWINCH"
     } ;

    log_trace("write control file... ") ;
    for (int i = 0 ; i < 9; i++)
        write_control(scandir,live,file[i],i) ;

    if (BOOT) {

        if (CATCH_LOG)
            write_bootlog(live, scandir) ;

        write_shutdownd(live, scandir) ;
    }

    create_service_fdholder(scandir, info) ;
    create_service_oneshot(scandir, info) ;
}

void sanitize_live(char const *live)
{
    log_flow() ;

    size_t livelen = strlen(live) ;
    char tmp[livelen + SS_BOOT_CONTAINER_DIR_LEN + 1 + strlen(OWNERSTR) + 1] ;

    /** run/66 */
    auto_check(live,0755,0,AUTO_CRTE_CHW) ;

    /** run/66/scandir */
    auto_strings(tmp,live,SS_SCANDIR) ;
    auto_check(tmp,0755,PERM1777,AUTO_CRTE_CHW_CHM) ;

    if (CONTAINER) {
        /** run/66/container */
        auto_strings(tmp + livelen,SS_BOOT_CONTAINER_DIR,"/",OWNERSTR) ;
        auto_check(tmp,0755,PERM1777,AUTO_CRTE_CHW_CHM) ;
        auto_file(tmp,SS_BOOT_CONTAINER_HALTCMD,"EXITCODE=0\nHALTCODE=p\n",22) ;
    }

    /** run/66/log */
    auto_strings(tmp + livelen, SS_LOG) ;
    auto_check(tmp,0755,PERM1777,AUTO_CRTE_CHW_CHM) ;

    /** /run/66/state*/
    auto_strings(tmp + livelen,SS_STATE + 1) ;
    auto_check(tmp,0755,PERM1777,AUTO_CRTE_CHW_CHM) ;
}

int ssexec_scandir_create(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r ;

    CONFIG_STR_LEN = compute_buf_size(SS_BINPREFIX, SS_EXTBINPREFIX, SS_EXTLIBEXECPREFIX, SS_LIBEXECPREFIX, SS_EXECLINE_SHEBANGPREFIX, 0) ;

    OWNER = info->owner ;
    OWNERSTR = info->ownerstr ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_SCANDIR_CREATE, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'b' :

                    if (OWNER)
                        log_die(LOG_EXIT_USER, "-b options can be set only with root") ;

                    BOOT = 1 ;
                    break ;

                case 'B' :

                    if (OWNER)
                        log_die(LOG_EXIT_USER, "-B options can be set only with root") ;

                    CONTAINER = 1 ;
                    BOOT = 1 ;
                    break ;

                case 's' :

                    skel = l.arg ;
                    break ;

                case 'c' :

                    CATCH_LOG = 0 ;
                    break ;

                case 'L' :

                    log_user = l.arg ;
                    break ;

                default :

                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (BOOT && OWNER && !CONTAINER)
        log_die(LOG_EXIT_USER, "-b options can be set only with root") ;

    if (!yourgid(&GIDOWNER,OWNER))
        log_dieusys(LOG_EXIT_SYS, "set gid of: ", OWNERSTR) ;

    GIDSTR[gid_fmt(GIDSTR,GIDOWNER)] = 0 ;

    if (BOOT && skel[0] != '/')
        log_die(LOG_EXIT_USER, "rc.shutdown: ", skel, " must be an absolute path") ;

    r = scan_mode(info->scandir.s, S_IFDIR) ;
    if (r < 0) log_die(LOG_EXIT_SYS, "scandir: ", info->scandir.s, " exist with unkown mode") ;
    if (!r) {

        log_trace("sanitize ", info->live.s, " ...") ;
        sanitize_live(info->live.s) ;
        log_info ("create scandir ", info->scandir.s, " ...") ;
        create_scandir(info->live.s, info->scandir.s, info) ;

    } else
        log_info("scandir: ", info->scandir.s, " already exist, keep it") ;

    return 0 ;
}
