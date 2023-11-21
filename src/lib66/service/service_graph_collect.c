/*
 * service_graph_collect.c
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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/stack.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/sanitize.h>
#include <66/state.h>
#include <66/graph.h>
#include <66/enum.h>

/** list all services of the system dependending of the flag passed.
 * STATE_FLAGS_TOPARSE -> call sanitize_source
 * STATE_FLAGS_TOPROPAGATE -> it build with the dependencies/requiredby services.
 * STATE_FLAGS_ISSUPERVISED -> only keep already supervised service*/
void service_graph_collect(graph_t *g, char const *slist, size_t slen, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info, uint32_t flag)
{
    log_flow () ;

    int r ;
    size_t pos = 0 ;
    ss_state_t ste = STATE_ZERO ;

    resolve_wrapper_t_ref wres = 0 ;

    for (; pos < slen ; pos += strlen(slist + pos) + 1) {

        char const *name = slist + pos ;

        if (service_resolve_array_search(ares, (*areslen), name) < 0) {

            resolve_service_t res = RESOLVE_SERVICE_ZERO ;
            /** need to make a copy of the resolve due of the freed
             * of the wres struct at the end of the process */
            resolve_service_t cp = RESOLVE_SERVICE_ZERO ;
            wres = resolve_set_struct(DATA_SERVICE, &res) ;

            /** double pass with resolve_read.
             * The service may already exist, respects the treename before the
             * call of sanitize_source if the -t option was not set by user.
             * The service do not exist yet, sanitize it with sanitize_source
             * and read again the resolve file to know the change */
            r = resolve_read_g(wres, info->base.s, name) ;
            if (r < 0)
                log_dieu(LOG_EXIT_SYS, "read resolve file: ", name) ;

            if (r) {

                if (!info->opt_tree) {

                    info->treename.len = 0 ;

                    if (!auto_stra(&info->treename, res.sa.s + res.treename))
                        log_die_nomem("stralloc") ;
                }

            } else if (FLAGS_ISSET(flag, STATE_FLAGS_TOPARSE)) {

                sanitize_source(name, info, flag) ;

            } else
                continue ;

            if (resolve_read_g(wres, info->base.s, name) <= 0)
                log_dieu(LOG_EXIT_SYS, "read resolve file of: ", name, " -- please make a bug report") ;

            if (!state_read(&ste, &res))
                log_dieu(LOG_EXIT_SYS, "read state file of: ", name, " -- please make a bug report") ;

            if (FLAGS_ISSET(flag, STATE_FLAGS_ISEARLIER)) {

                if (res.earlier) {

                    if (!service_resolve_copy(&cp, &res))
                        log_dieu(LOG_EXIT_SYS, "copy resolve file of: ", name, " -- please make a bug report") ;

                    ares[(*areslen)++] = cp ;
                    continue ;
                }
                resolve_free(wres) ;
                continue ;
            }

            if (FLAGS_ISSET(flag, STATE_FLAGS_ISSUPERVISED)) {

                if (ste.issupervised == STATE_FLAGS_TRUE) {

                    if (!service_resolve_copy(&cp, &res))
                        log_dieu(LOG_EXIT_SYS, "copy resolve file of: ", name, " -- please make a bug report") ;

                    ares[(*areslen)++] = cp ;

                } else {
                    resolve_free(wres) ;
                    continue ;
                }

            } else {

                if (!service_resolve_copy(&cp, &res))
                    log_dieu(LOG_EXIT_SYS, "copy resolve file of: ", name, " -- please make a bug report") ;

                ares[(*areslen)++] = cp ;
            }

            if (FLAGS_ISSET(flag, STATE_FLAGS_TOPROPAGATE)) {

                if (res.dependencies.ndepends && FLAGS_ISSET(flag, STATE_FLAGS_WANTUP)) {

                    size_t len = strlen(res.sa.s + res.dependencies.depends) ;
                    _init_stack_(stk, len + 1) ;
                    if (!stack_convert_string(&stk, res.sa.s + res.dependencies.depends, len))
                        log_dieusys(LOG_EXIT_SYS, "clean string") ;

                    service_graph_collect(g, stk.s, stk.len, ares, areslen, info, flag) ;

                }

                if (res.dependencies.nrequiredby && FLAGS_ISSET(flag, STATE_FLAGS_WANTDOWN)) {

                    size_t len = strlen(res.sa.s + res.dependencies.requiredby) ;
                    _init_stack_(stk, len + 1) ;
                    if (!stack_convert_string(&stk, res.sa.s + res.dependencies.requiredby, len))
                        log_dieusys(LOG_EXIT_SYS, "clean string") ;

                    service_graph_collect(g, stk.s, stk.len, ares, areslen, info, flag) ;
                }

            }
            resolve_free(wres) ;
        }
    }
}
