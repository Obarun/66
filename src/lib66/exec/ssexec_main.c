/*
 * ssexec_main.c
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

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/sgetopt.h>
#include <skalibs/types.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/tree.h>
#include <66/constants.h>

int ssexec_set_treeinfo(ssexec_t *info)
{
    log_flow() ;

    int r = tree_sethome(info) ;
    if (r <= 0) goto out ;

    if (!tree_get_permissions(info->tree.s,info->owner))
        r = -4 ;

    info->treeallow = 1 ;

    out:
    return r ;
}

void ssexec_set_info(ssexec_t *info)
{
    log_flow() ;

    int r ;

    info->owner = MYUID ;
    info->ownerlen = uid_fmt(info->ownerstr,info->owner) ;
    info->ownerstr[info->ownerlen] = 0 ;

    if (!set_ownersysdir(&info->base,info->owner)) log_dieusys(LOG_EXIT_SYS, "set owner directory") ;

    if (!info->skip_opt_tree) {

        r = ssexec_set_treeinfo(info) ;
        if (r == -4) log_die(LOG_EXIT_USER,"You're not allowed to use the tree: ",info->tree.s) ;
        if (r == -3) log_dieu(LOG_EXIT_USER,"find the current tree. You must use the -t options") ;
        if (r == -2) log_dieu(LOG_EXIT_USER,"set the tree name") ;
        if (r == -1) log_dieu(LOG_EXIT_USER,"parse seed file") ;
        if (!r) log_dieusys(LOG_EXIT_SYS,"find tree: ", info->treename.s) ;

    }

    r = set_livedir(&info->live) ;
    if (!r) log_die_nomem("stralloc") ;
    if(r < 0) log_die(LOG_EXIT_SYS,"live: ",info->live.s," must be an absolute path") ;

    if (!stralloc_copy(&info->livetree,&info->live)) log_die_nomem("stralloc") ;
    if (!stralloc_copy(&info->scandir,&info->live)) log_die_nomem("stralloc") ;

    r = set_livetree(&info->livetree,info->owner) ;
    if (!r) log_die_nomem("stralloc") ;
    if(r < 0) log_die(LOG_EXIT_SYS,"livetree: ",info->livetree.s," must be an absolute path") ;

    r = set_livescan(&info->scandir,info->owner) ;
    if (!r) log_die_nomem("stralloc") ;
    if(r < 0) log_die(LOG_EXIT_SYS,"scandir: ",info->scandir.s," must be an absolute path") ;
}

static inline void info_help (char const *help,char const *usage)
{
    log_flow() ;

    DEFAULT_MSG = 0 ;

    log_info(usage,"\n",help) ;
}

static inline void append_opts(char *opts,size_t baselen, ssexec_func_t *func)
{
    if ((*func) == &ssexec_init)
        auto_strings(opts + baselen,OPTS_INIT) ;
    else if ((*func) == &ssexec_enable)
        auto_strings(opts + baselen,OPTS_ENABLE) ;
    else if ((*func) == &ssexec_disable)
        auto_strings(opts + baselen,OPTS_DISABLE) ;
    else if ((*func) == &ssexec_start)
        auto_strings(opts + baselen,OPTS_START) ;
    else if ((*func) == &ssexec_stop)
        auto_strings(opts + baselen,OPTS_STOP) ;
    else if ((*func) == &ssexec_svctl)
        auto_strings(opts + baselen,OPTS_SVCTL) ;
    else if ((*func) == &ssexec_dbctl)
        auto_strings(opts + baselen,OPTS_DBCTL) ;
    else if ((*func) == &ssexec_env)
        auto_strings(opts + baselen,OPTS_ENV) ;
    else if ((*func) == &ssexec_all)
        auto_strings(opts + baselen,OPTS_ALL) ;
    else if ((*func) == &ssexec_tree)
        auto_strings(opts + baselen,OPTS_TREE) ;
}

int ssexec_main(int argc, char const *const *argv,char const *const *envp,ssexec_func_t *func, ssexec_t *info)
{
    log_flow() ;

    int r, n = 0, i = 0 ;
    char const *nargv[argc + 1] ;

    log_color = &log_color_disable ;

    /** 30 options should be large enough */
    char opts[30] ;
    char const *main = "hv:l:t:T:z" ;
    size_t mainlen = strlen(main) ;

    auto_strings(opts,main) ;

    append_opts(opts,mainlen,func) ;

    nargv[n++] = "fake_name" ;
    {
        subgetopt l = SUBGETOPT_ZERO ;

        int f = 0 ;
        for (;;)
        {
            int opt = subgetopt_r(argc,argv, opts, &l) ;

            if (opt == -1) break ;
            switch (opt)
            {
                case 'h' :  info_help(info->help,info->usage); return 0 ;
                case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) log_usage(info->usage) ;
                            info->opt_verbo = 1 ;
                            break ;
                case 'l' :

                    char str[UINT_FMT] ;
                    str[uint_fmt(str, SS_MAX_PATH)] = 0 ;

                    if (strlen(l.arg) > SS_MAX_PATH)
                        log_die(LOG_EXIT_USER,"live path is too long -- it can not exceed ", str) ;

                    if (!auto_stra(&info->live,l.arg))
                        log_die_nomem("stralloc") ;

                    info->opt_live = 1 ;
                    break ;

                case 't' :

                    char str[UINT_FMT] ;
                    str[uint_fmt(str, SS_MAX_TREENAME)] = 0 ;

                    if (strlen(l.arg) > SS_MAX_TREENAME)
                        log_die(LOG_EXIT_USER,"tree name is too long -- it can not exceed ", str) ;

                    if (!auto_stra(&info->treename,l.arg))
                        log_die_nomem("stralloc") ;

                    info->opt_tree = 1 ;
                    info->skip_opt_tree = 0 ;
                    break ;

                case 'T' :  if (!uint0_scan(l.arg, &info->timeout)) log_usage(info->usage) ;
                            info->opt_timeout = 1 ;
                            break ;
                case 'z' :  log_color = !isatty(1) ? &log_color_disable : &log_color_enable ;
                            info->opt_color = 1 ;
                            break ;
                default :
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

    ssexec_set_info(info) ;

    for (i = 0 ; i < argc ; i++ , argv++)
        nargv[n++] = *argv ;

    nargv[n] = 0 ;

    r = (*func)(n,nargv,envp,info) ;

    ssexec_free(info) ;

    return r ;
}
