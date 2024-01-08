/*
 * ssexec_shutdown_wrapper.c
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
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>

#include <skalibs/sgetopt.h>
#include <skalibs/exec.h>

#include <66/ssexec.h>
#include <66/config.h>

int ssexec_shutdown_wrapper(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    uint8_t acl = 0, force = 0, nowall = 0 ;
    char what[strlen(argv[0])] ;
    char const *msg = 0 ;
    char const *time = 0 ;
    char *when = "now" ;
    char *command = 0 ;

    auto_strings(what, argv[0]) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = subgetopt_r(argc, argv, OPTS_SHUTDOWN_WRAPPER, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 'f' :

                    force = 1 ;
                    break ;

                case 'F' :

                    force = 2 ;
                    break ;

                case 'm' :

                    msg = l.arg ;
                    break ;

                case 'a' :

                    acl++ ;
                    break ;

                case 't' :

                    time = l.arg ;
                    break ;

                case 'W' :

                    nowall++ ;
                    break ;

                default:

                    log_usage(info->usage, "\n", info->help) ;

            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argv[0])
        when = (char *) argv[0] ;

    if (!strcmp(what, "poweroff")) {

        command = "-p" ;

    } else if (!strcmp(what, "reboot")) {

        command = "-r" ;

    } else if (!strcmp(what, "halt")) {

        command = "-h" ;

    } else
        log_die(LOG_EXIT_USER, "invalid shutdown command: ", what) ;

    if (force) {

        unsigned int nargc = 5 + (force > 1 ? 1 : 0) + (nowall ? 1 : 0) ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;
        newargv[m++] = SS_BINPREFIX "66-hpr" ;
        newargv[m++] = "-f" ;
        if (force > 1)
            newargv[m++] = "-n" ;
        if (nowall)
            newargv[m++] = "-W" ;
        newargv[m++] = command ;
        newargv[m++] = "-l" ;
        newargv[m++] = info->live.s ;
        newargv[m] = 0 ;

        xexec_ae(newargv[0], newargv, (char const *const *) environ) ;
    }

    unsigned int nargc = 5 + (time ? 2 : 0) + (acl ? 1 : 0) + (msg ? 1 : 0) ;
    char const *newargv[nargc] ;
    unsigned int m = 0 ;

    newargv[m++] = SS_BINPREFIX "66-shutdown" ;
    newargv[m++] = command ;
    if (time) {
        newargv[m++] = "-t" ;
        newargv[m++] = time ;
    }
    if (acl)
        newargv[m++] = "-a" ;
    newargv[m++] = "-l" ;
    newargv[m++] = info->live.s ;
    newargv[m++] = when ;
    if (msg)
        newargv[m++] = msg ;
    newargv[m] = 0 ;

    xexec_ae(newargv[0], newargv, (char const *const *) environ) ;
}
