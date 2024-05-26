/*
 * ssexec_scandir_signal.c
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

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/sastr.h>

#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/env.h>
#include <skalibs/sgetopt.h>
#include <skalibs/stralloc.h>
#include <skalibs/exec.h>

#include <66/ssexec.h>
#include <66/svc.h>
#include <66/utils.h>
#include <66/constants.h>

#include <s6/config.h>

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
        "reload", // -h or -an
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

static int send_signal(char const *scandir, char const *signal)
{
    log_flow() ;

    unsigned int sig = 0 ;
    uint8_t down = 0 ;
    char csig[3] ;
    sig = parse_signal(signal) ;
    char fdholder[strlen(scandir) + 1 + SS_FDHOLDER_LEN + 1] ;
    char oneshotd[strlen(scandir) + 1 + SS_FDHOLDER_LEN + 1] ;

    auto_strings(fdholder, scandir, "/", SS_FDHOLDER) ;
    auto_strings(oneshotd, scandir, "/", SS_ONESHOTD) ;

    switch(sig) {

        /** start signal, should never happens */
        case 0:

            return 1 ;

        case 1: // stop

            csig[0] = 't' ;
            csig[1] = 0 ;
            break ;

        case 2: // reload

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

    if (!down) {

        svc_send_fdholder(fdholder, "dx") ;
        svc_send_fdholder(oneshotd, "dx") ;

    } else {

        svc_send_fdholder(fdholder, "U") ;
        svc_send_fdholder(oneshotd, "U") ;
    }

    return svc_scandir_send(scandir,csig) ;
}

static void scandir_up(char const *scandir, unsigned int timeout, unsigned int notif, char const *const *envp, ssexec_t *info)
{
    uid_t uid = getuid() ;
    gid_t gid = getgid() ;
    unsigned int no = notif ? 2 : 0 ;
    char const *newup[7 + no] ;
    unsigned int m = 0 ;
    char fmt[UINT_FMT] ;
    fmt[uint_fmt(fmt, timeout)] = 0 ;
    char snotif[UINT_FMT] ;
    snotif[uint_fmt(snotif, notif)] = 0 ;
    char maxservice[UINT_FMT] ;
    maxservice[uint_fmt(maxservice, SS_MAX_SERVICE_NAME)] = 0 ;

    newup[m++] = S6_BINPREFIX "s6-svscan" ;
    if (no) {
        newup[m++] = "-d" ;
        newup[m++] = snotif ;
    }
    newup[m++] = "-t" ;
    newup[m++] = fmt ;
    newup[m++] = "-L" ;
    newup[m++] = maxservice ;
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
    xmexec_m(newup, env->s, env->len) ;
}

int ssexec_scandir_signal(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r ;

    unsigned int timeout = 0, notif = 0, sig = 0, container = 0, boot = 0 ;
    char const *signal ;
    char const *userenv = 0 ;
    _alloc_sa_(env) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = subgetopt_r(argc,argv, OPTS_SCANDIR_SIGNAL, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'd' :

                    if (!uint0_scan(l.arg, &notif))
                        log_usage(info->usage, "\n", info->help) ;

                    if (notif < 3)
                        log_die(LOG_EXIT_USER, "notification fd must be 3 or more") ;

                    if (fcntl(notif, F_GETFD) < 0)
                        log_diesys(LOG_EXIT_USER, "invalid notification fd") ;

                    break ;

                case 's' :

                    if (!uint0_scan(l.arg, &timeout))
                        log_usage(info->usage, "\n", info->help) ;

                    break ;

                case 'e' :

                    userenv = l.arg ;

                    break ;

                case 'b' :

                    boot = 1 ;

                    break ;

                case 'B' :

                    container = 1 ;

                    break ;

                default :

                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    signal = argv[0] ;

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

            unsigned int m = 0 ;
            int nargc = 3 + (container ? 1 : 0) + (boot ? 1 : 0) ;
            char const *newargv[nargc] ;

            newargv[m++] = "create" ;

            if (container)
                newargv[m++] = "-B" ;

            if (boot)
                newargv[m++] = "-b" ;

            newargv[m++] = "create" ;
            newargv[m] = 0 ;

            if (ssexec_scandir_create(m, newargv, info))
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
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "check: ", info->scandir.s) ;
    else if (!r)
       log_diesys(LOG_EXIT_SYS, "scandir: ", info->scandir.s, " is not running") ;

    if (send_signal(info->scandir.s, signal) <= 0)
        log_dieu(LOG_EXIT_SYS, "send signal to scandir: ", info->scandir.s) ;

    return 0 ;
}


