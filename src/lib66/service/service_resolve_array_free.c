/*
 * service_resolve_array_free.c
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

#include <skalibs/stralloc.h>

#include <66/service.h>

void service_resolve_array_free(resolve_service_t *ares, unsigned int areslen)
{

    unsigned int pos = 0 ;
    for (; pos < areslen ; pos++)
        stralloc_free(&ares[pos].sa) ;
}

