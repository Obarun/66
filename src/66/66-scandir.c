/*
 * 66-scandir.c
 *
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>

#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/types.h>
#include <skalibs/env.h>
#include <skalibs/bytestr.h>//byte_count

#include <s6/config.h>
#include <execline/config.h>

#include <66/config.h>
#include <66/utils.h>
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

#define USAGE "66-scandir [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -b|B ] [ -c ] [ -L log_user ] [ -s skel ] [ -o owner ] create|remove"

static inline void info_help (void)
{
  static char const *help =
"66-scandir <options> create|remove\n"
"\n"
"options :\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -b: create scandir for a boot process\n"
"   -B: create scandir for a boot process inside a container\n"
"   -c: do not catch log\n"
"   -L: run catch-all logger as log_user user\n"
"   -s: skeleton directory\n"
"   -o: handles owner scandir\n"
;

    if (buffer_putsflush(buffer_1, help) < 0)
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}

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

void shebang(stralloc *sa,char const *opts)
{
    if (!auto_stra(sa, "#!" SS_EXECLINE_SHEBANGPREFIX "execlineb ",opts,"\n"))
        log_die_nomem("stralloc") ;

}

void append_shutdown(stralloc *sa,char const *live, char const *opts)
{
    if (!auto_stra(sa,SS_BINPREFIX "66-shutdown ",opts))
        log_die_nomem("stralloc") ;

    if (!CONTAINER)
        if (!auto_stra(sa," -a"))
            log_die_nomem("stralloc") ;

    if (!auto_stra(sa," -l ",live," -- now\n"))
        log_die_nomem("stralloc") ;

}

void write_shutdownd(char const *live, char const *scandir)
{
    log_flow() ;

    stralloc run = STRALLOC_ZERO ;
    size_t scandirlen = strlen(scandir) ;
    char shut[scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN + 5 + 1] ;

    auto_strings(shut,scandir,"/",SS_BOOT_SHUTDOWND) ;

    auto_check(shut,0755,0755,AUTO_CRTE_CHW_CHM) ;

    auto_strings(shut + scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN,"/fifo") ;

    auto_fifo(shut) ;

    shebang(&run,"-P") ;
    if (!auto_stra(&run,
        SS_BINPREFIX "66-shutdownd -l ",
        live," -s ",skel," -g 3000"))
            log_die_nomem("stralloc") ;

    if (CONTAINER)
        if (!auto_stra(&run," -B"))
            log_die_nomem("stralloc") ;

    if (!CATCH_LOG)
        if (!auto_stra(&run," -c"))
            log_die_nomem("stralloc") ;

    if (!auto_stra(&run,"\n"))
        log_die_nomem("stralloc") ;

    shut[scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN] = 0 ;

    auto_file(shut,"run",run.s,run.len) ;

    auto_strings(shut + scandirlen + 1 + SS_BOOT_SHUTDOWND_LEN,"/run") ;

    auto_chmod(shut,0755) ;

    stralloc_free(&run) ;
}

void write_bootlog(char const *live, char const *scandir)
{
    log_flow() ;

    int r ;
    uid_t uid ;
    gid_t gid ;
    size_t livelen = strlen(live) ;
    size_t scandirlen = strlen(scandir) ;
    size_t ownerlen = uid_fmt(OWNERSTR,OWNER) ;
    size_t loglen ;
    char path[livelen + 4 + ownerlen + 1] ;
    char logdir[scandirlen + SS_SCANDIR_LEN + SS_LOG_SUFFIX_LEN + 1 + 5 + 1] ;

    stralloc run = STRALLOC_ZERO ;
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

    /** make run file */
    shebang(&run,"-P") ;
    if (CONTAINER) {

        if (!auto_stra(&run,EXECLINE_BINPREFIX "fdmove -c 1 2\n"))
            log_die_nomem("stralloc") ;

    } else {

        if (!auto_stra(&run,
            EXECLINE_BINPREFIX "redirfd -w 1 /dev/null\n"))
                log_die_nomem("stralloc") ;
    }

    if (!auto_stra(&run,
            EXECLINE_BINPREFIX "redirfd -rnb 0 fifo\n" \
            S6_BINPREFIX "s6-setuidgid ",
            log_user,
            "\n" S6_BINPREFIX "s6-log -bpd3 -- 1"))
                log_die_nomem("stralloc") ;

    if (SS_LOGGER_TIMESTAMP < TIME_NONE)
        if (!auto_stra(&run,SS_LOGGER_TIMESTAMP == TIME_ISO ? " T " : " t "))
            log_die_nomem("stralloc") ;

    if (!auto_stra(&run,path,"\n"))
        log_die_nomem("stralloc") ;

    logdir[loglen] = 0 ;

    auto_file(logdir,"run",run.s,run.len) ;
    auto_file(logdir,"notification-fd","3\n",2) ;

    auto_strings(logdir + loglen,"/run") ;

    auto_chmod(logdir,0755) ;

    stralloc_free(&run) ;
}

