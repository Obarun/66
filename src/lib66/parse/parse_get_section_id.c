/*
 * parse_get_section_id.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/log.h>

#include <66/enum.h>

int parse_get_section_id(char const *name)
{
    log_flow() ;

    int sid = -1 ;
    sid = get_enum_by_key(name) ;

    if (sid < 0)
        log_warn("get section id") ;

    return sid ;
}