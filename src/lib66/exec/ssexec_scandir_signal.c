/*
 * ssexec_scandir_signal.c
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
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/exec.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>
#include <oblibs/files.h>

#include <66/ssexec.h>
#include <66/svc.h>
#include <66/utils.h>
#include <66/constants.h>

#include <s6/config.h>

extern opt_on_option_fn on_scandir_create ;

static inline unsigned int lookup (char const *const *table, char const *signal)
{
    log_flow() ;

    unsigned int i = 0 ;
    for (; table[i] ; i++) if (!strcmp(signal, table[i])) break ;
    return i ;
}

static inline unsigned int parse_signal (char const *signal)
{
    log_flow() ;

    static char const *const signal_table[] =
    {
        "start",
        "stop", // -t
        "reconfigure", // -h or -an
        "check", // -a
        "quit", // -q
        "abort", // -b
        "nuke", // -n
        "annihilate", // -N
        "zombies", // -z
        0
    } ;
    unsigned int i = lookup(signal_table, signal) ;
    if (!signal_table[i]) i = 9 ;
    return i ;
}

static void send_fdholder(char const *scandir, uint8_t down)
{
    char fdholder[strlen(scandir) + 1 + SS_FDHOLDER_LEN + 1] ;
    char oneshotd[strlen(scandir) + 1 + SS_FDHOLDER_LEN + 1] ;

    auto_strings(fdholder, scandir, "/", SS_FDHOLDER) ;
    auto_strings(oneshotd, scandir, "/", SS_ONESHOTD) ;

     if (!down) {

        svc_send_daemon(fdholder, "dx", EVENT_SUPERVISE_DOWN, 3000) ;
        svc_send_daemon(oneshotd, "dx", EVENT_SUPERVISE_DOWN, 3000) ;

    } else {

        svc_send_daemon(fdholder, "U", EVENT_READY, 3000) ;
        svc_send_daemon(oneshotd, "U", EVENT_READY, 3000) ;
    }
}

static int send_signal(char const *scandir, char const *signal)
{
    log_flow() ;

    unsigned int sig = 0 ;
    uint8_t down = 0 ;
    char csig[3] ;
    sig = parse_signal(signal) ;


    switch(sig) {

        /** start signal, should never happens */
        case 0:

            return 1 ;

        case 1: // stop

            csig[0] = 't' ;
            csig[1] = 0 ;
            break ;

        case 2: // reconfigure

            csig[0] = 'h' ;
            csig[1] = 0 ;
            down = 1 ;
            break ;

        case 3: // check

            csig[0] = 'a' ;
            csig[1] = 0 ;
            down = 1 ;
            break ;

        case 4: // quit

            csig[0] = 'q' ;
            csig[1] = 0 ;
            break ;

        case 5: // abort

            csig[0] = 'b' ;
            csig[1] = 0 ;
            break ;

        case 6: // nuke

            csig[0] = 'n' ;
            csig[1] = 0 ;
            break ;

        case 7: // annihilate

            csig[0] = 'N' ;
            csig[1] = 0 ;
            break ;

        case 8: // zombies

            csig[0] = 'z' ;
            csig[1] = 0 ;
            break ;

        default:
            log_die(LOG_EXIT_SYS, "unknown signal: ", signal) ;
    }

    send_fdholder(scandir, down) ;

    return svc_scandir_send(scandir,csig) ;
}

static void scandir_up(char const *scandir, unsigned int timeout, unsigned int notif, strbuf *env, ssexec_t *info)
{
    uid_t uid = getuid() ;
    gid_t gid = getgid() ;
    unsigned int no = notif ? 2 : 0 ;
    char const *newup[5 + no] ;
    unsigned int m = 0 ;
    char fmt[U32_FMT] ;
    fmt[u32_fmt(fmt, timeout)] = 0 ;
    char snotif[U32_FMT] ;
    snotif[u32_fmt(snotif, notif)] = 0 ;

    /* name-max is a 66 compile-time constant (SS_MAX_SERVICE_NAME), hardcoded in
     * 66-scandir: no -L to pass. Likewise services-max (SS_MAX_SERVICE) and the
     * console holder are not 66-scandir options anymore. */
    newup[m++] = SS_BINPREFIX "66-scandir" ;
    if (no) {
        newup[m++] = "-d" ;
        newup[m++] = snotif ;
    }
    newup[m++] = "-t" ;
    newup[m++] = fmt ;
    newup[m++] = scandir ;
    newup[m++] = 0 ;

    if (!uid && uid != info->owner) {
        /** -o <owner> was asked. Respect it
         * a start the s6-svscan process with the
         * good uid and gid */
        if (!yourgid(&gid, info->owner))
            log_dieusys(LOG_EXIT_SYS, "get gid of: ", info->ownerstr) ;
        if (setgid(gid) < 0)
            log_dieusys(LOG_EXIT_SYS, "setgid for: ", info->ownerstr) ;
        if (setuid(info->owner) < 0)
            log_dieusys(LOG_EXIT_SYS, "setuid for: ", info->ownerstr) ;

    }

    ssexec_free(info) ;
    // it merge char const *const *environ with en->s where env->s take precedence
    exec_path_merge_die(newup[0], newup, (char const *const *)environ, env->s, env->len) ;
}

