/*
 * parse_rename_interdependencies.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>
#include <oblibs/stack.h>

#include <skalibs/stralloc.h>

#include <66/parse.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/enum.h>
#include <66/constants.h>

static void parse_prefix(char *result, stack *stk, resolve_service_t *ares, unsigned int areslen, char const *prefix)
{
    log_flow() ;

    int aresid = -1 ;
    size_t pos = 0, mlen = strlen(prefix) ;

    FOREACH_STK(stk, pos) {

        aresid = service_resolve_array_search(ares, areslen, stk->s + pos) ;

        if (aresid < 0) {
            /** try with the name of the prefix as prefix */
            char tmp[mlen + 1 + strlen(stk->s + pos) + 1] ;

            auto_strings(tmp, prefix, ":", stk->s + pos) ;

            aresid = service_resolve_array_search(ares, areslen, tmp) ;
            if (aresid < 0)
                log_die(LOG_EXIT_USER, "service: ", stk->s + pos, " not available -- please make a bug report") ;
        }

        /** check if the dependencies is a external one. In this
         * case, the service is not considered as part of the prefix */
        if (ares[aresid].inns && (!strcmp(ares[aresid].sa.s + ares[aresid].inns, prefix)))
            auto_strings(result + strlen(result), prefix, ":", stk->s + pos, " ") ;
        else
            auto_strings(result + strlen(result), stk->s + pos, " ") ;
    }

    result[strlen(result) - 1] = 0 ;
}

static void parse_prefix_name(unsigned int idx, resolve_service_t *ares, unsigned int areslen, char const *prefix)
{
    log_flow() ;

    size_t mlen = strlen(prefix) ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &ares[idx]) ;

    if (ares[idx].dependencies.ndepends) {

        size_t depslen = strlen(ares[idx].sa.s + ares[idx].dependencies.depends) ;
        _init_stack_(stk, depslen + 1) ;

        if (!stack_convert_string(&stk, ares[idx].sa.s + ares[idx].dependencies.depends, depslen))
            log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

        size_t len = (mlen + 1 + SS_MAX_TREENAME + 2) * ares[idx].dependencies.ndepends ;
        char n[len] ;

        memset(n, 0, len * sizeof(char)); ;

        parse_prefix(n, &stk, ares, areslen, prefix) ;

        ares[idx].dependencies.depends = resolve_add_string(wres, n) ;

    }

    if (ares[idx].dependencies.nrequiredby) {

        size_t depslen = strlen(ares[idx].sa.s + ares[idx].dependencies.requiredby) ;
        _init_stack_(stk, depslen + 1) ;

        if (!stack_convert_string(&stk, ares[idx].sa.s + ares[idx].dependencies.requiredby, depslen))
            log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

        size_t len = (mlen + 1 + SS_MAX_TREENAME + 2) * ares[idx].dependencies.nrequiredby ;
        char n[len] ;

        memset(n, 0, len * sizeof(char)) ;

        parse_prefix(n, &stk, ares, areslen, prefix) ;

        ares[idx].dependencies.requiredby = resolve_add_string(wres, n) ;

    }

    free(wres) ;
}

void parse_rename_interdependences(resolve_service_t *res, char const *prefix, resolve_service_t *ares, unsigned int *areslen)
{
    log_flow() ;

    unsigned int aresid = 0 ;
    size_t plen = strlen(prefix) ;
    unsigned int visit[SS_MAX_SERVICE + 1] ;
    resolve_wrapper_t_ref awres = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    memset(visit, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;

    for (; aresid < *areslen ; aresid++) {

        if (!strcmp(ares[aresid].sa.s + ares[aresid].inns, prefix)) {

            if (ares[aresid].dependencies.ndepends || ares[aresid].dependencies.nrequiredby)
                parse_prefix_name(aresid, ares, *areslen, prefix) ;

            if (ares[aresid].logger.want && ares[aresid].type == TYPE_CLASSIC) {

                char n[strlen(ares[aresid].sa.s + ares[aresid].name) + SS_LOG_SUFFIX_LEN + 1] ;

                auto_strings(n, ares[aresid].sa.s + ares[aresid].name, SS_LOG_SUFFIX) ;

                if (!sastr_add_string(&sa, n))
                    log_die_nomem("stralloc") ;
            }

            char tmp[plen + 1 + strlen(ares[aresid].sa.s + ares[aresid].name) + 1] ;

            auto_strings(tmp, prefix, ":", ares[aresid].sa.s + ares[aresid].name) ;

            if (!sastr_add_string(&sa, ares[aresid].sa.s + ares[aresid].name))
                log_die_nomem("stralloc") ;
        }
    }

    res->dependencies.contents = parse_compute_list(wres, &sa, &res->dependencies.ncontents, 0) ;

    stralloc_free(&sa) ;
    free(wres) ;
    free(awres) ;
}