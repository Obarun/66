/*
 * ssexec_scandir_create.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
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
#include <fcntl.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/io.h>
#include <oblibs/fd.h>
#include <oblibs/strbuf.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>
#include <oblibs/files.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/config.h>
#include <66/ssexec.h>
#include <66/enum_parser.h>
#include <66/resolve.h>
#include <66/service.h>

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


inline static void auto_chown(char const *str)
{
    log_flow() ;

    log_trace("chown directory: ",str," to: ",OWNERSTR,":",GIDSTR) ;
    if (chown(str,OWNER,GIDOWNER) < 0)
        log_dieusys(LOG_EXIT_SYS,"chown: ",str) ;
}

inline static void auto_dir(char const *str,mode_t mode)
{
    log_flow() ;

    log_trace("create directory: ",str) ;
    if (!dir_create_parent(str,mode))
        log_dieusys(LOG_EXIT_SYS,"create directory: ",str) ;

    if (!OWNER) {
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
    }
    auto_chown(str) ;
}

inline static void auto_chmod(char const *str,mode_t mode)
{
    log_flow() ;

    if (chmod(str,mode) < 0)
        log_dieusys(LOG_EXIT_SYS,"chmod: ",str) ;
}

inline static void auto_file(char const *dst,char const *file,char const *contents,size_t conlen)
{
    log_flow() ;

    char f[strlen(dst) + 1 + strlen(file) + 1] ;
    auto_strings(f, dst, "/", file) ;

    log_trace("write file: ", f) ;
    if (!file_write_at(dst,file,contents,conlen))
        log_dieusys(LOG_EXIT_SYS,"write file: ",dst,"/",file) ;

    auto_chown(f) ;
}

inline static void auto_check(char const *str,mode_t type,mode_t perm,int what)
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

inline static void write_min_resolve(char const *dir, char const *name, uint32_t notify)
{
    log_flow() ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_service_addon_execute_t ex = RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    resolve_wrapper_t_ref wex = resolve_set_struct(DATA_SERVICE_EXECUTE, &ex) ;
    resolve_init(wres) ;
    resolve_init(wex) ;

    res.name = resolve_add_string(wres, name) ;
    res.type = E_PARSER_TYPE_CLASSIC ;
    res.has_execute = 1 ;
    ex.notify = notify ;

    /* resolve_write_cdb only copies the cdb into <dir>/.resolve/ -- it does not
     * create that directory, so make it first. */
    char rdir[strlen(dir) + SS_RESOLVE_LEN + 1] ;
    auto_strings(rdir, dir, SS_RESOLVE) ;
    auto_dir(rdir, 0755) ;

    log_trace("write resolve of: ", name, " at: ", dir) ;
    if (!resolve_write_at(wres, dir, name))
        log_dieusys(LOG_EXIT_SYS, "write resolve of: ", name) ;

    {
        char aname[strlen(name) + SS_ADDON_EXECUTE_SUFFIX_LEN + 1] ;
        auto_strings(aname, name, SS_ADDON_EXECUTE_SUFFIX) ;
        if (!resolve_write_at(wex, dir, aname))
            log_dieusys(LOG_EXIT_SYS, "write execute addon of: ", name) ;
    }

    resolve_free(wres) ;
    resolve_free(wex) ;
}

inline static void auto_fifo(char const *str)
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

inline static void auto_rm(char const *str)
{
    log_flow() ;

    int r ;
    r = scan_mode(str,S_IFDIR) ;
    if (r > 0)
    {
        log_info("Removing: ",str,"...") ;
        if (!dir_destroy(str)) log_dieusys(LOG_EXIT_SYS,"remove: ",str) ;
    }
}

inline static void log_perm(char const *str,uid_t *uid,gid_t *gid)
{
    log_flow() ;

    if (!youruid(uid,str)) log_dieusys(LOG_EXIT_SYS,"set uid of: ",str) ;
    if (!yourgid(gid,*uid)) log_dieusys(LOG_EXIT_SYS,"set gid of: ",str) ;
}

inline static void shebang(strbuf *b, char const *opts)
{
    log_flow() ;

    if (!auto_strbuf(b, "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb ", opts, "\n"))
        log_die_nomem("strbuf") ;
}

void append_shutdown(strbuf *b, char const *live, char const *opts)
{
    log_flow() ;

    if (!auto_strbuf(b,SS_BINPREFIX "66-shutdown ",opts))
        log_die_nomem("strbuf") ;

    if (!CONTAINER)
        if (!auto_strbuf(b," -a"))
            log_die_nomem("strbuf") ;

    if (!auto_strbuf(b," -l ",live," -- now\n"))
        log_die_nomem("strbuf") ;

}

