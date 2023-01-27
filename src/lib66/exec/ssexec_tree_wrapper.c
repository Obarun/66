/*
 * ssexec_reload.c
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
        log_usage(usage_tree) ;
    }

    int r, n = 0, i = 0 ;
    uint8_t ctl = 0 ;
    ssexec_func_t_ref func = 0 ;
    char const *nargv[argc + 1] ;

    if (!strcmp(argv[1], "create")) {

        nargv[n++] = PROG ;

        info->prog = PROG ;
        info->help = help_tree ;
        info->usage = usage_tree ;
        func = &ssexec_tree ;

        argc-- ;
        argv++ ;

    } else if (!strcmp(argv[1], "admin")) {

        nargv[n++] = PROG ;

        info->prog = PROG ;
        info->help = help_tree ;
        info->usage = usage_tree ;
        func = &ssexec_tree ;

        argc-- ;
        argv++ ;

    } else if (!strcmp(argv[1], "remove")) {

        nargv[n++] = PROG ;
        nargv[n++] = "-R" ;

        info->prog = PROG ;
        info->help = help_tree ;
        info->usage = usage_tree ;
        func = &ssexec_tree ;

        argc-- ;
        argv++ ;

    } else if (!strcmp(argv[1], "enable")) {

        nargv[n++] = PROG ;
        nargv[n++] = "-E" ;

        info->prog = PROG ;
        info->help = help_tree ;
        info->usage = usage_tree ;
        func = &ssexec_tree ;

        argc-- ;
        argv++ ;

    } else if (!strcmp(argv[1], "disable")) {

        nargv[n++] = PROG ;
        nargv[n++] = "-D" ;

        info->prog = PROG ;
        info->help = help_tree ;
        info->usage = usage_tree ;

        func = &ssexec_tree ;

        argc-- ;
        argv++ ;

    } else if (!strcmp(argv[1], "current")) {

        nargv[n++] = PROG ;
        nargv[n++] = "-c" ;

        info->prog = PROG ;
        info->help = help_tree ;
        info->usage = usage_tree ;

        func = &ssexec_tree ;

        argc-- ;
        argv++ ;

    } else if (!strcmp(argv[1], "up")) {

        info->prog = PROG ;
        info->help = help_treectl ;
        info->usage = usage_treectl ;
        func = &ssexec_treectl ;
        ctl++ ;

    } else if (!strcmp(argv[1], "down")) {

        info->prog = PROG ;
        info->help = help_treectl ;
        info->usage = usage_treectl ;
        func = &ssexec_treectl ;
        ctl++ ;

    } else if (!strcmp(argv[1], "unsupervise")) {

        info->prog = PROG ;
        info->help = help_treectl ;
        info->usage = usage_treectl ;
        func = &ssexec_treectl ;
        ctl++ ;

    } else {

        log_usage(usage_tree) ;
    }

    {
        subgetopt l = SUBGETOPT_ZERO ;

        int f = 0 ;
        for (;;) {

            int opt = subgetopt_r(argc, argv, "", &l) ;
            if (opt == -1) break ;

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
