/*
 * ssexec_tree_wrapper.c
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

#include <skalibs/sgetopt.h>

#include <66/ssexec.h>
#include <66/config.h>

int ssexec_tree_wrapper(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    if (!argv[1]) {
        PROG = "tree" ;
        log_usage(usage_tree_wrapper, "\n", help_tree_wrapper) ;
    }

    int r, n = 0, i = 0 ;
    uint8_t ctl = 0 ;
    ssexec_func_t_ref func = 0 ;
    char const *nargv[argc + 1] ;

    if (!strcmp(argv[1], "create")) {

        nargv[n++] = PROG ;

        info->prog = PROG ;
        info->help = help_tree_create ;
        info->usage = usage_tree_create ;
        func = &ssexec_tree_admin ;

        argc-- ;
        argv++ ;

    } else if (!strcmp(argv[1], "admin")) {

        nargv[n++] = PROG ;

        info->prog = PROG ;
        info->help = help_tree_admin ;
        info->usage = usage_tree_admin ;
        func = &ssexec_tree_admin ;

        argc-- ;
        argv++ ;

    } else if (!strcmp(argv[1], "remove")) {

        nargv[n++] = PROG ;
        nargv[n++] = "-R" ;

        info->prog = PROG ;
        info->help = help_tree_remove ;
        info->usage = usage_tree_remove ;
        func = &ssexec_tree_admin ;

        argc-- ;
        argv++ ;

    } else if (!strcmp(argv[1], "enable")) {

        nargv[n++] = PROG ;
        nargv[n++] = "-E" ;

        info->prog = PROG ;
        info->help = help_tree_enable ;
        info->usage = usage_tree_enable ;
        func = &ssexec_tree_admin ;

        argc-- ;
        argv++ ;

    } else if (!strcmp(argv[1], "disable")) {

        nargv[n++] = PROG ;
        nargv[n++] = "-D" ;

        info->prog = PROG ;
        info->help = help_tree_disable ;
        info->usage = usage_tree_disable ;

        func = &ssexec_tree_admin ;

        argc-- ;
        argv++ ;

    } else if (!strcmp(argv[1], "current")) {

        nargv[n++] = PROG ;
        nargv[n++] = "-c" ;

        info->prog = PROG ;
        info->help = help_tree_current ;
        info->usage = usage_tree_current ;

        func = &ssexec_tree_admin ;

        argc-- ;
        argv++ ;

    } else if (!strcmp(argv[1], "resolve")) {

        nargv[n++] = PROG ;

        info->prog = PROG ;
        info->help = help_tree_resolve ;
        info->usage = usage_tree_resolve ;

        func = &ssexec_tree_resolve ;

        argc-- ;
        argv++ ;

    } else if (!strcmp(argv[1], "status")) {

        nargv[n++] = PROG ;

        info->prog = PROG ;
        info->help = help_tree_status ;
        info->usage = usage_tree_status ;

        func = &ssexec_tree_status ;

        argc-- ;
        argv++ ;

    } else if (!strcmp(argv[1], "start")) {

        info->prog = PROG ;
        info->help = help_tree_start ;
        info->usage = usage_tree_start ;
        func = &ssexec_tree_signal ;
        ctl++ ;

    } else if (!strcmp(argv[1], "stop")) {

        info->prog = PROG ;
        info->help = help_tree_stop ;
        info->usage = usage_tree_stop ;
        func = &ssexec_tree_signal ;
        ctl++ ;

    } else if (!strcmp(argv[1], "free")) {

        info->prog = PROG ;
        info->help = help_tree_unsupervise ;
        info->usage = usage_tree_unsupervise ;
        func = &ssexec_tree_signal ;
        ctl++ ;

    } else {

        if (!strcmp(argv[1], "-h")) {
            info_help(help_tree_wrapper, usage_tree_wrapper) ;
            return 0 ;
        }

        log_usage(usage_tree_wrapper, "\n", help_tree_wrapper) ;
    }

    {
        subgetopt l = SUBGETOPT_ZERO ;

        int f = 0 ;
        for (;;) {

            int opt = subgetopt_r(argc, argv, OPTS_TREE_WRAPPER, &l) ;
            if (opt == -1) break ;
            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

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

    if (ctl) {
        /* swap the command and options e.g.
         * down -f <treename> <-> -f down <treename> */
        if (n > 2) {
            /* swap the command and options e.g.
             * down -f <-> -f down */
            nargv[n] = nargv[n-1] ;
            nargv[n-1] = nargv[0] ;
            nargv[++n] = 0 ;

        } else if (nargv[n-1][0] == '-') {
            nargv[n++] = nargv[0] ;
            nargv[n] = 0 ;
        } else {
            nargv[n] = nargv[n-1] ;
            nargv[n-1] = nargv[0] ;
            nargv[++n] = 0 ;
        }

    }

    r = (*func)(n, nargv, info) ;

    return r ;
}
