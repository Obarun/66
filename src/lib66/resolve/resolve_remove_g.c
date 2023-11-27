/*
 * resolve_remove_g.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/resolve.h>
#include <66/constants.h>

void resolve_remove_g(char const *base, char const *name, uint8_t data_type)
{
    log_flow() ;

    int e = errno ;
    size_t baselen = strlen(base) ;
    size_t namelen = strlen(name) ;
    char path[baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + namelen + 1] ;

    if (data_type == DATA_SERVICE) {

        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

        resolve_remove(path, name) ;

        unlink(path) ;
        errno = e ;

    } else if (data_type == DATA_TREE || data_type == DATA_TREE_MASTER) {

        auto_strings(path, base, SS_SYSTEM) ;

        resolve_remove(path, name) ;
    }
}
