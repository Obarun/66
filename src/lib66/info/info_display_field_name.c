/*
 * info_display_field_name.c
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
 * */

#include <stddef.h>

#include <oblibs/log.h>

#include <skalibs/lolstdio.h>

#include <66/info.h>

size_t info_display_field_name(char const *field)
{
    log_flow() ;

    size_t len = 0 ;
    if(field)
    {
        len = info_length_from_wchar(field) + 1 ;
        if (!bprintf(buffer_1,"%s%s%s ", log_color->info, field, log_color->off)) log_dieusys(LOG_EXIT_SYS,"write to stdout") ;
    }
    return len ;
}
