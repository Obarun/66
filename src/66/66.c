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

static inline void info_help (char const *help,char const *usage)
{
    log_flow() ;

    DEFAULT_MSG = 0 ;

    log_info(usage,"\n", help) ;
}

void set_treeinfo(ssexec_t *info)
{
    log_flow() ;

    int r = tree_sethome(info) ;
    if (r == -3)
        log_dieu(LOG_EXIT_USER, "find the current tree. You must use the -t options") ;
    if (r == -2)
        log_dieu(LOG_EXIT_USER, "set the tree name") ;
    if (r == -1)
        log_dieu(LOG_EXIT_USER, "parse seed file") ;
    if (!r)
        log_dieusys(LOG_EXIT_SYS, "find tree: ", info->treename.s) ;

    if (!tree_get_permissions(info->tree.s, info->owner))
        log_die(LOG_EXIT_USER,"You're not allowed to use the tree: ",info->tree.s) ;

    info->treeallow = 1 ;
}

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
    info->tree.len = 0 ;
    info->scandir.len = 0 ;
    info->treename.len = 0 ;

}

int main(int argc, char const *const *argv)
{

    if (!argv[1]) {
        PROG = "66" ;
        log_usage(usage_66) ;
    }

    int r, n = 0, i = 0 ;
    uint8_t sanitize = 0 ;
    /** 30 options should be large enough */
    char opts[30] ;
    char const *main = "hv:l:t:T:z" ;
    char str[UINT_FMT] ;
    char const *nargv[argc + 1] ;

    ssexec_t info = SSEXEC_ZERO ;
    ssexec_func_t_ref func = 0 ;
    log_color = &log_color_disable ;

    info_clean(&info) ;

    info.owner = getuid() ;
    info.ownerlen = uid_fmt(info.ownerstr, info.owner) ;
    info.ownerstr[info.ownerlen] = 0 ;

    if (!set_ownersysdir(&info.base, info.owner))
        log_dieusys(LOG_EXIT_SYS, "set owner directory") ;

    if (!strcmp(argv[1], "boot")) {

        PROG = "boot" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_boot ;
        info.usage = usage_boot ;
        func = &ssexec_boot ;

        sanitize++ ;

        auto_strings(opts, main, OPTS_BOOT) ;

    } else if (!strcmp(argv[1], "enable")) {

        PROG = "enable" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_enable ;
        info.usage = usage_enable ;
        func = &ssexec_enable ;

        auto_strings(opts, main, OPTS_ENABLE) ;

    } else if (!strcmp(argv[1], "disable")) {

        PROG = "disable" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_disable ;
        info.usage = usage_disable ;
        func = &ssexec_disable ;

        auto_strings(opts, main, OPTS_DISABLE) ;

    } else if (!strcmp(argv[1], "start")) {

        PROG = "start" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_start ;
        info.usage = usage_start ;
        func = &ssexec_start ;

        auto_strings(opts, main, OPTS_START) ;

    } else if (!strcmp(argv[1], "stop")) {

        PROG = "stop" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_stop ;
        info.usage = usage_stop ;
        func = &ssexec_stop ;

        auto_strings(opts, main, OPTS_STOP) ;

    }  else if (!strcmp(argv[1], "all")) {

        PROG = "all" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_treectl ;
        info.usage = usage_treectl ;
        func = &ssexec_treectl ;

        auto_strings(opts, main, OPTS_TREECTL) ;

    } else if (!strcmp(argv[1], "env")) {

        PROG = "env" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_env ;
        info.usage = usage_env ;
        func = &ssexec_env ;

        auto_strings(opts, main, OPTS_ENV) ;

    } else if (!strcmp(argv[1], "init")) {

        PROG = "init" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_init ;
        info.usage = usage_init ;
        func = &ssexec_init ;

        auto_strings(opts, main, OPTS_INIT) ;

    } else if (!strcmp(argv[1], "parse")) {

        PROG = "parse" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_parse ;
        info.usage = usage_parse ;
        func = &ssexec_parse ;

        auto_strings(opts, main, OPTS_PARSE) ;

    } else if (!strcmp(argv[1], "reconfigure")) {

        PROG = "reconfigure" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_reconfigure ;
        info.usage = usage_reconfigure ;
        func = &ssexec_reconfigure ;

        auto_strings(opts, main, OPTS_SUBSTART) ;

    } else if (!strcmp(argv[1], "reload")) {

        PROG = "reload" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_reload ;
        info.usage = usage_reload ;
        func = &ssexec_reload ;

        auto_strings(opts, main, OPTS_SUBSTART) ;

    } else if (!strcmp(argv[1], "restart")) {

        PROG = "restart" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_restart ;
        info.usage = usage_restart ;
        func = &ssexec_restart ;

        auto_strings(opts, main, OPTS_SUBSTART) ;

    } else if (!strcmp(argv[1], "unsupervise")) {

        PROG = "stop" ;
        nargv[n++] = PROG ;
        nargv[n++] = "-u" ;
        info.prog = PROG ;
        info.help = help_stop ;
        info.usage = usage_stop ;
        func = &ssexec_stop ;

        auto_strings(opts, main, OPTS_STOP) ;

    } else if (!strcmp(argv[1], "svctl")) {

        PROG = "svctl" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_svctl ;
        info.usage = usage_svctl ;
        func = &ssexec_svctl ;

        auto_strings(opts, main, OPTS_SVCTL) ;

    } else if (!strcmp(argv[1], "tree")) {

        PROG = "tree" ;
        nargv[n++] = PROG ;
        func = &ssexec_tree_wrapper ;

        auto_strings(opts, main, OPTS_TREE) ;

    } else if (!strcmp(argv[1], "inresolve")) {

        PROG = "inresolve" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_inresolve ;
        info.usage = usage_inresolve ;
        func = &ssexec_inresolve ;

        auto_strings(opts, main, OPTS_INRESOLVE) ;

    } else if (!strcmp(argv[1], "instate")) {

        PROG = "instate" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_instate ;
        info.usage = usage_instate ;
        func = &ssexec_instate ;

        auto_strings(opts, main, OPTS_INSTATE) ;

    } else if (!strcmp(argv[1], "intree")) {

        PROG = "intree" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_intree ;
        info.usage = usage_intree ;
        func = &ssexec_intree ;

        auto_strings(opts, main, OPTS_INTREE) ;

    } else if (!strcmp(argv[1], "inservice")) {

        PROG = "inservice" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_inservice ;
        info.usage = usage_inservice ;
        func = &ssexec_inservice ;

        auto_strings(opts, main, OPTS_INSERVICE) ;

    } else if (!strcmp(argv[1], "scanctl")) {

        PROG = "scanctl" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_scanctl ;
        info.usage = usage_scanctl ;
        func = &ssexec_scanctl ;

        auto_strings(opts, main, OPTS_SCANCTL) ;

    } else if (!strcmp(argv[1], "scandir")) {

        PROG = "scandir" ;
        nargv[n++] = PROG ;
        info.prog = PROG ;
        info.help = help_scandir ;
        info.usage = usage_scandir ;
        func = &ssexec_scandir ;

        auto_strings(opts, main, OPTS_SCANDIR) ;

    } else {

        log_usage(usage_66) ;
    }

    argc-- ;
    argv++ ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        int f = 0 ;
        for (;;)
        {
            int opt = subgetopt_r(argc, argv, opts, &l) ;

            if (opt == -1) break ;
            switch (opt)
            {
                case 'h' :

                    info_help(info.help, info.usage) ;
                    return 0 ;

                case 'v' :

                    if (!uint0_scan(l.arg, &VERBOSITY))
                        log_usage(info.usage) ;
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
                        log_usage(info.usage) ;
                    info.opt_timeout = 1 ;
                    break ;

                case 'z' :

                    log_color = !isatty(1) ? &log_color_disable : &log_color_enable ;
                    info.opt_color = 1 ;
                    break ;

                default :

                    for (i = 0 ; i < n ; i++) {

                        if (!argv[l.ind])
                            log_usage(info.usage) ;

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
