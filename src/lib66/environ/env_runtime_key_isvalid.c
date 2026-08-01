/*
 * env_runtime_key_isvalid.c
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
#include <string.h>

#include <oblibs/log.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/environ.h>

int env_runtime_key_isvalid(char const *key)
{
    log_flow() ;

    size_t len = key ? strlen(key) : 0 ;

    if (!len)
        return (errno = EINVAL, 0) ;

    if (SS_LIVEENV_EVENT_GONE_LEN + len > SS_MAX_SERVICE_NAME)
        return (errno = ENAMETOOLONG, 0) ;

    if (key[0] != '_' && (key[0] < 'A' || key[0] > 'Z') && (key[0] < 'a' || key[0] > 'z'))
        return (errno = EINVAL, 0) ;

    for (size_t pos = 1 ; pos < len ; pos++) {

        char c = key[pos] ;

        if (c != '_' && (c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && (c < '0' || c > '9'))
            return (errno = EINVAL, 0) ;
    }

    return 1 ;
}
