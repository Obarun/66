/*
 * ssexec_snapshot_wrapper.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
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
#include <stdint.h>

#include <oblibs/log.h>

#include <skalibs/sgetopt.h>

#include <66/ssexec.h>

int ssexec_snapshot_wrapper(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    if (!argv[1]) {
        PROG = "snapshot" ;
        log_usage(usage_snapshot_wrapper, "\n", help_snapshot_wrapper) ;
    }

    int r, n = 0, i = 0 ;
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

                    info_help(help_snapshot_wrapper, usage_snapshot_wrapper) ;
                    return 0 ;

                default:

                    log_usage(usage_snapshot_wrapper, "\n", help_snapshot_wrapper) ;

            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (!ctl) {
        argc-- ;
        argv++ ;
    }

    if (!argc)
        log_usage(usage_snapshot_wrapper, "\n", help_snapshot_wrapper) ;

    if (!strcmp(argv[0], "create")) {

        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_snapshot_create ;
        info->usage = usage_snapshot_create ;
        func = &ssexec_snapshot_create ;

    } else if (!strcmp(argv[0], "restore")) {

        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_snapshot_restore ;
        info->usage = usage_snapshot_restore ;
        func = &ssexec_snapshot_restore ;

    } else if (!strcmp(argv[0], "remove")) {

        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_snapshot_remove ;
        info->usage = usage_snapshot_remove ;
        func = &ssexec_snapshot_remove ;

    } else if (!strcmp(argv[0], "list")) {

        nargv[n++] = argv[0] ;
        info->prog = PROG ;
        info->help = help_snapshot_list ;
        info->usage = usage_snapshot_list ;
        func = &ssexec_snapshot_list ;

    } else
        log_usage(usage_snapshot_wrapper, "\n", help_snapshot_wrapper) ;


    argc-- ;
    argv++ ;

    for (i = 0 ; i < argc ; i++ , argv++)
        nargv[n++] = *argv ;

    nargv[n] = 0 ;

    r = (*func)(n, nargv, info) ;

    return r ;
}
