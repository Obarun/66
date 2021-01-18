/*
 * 66-scanctl.c
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
#include <oblibs/obgetopt.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>
#include <skalibs/djbunix.h>
#include <skalibs/exec.h>

#include <s6/config.h>

#include <66/utils.h>

static char TMPENV[MAXENV + 1] ;

#define USAGE "66-scanctl [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -d notif ] [ -t rescan ] [ -e environment ] [ -o owner ] signal"

static inline void info_help (void)
{
    DEFAULT_MSG = 0 ;

    static char const *help =
"\n"
"options :\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -d: notify readiness on file descriptor\n"
"   -t: rescan scandir every milliseconds\n"
"   -e: directory environment\n"
"   -o: handles scandir of owner\n"
;

    log_info(USAGE,"\n",help) ;
}

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
        "stop",
        "reload",
        "quit",
        "nuke",
        "zombies",
        0
    } ;
  unsigned int i = lookup(signal_table, signal) ;
  if (!signal_table[i]) i = 6 ;
  return i ;
}

static int send_signal(char const *scandir, char const *signal)
{
    log_flow() ;

    unsigned int sig = 0 ;
    size_t siglen = strlen(signal) ;
    char csig[siglen + 1] ;
    sig = parse_signal(signal) ;
    if (sig < 6)
    {
        switch(sig)
        {
            /** start signal, should never happens */
            case 0:

                return 1 ;

            case 1:

                csig[0] = 't' ;
                csig[1] = 0 ;
                break ;

            case 2:

                csig[0] = 'h' ;
                csig[1] = 0 ;
                break ;

            case 3:

                csig[0] = 'q' ;
                csig[1] = 0 ;
                break ;

            case 4:

                csig[0] = 'n' ;
                csig[1] = 0 ;
                break ;

            case 5:

                csig[0] = 'z' ;
                csig[1] = 0 ;
                break ;

            default: break ;
        }
    }
    else {
        auto_strings(csig,signal) ;
    }

    log_info("Sending -",csig," signal to scandir: ",scandir,"...") ;
    return scandir_send_signal(scandir,csig) ;
}

static void scandir_up(char const *scandir, unsigned int timeout, unsigned int notif, char const *const *envp)
{
    int r ;
    r = scandir_ok(scandir) ;
    if (r < 0) log_dieusys(LOG_EXIT_SYS, "check: ", scandir) ;
    if (r)
    {
        log_trace("scandir: ",scandir," already running") ;
        return ;
    }

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

    //log_info("Starts scandir ",scandir," ...") ;
    xexec_ae(newup[0], newup, envp) ;
}

int main(int argc, char const *const *argv, char const *const *envp)
{
    int r ;
    uid_t owner = MYUID ;
    unsigned int timeout = 0, notif = 0, sig = 0 ;

    char const *newenv[MAXENV+1] ;
    char const *const *genv = 0 ;
    char const *signal ;

    stralloc scandir = STRALLOC_ZERO ;
    stralloc envdir = STRALLOC_ZERO ;

    log_color = &log_color_disable ;

    PROG = "66-scanctl" ;
    {
        subgetopt_t l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">hv:zl:o:d:t:e:", &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
            switch (opt)
            {
                case 'h' :

                    info_help() ;
                    return 0 ;

                case 'v' :

                    if (!uint0_scan(l.arg, &VERBOSITY))
                        log_usage(USAGE) ;

                    break ;

                case 'z' :

                    log_color = !isatty(1) ? &log_color_disable : &log_color_enable ;
                    break ;

                case 'l' :

                    if (!stralloc_cats(&scandir,l.arg) ||
                    !stralloc_0(&scandir))
                        log_die_nomem("stralloc") ;

                    break ;

                case 'o' :

                    if (MYUID)
                        log_die(LOG_EXIT_USER, "only root can use -o option") ;

                    if (!youruid(&owner,l.arg))
                        log_dieusys(LOG_EXIT_SYS,"get uid of: ",l.arg) ;

                    break ;

                case 'd' :

                    if (!uint0_scan(l.arg, &notif))
                        log_usage(USAGE) ;

                    if (notif < 3)
                        log_die(LOG_EXIT_USER, "notification fd must be 3 or more") ;

                    if (fcntl(notif, F_GETFD) < 0)
                        log_diesys(LOG_EXIT_USER, "invalid notification fd") ;

                    break ;

                case 't' :

                    if (!uint0_scan(l.arg, &timeout))
                        log_usage(USAGE) ;

                    break ;

                case 'e' :

                    if(!auto_stra(&envdir,l.arg))
                        log_die_nomem("stralloc") ;

                    break ;

                default :

                    log_usage(USAGE) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1) log_usage(USAGE) ;
    signal = argv[0] ;
    r = set_livedir(&scandir) ;
    if (r < 0) log_die(LOG_EXIT_USER,"live: ",scandir.s," must be an absolute path") ;
    if (!r) log_dieusys(LOG_EXIT_SYS,"set live directory") ;
    r = set_livescan(&scandir,owner) ;
    if (r < 0) log_die(LOG_EXIT_USER,"scandir: ", scandir.s, " must be an absolute path") ;
    if (!r) log_dieusys(LOG_EXIT_SYS,"set scandir directory") ;

    if (envdir.len)
    {
        if (envdir.s[0] != '/')
            log_die(LOG_EXIT_USER,"environment: ",envdir.s," must be an absolute path") ;

        r = environ_get_envfile_n_merge(envdir.s,envp,newenv,TMPENV) ;
        if (r <= 0 && r != -8){ environ_get_envfile_error(r,envdir.s) ; log_dieusys(LOG_EXIT_SYS,"build environment with: ",envdir.s) ; }
        genv = newenv ;
    }
    else genv = envp ;

    sig = parse_signal(signal) ;

    if (!sig) {

        char scan[scandir.len + 1] ;
        auto_strings(scan,scandir.s) ;

        stralloc_free(&envdir) ;
        stralloc_free(&scandir) ;

        scandir_up(scan,timeout,notif,genv) ;
        /** if already running, scandir_up() return */
        return 0 ;
    }

    r = scandir_ok(scandir.s) ;
    if (!r) log_diesys(LOG_EXIT_SYS,"scandir: ",scandir.s," is not running") ;
    else if (r < 0) log_dieusys(LOG_EXIT_SYS, "check: ", scandir.s) ;

    if (send_signal(scandir.s,signal) <= 0) goto err ;

    stralloc_free(&scandir) ;
    return 0 ;
    err:
        stralloc_free(&scandir) ;
        return 111 ;
}


