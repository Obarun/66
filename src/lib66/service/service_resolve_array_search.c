/*
 * service_resolve_array_search.c
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
#include <oblibs/log.h>
#include <66/service.h>

int service_resolve_array_search(resolve_service_t *ares, unsigned int areslen, char const *name)
{
    log_flow() ;

    unsigned int pos = 0 ;

    for (; pos < areslen ; pos++) {

        char const *n = ares[pos].sa.s + ares[pos].name ;
            if (!strcmp(name, n))
                return pos ;
    }

    return -1 ;
}

