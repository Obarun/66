/*
 * parse_store_environ.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdlib.h> //free
#include <errno.h>
#include <sys/stat.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/environ.h>
#include <oblibs/types.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum_parser.h>
#include <66/environ.h>

static int get_import_field(resolve_service_t *res, strbuf *store)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf list = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf modif = STRBUF_ZERO ;
    size_t pos = 0 ;
    uint32_t n = 0 ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    if (!auto_strbuf(&modif, store->s))
        log_warnusys_return(LOG_EXIT_SYS, "clean string") ;

    if (!environ_merge_string(&sa, store->s))
        log_warnusys_return(LOG_EXIT_SYS, "merge environment string") ;

    FOREACH_SBL(&sa, pos) {

        char *line = sa.s + pos ;
        _alloc_sbl_(key, strlen(line)) ;
        _alloc_sbl_(val, strlen(line)) ;

        if (!environ_get_key(&key, line))
            return 0 ;

        if (!strcmp(key.s, enum_str_parser_section_environ[E_PARSER_SECTION_ENVIRON_IMPORTFILE])) {

            if (!environ_get_value(&val, line))
                return 0 ;

            if (val.s[0] != '/')
                log_warnu_return(LOG_EXIT_ZERO, "ImportFile must be an absolute path: ", val.s) ;

            if (scan_mode(val.s, S_IFDIR) > 0)
                log_warnu_return(LOG_EXIT_ZERO, "ImportFile is a directory: ", val.s, " -- only file are allowed") ;

            if (!sbl_add(&list, val.s))
                return 0 ;

            if (!sbl_replace_nline(&modif, line, " "))
                return 0 ;

            n++ ;
        }
    }

    if (list.len) {

        if (!sbl_rebuild_oneline(&list))
            return 0 ;

        res->environ.nimportfile = n ;
        res->environ.importfile = resolve_add_string(wres, list.s) ;
    }

    if (!environ_rebuild(&modif))
        return 0 ;

    store->len = 0 ;


    auto_strings(store->s, modif.s) ;
    store->len = strlen(store->s) ;

    free(wres) ;

    return 1 ;
}

static int store_environ(resolve_service_t *res, strbuf *store)
{
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    if (!get_import_field(res, store))
        return 0 ;

    res->environ.env = resolve_add_string(wres, store->s) ;

    if (!env_resolve_conf(&sa, res))
        return 0 ;

    res->environ.envdir = resolve_add_string(wres, sa.s) ;

    free(wres) ;

    return 1 ;
}

int parse_store_environ(resolve_service_t *res, strbuf *store, resolve_enum_table_t table)
{
    log_flow() ;

    uint32_t kid = table.u.parser.id ;

    switch(kid) {

        case E_PARSER_SECTION_ENVIRON_ENVAL:

            if (!store_environ(res, store))
                return 0 ;

            break ;

        default:
            /** never happen*/
            log_warn_return(LOG_EXIT_ZERO, "unknown id key in section environment -- please make a bug report") ;
    }

    return 1 ;
}
