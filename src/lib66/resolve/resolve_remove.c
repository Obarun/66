/*
 * resolve_remove.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <unistd.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/resolve.h>
#include <66/constants.h>

void resolve_remove(char const *base, char const *name)
{
    log_flow() ;

    int e = errno ;
    size_t baselen = strlen(base) ;
    size_t namelen = strlen(name) ;

    char file[baselen + SS_RESOLVE_LEN + 1 + namelen +1] ;
    auto_strings(file, base, SS_RESOLVE, "/", name) ;

    unlink(file) ;
    errno = e ;
}

