/*
 * symlink_atomic.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/symlink.h>

int symlink_atomic(char const *target, char const *name)
{
    log_flow() ;

    // the temporary beside @name: a rename does not cross filesystems
    char dir[strlen(name) + 1] ;

    if (!ob_dirname(dir, name))
        return -1 ;

    return symlink_atomic_at(dir, target, name) ;
}
