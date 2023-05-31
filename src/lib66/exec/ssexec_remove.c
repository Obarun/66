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

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
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

static void auto_remove(char const *path)
{
    log_trace("remove directory: ", path) ;
    if (!dir_rm_rf(path))
        log_dieusys(LOG_EXIT_SYS, "remove directory: ", path) ;
}

static void remove_service(resolve_service_t *res, ssexec_t *info)
{
    int r ;
    char *path = 0 ;
    char sym[strlen(res->sa.s + res->path.home) + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + SS_MAX_SERVICE_NAME + 1] ;

    path = realpath(res->sa.s + res->path.servicedir, 0) ;
    if (!path)
        log_dieusys(LOG_EXIT_SYS, "retrieve real path of: ", res->sa.s + res->path.servicedir) ;

    auto_remove(path) ;

    free(path) ;

    tree_service_remove(info->base.s, res->sa.s + res->treename, res->sa.s + res->name) ;

    if ((res->logger.want && (res->type == TYPE_CLASSIC || res->type == TYPE_ONESHOT)) && !res->inmodule) {

        resolve_service_t lres = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref lwres = resolve_set_struct(DATA_SERVICE, &lres) ;

        r = resolve_read_g(lwres, info->base.s, res->sa.s + res->logger.name) ;
        if (r <= 0)
            log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", res->sa.s + res->logger.name) ;

        path = realpath(lres.sa.s + lres.path.servicedir, 0) ;
        if (!path)
            log_dieusys(LOG_EXIT_SYS, "retrieve real path of: ", lres.sa.s + lres.path.servicedir) ;

        auto_remove(path) ;

        free(path) ;

        auto_remove(lres.sa.s + lres.logger.destination) ;

        tree_service_remove(info->base.s, lres.sa.s + lres.treename, lres.sa.s + lres.name) ;

        auto_strings(sym, lres.sa.s + lres.path.home, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", lres.sa.s + lres.name) ;

        log_trace("remove symlink: ", sym) ;
        unlink_void(sym) ;

        log_info("removed successfully: ", lres.sa.s + lres.name) ;

        resolve_free(lwres) ;
    }

    auto_strings(sym, res->sa.s + res->path.home, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", res->sa.s + res->name) ;

    log_trace("remove symlink: ", sym) ;
    unlink_void(sym) ;

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

    resolve_service_t ares[argc] ;

    for(; pos < argc ; pos++) {

        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        wres = resolve_set_struct(DATA_SERVICE, &res) ;

        r = resolve_read_g(wres, info->base.s, argv[pos]) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", argv[pos]) ;

        if (!r)
            log_dieu(LOG_EXIT_USER, "find service: ", argv[pos], " -- did you parsed it ?") ;

        if (!state_read(&ste, &res))
            log_dieusys(LOG_EXIT_SYS, "read state file of: ", argv[pos], " -- please make a bug report") ;

        if (service_is(&ste, STATE_FLAGS_ISSUPERVISED) == STATE_FLAGS_TRUE)
            if (!sastr_add_string(&sa, argv[pos]))
                log_dieusys(LOG_EXIT_SYS, "add service: ", argv[pos], " to selection") ;

        ares[areslen++] = res ;
    }

    if (sa.len) {

        pos = 0 ;
        char const *prog = PROG ;
        int nargc = 2 + siglen + sastr_nelement(&sa) ;
        char const *newargv[nargc] ;
        unsigned int m = 0 ;

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
    }

    for (pos = 0 ; pos < areslen ; pos++) {

        remove_service(&ares[pos], info) ;

        if (ares[pos].type == TYPE_MODULE) {

            if (ares[pos].dependencies.ncontents) {

                size_t pos = 0 ;
                sa.len = 0 ;
                resolve_service_t mres = RESOLVE_SERVICE_ZERO ;
                resolve_wrapper_t_ref mwres = resolve_set_struct(DATA_SERVICE, &mres) ;

                if (!sastr_clean_string(&sa, ares[pos].sa.s + ares[pos].dependencies.contents))
                    log_dieu(LOG_EXIT_SYS, "clean string") ;

                FOREACH_SASTR(&sa, pos) {

                    r = resolve_read_g(mwres, info->base.s, sa.s + pos) ;
                    if (r <= 0)
                        log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", sa.s + pos) ;

                    remove_service(&mres, info) ;
                }

                resolve_free(mwres) ;
            }
        }
    }

    stralloc_free(&sa) ;
    service_resolve_array_free(ares, areslen) ;
    free(wres) ;

    return 0 ;
}
