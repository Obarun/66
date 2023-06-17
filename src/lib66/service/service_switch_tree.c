/*
 * service_switch_tree.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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

void service_switch_tree(resolve_service_t *res, char const *base, char const *totreename)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    tree_service_remove(base, res->sa.s + res->treename, res->sa.s + res->name) ;

    tree_service_add(base, totreename, res->sa.s + res->name) ;

    if (!service_resolve_modify_field(res, E_RESOLVE_SERVICE_TREENAME, totreename))
        log_dieu(LOG_EXIT_SYS, "modify resolve file of: ", res->sa.s + res->name) ;

    free(wres) ;
}
