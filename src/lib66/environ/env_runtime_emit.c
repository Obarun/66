/*
 * env_runtime_emit.c
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

#include <stdint.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/constants.h>
#include <66/environ.h>
#include <66/svc.h>

void env_runtime_emit(char const *scandir, uint8_t who, char const *prefix, char const *key)
{
    log_flow() ;

    size_t len = strlen(key), slen = strlen(scandir), plen = strlen(prefix) ;

    char name[plen + len + 1] ;
    auto_strings(name, prefix, key) ;

    char dir[slen + 1 + SS_EVENTD_LEN + 1] ;
    auto_strings(dir, scandir, "/", SS_EVENTD) ;

    if (!svcd_notify(dir, 'e', who, name))
        log_warnusys("emit event: ", name) ;
}
