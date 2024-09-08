/*
 * parse_rename_interdependencies.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <oblibs/lexer.h>

#include <66/parse.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/enum.h>
#include <66/constants.h>
#include <66/hash.h>

static void parse_prefix(char *result, stack *stk, struct resolve_hash_s **hres, char const *prefix)
{
    log_flow() ;

    size_t pos = 0, mlen = strlen(prefix) ;
    struct resolve_hash_s *hash ;

    FOREACH_STK(stk, pos) {

        hash = hash_search(hres, stk->s + pos) ;
        if (hash == NULL) {

            /** try with the name of the prefix as prefix */
            char tmp[mlen + 1 + strlen(stk->s + pos) + 1] ;

            auto_strings(tmp, prefix, ":", stk->s + pos) ;

            hash = hash_search(hres, tmp) ;
            if (hash == NULL)
                log_die(LOG_EXIT_USER, "service: ", stk->s + pos, " not available -- please make a bug report") ;
        }

        /** check if the dependencies is a external one. In this
         * case, the service is not considered as part of the ns */
        if (hash->res.inns && (!strcmp(hash->res.sa.s + hash->res.inns, prefix)) && !str_start_with(hash->res.sa.s + hash->res.name, prefix))
            auto_strings(result + strlen(result), prefix, ":", stk->s + pos, " ") ;
        else
            auto_strings(result + strlen(result), hash->res.sa.s + hash->res.name, " ") ;
    }

    result[strlen(result) - 1] = 0 ;
}

static void parse_prefix_name(resolve_service_t *res, struct resolve_hash_s **hres, char const *prefix)
{
    log_flow() ;

    size_t mlen = strlen(prefix) ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    if (res->dependencies.ndepends) {

        size_t depslen = strlen(res->sa.s + res->dependencies.depends) ;
        _alloc_stk_(stk, depslen + 1) ;

        if (!stack_string_clean(&stk, res->sa.s + res->dependencies.depends))
            log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

        size_t len = (mlen + 1 + SS_MAX_TREENAME + 2) * res->dependencies.ndepends ;
        char n[len] ;

        memset(n, 0, len * sizeof(char)); ;

        parse_prefix(n, &stk, hres, prefix) ;

        res->dependencies.depends = resolve_add_string(wres, n) ;

    }

    if (res->dependencies.nrequiredby) {

        size_t depslen = strlen(res->sa.s + res->dependencies.requiredby) ;
        _alloc_stk_(stk, depslen + 1) ;

        if (!stack_string_clean(&stk, res->sa.s + res->dependencies.requiredby))
            log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

        size_t len = (mlen + 1 + SS_MAX_TREENAME + 2) * res->dependencies.nrequiredby ;
        char n[len] ;

        memset(n, 0, len * sizeof(char)) ;

        parse_prefix(n, &stk, hres, prefix) ;

        res->dependencies.requiredby = resolve_add_string(wres, n) ;

    }

    free(wres) ;
}

void parse_rename_interdependences(resolve_service_t *res, char const *prefix, struct resolve_hash_s **hres, ssexec_t *info)
{
    log_flow() ;

    struct resolve_hash_s *c, *tmp ;
    _alloc_stk_(stk, hash_count(hres) * SS_MAX_SERVICE_NAME + 1) ;
    resolve_wrapper_t_ref wres = 0 ;

    HASH_ITER(hh, *hres, c, tmp) {

        if (!strcmp(c->res.sa.s + c->res.inns, prefix)) {

            if (c->res.dependencies.ndepends || c->res.dependencies.nrequiredby)
                parse_prefix_name(&c->res, hres, prefix) ;

            if (c->res.logger.want && (c->res.type == TYPE_CLASSIC || c->res.type == TYPE_ONESHOT)) {

                size_t namelen = strlen(c->res.sa.s + c->res.name) ;
                char logname[namelen + SS_LOG_SUFFIX_LEN + 1] ;
                wres = resolve_set_struct(DATA_SERVICE, &c->res) ;

                auto_strings(logname, c->res.sa.s + c->res.name, SS_LOG_SUFFIX) ;

                c->res.logger.name = resolve_add_string(wres, logname) ;

                c->res.logger.execute.run.runas = c->res.logger.execute.run.runas ? resolve_add_string(wres, c->res.sa.s + c->res.logger.execute.run.runas) : resolve_add_string(wres, SS_LOGGER_RUNNER) ;

                parse_create_logger(hres, &c->res, info) ;

                if (c->res.type == TYPE_CLASSIC) {
                    if (!stack_add_g(&stk, logname))
                        log_die_nomem("stack overflow") ;
                }

                free(wres) ;

            }

            if (stack_retrieve_element(&stk, c->res.sa.s + c->res.name) < 0 )
                if (!stack_add_g(&stk, c->res.sa.s + c->res.name))
                    log_die_nomem("stack overflow") ;
        }
    }

    wres = resolve_set_struct(DATA_SERVICE, res) ;

    res->dependencies.contents = parse_compute_list(wres, &stk, &res->dependencies.ncontents, 0) ;

    free(wres) ;
}
