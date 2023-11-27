/*
 * 66.c
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
#include <unistd.h>//getuid, isatty

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/sgetopt.h>
#include <skalibs/types.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/sanitize.h>
#include <66/tree.h>
#include <66/hpr.h>

static void set_info(ssexec_t *info)
{
    log_flow() ;

    int r ;

    set_treeinfo(info) ;

    r = set_livedir(&info->live) ;
    if (!r)
        log_die_nomem("stralloc") ;
    if(r < 0)
        log_die(LOG_EXIT_SYS, "live: ", info->live.s, " must be an absolute path") ;

    if (!stralloc_copy(&info->scandir, &info->live))
        log_die_nomem("stralloc") ;

    r = set_livescan(&info->scandir, info->owner) ;
    if (!r)
        log_die_nomem("stralloc") ;
    if(r < 0)
        log_die(LOG_EXIT_SYS, "scandir: ", info->scandir.s, " must be an absolute path") ;
}

static void info_clean(ssexec_t *info)
{

    info->base.len = 0 ;
    info->live.len = 0 ;
    info->scandir.len = 0 ;
    info->treename.len = 0 ;

}

int main(int argc, char const *const *argv)
{

    if (!argv[1]) {
        PROG = "66" ;
        log_usage( usage_66, "\n", help_66) ;
        return 0 ;
    }

    int r, n = 0, i = 0 ;
    uint8_t sanitize = 0 ;
    char str[UINT_FMT] ;
    char const *nargv[argc + 3] ;

    ssexec_t info = SSEXEC_ZERO ;
    ssexec_func_t_ref func = 0 ;
    log_color = &log_color_disable ;

    info_clean(&info) ;

    info.owner = getuid() ;
    info.ownerlen = uid_fmt(info.ownerstr, info.owner) ;
    info.ownerstr[info.ownerlen] = 0 ;

    if (!set_ownersysdir(&info.base, info.owner))
        log_dieusys(LOG_EXIT_SYS, "set owner directory") ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        int f = 0 ;
        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_MAIN, &l) ;

            if (opt == -1) break ;
            switch (opt)
            {
                case 'h' :

                    info_help(help_66, usage_66) ;
                    return 0 ;

                case 'v' :

                    if (!uint0_scan(l.arg, &VERBOSITY))
                        log_usage(usage_66, "\n", help_66) ;
                    info.opt_verbo = 1 ;
                    break ;

                case 'l' :

                    str[uint_fmt(str, SS_MAX_PATH)] = 0 ;

                    if (strlen(l.arg) > SS_MAX_PATH)
                        log_die(LOG_EXIT_USER, "live path is too long -- it can not exceed ", str) ;

                    if (!auto_stra(&info.live, l.arg))
                        log_die_nomem("stralloc") ;

                    info.opt_live = 1 ;
                    break ;

                case 't' :

                    str[uint_fmt(str, SS_MAX_TREENAME)] = 0 ;

                    if (strlen(l.arg) > SS_MAX_TREENAME)
                        log_die(LOG_EXIT_USER, "tree name is too long -- it can not exceed ", str) ;

                    if (!auto_stra(&info.treename, l.arg))
                        log_die_nomem("stralloc") ;

                    info.opt_tree = 1 ;
                    break ;

                case 'T' :

                    if (!uint0_scan(l.arg, &info.timeout))
                        log_usage(usage_66, "\n", help_66) ;
                    info.opt_timeout = 1 ;
                    break ;

                case 'z' :

                    log_color = !isatty(1) ? &log_color_disable : &log_color_enable ;
                    info.opt_color = 1 ;
                    break ;

                case '?' :

                    log_usage(usage_66, "\n", help_66) ;

                default :

                    for (i = 0 ; i < n ; i++) {

                        if (!argv[l.ind])
                            log_usage(usage_66, "\n", help_66) ;

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
                            // distinction between e.g -e<value> and -e <value>
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

    if (!strcmp(argv[0], "boot")) {

        PROG = "boot" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_boot ;
        info.usage = usage_boot ;
        func = &ssexec_boot ;

        sanitize++ ;

    } else if (!strcmp(argv[0], "enable")) {

        PROG = "enable" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_enable ;
        info.usage = usage_enable ;
        func = &ssexec_enable ;

    } else if (!strcmp(argv[0], "disable")) {

        PROG = "disable" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_disable ;
        info.usage = usage_disable ;
        func = &ssexec_disable ;

    } else if (!strcmp(argv[0], "start")) {

        PROG = "start" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_start ;
        info.usage = usage_start ;
        func = &ssexec_start ;

    } else if (!strcmp(argv[0], "stop")) {

        PROG = "stop" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_stop ;
        info.usage = usage_stop ;
        func = &ssexec_stop ;

    } else if (!strcmp(argv[0], "configure")) {

        PROG = "configure" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_env ;
        info.usage = usage_env ;
        func = &ssexec_env ;

    } else if (!strcmp(argv[0], "init")) {

        PROG = "init" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_init ;
        info.usage = usage_init ;
        func = &ssexec_init ;

    } else if (!strcmp(argv[0], "parse")) {

        PROG = "parse" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_parse ;
        info.usage = usage_parse ;
        func = &ssexec_parse ;

    } else if (!strcmp(argv[0], "reconfigure")) {

        PROG = "reconfigure" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_reconfigure ;
        info.usage = usage_reconfigure ;
        func = &ssexec_reconfigure ;

    } else if (!strcmp(argv[0], "reload")) {

        PROG = "reload" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_reload ;
        info.usage = usage_reload ;
        func = &ssexec_reload ;

    } else if (!strcmp(argv[0], "restart")) {

        PROG = "restart" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_restart ;
        info.usage = usage_restart ;
        func = &ssexec_restart ;

    } else if (!strcmp(argv[0], "free")) {

        PROG = "free" ;
        nargv[n++] = PROG ;
        nargv[n++] = "-u" ;
        info.prog = PROG ;
        info.help = help_free ;
        info.usage = usage_free ;
        func = &ssexec_stop ;

    } else if (!strcmp(argv[0], "signal")) {

        PROG = "signal" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_signal ;
        info.usage = usage_signal ;
        func = &ssexec_signal ;

    } else if (!strcmp(argv[0], "status")) {

        PROG = "status" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_status ;
        info.usage = usage_status ;
        func = &ssexec_status ;

    } else if (!strcmp(argv[0], "resolve")) {

        PROG = "resolve" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_resolve ;
        info.usage = usage_resolve ;
        func = &ssexec_resolve ;

    } else if (!strcmp(argv[0], "state")) {

        PROG = "state" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_state ;
        info.usage = usage_state ;
        func = &ssexec_state ;

    } else if (!strcmp(argv[0], "remove")) {

        PROG = "remove" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_remove ;
        info.usage = usage_remove ;
        func = &ssexec_remove ;

    } else if (!strcmp(argv[0], "tree")) {

        PROG = "tree" ;
        nargv[n++] = PROG ;
        func = &ssexec_tree_wrapper ;

    } else if (!strcmp(argv[0], "scandir")) {

        PROG = "scandir" ;
        nargv[n++] = PROG ;
        func = &ssexec_scandir_wrapper ;

    } else if (!strcmp(argv[0], "poweroff")) {

        PROG = "poweroff" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_poweroff ;
        info.usage = usage_poweroff ;
        func = &ssexec_shutdown_wrapper ;

    } else if (!strcmp(argv[0], "reboot")) {

        PROG = "reboot" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_reboot ;
        info.usage = usage_reboot ;
        func = &ssexec_shutdown_wrapper ;

    } else if (!strcmp(argv[0], "halt")) {

        PROG = "halt" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_halt ;
        info.usage = usage_halt ;
        func = &ssexec_shutdown_wrapper ;

    } else if (!strcmp(argv[0], "wall")) {

        PROG = "wall" ;
        if (!argv[1])
            log_die(LOG_EXIT_USER, "missing message to send") ;
        /** message need to be double quoted.
         * we don't check that here */
        hpr_wall(argv[1]) ;
        return 0 ;

    } else if (!strcmp(argv[0], "version")) {

        PROG = "version" ;
        log_info(SS_VERSION) ;
        return 0 ;

    } else {

        PROG = "66" ;
        if (!strcmp(argv[0], "-h")) {
            info_help(help_66, usage_66) ;
            return 0 ;
        }

        log_usage(usage_66, "\n", help_66) ;
    }

    argc-- ;
    argv++ ;

    if (!sanitize)
        sanitize_system(&info) ;

    set_info(&info) ;

    for (i = 0 ; i < argc ; i++ , argv++)
        nargv[n++] = *argv ;

    nargv[n] = 0 ;

    r = (*func)(n, nargv, &info) ;

    ssexec_free(&info) ;

    return r ;

}
