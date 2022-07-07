/*
 * ssexec_init.c
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
#include <sys/types.h>
#include <unistd.h>//chown
#include <stdio.h>

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/types.h>//scan_mode
#include <oblibs/directory.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/utils.h>
#include <66/constants.h>
#include <66/tree.h>
#include <66/svc.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/state.h>
#include <66/sanitize.h>

static int init_parse_options(char const *opts)
{
    int r = get_len_until(opts, '=') ;
    if (r < 0)
        log_die(LOG_EXIT_USER, "invalid opts: ", opts) ;

    char tmp[9] ;

    auto_strings(tmp, opts + r + 1) ;

    if (!strcmp(tmp, "disabled"))
        return 0 ;

    else if (!strcmp(tmp, "enabled"))
        return 1 ;

    else
        log_die(LOG_EXIT_USER, "invalid opts: ", tmp) ;

}

static void doit(stralloc *sa, char const *svdirs, char const *treename, uint8_t earlier)
{
    unsigned int pos = 0, count = 0, nservice = sastr_nelement(sa) ;
    resolve_service_t ares[nservice] ;

    FOREACH_SASTR(sa, pos) {

        char *service = sa->s + pos ;

        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

        if (!resolve_read(wres, svdirs, service))
            log_dieusys(LOG_EXIT_SYS,"read resolve file of: ", service) ;

        /**
         * boot time. We only pick the earlier service.
         * The rest is initialized at stage2.
         * */
        if (earlier) {
            if (res.earlier)
                ares[count++] = res ;

        } else
            ares[count++] = res ;
    }

    nservice = count ;

    //0 don't remove down file -> 2 remove down file
    if (!sanitize_init(ares, nservice, !earlier ? 0 : STATE_FLAGS_ISEARLIER))
        log_dieu(LOG_EXIT_SYS,"initiate services of tree: ", treename) ;

    service_resolve_array_free(ares, nservice) ;
}

int ssexec_init(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r, what = -1 ;
    uint8_t nopts = 0, earlier = 0 ;
    char const *treename = 0 ;
    char opts[14] ;
    gid_t gidowner ;
    resolve_service_master_t mres = RESOLVE_SERVICE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_MASTER, &mres) ;
    stralloc sa = STRALLOC_ZERO ;

    if (!yourgid(&gidowner,info->owner))
        log_dieusys(LOG_EXIT_SYS,"get gid") ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = getopt_args(argc,argv, ">" OPTS_INIT, &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;

            switch (opt) {

                case 'o' :

                    auto_strings(opts, l.arg) ;
                    nopts++ ;

                default:

                    log_usage(usage_init) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(usage_init) ;

    treename = argv[1] ;

    if (nopts)
        what = init_parse_options(opts) ;

    size_t treenamelen = strlen(treename) ;
    size_t treelen = info->base.len + SS_SYSTEM_LEN + 1 + treenamelen + 1 + SS_SVDIRS_LEN ;
    char tree[treelen + 1] ;
    auto_strings(tree, info->base.s, SS_SYSTEM, "/", treename) ;

    if (!tree_isvalid(info->base.s, treename))
        log_diesys(LOG_EXIT_USER, "invalid tree directory: ", treename) ;

    if (!tree_get_permissions(tree, info->owner))
        log_die(LOG_EXIT_USER, "You're not allowed to use the tree: ", tree) ;

    r = scan_mode(info->scandir.s, S_IFDIR) ;
    if (r < 0) log_die(LOG_EXIT_SYS,info->scandir.s, " conflicted format") ;
    if (!r) log_die(LOG_EXIT_USER,"scandir: ", info->scandir.s, " doesn't exist") ;

    r = scandir_ok(info->scandir.s) ;
    if (r != 1) earlier = 1 ;

    auto_strings(tree + info->base.len + SS_SYSTEM_LEN + 1 + treenamelen, SS_SVDIRS) ;

    if (!resolve_read_g(wres, tree, SS_MASTER + 1))
        log_dieu(LOG_EXIT_SYS, "read resolve service Master file of tree: ", treename) ;

    if (what < 0) {

        if (mres.ncontents) {

            if (!sastr_clean_string(&sa, mres.sa.s + mres.contents))
                log_dieu(LOG_EXIT_SYS, "clean string: ", mres.sa.s + mres.contents) ;

        } else {

            log_info("Initialization report: no enabled services to initiate at tree: ", treename) ;
            goto end ;
        }

    } else if (!what) {

        if (mres.ndisabled) {

            if (!sastr_clean_string(&sa, mres.sa.s + mres.disabled))
                log_dieu(LOG_EXIT_SYS, "clean string: ", mres.sa.s + mres.disabled) ;

        } else {

            log_info("Initialization report: no disabled services to initiate at tree: ", treename) ;
            goto end ;
        }

    } else if (what) {

        if (mres.nenabled) {

            if (!sastr_clean_string(&sa, mres.sa.s + mres.enabled))
                log_dieu(LOG_EXIT_SYS, "clean string: ", mres.sa.s + mres.enabled) ;

        } else {

            log_info("Initialization report: no enabled services to initiate at tree: ", treename) ;
            goto end ;
        }
    }

    doit(&sa, tree, treename, earlier) ;

    end:
        resolve_free(wres) ;
        stralloc_free(&sa) ;
        return 0 ;
}