static void write_strbuf(strbuf *b, char const *dst, char const *file)
{
    log_flow() ;

    char w[strlen(dst) + 1 + strlen(file) + 1] ;
    auto_strings(w, dst, "/", file) ;

    int fd = io_open_mode(w, O_WRONLY | O_NONBLOCK | O_TRUNC | O_CREAT, 0666) ;
    if (fd < 0 || !io_set_block(fd))
        log_die(LOG_EXIT_SYS, "open: ", w) ;

    if (io_allwrite(fd, b->s, b->len) != b->len)
        log_dieusys(LOG_EXIT_SYS, "write to: ", dst, "/", file) ;

    close_fd(fd) ;
    strbuf_free(b) ;
}

void write_shutdownd(char const *live, char const *scandir)
{
    log_flow() ;

    strbuf b = STRBUF_ZERO ;
    size_t scandirlen = strlen(scandir) ;
    char shut[scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN + 5 + 1] ;

    auto_strings(shut,scandir,"/",SS_BOOT_SHUTDOWND) ;

    auto_check(shut,0755,0755,AUTO_CRTE_CHW_CHM) ;

    auto_strings(shut + scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN,"/fifo") ;

    auto_fifo(shut) ;

    shut[scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN] = 0 ;

    write_min_resolve(shut, SS_BOOT_SHUTDOWND, 0) ;

    shebang(&b, "-P") ;
    if (!auto_strbuf(&b,
        SS_LIBEXECPREFIX "66-shutdownd -l ",
        live," -s ",skel," -g 3000"))
            log_die_nomem("strbuf") ;

    if (CONTAINER)
        if (!auto_strbuf(&b," -B"))
            log_die_nomem("strbuf") ;

    if (!CATCH_LOG)
        if (!auto_strbuf(&b," -c"))
            log_die_nomem("strbuf") ;

    if (!auto_strbuf(&b,"\n"))
        log_die_nomem("strbuf") ;

    write_strbuf(&b, shut, "run") ;

    auto_strings(shut + scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN,"/run") ;

    auto_chmod(shut,0755) ;
}

void write_bootlog(char const *live, char const *scandir)
{
    log_flow() ;

    int r ;
    uid_t uid = -1 ;
    gid_t gid = -1 ;
    size_t livelen = strlen(live), scandirlen = strlen(scandir), ownerlen = uid_format(OWNERSTR,OWNER), loglen = 0 ;
    strbuf b = STRBUF_ZERO ;
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

    write_min_resolve(logdir, SS_SCANDIR SS_LOG_SUFFIX, 3) ;

    /** make run file */
    shebang(&b,"-P") ;
    if (CONTAINER) {

        if (!auto_strbuf(&b,SS_EXECLINE_BINPREFIX "fdmove -c 1 2\n"))
            log_die_nomem("strbuf") ;

    } else {

        if (!auto_strbuf(&b,
            SS_EXECLINE_BINPREFIX "redirfd -w 1 /dev/null\n"))
                log_die_nomem("strbuf") ;
    }

    if (!auto_strbuf(&b,
            SS_EXECLINE_BINPREFIX "redirfd -rnb 0 fifo\n" \
            SS_BINPREFIX "execl-runas ",
            log_user,
            "\n" SS_BINPREFIX "66-log -bpd3 -- 1"))
                log_die_nomem("strbuf") ;

    if (SS_LOGGER_TIMESTAMP < E_PARSER_TIME_NONE)
        if (!auto_strbuf(&b, SS_LOGGER_TIMESTAMP == E_PARSER_TIME_ISO ? " T " : " t "))
            log_die_nomem("strbuf") ;

    if (!auto_strbuf(&b,path,"\n"))
        log_die_nomem("strbuf") ;

    write_strbuf(&b, logdir, "run") ;

    auto_file(logdir, SS_NOTIFICATION, "3\n",2) ;

    auto_strings(logdir + loglen,"/run") ;

    auto_chmod(logdir,0755) ;
    auto_chown(logdir) ;
}

