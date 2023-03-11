/*
 * ssexec_scandir_signal.c
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
#include <fcntl.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>
#include <oblibs/types.h>

#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/env.h>
#include <skalibs/sgetopt.h>
#include <skalibs/stralloc.h>
#include <skalibs/exec.h>

#include <66/ssexec.h>
#include <66/svc.h>

#include <s6/config.h>

static char TMPENV[MAXENV + 1] ;

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
        "rescan", // -a
        "quit", // -q
        "halt", // -qb
        "abort", // -b
        "nuke", // -n
        "annihilate", // -N
        "zombies", // -z
        0
    } ;
    unsigned int i = lookup(signal_table, signal) ;
    if (!signal_table[i]) i = 10 ;
    return i ;
}

static int send_signal(char const *scandir, char const *signal)
{
    log_flow() ;

    unsigned int sig = 0 ;
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
            break ;

        case 3: // rescan

            csig[0] = 'a' ;
            csig[1] = 0 ;
            break ;

        case 4: // quit

            csig[0] = 'q' ;
            csig[1] = 0 ;
            break ;

        case 5: // halt

            csig[0] = 'q' ;
            csig[1] = 'b' ;
            csig[2] = 0 ;
            break ;

        case 6: // abort

            csig[0] = 'b' ;
            csig[1] = 0 ;
            break ;

        case 7: // nuke

            csig[0] = 'n' ;
            csig[1] = 0 ;
            break ;

        case 8: // annihilate

            csig[0] = 'N' ;
            csig[1] = 0 ;
            break ;

        case 9: // zombies

            csig[0] = 'z' ;
            csig[1] = 0 ;
            break ;

        default:
            log_die(LOG_EXIT_SYS, "unknown signal: ", signal) ;
    }

    return svc_scandir_send(scandir,csig) ;
}

static void scandir_up(char const *scandir, unsigned int timeout, unsigned int notif, char const *const *envp)
{
    unsigned int no = notif ? 2 : 0 ;
    char const *newup[6 + no] ;
    unsigned int m = 0 ;
    char fmt[UINT_FMT] ;
    fmt[uint_fmt(fmt, timeout)] = 0 ;
    char snotif[UINT_FMT] ;
    snotif[uint_fmt(snotif, notif)] = 0 ;

    newup[m++] = S6_BINPREFIX "s6-svscan" ;
    if (no) {
        newup[m++] = "-d" ;
        newup[m++] = snotif ;
    }
    newup[m++] = "-t" ;
    newup[m++] = fmt ;
    newup[m++] = "--" ;
    newup[m++] = scandir ;
    newup[m++] = 0 ;

    xexec_ae(newup[0], newup, envp) ;
}

int ssexec_scandir_signal(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r ;

    unsigned int timeout = 0, notif = 0, sig = 0, container = 0 ;

    char const *newenv[MAXENV+1] ;
    char const *const *genv = 0 ;
    char const *const *genvp = (char const *const *)environ ;
    char const *signal ;

    stralloc envdir = STRALLOC_ZERO ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = subgetopt_r(argc,argv, OPTS_SCANCTL, &l) ;
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

                    if(!auto_stra(&envdir,l.arg))
                        log_die_nomem("stralloc") ;

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

    if (envdir.len) {

        stralloc modifs = STRALLOC_ZERO ;
        if (envdir.s[0] != '/')
            log_die(LOG_EXIT_USER,"environment: ",envdir.s," must be an absolute path") ;

        if (!environ_clean_envfile_unexport(&modifs,envdir.s))
            log_dieu(LOG_EXIT_SYS,"clean environment file of: ",envdir.s) ;

        size_t envlen = env_len(genvp) ;
        size_t n = env_len(genvp) + 1 + byte_count(modifs.s,modifs.len,'\0') ;
        size_t mlen = modifs.len ;
        memcpy(TMPENV,modifs.s,mlen) ;
        TMPENV[mlen] = 0 ;

        if (!env_merge(newenv, n, genvp, envlen, TMPENV, mlen))
            log_dieu(LOG_EXIT_SYS,"merge environment") ;

        stralloc_free(&modifs) ;

        genv = newenv ;
    }
    else genv = genvp ;

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
            int nargc = 3 + (container ? 1 : 0) ;
            char const *newargv[nargc] ;

            newargv[m++] = "create" ;

            if (container)
                newargv[m++] = "-B" ;

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

        stralloc_free(&envdir) ;
        ssexec_free(info) ;

        scandir_up(scandir, timeout, notif, genv) ;
        return 0 ;
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


