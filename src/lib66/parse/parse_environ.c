/*
 * parse_environ.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdlib.h> // free
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/environ.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>
#include <66/environ.h>

static int env_import_field(resolve_service_addon_environ_t *e, resolve_wrapper_t_ref ewres, strbuf *store)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf list = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf clean = STRBUF_ZERO ;
    size_t pos = 0 ;
    uint32_t n = 0 ;

    if (!environ_merge_string(&sa, store->s))
        log_warnusys_return(LOG_EXIT_SYS, "merge environment string") ;

    FOREACH_SBL(&sa, pos) {

        char *line = sa.s + pos ;
        _alloc_sbl_(key, strlen(line)) ;

        if (!environ_get_key(&key, line))
            return 0 ;

        if (!strcmp(key.s, enum_str_parser_section_environ[E_PARSER_SECTION_ENVIRON_IMPORTFILE])) {

            _alloc_sbl_(val, strlen(line)) ;

            if (!environ_get_value(&val, line))
                return 0 ;

            if (val.s[0] != '/')
                log_warnu_return(LOG_EXIT_ZERO, "ImportFile must be an absolute path: ", val.s) ;

            if (scan_mode(val.s, S_IFDIR) > 0)
                log_warnu_return(LOG_EXIT_ZERO, "ImportFile is a directory: ", val.s, " -- only file are allowed") ;

            if (!sbl_add(&list, val.s))
                return 0 ;

            n++ ;

        } else if (!sbl_add(&clean, line))
            return 0 ;
    }

    if (list.len) {

        if (!sbl_rebuild_oneline(&list))
            return 0 ;

        e->nimportfile = n ;
        e->importfile = resolve_add_string(ewres, list.s) ;
    }

    store->len = 0 ;

    if (!environ_untrim(store, &clean) || !strbuf_uncounted(store))
        return 0 ;

    return 1 ;
}

int parse_environ(struct resolve_hash_s *c, parse_build_ctx_t *ctx)
{
    log_flow() ;

    parse_store_t *st = ctx->st ;
    resolve_service_t *res = &c->res ;
    resolve_service_addon_environ_t *e = &c->environ ;
    uint8_t conf = ctx->conf ;

    res->has_environ = 0 ;

    if (!parse_store_present(st, E_PARSER_SECTION_ENVIRONMENT, E_PARSER_SECTION_ENVIRON_ENVAL))
        return 1 ;

    res->has_environ = 1 ;

    size_t len = 0 ;
    char const *raw = parse_store_get(st, E_PARSER_SECTION_ENVIRONMENT, E_PARSER_SECTION_ENVIRON_ENVAL, &len) ;

    _cleanup_strbuf_ strbuf store = STRBUF_ZERO ;
    if (!strbuf_copyb(&store, raw, len) || !strbuf_uncounted(&store))
        return 0 ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    resolve_wrapper_t_ref ewres = resolve_set_struct(DATA_SERVICE_ENVIRON, e) ;

    resolve_init(ewres) ; // offset 0 = "" convention

    if (!env_import_field(e, ewres, &store)) {
        free(ewres) ;
        return 0 ;
    }

    e->env = resolve_add_string(ewres, store.s) ;

    if (!env_resolve_conf(&sa, res)) {
        free(ewres) ;
        return 0 ;
    }

    e->envdir = resolve_add_string(ewres, sa.s) ;

    e->env_overwrite = conf ;

    free(ewres) ;

    return 1 ;
}