void write_control(char const *scandir,char const *live, char const *filename, int file)
{
    log_flow() ;

    strbuf b = STRBUF_ZERO ;
    size_t scandirlen = strlen(scandir), filen = strlen(filename) ;
    char mode[scandirlen + SS_SVSCAN_LEN + filen + 1] ;

    auto_strings(mode,scandir,SS_SVSCAN) ;

    shebang(&b,"-P") ;

    if (file == FINISH)
    {
        if (CONTAINER) {

            if (!auto_strbuf(&b,
                SS_BINPREFIX "execl-envfile ",live, SS_BOOT_CONTAINER_DIR "/",OWNERSTR,"\n" \
                SS_EXECLINE_BINPREFIX "fdclose 1\n" \
                SS_EXECLINE_BINPREFIX "fdclose 2\n" \
                SS_EXECLINE_BINPREFIX "wait { }\n" \
                SS_EXECLINE_BINPREFIX "foreground {\n" \
                SS_BINPREFIX "66-hpr -f -n -${HALTCODE} -l ",live," \n}\n" \
                SS_EXECLINE_BINPREFIX "exit ${EXITCODE}\n"))
                    log_die_nomem("strbuf") ;

        } else if (BOOT) {

            if (!auto_strbuf(&b,
                SS_EXECLINE_BINPREFIX "redirfd -w 2 /dev/console\n" \
                SS_EXECLINE_BINPREFIX "fdmove -c 1 2\n" \
                SS_EXECLINE_BINPREFIX "foreground { " SS_BINPREFIX "66-echo -- \"scandir ",
                scandir," exited. Rebooting.\" }\n" \
                SS_BINPREFIX "66-hpr -r -f -l ",
                live,"\n"))
                    log_die_nomem("strbuf") ;

        } else {

            if (!auto_strbuf(&b,
                SS_BINPREFIX "66-echo -- \"scandir ",
                scandir," stopped...\"\n"))
                    log_die_nomem("strbuf") ;
        }
        goto write ;
    }

    if (file == CRASH)
    {

        if (CONTAINER) {

            if (!auto_strbuf(&b,
                SS_EXECLINE_BINPREFIX "foreground {\n" \
                SS_EXECLINE_BINPREFIX "fdmove -c 1 2\n" \
                SS_BINPREFIX "66-echo \"scandir crashed. Killing everythings and exiting.\"\n}\n" \
                SS_EXECLINE_BINPREFIX "foreground {\n" \
                SS_BINPREFIX "66-nuke\n}\n" \
                SS_EXECLINE_BINPREFIX "wait { }\n" \
                SS_BINPREFIX "66-hpr -f -n -p -l ",live,"\n"))
                    log_die_nomem("strbuf") ;
        }
        else {

            if (!auto_strbuf(&b,
                SS_EXECLINE_BINPREFIX "redirfd -w 2 /dev/console\n" \
                SS_EXECLINE_BINPREFIX "fdmove -c 1 2\n" \
                SS_EXECLINE_BINPREFIX "foreground { " SS_BINPREFIX "66-echo -- \"scandir ",
                scandir, " crashed."))
                    log_die_nomem("strbuf") ;

            if (BOOT) {

                if (!auto_strbuf(&b,
                    " Rebooting.\" }\n" \
                    SS_BINPREFIX "66-hpr -r -f -l ",
                    live,"\n"))
                        log_die_nomem("strbuf") ;

            } else if (!auto_strbuf(&b,"\" }\n"))
                log_die_nomem("strbuf") ;
        }

        goto write ;
    }
    if (!BOOT) {

        if (!auto_strbuf(&b,
            SS_EXECLINE_BINPREFIX "foreground { " SS_BINPREFIX "66 -v3 -l ",
            live," tree stop }\n"))
                log_die_nomem("strbuf") ;

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

        write_strbuf(&b, mode, filename + 1) ;

        auto_strings(mode + scandirlen + SS_SVSCAN_LEN, filename) ;

        auto_chmod(mode,0755) ;
        auto_chown(mode) ;
}

static void create_service_oneshot(char const *scandir, ssexec_t *info)
{
    log_flow() ;

    (void)info ;

    size_t scandirlen = strlen(scandir) ;
    size_t fdlen = scandirlen + 1 + SS_ONESHOTD_LEN ;

    char dst[fdlen + 16] ;

    /* 66-oneshotd binds its own socket and self-protects via SO_PEERCRED:
     * just a service dir, a readiness fd (>= 3) and a one-line run. */
    auto_strings(dst, scandir, "/", SS_ONESHOTD) ;
    auto_dir(dst, 0755) ;
    auto_chown(dst) ;

    write_min_resolve(dst, SS_ONESHOTD, 3) ;

    auto_file(dst, SS_NOTIFICATION, "3\n", 2) ;

    size_t runlen = strlen(SS_EXECLINE_SHEBANGPREFIX) + strlen(SS_LIBEXECPREFIX) + 64 + 1 ;

    char run[runlen] ;
    auto_strings(run, "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n", \
                "fdmove -c 2 1\n", \
                SS_LIBEXECPREFIX "66-oneshotd -d 3 -- s\n") ;

    auto_strings(dst, scandir, "/", SS_ONESHOTD, "/run") ;

    // -1 file_write do not accept closed string
    if (!file_write(dst, run, strlen(run) - 1))
        log_dieusys(LOG_EXIT_SYS, "write: ", dst) ;

    auto_chmod(dst, 0755) ;
    auto_chown(dst) ;
}

static void create_service_fdholder(char const *scandir, ssexec_t *info)
{
    log_flow() ;

    (void)info ;

    size_t scandirlen = strlen(scandir) ;
    size_t fdlen = scandirlen + 1 + SS_FDHOLDER_LEN ;

    char dst[fdlen + 16] ;

    /* 66-fdholderd binds its own socket and self-protects:
     * just a service dir, a readiness fd (>= 3) and a one-line run. */
    auto_strings(dst, scandir, "/", SS_FDHOLDER) ;
    auto_dir(dst, 0755) ;
    auto_chown(dst) ;

    write_min_resolve(dst, SS_FDHOLDER, 3) ;

    auto_file(dst, SS_NOTIFICATION, "3\n", 2) ;

    size_t runlen = strlen(SS_EXECLINE_SHEBANGPREFIX) + strlen(SS_LIBEXECPREFIX) + 64 + 1 ;

    char run[runlen] ;
    auto_strings(run, "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb -P\n", \
                "fdmove -c 2 1\n", \
                SS_LIBEXECPREFIX "66-fdholderd -d 3 -- s\n") ;

    auto_strings(dst, scandir, "/", SS_FDHOLDER, "/run") ;

    // -1 file_write do not accept closed string
    if (!file_write(dst, run, strlen(run) - 1))
        log_dieusys(LOG_EXIT_SYS, "write: ", dst) ;

    auto_chmod(dst, 0755) ;
    auto_chown(dst) ;
}

static void create_scandir(char const *live, char const *scandir, ssexec_t *info)
{
    log_flow() ;

    size_t scanlen = strlen(scandir) ;
    char tmp[scanlen + SS_SVSCAN_LEN + 1] ;

    /** run/66/scandir/<uid> */
    auto_strings(tmp,scandir) ;

    auto_check(tmp,0755,0,AUTO_CRTE_CHW) ;

    /** run/66/scandir/uid/.66-scandir */
    auto_strings(tmp + scanlen, SS_SVSCAN) ;

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
        /** run/66/container/UID */
        auto_strings(tmp + livelen,SS_BOOT_CONTAINER_DIR,"/",OWNERSTR) ;
        auto_check(tmp,0755,PERM1777,AUTO_CRTE_CHW_CHM) ;
        auto_file(tmp,SS_BOOT_CONTAINER_HALTFILE,"EXITCODE=0\nHALTCODE=p\n",22) ;
    }

    /** run/66/log */
    auto_strings(tmp + livelen, SS_LOG) ;
    auto_check(tmp,0755,PERM1777,AUTO_CRTE_CHW_CHM) ;

    /** /run/66/state*/
    auto_strings(tmp + livelen,SS_STATE + 1) ;
    auto_check(tmp,0755,PERM1777,AUTO_CRTE_CHW_CHM) ;
}