void write_control(char const *scandir,char const *live, char const *filename, int file)
{
    log_flow() ;

    size_t scandirlen = strlen(scandir) ;
    size_t filen = strlen(filename) ;
    char mode[scandirlen + SS_SVSCAN_LOG_LEN + filen + 1] ;
    stralloc sa = STRALLOC_ZERO ;

    shebang(&sa,"-P") ;

    if (file == FINISH)
    {
        if (CONTAINER) {

            if (!auto_stra(&sa,
                SS_BINPREFIX "execl-envfile ",live, SS_BOOT_CONTAINER_DIR "/",OWNERSTR,"\n" \
                EXECLINE_BINPREFIX "fdclose 1\n" \
                EXECLINE_BINPREFIX "fdclose 2\n" \
                EXECLINE_BINPREFIX "wait { }\n" \
                EXECLINE_BINPREFIX "foreground {\n" \
                SS_BINPREFIX "66-hpr -f -n -${HALTCODE} -l ",live," \n}\n" \
                EXECLINE_BINPREFIX "exit ${EXITCODE}\n"))
                    log_die_nomem("stralloc") ;

        } else if (BOOT) {

            if (!auto_stra(&sa,
                EXECLINE_BINPREFIX "redirfd -w 2 /dev/console\n" \
                EXECLINE_BINPREFIX "fdmove -c 1 2\n" \
                EXECLINE_BINPREFIX "foreground { " SS_BINPREFIX "66-echo -- \"scandir ",
                scandir," exited. Rebooting.\" }\n" \
                SS_BINPREFIX "66-hpr -r -f -l ",
                live,"\n"))
                    log_die_nomem("stralloc") ;

        } else {

            if (!auto_stra(&sa,
                SS_BINPREFIX "66-echo -- \"scandir ",
                scandir," shutted down...\"\n"))
                    log_die_nomem("stralloc") ;
        }
        goto write ;
    }

    if (file == CRASH)
    {

        if (CONTAINER) {

            if (!auto_stra(&sa,
                EXECLINE_BINPREFIX "foreground {\n" \
                EXECLINE_BINPREFIX "fdmove -c 1 2\n" \
                SS_BINPREFIX "66-echo \"scandir crashed. Killing everythings and exiting.\"\n}\n" \
                EXECLINE_BINPREFIX "foreground {\n" \
                EXECLINE_BINPREFIX "66-nuke\n}\n" \
                EXECLINE_BINPREFIX "wait { }\n" \
                SS_BINPREFIX "66-hpr -f -n -p -l ",live,"\n"))
                    log_die_nomem("stralloc") ;
        }
        else {

            if (!auto_stra(&sa,
                EXECLINE_BINPREFIX "redirfd -w 2 /dev/console\n" \
                EXECLINE_BINPREFIX "fdmove -c 1 2\n" \
                EXECLINE_BINPREFIX "foreground { " SS_BINPREFIX "66-echo -- \"scandir ",
                scandir, " crashed."))
                    log_die_nomem("stralloc") ;

            if (BOOT) {

                if (!auto_stra(&sa,
                    " Rebooting.\" }\n" \
                    SS_BINPREFIX "66-hpr -r -f -l ",
                    live,"\n"))
                        log_die_nomem("stralloc") ;

            } else if (!auto_stra(&sa,"\" }\n"))
                log_die_nomem("stralloc") ;
        }

        goto write ;
    }
    if (!BOOT) {

        if (!auto_stra(&sa,
            EXECLINE_BINPREFIX "foreground { " SS_BINPREFIX "66-all -v3 -l ",
            live," unsupervise }\n"))
                log_die_nomem("stralloc") ;

    }

    switch(file)
    {
        case PWR:
        case USR1:

            if (BOOT)
                append_shutdown(&sa,live,"-p") ;

            break ;
        case USR2:

            if (BOOT)
                append_shutdown(&sa,live,"-h") ;

            break ;
        case TERM:
            if (!BOOT)
                if (!auto_stra(&sa, SS_BINPREFIX "66-scanctl -l ",live," stop\n"))
                    log_die_nomem("stralloc") ;

            break ;
        case QUIT:

            if (!BOOT)
                if (!auto_stra(&sa, SS_BINPREFIX "66-scanctl -l ",live," quit\n"))
                    log_die_nomem("stralloc") ;

            break ;

        case INT:

            if (BOOT)
                append_shutdown(&sa,live,"-r") ;

            break ;

        case WINCH:
            break ;

        default:
            break ;
    }

    write:
        auto_strings(mode,scandir,SS_SVSCAN_LOG) ;

        auto_file(mode,filename+1,sa.s,sa.len) ;

        auto_strings(mode + scandirlen + SS_SVSCAN_LOG_LEN, filename) ;

        auto_chmod(mode,0755) ;

    stralloc_free(&sa) ;
}

