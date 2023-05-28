/*
 * write_uint.c
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

#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/files.h>

#include <skalibs/types.h>

int write_uint(char const *dst, char const *name, uint32_t ui)
{
    log_flow() ;

    char number[UINT32_FMT] ;

    log_trace("write file: ", dst, "/", name) ;
    if (!file_write_unsafe(dst, name, number, uint32_fmt(number,ui)))
        log_warnusys_return(LOG_EXIT_ZERO, "write: ", dst, "/", name) ;

    return 1 ;
}
