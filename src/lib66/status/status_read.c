/*
 * status_read.c
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

#include <errno.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/files.h>

#include <66/status.h>

int status_read(service_status_t *st, char const *file)
{
    log_flow() ;

    /* read one extra byte so an oversized (corrupt) file is rejected too, not
     * just a short one. */
    char pack[STATUS_STATE_SIZE + 1] ;
    ssize_t r = file_read(file, pack, STATUS_STATE_SIZE + 1) ;

    if (r < 0) {
        if (errno == ENOENT) {
            *st = service_status_zero ;
            return 1 ;
        }
        return -1 ;
    }

    if (r != STATUS_STATE_SIZE || (uint8_t)pack[0] != STATUS_VERSION) {
        *st = service_status_zero ;
        return 0 ;
    }

    status_unpack(pack, st) ;

    return 1 ;
}
