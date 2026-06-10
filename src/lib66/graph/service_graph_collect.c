/*
 * graph_collect.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/types.h>
#include <oblibs/sbl.h>
#include <oblibs/lexer.h>

#include <66/graph.h>
#include <66/ssexec.h>
#include <66/state.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/sanitize.h>
#include <66/enum_parser.h>

uint32_t service_graph_ncollect(service_graph_t *g, const char *list, size_t len, ssexec_t *info, uint32_t flag)
{
    log_flow() ;

    uint32_t n = 0 ;
    size_t pos = 0 ;

    for (; pos < len ; pos += strlen(list + pos) + 1)
        n += service_graph_collect(g, list + pos, info, flag) ;

    return n ;
}

uint32_t service_graph_collect(service_graph_t *g, const char *name, ssexec_t *info, uint32_t flag)
{
    log_flow() ;

    int r ;
    uint32_t n = 0 ;
    bool readagain = false ;
    ss_state_t ste = STATE_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    struct resolve_hash_s *hash = NULL ;

    hash = hash_search(&g->hres, name) ;

    if (hash == NULL) {

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

                if (!auto_strbuf(&info->treename, res.sa.s + res.treename))
                    log_die_nomem("strbuf") ;
            }

        }

        if (!r) {
            if (FLAGS_ISSET(flag, GRAPH_COLLECT_PARSE)) {
                readagain = true ;
                sanitize_source(name, info, flag) ;
            } else {
                resolve_free(wres) ;
                return n ;
            }
        }

        if (readagain)
            if (resolve_read_g(wres, info->base.s, name) <= 0)
                log_dieu(LOG_EXIT_SYS, "read resolve file of: ", name, " -- please make a bug report") ;

        if (!state_read(&ste, &res))
            log_dieu(LOG_EXIT_SYS, "read state file of: ", name, " -- please make a bug report") ;

        log_trace("add service: ", name, " to the service selection") ;
        if (!hash_add(&g->hres, name, res))
            log_dieu(LOG_EXIT_SYS, "append service selection with: ", name) ;

        n++ ;

        if (res.dependencies.ndepends) {

            size_t len = strlen(res.sa.s + res.dependencies.depends) ;
            _alloc_sbl_(stk, len + 1) ;

            if (!sbl_clean_string(&stk, res.sa.s + res.dependencies.depends))
                log_dieusys(LOG_EXIT_SYS, "clean string") ;

            n += service_graph_ncollect(g, stk.s, stk.len, info, flag) ;
        }

        if (res.dependencies.nrequiredby) {

            size_t len = strlen(res.sa.s + res.dependencies.requiredby) ;
            _alloc_sbl_(stk, len + 1) ;

            if (!sbl_clean_string(&stk, res.sa.s + res.dependencies.requiredby))
                log_dieusys(LOG_EXIT_SYS, "clean string") ;

            n += service_graph_ncollect(g, stk.s, stk.len, info, flag) ;
        }

        /**
         * In case of crash of a command and for whatever the reason, the
         * service inside the module may not corresponds to the state of the
         * module itself.
         *
         * Whatever the current state of service inside the module, we keep
         * trace of its because others commands will look for these inner services.
         *
         * At the end of any process, the ssexec_signal will deal properly
         * with the current state and the desire state of the service. */
        if (res.type == E_PARSER_TYPE_MODULE && res.dependencies.ncontents) {

            size_t len = strlen(res.sa.s + res.dependencies.contents) ;
            _alloc_sbl_(stk, len + 1) ;

            if (!sbl_clean_string(&stk, res.sa.s + res.dependencies.contents))
                log_dieusys(LOG_EXIT_SYS, "clean string") ;

            n += service_graph_ncollect(g, stk.s, stk.len, info, flag) ;
        }
    }

    free(wres) ;
    return n ;
}

