/*
 * ssexec_remove.c
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
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>// unlink

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/stack.h>
#include <oblibs/sastr.h>
#include <oblibs/directory.h>

#include <skalibs/posixplz.h>
#include <skalibs/stralloc.h>
#include <skalibs/sgetopt.h>

#include <66/state.h>
#include <66/enum.h>
#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/config.h>
#include <66/constants.h>
#include <66/svc.h>
#include <66/utils.h>

static void auto_remove(char const *path)
{
    log_trace("remove directory: ", path) ;
    if (!dir_rm_rf(path))
        log_dieusys(LOG_EXIT_SYS, "remove directory: ", path) ;
}

static void remove_deps(resolve_service_t *res, resolve_service_t *ares, unsigned int *areslen, stralloc *sa, ssexec_t *info)
{

    if (!res->dependencies.nrequiredby)
        return ;

    int r ;
    unsigned int pos = 0 ;
    ss_state_t ste = STATE_ZERO ;
    resolve_wrapper_t_ref wres = 0 ;
    _init_stack_(stk, strlen(res->sa.s + res->dependencies.requiredby)) ;

    if (!stack_convert_string_g(&stk, res->sa.s + res->dependencies.requiredby))
        log_dieu(LOG_EXIT_SYS, "convert string") ;

    FOREACH_STK(&stk, pos) {

        resolve_service_t dres = RESOLVE_SERVICE_ZERO ;
        wres = resolve_set_struct(DATA_SERVICE, &dres) ;

        r = resolve_read_g(wres, info->base.s, stk.s + pos) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", stk.s + pos) ;

        if (!r) {
            log_warn("service: ", stk.s + pos, " is already removed -- ignoring it") ;
            continue ;
        }

        if (!state_read(&ste, &dres))
            log_dieusys(LOG_EXIT_SYS, "read state file of: ", stk.s + pos, " -- please make a bug report") ;

        if (ste.issupervised == STATE_FLAGS_TRUE)
            if (!sastr_add_string(sa, stk.s + pos))
                log_dieusys(LOG_EXIT_SYS, "add service: ", stk.s + pos, " to selection") ;

        ares[(*areslen)++] = dres ;

        if (dres.dependencies.nrequiredby)
            remove_deps(&dres, ares, areslen, sa, info) ;
    }

    free(wres) ;
}

static void remove_service(resolve_service_t *res, ssexec_t *info)
{
    int r ;
    char sym[strlen(res->sa.s + res->path.home) + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + SS_MAX_SERVICE_NAME + 1] ;

    auto_remove(res->sa.s + res->path.servicedir) ;

    if (res->environ.envdir)
        auto_remove(res->sa.s + res->environ.envdir) ;

    tree_service_remove(info->base.s, res->sa.s + res->treename, res->sa.s + res->name) ;

    if ((res->logger.want && (res->type == TYPE_CLASSIC || res->type == TYPE_ONESHOT)) && !res->inns) {

        resolve_service_t lres = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref lwres = resolve_set_struct(DATA_SERVICE, &lres) ;

        r = resolve_read_g(lwres, info->base.s, res->sa.s + res->logger.name) ;
        if (r <= 0)
            log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", res->sa.s + res->logger.name) ;

        auto_remove(lres.sa.s + lres.path.servicedir) ;

        auto_remove(lres.sa.s + lres.logger.destination) ;

        tree_service_remove(info->base.s, lres.sa.s + lres.treename, lres.sa.s + lres.name) ;

        auto_strings(sym, lres.sa.s + lres.path.home, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", lres.sa.s + lres.name) ;

        log_trace("remove symlink: ", sym) ;
        unlink_void(sym) ;

        log_trace("remove symlink: ", lres.sa.s + lres.live.scandir) ;
        unlink_void(lres.sa.s + lres.live.scandir) ;

        log_info("removed successfully: ", lres.sa.s + lres.name) ;

        resolve_free(lwres) ;
    }

    auto_strings(sym, res->sa.s + res->path.home, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", res->sa.s + res->name) ;

    log_trace("remove symlink: ", sym) ;
    unlink_void(sym) ;

    log_trace("remove symlink: ", res->sa.s + res->live.scandir) ;
    unlink_void(res->sa.s + res->live.scandir) ;

    log_info("removed successfully: ", res->sa.s + res->name) ;
}

int ssexec_remove(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r ;
    size_t pos = 0 ;
    uint32_t flag = 0 ;
    uint8_t siglen = 0 ;
    unsigned int areslen = 0 ;
    ss_state_t ste = STATE_ZERO ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres = 0 ;

    FLAGS_SET(flag, STATE_FLAGS_TOPROPAGATE|STATE_FLAGS_ISSUPERVISED|STATE_FLAGS_TOUNSUPERVISE|STATE_FLAGS_WANTDOWN) ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;) {

            int opt = subgetopt_r(argc,argv, OPTS_START, &l) ;
            if (opt == -1) break ;

            switch (opt) {

                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 'P' :

                    FLAGS_CLEAR(flag, STATE_FLAGS_TOPROPAGATE) ;
                    siglen++ ;
                    break ;

                default :

                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(info->usage, "\n", info->help) ;

    resolve_service_t ares[SS_MAX_SERVICE + 1] ;
    memset(ares, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;

    for(; pos < argc ; pos++) {

        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        wres = resolve_set_struct(DATA_SERVICE, &res) ;

        r = resolve_read_g(wres, info->base.s, argv[pos]) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", argv[pos]) ;

        if (!r)
            log_dieu(LOG_EXIT_USER, "find service: ", argv[pos], " -- did you parse it?") ;

        if (!state_read(&ste, &res))
            log_dieusys(LOG_EXIT_SYS, "read state file of: ", argv[pos], " -- please make a bug report") ;

        if (ste.issupervised == STATE_FLAGS_TRUE)
            if (!sastr_add_string(&sa, argv[pos]))
                log_dieusys(LOG_EXIT_SYS, "add service: ", argv[pos], " to selection") ;

        ares[areslen++] = res ;

        if (!siglen)
            remove_deps(&res, ares, &areslen, &sa, info) ;
    }

    r = svc_scandir_ok(info->scandir.s) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "check: ", info->scandir.s) ;

    if (sa.len && r) {

        pos = 0 ;
        char const *prog = PROG ;
        int nargc = 2 + siglen + sastr_nelement(&sa) ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

        char const *help = info->help ;
        char const *usage = info->usage ;

        info->help = help_stop ;
        info->usage = usage_stop ;

        newargv[m++] = "stop" ;
        newargv[m++] = "-u" ;
        if (siglen)
            newargv[m++] = "-P" ;

        FOREACH_SASTR(&sa, pos)
            newargv[m++] = sa.s + pos ;

        newargv[m] = 0 ;

        PROG = "stop" ;
        if (ssexec_stop(nargc, newargv, info))
            log_dieu(LOG_EXIT_SYS, "stop service selection") ;
        PROG = prog ;

        info->help = help ;
        info->usage = usage ;
    }

    for (pos = 0 ; pos < areslen ; pos++) {

        remove_service(&ares[pos], info) ;

        if (ares[pos].type == TYPE_MODULE) {

            if (ares[pos].dependencies.ncontents) {

                size_t pos = 0 ;
                resolve_service_t mres = RESOLVE_SERVICE_ZERO ;
                resolve_wrapper_t_ref mwres = resolve_set_struct(DATA_SERVICE, &mres) ;
                _init_stack_(stk, strlen(ares[pos].sa.s + ares[pos].dependencies.contents)) ;

                if (!stack_convert_string_g(&stk, ares[pos].sa.s + ares[pos].dependencies.contents))
                    log_dieu(LOG_EXIT_SYS, "convert string") ;

                FOREACH_STK(&stk, pos) {

                    r = resolve_read_g(mwres, info->base.s, stk.s + pos) ;
                    if (r <= 0)
                        log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", stk.s + pos) ;

                    remove_service(&mres, info) ;
                }

                resolve_free(mwres) ;
            }

            /** remove directory made by the module at configuration time */
            char dir[SS_MAX_PATH_LEN + 1] ;

            if (!info->owner) {

                auto_strings(dir, SS_SERVICE_ADMDIR, ares[pos].sa.s + ares[pos].name) ;

            } else {

                if (!set_ownerhome_stack(dir))
                    log_dieusys(LOG_EXIT_SYS, "unable to find the home directory of the user") ;

                size_t dirlen = strlen(dir) ;

                auto_strings(dir + dirlen, SS_SERVICE_USERDIR, ares[pos].sa.s + ares[pos].name) ;
            }

            auto_remove(dir) ;
        }
    }

    stralloc_free(&sa) ;
    service_resolve_array_free(ares, areslen) ;
    free(wres) ;

    return 0 ;
}
