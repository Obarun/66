/*
 * visit.c
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

#include <stddef.h>

#include <oblibs/log.h>

#include <66/utils.h>

void visit_init(visit_t *visit, size_t len)
{
    log_flow() ;

    size_t pos = 0 ;
    for (; pos < len; pos++)
        visit[pos] = VISIT_WHITE ;

}
