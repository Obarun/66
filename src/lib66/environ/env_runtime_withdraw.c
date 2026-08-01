/*
 * env_runtime_withdraw.c
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
#include <unistd.h> // unlink

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/environ.h>

int env_runtime_withdraw(char const *dir, char const *key)
{
    log_flow() ;

    size_t dlen = strlen(dir), klen = strlen(key) ;
    if (!env_runtime_key_isvalid(key))
        return -1 ;

    char file[dlen + 1 + klen + 1] ;
    auto_strings(file, dir, "/", key) ;

    if (unlink(file) < 0)
        return errno == ENOENT ? 0 : -1 ;

    return 1 ;
}
