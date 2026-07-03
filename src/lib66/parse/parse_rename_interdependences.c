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
#include <66/utils.h>

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

static void parse_prefix_name(resolve_service_t *res, hash_t *hres, char const *prefix)
{
    log_flow() ;

    size_t mlen = strlen(prefix) ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    if (res->dependencies.ndepends) {

        size_t depslen = strlen(res->sa.s + res->dependencies.depends) ;
        _alloc_sbl_(stk, depslen + 1) ;

        if (!sbl_clean_string(&stk, res->sa.s + res->dependencies.depends))
            log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

        size_t len = (mlen + 1 + SS_MAX_TREENAME + 2) * res->dependencies.ndepends ;
        char n[len] ;

        memset(n, 0, len * sizeof(char)); ;

        parse_prefix(n, &stk, hres, prefix) ;

        res->dependencies.depends = resolve_add_string(wres, n) ;

    }

    if (res->dependencies.nrequiredby) {

        size_t depslen = strlen(res->sa.s + res->dependencies.requiredby) ;
        _alloc_sbl_(stk, depslen + 1) ;

        if (!sbl_clean_string(&stk, res->sa.s + res->dependencies.requiredby))
            log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

        size_t len = (mlen + 1 + SS_MAX_TREENAME + 2) * res->dependencies.nrequiredby ;
        char n[len] ;

        memset(n, 0, len * sizeof(char)) ;

        parse_prefix(n, &stk, hres, prefix) ;

        res->dependencies.requiredby = resolve_add_string(wres, n) ;

    }

    free(wres) ;
}

void parse_rename_interdependences(resolve_service_t *res, char const *prefix, hash_t *hres, ssexec_t *info)
{
    log_flow() ;

    struct resolve_hash_s *c, *tmp ;
    _alloc_sbl_(stk, resolve_hash_count(hres) * SS_MAX_SERVICE_NAME + 1) ;
    resolve_wrapper_t_ref wres = 0 ;

    HASH_FOREACH(hres, c, tmp) {

        if (!strcmp(c->res.sa.s + c->res.inns, prefix)) {

            if (c->res.dependencies.ndepends || c->res.dependencies.nrequiredby)
                parse_prefix_name(&c->res, hres, prefix) ;

            if (c->res.logger.want && (c->res.type == E_PARSER_TYPE_CLASSIC || c->res.type == E_PARSER_TYPE_ONESHOT)) {

                size_t namelen = strlen(c->res.sa.s + c->res.name) ;
                char logname[namelen + SS_LOG_SUFFIX_LEN + 1] ;
                wres = resolve_set_struct(DATA_SERVICE, &c->res) ;

                auto_strings(logname, c->res.sa.s + c->res.name, SS_LOG_SUFFIX) ;

                c->res.logger.name = resolve_add_string(wres, logname) ;

                c->res.logger.execute.run.runas = c->res.logger.execute.run.runas ? resolve_add_string(wres, c->res.sa.s + c->res.logger.execute.run.runas) : resolve_add_string(wres, SS_LOGGER_RUNNER) ;

                /** the validator answers "was the key set?" from the frontend;
                 * this post-parse path has only the resolve, so re-read the
                 * service's frontend (still on disk, just parsed) to build it. */
                char *fpath = c->res.sa.s + c->res.path.frontend ;
                char fdir[strlen(fpath) + 1], fname[strlen(fpath) + 1] ;
                _cleanup_strbuf_ strbuf fe = STRBUF_ZERO ;
                parse_validator_t validator ;

                if (!ob_dirname(fdir, fpath) || !ob_basename(fname, fpath))
                    log_dieu(LOG_EXIT_SYS, "split frontend path: ", fpath) ;

                if (read_svfile(&fe, fname, fdir) <= 0)
                    log_dieu(LOG_EXIT_SYS, "read frontend service at: ", fpath) ;

                if (!parse_validator_init(&validator, fe.s))
                    log_dieu(LOG_EXIT_SYS, "init parser validator of service: ", fname) ;

                parse_create_logger(&validator, hres, &c->res, info) ;

                if (c->res.type == E_PARSER_TYPE_CLASSIC) {
                    if (!sbl_add(&stk, logname))
                        log_die_nomem("stack overflow") ;
                }

                free(wres) ;

            }

            if (sbl_search(&stk, c->res.sa.s + c->res.name) < 0 )
                if (!sbl_add(&stk, c->res.sa.s + c->res.name))
                    log_die_nomem("stack overflow") ;
        }
    }

    wres = resolve_set_struct(DATA_SERVICE, res) ;

    res->dependencies.contents = parse_compute_list(wres, &stk, &res->dependencies.ncontents, 0) ;

    free(wres) ;
}