static char const *scandir_signal_name = 0 ;
static unsigned int signal_timeout = 0 ;
static unsigned int signal_notif = 0 ;
static unsigned int signal_container = 0 ;
static unsigned int signal_boot = 0 ;
static char const *signal_userenv = 0 ;

void scandir_signal_set_name(char const *name)
{
    scandir_signal_name = name ;
}

int on_scandir_signal(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {

        case 'd' :

            if (!u32_scan_strict(arg, &signal_notif))
                log_die(LOG_EXIT_USER, "invalid notification fd: ", arg) ;

            if (signal_notif < 3)
                log_die(LOG_EXIT_USER, "notification fd must be 3 or more") ;

            if (fcntl(signal_notif, F_GETFD) < 0)
                log_diesys(LOG_EXIT_USER, "invalid notification fd") ;

            break ;

        case 's' :

            if (!u32_scan_strict(arg, &signal_timeout))
                log_die(LOG_EXIT_USER, "invalid rescan value: ", arg) ;

            break ;

        case 'e' :

            signal_userenv = arg ;

            break ;

        case 'b' :

            signal_boot = 1 ;

            break ;

        case 'B' :

            signal_container = 1 ;

            break ;
    }

    return 0 ;
}

int ssexec_scandir_signal(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    (void)argc ;
    (void)argv ;

    ssexec_t *info = data ;

    int r ;

    unsigned int timeout = signal_timeout, notif = signal_notif, sig = 0 ;
    unsigned int container = signal_container, boot = signal_boot ;
    char const *signal = scandir_signal_name ;
    char const *userenv = signal_userenv ;

    /* the locals now hold the whole option state: reset the statics so a nested
     * re-dispatch of a scandir signal starts clean. */
    signal_timeout = 0 ;
    signal_notif = 0 ;
    signal_container = 0 ;
    signal_boot = 0 ;
    scandir_signal_name = 0 ;
    signal_userenv = 0 ;

    _cleanup_strbuf_ strbuf env = STRBUF_ZERO ;

    if (!environ_merge_dir(&env, info->environment.s))
        log_dieusys(LOG_EXIT_SYS, "merge environment directory: ", info->environment.s) ;

    if (userenv) {

        if (userenv[0] != '/')
            log_dieusys(LOG_EXIT_USER,"environment directory must be an absolute path: ", userenv) ;

        if (!environ_merge_dir(&env, userenv))
           log_dieu(LOG_EXIT_SYS,"merge environment directory: ", userenv) ;

    }

    sig = parse_signal(signal) ;

    if (!sig) {

        char scandir[info->scandir.len + 1] ;
        auto_strings(scandir, info->scandir.s) ;

        int r ;
        r = scan_mode(scandir, S_IFDIR) ;
        if (r < 0)
           log_die(LOG_EXIT_SYS, scandir, " conflicted format") ;

        if (!r) {

            char const *newargv[] = { "create", 0 } ;

            if (container)
                on_scandir_create('B', 0, info) ;

            if (boot)
                on_scandir_create('b', 0, info) ;

            if (ssexec_scandir_create(1, newargv, info))
                log_dieu(LOG_EXIT_SYS, "create scandir: ", scandir) ;
        }

        r = svc_scandir_ok(scandir) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "check: ", scandir) ;

        if (r) {
            log_trace("scandir: ", scandir, " already running") ;
            return 0 ;
        }

        scandir_up(scandir, timeout, notif, &env, info) ;
    }

    r = svc_scandir_ok(info->scandir.s) ;
    if (r <= 0) {
       /** TODO:
        *
        * We have a race condition here with nested scandir.
        * s6-supervise may have already sent a down signal to the
        * scandir.
        * When the stop script of nested scandir is executed,
        * the scandir is already down and crash.
        *
        * For now, be sure to also remove the fdholder and oneshotd of
        * the nested scandir to avoid issue at next start of the scandir.
        */
        send_fdholder(info->scandir.s, 0) ;

        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "check: ", info->scandir.s) ;
        else
            log_diesys(LOG_EXIT_SYS, "scandir: ", info->scandir.s, " is not running") ;
    }

    if (send_signal(info->scandir.s, signal) <= 0)
        log_dieu(LOG_EXIT_SYS, "send signal to scandir: ", info->scandir.s) ;

    return 0 ;
}

