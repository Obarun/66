/*
 * ssexec_scandir_wrapper.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
    uint8_t ctl = 0 ;
    ssexec_func_t_ref func = 0 ;
    char const *nargv[argc + 1] ;

    if (argv[1][0] == '-') {

        ctl++ ;
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = subgetopt_r(argc, argv, OPTS_SCANDIR_WRAPPER, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(help_scandir_wrapper, usage_scandir_wrapper) ;
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

                    break ;

                default:

                    log_usage(usage_scandir_wrapper, "\n", help_scandir_wrapper) ;

            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (!ctl) {
        argc-- ;
        argv++ ;
    }

    if (!argc)
        log_usage(usage_scandir_wrapper, "\n", help_scandir_wrapper) ;

    if (!strcmp(argv[0], "create")) {

        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_scandir_create ;
        info->usage = usage_scandir_create ;
        func = &ssexec_scandir_create ;

    } else if (!strcmp(argv[0], "remove")) {

        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_scandir_remove ;
        info->usage = usage_scandir_remove ;
        func = &ssexec_scandir_remove ;

    } else if (!strcmp(argv[0], "start")) {


        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_scandir_start ;
        info->usage = usage_scandir_start ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[0], "stop")) {


        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_scandir_stop ;
        info->usage = usage_scandir_stop ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[0], "reconfigure")) {


        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_scandir_reconfigure ;
        info->usage = usage_scandir_reconfigure ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[0], "rescan")) {

        nargv[n++] = PROG ;
        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_scandir_rescan ;
        info->usage = usage_scandir_rescan ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[0], "quit")) {


        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_scandir_quit ;
        info->usage = usage_scandir_quit ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[0], "halt")) {


        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_scandir_halt ;
        info->usage = usage_scandir_halt ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[0], "abort")) {


        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_scandir_abort ;
        info->usage = usage_scandir_abort ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[0], "nuke")) {

        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_scandir_nuke ;
        info->usage = usage_scandir_nuke ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[0], "annihilate")) {

        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_scandir_annihilate ;
        info->usage = usage_scandir_annihilate ;
        func = &ssexec_scandir_signal ;

    } else if (!strcmp(argv[0], "zombies")) {

        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_scandir_zombies ;
        info->usage = usage_scandir_zombies ;
        func = &ssexec_scandir_signal ;

    } else
        log_usage(usage_scandir_wrapper, "\n", help_scandir_wrapper) ;


    argc-- ;
    argv++ ;

    for (i = 0 ; i < argc ; i++ , argv++)
        nargv[n++] = *argv ;

    nargv[n++] = nargv[0] ;
    nargv[n] = 0 ;

    r = (*func)(n, nargv, info) ;

    return r ;
}
