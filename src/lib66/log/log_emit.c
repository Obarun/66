/*
 * log_emit.c
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

#include <sys/types.h>
#include <time.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/stream.h>
#include <oblibs/clock.h>

#include <66/log.h>

void log_emit(char const *line, size_t len, size_t msgoff, uint8_t type, struct timespec const *stamp, char const *name, uint8_t withname)
{
    int ok = 1 ;

    if (type == LOG_STAMP_TAI64N) {

        char local[CLOCK_LOCAL_LEN + 1] ;
        size_t ll = clock_local_fmt(local, stamp) ;

        ok = ostream_put(ostream_1, local, ll)
          && ostream_put(ostream_1, " ", 1) ;

    } else {
        ok = ostream_put(ostream_1, line, msgoff) ;
    }

    if (ok && withname)
        ok = ostream_puts(ostream_1, name)
          && ostream_put(ostream_1, ": ", 2) ;

    if (ok)
        ok = ostream_put(ostream_1, line + msgoff, len - msgoff) ;

    if (!ok || !ostream_put(ostream_1, "\n", 1))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}