void create_scandir(char const *live, char const *scandir)
{
    log_flow() ;

    size_t scanlen = strlen(scandir) ;
    char tmp[scanlen + 11 + 1] ;

    /** run/66/scandir/<uid> */
    auto_strings(tmp,scandir) ;

    auto_check(tmp,0755,0,AUTO_CRTE_CHW) ;

    /** run/66/scandir/name/.svscan */
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
}

void sanitize_live(char const *live)
{
    log_flow() ;

    size_t livelen = strlen(live) ;
    char tmp[livelen + SS_BOOT_CONTAINER_DIR_LEN + 1] ;

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

    /** run/66/tree */
    auto_strings(tmp + livelen,SS_TREE) ;
    auto_check(tmp,0755,PERM1777,AUTO_CRTE_CHW_CHM) ;

    /** run/66/log */
    auto_strings(tmp + livelen,SS_LOG) ;
    auto_check(tmp,0755,PERM1777,AUTO_CRTE_CHW_CHM) ;

    /** /run/66/state*/
    auto_strings(tmp + livelen,SS_STATE + 1) ;
    auto_check(tmp,0755,PERM1777,AUTO_CRTE_CHW_CHM) ;
}

int main(int argc, char const *const *argv, char const *const *envp)
{
    int r ;
    unsigned int cmd, create, remove ;

    stralloc live = STRALLOC_ZERO ;
    stralloc scandir = STRALLOC_ZERO ;

    log_color = &log_color_disable ;

    cmd = create = remove = 0 ;

    OWNER = MYUID ;

    PROG = "66-scandir" ;
    {
        subgetopt_t l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">hzv:bl:s:o:L:cB", &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
            switch (opt)
            {
                case 'h' :

                    info_help() ;
                    return 0 ;

                case 'z' :

                    log_color = !isatty(1) ? &log_color_disable : &log_color_enable ;
                    break ;

                case 'v' :

                    if (!uint0_scan(l.arg, &VERBOSITY))
                        log_usage(USAGE) ;
                    break ;

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

                    log_usage(USAGE) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (!argc)
        log_usage(USAGE) ;

    cmd = parse_command(argv[0]) ;

    if (cmd == 3) {
        log_usage(USAGE) ;
    } else if (!cmd) {
        create = 1 ;
    } else  {
        remove = 1 ;
    }

    if (BOOT && OWNER)
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

    r = scandir_ok(scandir.s) ;
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

        /** /run/66/tree/uid */
        if (!stralloc_copy(&scandir,&live)) log_die_nomem("stralloc") ;
        r = set_livetree(&scandir,OWNER) ;
        if (!r) log_dieusys(LOG_EXIT_SYS,"set livetree directory") ;
        auto_rm(scandir.s) ;

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
