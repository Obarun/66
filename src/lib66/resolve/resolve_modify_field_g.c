/*
 * resolve_modify_field_g.c
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

#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/constants.h>
#include <66/graph.h>


int resolve_modify_field_g(resolve_wrapper_t_ref wres, char const *base, char const *element, uint8_t field, char const *value)
{

    log_flow() ;

    size_t baselen = strlen(base), tot = baselen + SS_SYSTEM_LEN + 1, treelen = 0 ;
    char *treename = 0 ;

    if (wres->type == DATA_SERVICE) {
        treename = ((resolve_service_t *)wres->obj)->sa.s + ((resolve_service_t *)wres->obj)->treename ;
        treelen = strlen(treename) ;
        tot += treelen + SS_SVDIRS_LEN + 1 ;

    }

    char solve[tot] ;

    if (wres->type == DATA_SERVICE) {

        auto_strings(solve, base, SS_SYSTEM, "/", treename, SS_SVDIRS) ;

    } else if (wres->type == DATA_TREE || wres->type == DATA_TREE_MASTER) {

        auto_strings(solve, base, SS_SYSTEM) ;
    }

    if (!resolve_read(wres, solve, element))
        log_warnusys_return(LOG_EXIT_ZERO, "read resolve file of: ", solve, "/", element) ;

    if (!resolve_modify_field(wres, field, value))
        log_warnusys_return(LOG_EXIT_ZERO, "modify resolve file of: ", solve, "/", element) ;

    if (!resolve_write(wres, solve, element))
        log_warnusys_return(LOG_EXIT_ZERO, "write resolve file of :", solve, "/", element) ;

    return 1 ;

}
