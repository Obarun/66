/*
 * parse_store_environ.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <oblibs/sastr.h>
#include <oblibs/stack.h>
#include <oblibs/environ.h>
#include <oblibs/types.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/enum.h>
#include <66/utils.h>
#include <66/environ.h>

static int get_import_field(resolve_service_t *res, stack *store)
{
    _alloc_sa_(sa) ;
    _alloc_sa_(list) ;
    _alloc_sa_(modif) ;
    size_t pos = 0 ;
    uint32_t n = 0 ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    if (!auto_stra(&modif, store->s))
        log_warnusys_return(LOG_EXIT_SYS, "clean string") ;

    if (!environ_merge_string(&sa, store->s))
        log_warnusys_return(LOG_EXIT_SYS, "merge environment string") ;

    FOREACH_SASTR(&sa, pos) {

        char *line = sa.s + pos ;
        _alloc_stk_(key, strlen(line)) ;
        _alloc_stk_(val, strlen(line)) ;

        if (!environ_get_key(&key, line))
            return 0 ;

        if (!strcmp(key.s, enum_str_key_section_environ[KEY_ENVIRON_IMPORTFILE])) {

            if (!environ_get_value(&val, line))
                return 0 ;

            if (val.s[0] != '/')
                log_warnu_return(LOG_EXIT_ZERO, "ImportFile must be an absolute path: ", val.s) ;

            if (scan_mode(val.s, S_IFDIR) > 0)
                log_warnu_return(LOG_EXIT_ZERO, "ImportFile is a directory: ", val.s, " -- only file are allowed") ;

            if (!sastr_add_string(&list, val.s))
                return 0 ;

            if (!sastr_replace_g(&modif, line, " "))
                return 0 ;

            n++ ;
        }
    }

    if (!sastr_rebuild_in_oneline(&list))
        return 0 ;

    res->environ.nimportfile = n ;
    res->environ.importfile = resolve_add_string(wres, list.s) ;

    if (!environ_rebuild(&modif))
        return 0 ;

    stack_reset(store) ;

    if (modif.len > store->maxlen)
        return (errno = EOVERFLOW, 0) ;

    auto_strings(store->s, modif.s) ;
    store->len = strlen(store->s) ;

    free(wres) ;

    return 1 ;
}

static int store_environ(resolve_service_t *res, stack *store)
{
    _alloc_sa_(sa) ;
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

int parse_store_environ(resolve_service_t *res, stack *store, const int sid, const int kid)
{
    log_flow() ;

    switch(kid) {

        case KEY_ENVIRON_ENVAL:

            if (!store_environ(res, store))
                return 0 ;

            break ;

        default:
            /** never happen*/
            log_warn_return(LOG_EXIT_ZERO, "unknown id key in section environment -- please make a bug report") ;
    }

    return 1 ;
}
