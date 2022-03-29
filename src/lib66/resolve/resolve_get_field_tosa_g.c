/*
 * resolve_get_field_tosa_g.c
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
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/tree.h>
#include <66/service.h>
#include <66/resolve.h>

int resolve_get_field_tosa_g(stralloc *sa, char const *base, char const *treename, char const *element, uint8_t data_type, uint8_t field)
{
    log_flow() ;
    int e = 0 ;
    size_t baselen = strlen(base), tot = baselen + SS_SYSTEM_LEN + 1, treelen = 0 ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_service_master_t mres = RESOLVE_SERVICE_MASTER_ZERO ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_tree_master_t tmres = RESOLVE_TREE_MASTER_ZERO ;

    resolve_wrapper_t_ref wres = 0 ;

    if (data_type == DATA_SERVICE) {
        treelen = strlen(treename) ;
        tot += treelen + SS_SVDIRS_LEN + 1 ;
    }

    char solve[tot] ;

    if (data_type == DATA_SERVICE || data_type == DATA_SERVICE_MASTER) {

        if (data_type == DATA_SERVICE) {

            wres = resolve_set_struct(data_type, &res) ;

        } else if (data_type == DATA_SERVICE_MASTER) {

            wres = resolve_set_struct(data_type, &mres) ;
        }

        auto_strings(solve, base, SS_SYSTEM, "/", treename, SS_SVDIRS) ;

    } else if (data_type == DATA_TREE || data_type == DATA_TREE_MASTER) {

        if (data_type == DATA_TREE) {

            wres = resolve_set_struct(data_type, &tres) ;

        } else if (data_type == DATA_TREE_MASTER) {

            wres = resolve_set_struct(data_type, &tmres) ;
        }

        auto_strings(solve, base, SS_SYSTEM) ;
    }

    if (!resolve_read(wres, solve, element))
        goto err ;

    if (!resolve_get_field_tosa(sa, wres, field))
        goto err ;

    {
        char t[sa->len + 1] ;
        auto_strings(t, sa->s) ;

        sa->len = 0 ;

        if (!sastr_clean_string(sa, t))
            goto err ;
    }
    e = 1 ;

    err:
        resolve_free(wres) ;
        return e ;
}
