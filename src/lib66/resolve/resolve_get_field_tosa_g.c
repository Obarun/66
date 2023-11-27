/*
 * resolve_get_field_tosa_g.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>

int resolve_get_field_tosa_g(stralloc *sa, char const *base, char const *name, uint8_t data_type, uint8_t field)
{
    log_flow() ;

    if (!name)
        return 0 ;

    int e = 0 ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_tree_master_t tmres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = 0 ;

    if (data_type == DATA_SERVICE) {

        wres = resolve_set_struct(data_type, &res) ;

    } else if (data_type == DATA_TREE) {

        wres = resolve_set_struct(data_type, &tres) ;

    } else if (data_type == DATA_TREE_MASTER) {

        wres = resolve_set_struct(data_type, &tmres) ;

    } else return 0 ;

    if (resolve_read_g(wres, base, name) <= 0)
        goto err ;

    if (!resolve_get_field_tosa(sa, wres, field))
        goto err ;

    /**
     * check if field isn't empty
     * */
    if (sa->len)
        if (!sastr_clean_string_flush_sa(sa, sa->s))
            goto err ;

    e = 1 ;
    err:
        resolve_free(wres) ;
        return e ;
}
