/*
 * resolve_search.c
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

#include <skalibs/genalloc.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/graph.h>

int resolve_search(genalloc *ga, char const *name, uint8_t type)
{
    log_flow() ;

    size_t len, pos = 0 ;

    if (type == DATA_SERVICE) {

        len = genalloc_len(resolve_service_t, ga) ;

        for (;pos < len ; pos++) {

            char *s = genalloc_s(resolve_service_t,ga)[pos].sa.s + genalloc_s(resolve_service_t,ga)[pos].name ;
            if (!strcmp(name,s))
                return pos ;
        }

    } else if (type == DATA_SERVICE_MASTER) {

        len = genalloc_len(resolve_service_master_t, ga) ;

        for (;pos < len ; pos++) {

            char *s = genalloc_s(resolve_service_master_t,ga)[pos].sa.s + genalloc_s(resolve_service_master_t,ga)[pos].name ;
            if (!strcmp(name,s))
                return pos ;
        }

    } else if (type == DATA_TREE) {

        len = genalloc_len(resolve_tree_t, ga) ;

        for (;pos < len ; pos++) {

            char *s = genalloc_s(resolve_tree_t,ga)[pos].sa.s + genalloc_s(resolve_tree_t,ga)[pos].name ;
            if (!strcmp(name,s))
                return pos ;
        }

    } else if (type == DATA_TREE_MASTER) {

        len = genalloc_len(resolve_tree_master_t, ga) ;

        for (;pos < len ; pos++) {

            char *s = genalloc_s(resolve_tree_master_t,ga)[pos].sa.s + genalloc_s(resolve_tree_master_t,ga)[pos].name ;
            if (!strcmp(name,s))
                return pos ;
        }
    }

    return -1 ;
}
