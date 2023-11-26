/*
 * svc_remove.c
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
#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/stack.h>

#include <skalibs/posixplz.h>

#include <66/svc.h>
#include <66/service.h>
#include <66/ssexec.h>
#include <66/tree.h>
#include <66/enum.h>
#include <66/resolve.h>
#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>

static void auto_remove(char const *path)
{
    log_trace("remove directory: ", path) ;
    if (!dir_rm_rf(path))
        log_dieusys(LOG_EXIT_SYS, "remove directory: ", path) ;
}

static void remove_service(resolve_service_t *res, ssexec_t *info)
{
    log_flow() ;

    char sym[strlen(res->sa.s + res->path.home) + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + SS_MAX_SERVICE_NAME + 1] ;

    auto_remove(res->sa.s + res->path.servicedir) ;

    if (res->environ.envdir)
        auto_remove(res->sa.s + res->environ.envdir) ;

    tree_service_remove(info->base.s, res->sa.s + res->treename, res->sa.s + res->name) ;

    auto_strings(sym, res->sa.s + res->path.home, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", res->sa.s + res->name) ;

    log_trace("remove symlink: ", sym) ;
    unlink_void(sym) ;

    log_trace("remove symlink: ", res->sa.s + res->live.scandir) ;
    unlink_void(res->sa.s + res->live.scandir) ;

    log_info("removed successfully: ", res->sa.s + res->name) ;
}

void svc_remove(unsigned int *alist, unsigned int alen, graph_t *graph, resolve_service_t *ares, unsigned int areslen, ssexec_t *info)
{

    log_flow() ;

    int r ;
    unsigned int pos = 0, aresid = 0 ;

    for (pos = 0 ; pos < alen ; pos++) {

        char *name = graph->data.s + genalloc_s(graph_hash_t, &graph->hash)[alist[pos]].vertex ;

        aresid = service_resolve_array_search(ares, areslen, name) ;
        if (aresid < 0)
            log_die(LOG_EXIT_USER, "service: ", name, " not available -- please make a bug report") ;

        remove_service(&ares[aresid], info) ;

        if (ares[aresid].type == TYPE_MODULE) {

            if (ares[aresid].dependencies.ncontents) {

                size_t idx = 0 ;
                resolve_service_t mres = RESOLVE_SERVICE_ZERO ;
                resolve_wrapper_t_ref mwres = resolve_set_struct(DATA_SERVICE, &mres) ;
                _init_stack_(stk, strlen(ares[aresid].sa.s + ares[aresid].dependencies.contents)) ;

                if (!stack_convert_string_g(&stk, ares[aresid].sa.s + ares[aresid].dependencies.contents))
                    log_dieu(LOG_EXIT_SYS, "convert string") ;

                FOREACH_STK(&stk, idx) {

                    r = resolve_read_g(mwres, info->base.s, stk.s + idx) ;
                    if (r <= 0)
                        log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", stk.s + idx) ;

                    remove_service(&mres, info) ;
                }

                resolve_free(mwres) ;
            }

            /** remove directory made by the module at configuration time */
            char dir[SS_MAX_PATH_LEN + 1] ;

            if (!info->owner) {

                auto_strings(dir, SS_SERVICE_ADMDIR, ares[aresid].sa.s + ares[aresid].name) ;

            } else {

                if (!set_ownerhome_stack(dir))
                    log_dieusys(LOG_EXIT_SYS, "unable to find the home directory of the user") ;

                size_t dirlen = strlen(dir) ;

                auto_strings(dir + dirlen, SS_SERVICE_USERDIR, ares[aresid].sa.s + ares[aresid].name) ;
            }

            auto_remove(dir) ;
        }
    }
}
