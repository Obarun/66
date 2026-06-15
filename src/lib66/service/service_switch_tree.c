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
 * except according to the terms contained in the LICENSE file./
 */

#include <stdlib.h>

#include <oblibs/log.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/tree.h>
#include <66/ssexec.h>

void service_switch_tree(resolve_service_t *res, char const *totreename, ssexec_t *info)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_MAIN_ZERO ;
    table.u.service.id = E_RESOLVE_SERVICE_CONFIG_TREENAME ;

    tree_service_remove(info->base.s, res->sa.s + res->treename, res->sa.s + res->name) ;

    tree_service_add(totreename, res->sa.s + res->name, info) ;

    service_resolve_modify_field(res, table.u.service, totreename) ;

    if (!resolve_write_g(wres, info->base.s, res->sa.s + res->name))
        log_dieu(LOG_EXIT_SYS, "write  resolve file of: ", res->sa.s + res->name) ;

    free(wres) ;
}
