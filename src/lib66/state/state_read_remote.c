/*
 * state_read_remote.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/state.h>
#include <66/constants.h>

int state_read_remote(ss_state_t *sta, char const *dst)
{
    log_flow() ;

    char file[strlen(dst) + 1 + SS_STATUS_LEN + 1] ;

    auto_strings(file, dst, "/", SS_STATUS) ;

    return state_read_file(sta, file) ;
}
