/*
 * status_write.c
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

#include <oblibs/log.h>
#include <oblibs/files.h>

#include <66/status.h>

int status_write(service_status_t const *st, char const *file)
{
    log_flow() ;

    char pack[STATUS_STATE_SIZE] ;

    status_pack(pack, st) ;

    if (!file_write_atomic(file, pack, STATUS_STATE_SIZE))
        return 0 ;

    return 1 ;
}
