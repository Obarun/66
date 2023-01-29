/*
 * ssexec_scandir.c
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
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>

#include <oblibs/log.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>

#include <skalibs/stralloc.h>
#include <skalibs/sgetopt.h>
#include <skalibs/djbunix.h>
#include <skalibs/types.h>
#include <skalibs/env.h>
#include <skalibs/bytestr.h>//byte_count
#include <skalibs/exec.h>
#include <skalibs/buffer.h>

#include <s6/config.h>
#include <execline/config.h>

#include <66/config.h>
#include <66/ssexec.h>
#include <66/svc.h>
#include <66/utils.h>
#include <66/enum.h>
#include <66/constants.h>

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
static char OWNERSTR[UID_FMT] ;
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

#define USAGE "66-scandir [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -b|B ] [ -c ] [ -L log_user ] [ -s skel ] [ -o owner ] create|remove"

static inline unsigned int lookup (char const *const *table, char const *signal)
{
    log_flow() ;

    unsigned int i = 0 ;
    for (; table[i] ; i++) if (!strcmp(signal, table[i])) break ;
    return i ;
}

static inline unsigned int parse_command (char const *command)
{
    log_flow() ;

    static char const *const command_table[] =
    {
        "create",
        "remove",
        0
    } ;
  unsigned int i = lookup(command_table, command) ;
  if (!command_table[i]) i = 3 ;
  return i ;
}

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

    log_trace("write file: ",dst,"/",file) ;
    if (!file_write_unsafe(dst,file,contents,conlen))
        log_dieusys(LOG_EXIT_SYS,"write file: ",dst,"/",file) ;
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
        if (rm_rf(str) < 0) log_dieusys(LOG_EXIT_SYS,"remove: ",str) ;
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
    uid_t uid ;
    gid_t gid ;
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

    auto_file(logdir,"notification-fd","3\n",2) ;

    auto_strings(logdir + loglen,"/run") ;

    auto_chmod(logdir,0755) ;
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
                scandir," shutted down...\"\n"))
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
            EXECLINE_BINPREFIX "foreground { " SS_BINPREFIX "66-all -v3 -l ",
            live," down }\n"))
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
            if (!BOOT)
                if (!auto_buf(&b, SS_BINPREFIX "66-scanctl -l ",live," stop\n"))
                    log_die_nomem("buffer") ;

            break ;
        case QUIT:

            if (!BOOT)
                if (!auto_buf(&b, SS_BINPREFIX "66-scanctl -l ",live," quit\n"))
                    log_die_nomem("buffer") ;

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
}

void auto_empty_file(char const *dst, char const *filename, char const *contents)
{
    size_t dstlen = strlen(dst), filen = strlen(filename) ;

    char tmp[dstlen + filen + 1] ;
    auto_strings(tmp, dst, filename) ;

    if (!file_write_unsafe_g(tmp, contents))
        log_dieusys(LOG_EXIT_SYS, "create file: ", tmp) ;
}

static void create_service_skel(char const *service, char const *target, char const *notif)
{
    size_t targetlen = strlen(target) ;
    size_t servicelen = strlen(service) + 1 ;

    char dst[targetlen + 1 + servicelen + 22 + 1] ;
    auto_strings(dst, target, "/", service, "/data/rules/uid/0") ;

    auto_dir(dst, 0755) ;
    auto_empty_file(dst, "/allow", "") ;

    char sym[targetlen + 1 + servicelen + 22 + 1] ;
    auto_strings(sym, target, "/", service, "/data/rules/uid/self") ;

    log_trace("point symlink: ", sym, " to ", "0") ;
    if (symlink("0", sym) < 0)
        log_dieusys(LOG_EXIT_SYS, "symlink: ", sym) ;

    auto_strings(dst, target, "/", service, "/data/rules/gid/0") ;
    auto_dir(dst, 0755) ;
    auto_empty_file(dst, "/allow", "") ;

    auto_strings(dst, target, "/", service, "/") ;
    auto_file(dst, "notification-fd", notif, strlen(notif)) ;
}

static void create_service_oneshot(char const *scandir)
{
    size_t scandirlen = strlen(scandir) ;
    size_t fdlen = scandirlen + 1 + SS_ONESHOTD_LEN ;

    create_service_skel(SS_ONESHOTD, scandir, "3\n") ;
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
}

static void create_service_fdholder(char const *scandir)
{
    size_t scandirlen = strlen(scandir) ;
    size_t fdlen = scandirlen + 1 + SS_FDHOLDER_LEN ;

    create_service_skel(SS_FDHOLDER, scandir, "1\n") ;

    char dst[fdlen + 21 + 1] ;
    auto_strings(dst, scandir, "/", SS_FDHOLDER, "/data/rules/uid/0/env") ;

    auto_dir(dst, 0755) ;

    auto_empty_file(dst, "/S6_FDHOLDER_GETDUMP", "\n") ;
    auto_empty_file(dst, "/S6_FDHOLDER_LIST", "\n") ;
    auto_empty_file(dst, "/S6_FDHOLDER_SETDUMP", "\n") ;

    auto_strings(dst + fdlen + 21, "/S6_FDHOLDER_STORE_REGEX" ) ;

    if(!openwritenclose_unsafe(dst, "^" SS_FDHOLDER_PIPENAME "\n", SS_FDHOLDER_PIPENAME_LEN + 2))
        log_dieusys(LOG_EXIT_SYS, "write: ", dst) ;

    char sym[fdlen + 48 + 1] ;
    auto_strings(sym, scandir, "/", SS_FDHOLDER, "/data/rules/uid/0/env/S6_FDHOLDER_RETRIEVE_REGEX") ;

    auto_strings(dst, "S6_FDHOLDER_STORE_REGEX") ;

    log_trace("point symlink: ", sym, " to ", dst) ;
    if (symlink(dst, sym) < 0)
        log_dieusys(LOG_EXIT_SYS, "symlink: ", dst) ;

    auto_strings(sym, scandir, "/", SS_FDHOLDER, "/data/rules/gid/0/env") ;
    auto_strings(dst, "../../uid/0/env") ;

    log_trace("point symlink: ", sym, " to ", dst) ;
    if (symlink(dst, sym) < 0)
        log_dieusys(LOG_EXIT_SYS, "symlink: ", dst) ;

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
    if(!openwritenclose_unsafe(dst, run, runlen - 1))
        log_dieusys(LOG_EXIT_SYS, "write: ", dst) ;

    if (chmod(dst, 0755) < 0)
        log_dieusys(LOG_EXIT_SYS, "chmod: ", dst) ;

    auto_strings(dst, scandir, "/", SS_FDHOLDER, "/data/autofilled") ;

    // -1 openwritenclose_unsafe do not accept closed string
    if(!openwritenclose_unsafe(dst, "\n", 1))
        log_dieusys(LOG_EXIT_SYS, "write: ", dst) ;

}

void create_scandir(char const *live, char const *scandir)
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

    if (BOOT)
    {
        if (CATCH_LOG)
            write_bootlog(live, scandir) ;

        write_shutdownd(live, scandir) ;
    }

    create_service_fdholder(scandir) ;
    create_service_oneshot(scandir) ;
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

int ssexec_scandir(int argc, char const *const *argv, ssexec_t *info)
{
    int r ;
    unsigned int cmd, create, remove ;

    stralloc live = STRALLOC_ZERO ;
    stralloc scandir = STRALLOC_ZERO ;

    CONFIG_STR_LEN = compute_buf_size(SS_BINPREFIX, SS_EXTBINPREFIX, SS_EXTLIBEXECPREFIX, SS_LIBEXECPREFIX, SS_EXECLINE_SHEBANGPREFIX, 0) ;

    cmd = create = remove = 0 ;

    OWNER = MYUID ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_SCANDIR, &l) ;
            if (opt == -1) break ;

            switch (opt)
            {
                 case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 'b' :

                    BOOT = 1 ;
                    break ;

                case 'B' :

                    CONTAINER = 1 ;
                    BOOT = 1 ;
                    break ;

                case 'l' :

                    if (!stralloc_cats(&live,l.arg) ||
                    !stralloc_0(&live))
                        log_die_nomem("stralloc") ;

                    break ;

                case 's' :

                    skel = l.arg ;
                    break ;

                case 'o' :

                    if (MYUID)
                        log_die(LOG_EXIT_USER, "only root can use -o option") ;

                    if (!youruid(&OWNER,l.arg))
                        log_dieusys(LOG_EXIT_SYS,"get uid of: ",l.arg) ;

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

    if (!argc)
        log_usage(info->usage, "\n", info->help) ;

    cmd = parse_command(argv[0]) ;

    if (cmd == 3) {
        log_usage(usage_scandir) ;
    } else if (!cmd) {
        create = 1 ;
    } else  {
        remove = 1 ;
    }

    if (BOOT && OWNER && !CONTAINER)
        log_die(LOG_EXIT_USER,"-b options can be set only with root") ;

    OWNERSTR[uid_fmt(OWNERSTR,OWNER)] = 0 ;

    if (!yourgid(&GIDOWNER,OWNER))
        log_dieusys(LOG_EXIT_SYS,"set gid of: ",OWNERSTR) ;

    GIDSTR[gid_fmt(GIDSTR,GIDOWNER)] = 0 ;

    /** live -> /run/66/ */
    r = set_livedir(&live) ;
    if (r < 0) log_die(LOG_EXIT_USER,"live: ",live.s," must be an absolute path") ;
    if (!r) log_dieusys(LOG_EXIT_SYS,"set live directory") ;

    if (!stralloc_copy(&scandir,&live)) log_die_nomem("stralloc") ;

    /** scandir -> /run/66/scandir/ */
    r = set_livescan(&scandir,OWNER) ;
    if (r < 0) log_die(LOG_EXIT_USER,"scandir: ", scandir.s, " must be an absolute path") ;
    if (!r) log_dieusys(LOG_EXIT_SYS,"set scandir directory") ;

    if (BOOT && skel[0] != '/')
        log_die(LOG_EXIT_USER, "rc.shutdown: ",skel," must be an absolute path") ;

    r = scan_mode(scandir.s, S_IFDIR) ;
    if (r < 0) log_die(LOG_EXIT_SYS,"scandir: ",scandir.s," exist with unkown mode") ;
    if (!r && !create && !remove) log_die(LOG_EXIT_USER,"scandir: ",scandir.s," doesn't exist") ;
    if (!r && create)
    {
        log_trace("sanitize ",live.s," ...") ;
        sanitize_live(live.s) ;
        log_info ("create scandir ",scandir.s," ...") ;
        create_scandir(live.s, scandir.s) ;
    }

    if (r && create)
    {
        log_info("scandir: ",scandir.s," already exist, keep it") ;
        goto end ;
    }

    r = svc_scandir_ok(scandir.s) ;
    if (r < 0) log_dieusys(LOG_EXIT_SYS, "check: ", scandir.s) ;
    if (r && remove) log_dieu(LOG_EXIT_USER,"remove: ",scandir.s,": is running")  ;
    if (remove)
    {
        /** /run/66/scandir/0 */
        auto_rm(scandir.s) ;

        if (CONTAINER) {
            /** /run/66/scandir/container */
            if (!stralloc_copy(&scandir,&live)) log_die_nomem("stralloc") ;
            if (!auto_stra(&scandir,SS_BOOT_CONTAINER_DIR,"/",OWNERSTR))
                log_die_nomem("stralloc") ;
            auto_rm(scandir.s) ;
        }

        /** run/66/state/uid */
        if (!stralloc_copy(&scandir,&live)) log_die_nomem("stralloc") ;
        r = set_livestate(&scandir,OWNER) ;
        if (!r) log_dieusys(LOG_EXIT_SYS,"set livestate directory") ;
        auto_rm(scandir.s) ;
    }

    end:
    stralloc_free(&scandir) ;
    stralloc_free(&live) ;

    return 0 ;
}
