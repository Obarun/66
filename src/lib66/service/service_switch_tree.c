/*
 * service_switch_tree.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <oblibs/strbuf.h>

#include <oblibs/log.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/tree.h>

int service_switch_tree(resolve_service_t *res, char const *totreename, char const *base)
{
    log_flow() ;

    int e = 1 ;
    char const *name = res->sa.s + res->name ;
    // get informations from a fresh source
    resolve_service_t fresh = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t wres = RESOLVE_WRAPPER(DATA_SERVICE, &fresh) ;
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_MAIN_ZERO ;
    table.u.service.id = E_RESOLVE_SERVICE_CONFIG_TREENAME ;

    if (tree_service_remove(base, res->sa.s + res->treename, name) < 0 ||
        tree_contents_edit(base, totreename, name, true) <= 0)
        return -1 ;

    if (!resolve_modify_field(&wres, base, name, table, totreename))
        e = -1 ;

    resolve_free_obj(&wres) ;

    service_resolve_modify_field(res, table.u.service, totreename) ;

    return e ;
}
