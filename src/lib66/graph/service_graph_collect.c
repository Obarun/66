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

#include <66/graph.h>
#include <66/ssexec.h>
#include <66/state.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/event_rule.h>
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

    hash = resolve_hash_search(&g->hres, name) ;

    if (hash == NULL) {

        /** double pass with resolve_read.
         * The service may already exist, respects the treename before the
         * call of sanitize_source if the -t option was not set by user.
         * The service do not exist yet, sanitize it with sanitize_source
         * and read again the resolve file to know the change */
        r = resolve_read(wres, info->base.s, name) ;
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
            if (resolve_read(wres, info->base.s, name) <= 0)
                log_dieu(LOG_EXIT_SYS, "read resolve file of: ", name, " -- please make a bug report") ;

        if (!state_read(&ste, &res))
            log_dieu(LOG_EXIT_SYS, "read state file of: ", name, " -- please make a bug report") ;

        log_trace("add service: ", name, " to the service selection") ;
        if (!resolve_hash_add(&g->hres, name, res))
            log_dieu(LOG_EXIT_SYS, "append service selection with: ", name) ;

        n++ ;

        /** the dependencies live in an autonomous addon; load it into the hash
         * node so the edges can be walked (gated on the core has_dependencies). */
        struct resolve_hash_s *added = resolve_hash_search(&g->hres, name) ;
        resolve_service_addon_dependencies_t *dep = &added->dependencies ;
        if (res.has_dependencies) {
            resolve_wrapper_t_ref wdep = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, dep) ;
            if (resolve_read(wdep, info->base.s, name) <= 0)
                log_dieu(LOG_EXIT_SYS, "read dependencies addon of: ", name) ;
            free(wdep) ;
        }

        if (dep->ndepends) {

            size_t len = strlen(dep->sa.s + dep->depends) ;
            _alloc_sbl_(stk, len + 1) ;

            if (!sbl_clean_string(&stk, dep->sa.s + dep->depends))
                log_dieusys(LOG_EXIT_SYS, "clean string") ;

            n += service_graph_ncollect(g, stk.s, stk.len, info, flag) ;
        }

        if (dep->nrequiredby) {

            size_t len = strlen(dep->sa.s + dep->requiredby) ;
            _alloc_sbl_(stk, len + 1) ;

            if (!sbl_clean_string(&stk, dep->sa.s + dep->requiredby))
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
        if (res.type == E_PARSER_TYPE_MODULE && dep->ncontents) {

            size_t len = strlen(dep->sa.s + dep->contents) ;
            _alloc_sbl_(stk, len + 1) ;

            if (!sbl_clean_string(&stk, dep->sa.s + dep->contents))
                log_dieusys(LOG_EXIT_SYS, "clean string") ;

            n += service_graph_ncollect(g, stk.s, stk.len, info, flag) ;
        }

        /* a service/signal reactor's From sources are establishment edges: pull
         * them into the selection so the arm supervises them before eventd
         * subscribes. Read from the event addon (single source of truth), gated on
         * GRAPH_WANT_EVENTDEPS so a fire-time start (opt_react) never re-pulls. */
        if (FLAGS_ISSET(flag, GRAPH_WANT_EVENTDEPS) && res.has_event) {

            resolve_service_addon_event_t *ev = &added->event ;
            resolve_wrapper_t_ref wev = resolve_set_struct(DATA_SERVICE_EVENT, ev) ;
            if (resolve_read(wev, info->base.s, name) <= 0)
                log_dieu(LOG_EXIT_SYS, "read event addon of: ", name) ;
            free(wev) ;

            if ((ev->type == EVENT_SOURCE_SERVICE || ev->type == EVENT_SOURCE_SIGNAL) && ev->nfrom) {

                size_t len = strlen(ev->sa.s + ev->from) ;
                _alloc_sbl_(stk, len + 1) ;

                if (!sbl_clean_string(&stk, ev->sa.s + ev->from))
                    log_dieusys(LOG_EXIT_SYS, "clean string") ;

                n += service_graph_ncollect(g, stk.s, stk.len, info, flag) ;
            }
        }
    }

    free(wres) ;
    return n ;
}

