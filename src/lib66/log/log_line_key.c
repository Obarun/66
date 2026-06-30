/*
 * log_line_key.c
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

#include <time.h>
#include <stddef.h>

#include <oblibs/clock.h>

#include <66/log.h>

int log_line_key(char const *line, size_t len, struct timespec *ts, size_t *msgoff)
{
    if (len >= CLOCK_TAI64N_LEN && line[0] == '@' && clock_tai64n_scan(line, ts)) {
        size_t p = CLOCK_TAI64N_LEN ;
        if (p < len && line[p] == ' ')
            p++ ;
        *msgoff = p ;
        return LOG_STAMP_TAI64N ;
    }

    size_t end ;
    if (log_iso_scan(line, len, ts, &end)) {
        while (end < len && line[end] == ' ')
            end++ ;
        *msgoff = end ;
        return LOG_STAMP_ISO ;
    }

    *msgoff = 0 ;
    return LOG_STAMP_NONE ;
}