int on_scandir_create(int id, char const *arg, void *data)
{
    ssexec_t *info = data ;

    switch (id) {

        case 'b' :

            if (info->owner)
                log_die(LOG_EXIT_USER, "-b options can be set only with root") ;

            BOOT = 1 ;
            break ;

        case 'B' :

            if (info->owner)
                log_die(LOG_EXIT_USER, "-B options can be set only with root") ;

            CONTAINER = 1 ;
            BOOT = 1 ;
            break ;

        case 's' :

            skel = arg ;
            break ;

        case 'c' :

            CATCH_LOG = 0 ;
            break ;

        case 'L' :

            log_user = arg ;
            break ;
    }

    return 0 ;
}


int ssexec_scandir_create(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    (void)argc ;
    (void)argv ;

    ssexec_t *info = data ;

    int r ;

    OWNER = info->owner ;
    OWNERSTR = info->ownerstr ;

    if (BOOT && OWNER && !CONTAINER)
        log_die(LOG_EXIT_USER, "-b options can be set only with root") ;

    if (!yourgid(&GIDOWNER,OWNER))
        log_dieusys(LOG_EXIT_SYS, "set gid of: ", OWNERSTR) ;

    GIDSTR[gid_format(GIDSTR,GIDOWNER)] = 0 ;

    if (BOOT && skel[0] != '/')
        log_die(LOG_EXIT_USER, "rc.shutdown: ", skel, " must be an absolute path") ;

    r = scan_mode(info->scandir.s, S_IFDIR) ;
    if (r < 0) log_die(LOG_EXIT_SYS, "scandir: ", info->scandir.s, " exist with unkown mode") ;
    if (!r) {

        log_trace("sanitize ", info->live.s, " ...") ;
        sanitize_live(info->live.s) ;
        log_info ("Create scandir ", info->scandir.s, " ...") ;
        create_scandir(info->live.s, info->scandir.s, info) ;

    } else
        log_info("Scandir: ", info->scandir.s, " already exist, keeping it") ;

    return 0 ;
}
