/*
 * parse_rename_interdependences.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
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
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/hash.h>

#include <66/parse.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/enum_parser.h>
#include <66/constants.h>

static void parse_prefix(char *result, strbuf *stk, hash_t *hres, char const *prefix)
{
    log_flow() ;

    size_t pos = 0, mlen = strlen(prefix) ;
    struct resolve_hash_s *hash ;

    FOREACH_SBL(stk, pos) {

        hash = resolve_hash_search(hres, stk->s + pos) ;
        if (hash == NULL) {

            /** try with the name of the prefix as prefix */
            char tmp[mlen + 1 + strlen(stk->s + pos) + 1] ;

            auto_strings(tmp, prefix, ":", stk->s + pos) ;

            hash = resolve_hash_search(hres, tmp) ;
            if (hash == NULL)
                log_die(LOG_EXIT_USER, "service: ", stk->s + pos, " not available -- please make a bug report") ;
        }

        /** check if the dependencies is a external one. In this
         * case, the service is not considered as part of the ns */
        if (hash->res.inns && (!strcmp(hash->res.sa.s + hash->res.inns, prefix)) && str_start_with(hash->res.sa.s + hash->res.name, prefix))
            auto_strings(result + strlen(result), prefix, ":", stk->s + pos, " ") ;
        else
            auto_strings(result + strlen(result), hash->res.sa.s + hash->res.name, " ") ;
    }

    result[strlen(result) - 1] = 0 ;
}

static void parse_prefix_name(resolve_service_addon_dependencies_t *dep, hash_t *hres, char const *prefix)
{
    log_flow() ;

    size_t mlen = strlen(prefix) ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, dep) ;

    if (dep->ndepends) {

        size_t depslen = strlen(dep->sa.s + dep->depends) ;
        _alloc_sbl_(stk, depslen + 1) ;

        if (!sbl_clean_string(&stk, dep->sa.s + dep->depends))
            log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

        size_t len = (mlen + 1 + SS_MAX_TREENAME + 2) * dep->ndepends ;
        char n[len] ;

        memset(n, 0, len * sizeof(char)); ;

        parse_prefix(n, &stk, hres, prefix) ;

        dep->depends = resolve_add_string(wres, n) ;

    }

    if (dep->nrequiredby) {

        size_t depslen = strlen(dep->sa.s + dep->requiredby) ;
        _alloc_sbl_(stk, depslen + 1) ;

        if (!sbl_clean_string(&stk, dep->sa.s + dep->requiredby))
            log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

        size_t len = (mlen + 1 + SS_MAX_TREENAME + 2) * dep->nrequiredby ;
        char n[len] ;

        memset(n, 0, len * sizeof(char)) ;

        parse_prefix(n, &stk, hres, prefix) ;

        dep->requiredby = resolve_add_string(wres, n) ;

    }

    free(wres) ;
}

void parse_rename_interdependences(resolve_service_t *res, resolve_service_addon_dependencies_t *dep, char const *prefix, hash_t *hres, ssexec_t *info)
{
    log_flow() ;

    struct resolve_hash_s *c, *tmp ;
    _alloc_sbl_(stk, resolve_hash_count(hres) * SS_MAX_SERVICE_NAME + 1) ;
    resolve_wrapper_t_ref wres = 0 ;

    HASH_FOREACH(hres, c, tmp) {

        if (!strcmp(c->res.sa.s + c->res.inns, prefix)) {

            if (c->dependencies.ndepends || c->dependencies.nrequiredby)
                parse_prefix_name(&c->dependencies, hres, prefix) ;

            if (c->res.logger && (c->res.type == E_PARSER_TYPE_CLASSIC || c->res.type == E_PARSER_TYPE_ONESHOT)) {

                size_t namelen = strlen(c->res.sa.s + c->res.name) ;
                char logname[namelen + SS_LOG_SUFFIX_LEN + 1] ;

                // the logger name is always <service>-log; the logger config lives
                // in c->logger, untouched by the rename above.
                auto_strings(logname, c->res.sa.s + c->res.name, SS_LOG_SUFFIX) ;

                parse_create_logger(hres, c, info) ;

                if (c->res.type == E_PARSER_TYPE_CLASSIC) {
                    if (!sbl_add(&stk, logname))
                        log_die_nomem("stack overflow") ;
                }

            }

            if (sbl_search(&stk, c->res.sa.s + c->res.name) < 0 )
                if (!sbl_add(&stk, c->res.sa.s + c->res.name))
                    log_die_nomem("stack overflow") ;
        }
    }

    (void)res ;
    wres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, dep) ;

    dep->contents = parse_compute_list(wres, &stk, &dep->ncontents, 0) ;

    free(wres) ;
}
