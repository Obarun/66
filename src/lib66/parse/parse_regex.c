/*
 * parse_regex.c
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

#include <stdint.h>
#include <stdlib.h> // free

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/enum_parser.h>

int parse_regex(parse_store_t *st, resolve_service_t *res, resolve_service_addon_regex_t *rx, uint8_t *has_regex)
{
    log_flow() ;

    *has_regex = 0 ;

    // the [Regex] section is only meaningful for module services
    if (res->type != E_PARSER_TYPE_MODULE)
        return 1 ;

    uint8_t const configure = parse_store_present(st, E_PARSER_SECTION_REGEX, E_PARSER_SECTION_REGEX_CONFIGURE) ;

    // Directories/Files/Infiles: parenthesised lists -> sbl blob + count
    struct { uint32_t kid ; uint32_t *field ; uint32_t *nfield ; } lists[3] = {
        { E_PARSER_SECTION_REGEX_DIRECTORIES, &rx->directories, &rx->ndirectories },
        { E_PARSER_SECTION_REGEX_FILES,       &rx->files,       &rx->nfiles },
        { E_PARSER_SECTION_REGEX_INFILES,     &rx->infiles,     &rx->ninfiles },
    } ;

    // no regex key present -> no addon, keep sa untouched (offset 0 convention only
    // once we actually init below, matching the sibling store readers)
    if (!configure &&
        !parse_store_present(st, E_PARSER_SECTION_REGEX, E_PARSER_SECTION_REGEX_DIRECTORIES) &&
        !parse_store_present(st, E_PARSER_SECTION_REGEX, E_PARSER_SECTION_REGEX_FILES) &&
        !parse_store_present(st, E_PARSER_SECTION_REGEX, E_PARSER_SECTION_REGEX_INFILES))
        return 1 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_REGEX, rx) ;
    resolve_init(wres) ;

    // Configure: a plain string
    if (configure) {
        char const *v = parse_store_get(st, E_PARSER_SECTION_REGEX, E_PARSER_SECTION_REGEX_CONFIGURE, 0) ;
        rx->configure = resolve_add_string(wres, v) ;
    }

    for (unsigned int i = 0 ; i < 3 ; i++) {

        if (!parse_store_present(st, E_PARSER_SECTION_REGEX, lists[i].kid))
            continue ;

        size_t len = 0 ;
        char const *v = parse_store_get(st, E_PARSER_SECTION_REGEX, lists[i].kid, &len) ;

        resolve_enum_table_t t = E_TABLE_PARSER_SECTION_REGEX_ZERO ;
        t.u.parser.id = lists[i].kid ;

        _alloc_sbl_(stk, len + 1) ;

        if (!strbuf_copyb(&stk, v, len))
            log_die_nomem("stack") ;

        if (!parse_list(&stk)) {
            free(wres) ;
            parse_error_return(0, 8, t) ;
        }

        if (stk.len)
            *lists[i].field = parse_compute_list(wres, &stk, lists[i].nfield, 0) ;
    }

    free(wres) ;

    *has_regex = 1 ;

    return 1 ;
}
