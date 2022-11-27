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
#include <oblibs/sastr.h>
#include <oblibs/types.h>

#include <skalibs/stralloc.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/sanitize.h>
#include <66/state.h>
#include <66/graph.h>
#include <66/enum.h>

/** list all services of the system
 * STATE_FLAGS_TOINIT -> it call sanitize_source() function.
 * STATE_FLAGS_TOPROPAGATE -> it build with the dependencies/requiredby services.
 * STATE_FLAGS_ISSUPERVISED -> only keep already supervised service*/
void service_graph_collect(graph_t *g, char const *alist, size_t alen, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info, uint32_t flag)
{
    log_flow () ;

    size_t pos = 0 ;
    ss_state_t ste = STATE_ZERO ;
    stralloc sa = STRALLOC_ZERO ;

    resolve_wrapper_t_ref wres = 0 ;

    for (; pos < alen ; pos += strlen(alist + pos) + 1) {

        sa.len = 0 ;
        char const *name = alist + pos ;

        if (service_resolve_array_search(ares, (*areslen), name) < 0) {

            resolve_service_t res = RESOLVE_SERVICE_ZERO ;
            /** need to make a copy of the resolve due of the freed
             * of the wres struct at the end of the process */
            resolve_service_t cp = RESOLVE_SERVICE_ZERO ;
            wres = resolve_set_struct(DATA_SERVICE, &res) ;

            if (FLAGS_ISSET(flag, STATE_FLAGS_TOINIT))
                sanitize_source(name, info, flag) ;

            if (!resolve_read_g(wres, info->base.s, name))
                log_dieu(LOG_EXIT_SYS, "read resolve file of: ", name, " -- please make a bug report") ;

            if (FLAGS_ISSET(flag, STATE_FLAGS_ISSUPERVISED)) {

                if (!state_read(&ste, res.sa.s + res.path.home, name))
                    log_dieu(LOG_EXIT_SYS, "read state file of: ", name, " -- please make a bug report") ;

                if (service_is(&ste, STATE_FLAGS_ISSUPERVISED)) {

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

                if (res.dependencies.ndepends && FLAGS_ISSET(flag, STATE_FLAGS_WANTDOWN)) {

                    if (!sastr_clean_string(&sa, res.sa.s + res.dependencies.depends))
                        log_dieu(LOG_EXIT_SYS, "clean string") ;

                    service_graph_collect(g, sa.s, sa.len, ares, areslen, info, flag) ;

                } else if (res.dependencies.nrequiredby && FLAGS_ISSET(flag, STATE_FLAGS_WANTUP)) {

                    if (!sastr_clean_string(&sa, res.sa.s + res.dependencies.requiredby))
                        log_dieu(LOG_EXIT_SYS, "clean string") ;

                    service_graph_collect(g, sa.s, sa.len, ares, areslen, info, flag) ;
                }
            }
            resolve_free(wres) ;
        }
    }

    stralloc_free(&sa) ;
}
