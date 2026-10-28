/*
 * state_read_file.c
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

#include <66/state.h>

int state_read_file(ss_state_t *sta, char const *file)
{
    log_flow() ;

    char pack[STATE_STATE_SIZE] ;
    ssize_t r = file_read(file, pack, STATE_STATE_SIZE) ;

    if (r < 0)
        return errno == ENOENT ? 0 : -1 ;

    if (r < STATE_STATE_SIZE)
        return (errno = EINVAL, -1) ;

    state_unpack(pack, sta) ;

    return 1 ;
}
