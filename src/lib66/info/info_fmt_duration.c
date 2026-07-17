/*
 * info_fmt_duration.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>

#define INFO_MINUTE 60
#define INFO_HOUR   3600
#define INFO_DAY    86400

size_t info_fmt_duration(char *s, uint64_t secs)
{
    log_flow() ;

    char a[U64_FMT], b[U64_FMT] ;

    if (secs < INFO_MINUTE) {

        a[u64_fmt(a, secs)] = 0 ;
        auto_strings(s, a, "s") ;

    } else if (secs < INFO_HOUR) {

        a[u64_fmt(a, secs / INFO_MINUTE)] = 0 ;
        b[u64_fmt(b, secs % INFO_MINUTE)] = 0 ;

        if (secs % INFO_MINUTE)
            auto_strings(s, a, "min ", b, "s") ;
        else
            auto_strings(s, a, "min") ;

    } else if (secs < INFO_DAY) {

        a[u64_fmt(a, secs / INFO_HOUR)] = 0 ;
        b[u64_fmt(b, (secs % INFO_HOUR) / INFO_MINUTE)] = 0 ;

        if ((secs % INFO_HOUR) / INFO_MINUTE)
            auto_strings(s, a, "h ", b, "min") ;
        else
            auto_strings(s, a, "h") ;

    } else {

        uint64_t days = secs / INFO_DAY ;

        a[u64_fmt(a, days)] = 0 ;
        b[u64_fmt(b, (secs % INFO_DAY) / INFO_HOUR)] = 0 ;

        if ((secs % INFO_DAY) / INFO_HOUR)
            auto_strings(s, a, days > 1 ? " days " : " day ", b, "h") ;
        else
            auto_strings(s, a, days > 1 ? " days" : " day") ;
    }

    return strlen(s) ;
}
