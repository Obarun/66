/*
 * ssexec_scandir_wrapper.c
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

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/sgetopt.h>

#include <66/ssexec.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/utils.h>

int ssexec_scandir_wrapper(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    if (!argv[1]) {
        PROG = "scandir" ;
        log_usage(usage_scandir_wrapper, "\n", help_scandir_wrapper) ;
    }

    int r, n = 0, i = 0 ;
    uid_t owner = -1 ;
    ssexec_func_t_ref func = 0 ;
    char const *nargv[argc + 1] ;

    if (!strcmp(argv[1], "create")) {

        nargv[n++] = PROG ;
        info->prog = PROG ;
        info->help = help_scandir_create ;
        info->usage = usage_scandir_create ;
        func = &ssexec_scandir_create ;

    } else if (!strcmp(argv[1], "remove")) {

        nargv[n++] = PROG ;
        info->prog = PROG ;
        info->help = help_scandir_remove ;
        info->usage = usage_scandir_remove ;
        func = &ssexec_scandir_remove ;

    } else if (!strcmp(argv[1], "start")) {

        nargv[n++] = PROG ;
        nargv[n++] = argv[1] ;
        info->prog = PROG ;
        info->help = help_scandir_start ;
        info->usage = usage_scandir_start ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[1], "stop")) {

        nargv[n++] = PROG ;
        nargv[n++] = argv[1] ;
        info->prog = PROG ;
        info->help = help_scandir_stop ;
        info->usage = usage_scandir_stop ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[1], "reconfigure")) {

        nargv[n++] = PROG ;
        nargv[n++] = argv[1] ;
        info->prog = PROG ;
        info->help = help_scandir_reconfigure ;
        info->usage = usage_scandir_reconfigure ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[1], "rescan")) {

        nargv[n++] = PROG ;
        nargv[n++] = argv[1] ;
        info->prog = PROG ;
        info->help = help_scandir_rescan ;
        info->usage = usage_scandir_rescan ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[1], "quit")) {

        nargv[n++] = PROG ;
        nargv[n++] = argv[1] ;
        info->prog = PROG ;
        info->help = help_scandir_quit ;
        info->usage = usage_scandir_quit ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[1], "halt")) {

        nargv[n++] = PROG ;
        nargv[n++] = argv[1] ;
        info->prog = PROG ;
        info->help = help_scandir_halt ;
        info->usage = usage_scandir_halt ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[1], "abort")) {

        nargv[n++] = PROG ;
        nargv[n++] = argv[1] ;
        info->prog = PROG ;
        info->help = help_scandir_abort ;
        info->usage = usage_scandir_abort ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[1], "nuke")) {

        nargv[n++] = PROG ;
        nargv[n++] = argv[1] ;
        info->prog = PROG ;
        info->help = help_scandir_nuke ;
        info->usage = usage_scandir_nuke ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[1], "annihilate")) {

        nargv[n++] = PROG ;
        nargv[n++] = argv[1] ;
        info->prog = PROG ;
        info->help = help_scandir_annihilate ;
        info->usage = usage_scandir_annihilate ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[1], "zombies")) {

        nargv[n++] = PROG ;
        nargv[n++] = argv[1] ;
        info->prog = PROG ;
        info->help = help_scandir_zombies ;
        info->usage = usage_scandir_zombies ;
        func = &ssexec_scandir_signal ;

    } else {

        if (!strcmp(argv[1], "-h")) {
            info_help(help_scandir_wrapper, usage_scandir_wrapper) ;
            return 0 ;
        }

        log_usage(usage_scandir_wrapper, "\n", help_scandir_wrapper) ;
    }

    argc-- ;
    argv++ ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        int f = 0 ;
        for (;;) {

            int opt = subgetopt_r(argc, argv, OPTS_SCANDIR_WRAPPER, &l) ;
            if (opt == -1) break ;
            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 'o' :

                    if (MYUID)
                        log_die(LOG_EXIT_USER, "only root can use -o option") ;

                    if (!youruid(&owner,l.arg))
                        log_dieusys(LOG_EXIT_SYS, "get uid of: ", l.arg) ;

                    info->owner = owner ;
                    info->ownerlen = uid_fmt(info->ownerstr, info->owner) ;
                    info->ownerstr[info->ownerlen] = 0 ;

                    info->scandir.len = 0 ;
                    if (!auto_stra(&info->scandir, info->live.s, SS_SCANDIR, "/", info->ownerstr))
                        log_die_nomem("stralloc") ;

                default:

                    for (i = 0 ; i < n ; i++) {

                        if (!argv[l.ind])
                            log_usage(info->usage) ;

                        if (l.arg) {

                            if (!strcmp(nargv[i],argv[l.ind - 2]) || !strcmp(nargv[i],l.arg))
                                f = 1 ;

                        } else {

                            if (!strcmp(nargv[i],argv[l.ind]))
                                f = 1 ;
                        }
                    }

                    if (!f) {

                        if (l.arg) {
                            // distinction between e.g -enano and -e nano
                            if (argv[l.ind - 1][0] != '-')
                                nargv[n++] = argv[l.ind - 2] ;

                            nargv[n++] = argv[l.ind - 1] ;

                        } else {

                            nargv[n++] = argv[l.ind] ;
                        }
                    }
                    f = 0 ;
                    break ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    for (i = 0 ; i < argc ; i++ , argv++)
        nargv[n++] = *argv ;

    nargv[n] = 0 ;

    r = (*func)(n, nargv, info) ;

    return r ;
}
