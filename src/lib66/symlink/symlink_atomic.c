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
 * except according to the terms contained in the LICENSE file./
 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/fd.h>

int symlink_atomic(char const *target, char const *name)
{
    /* fast path: @name does not exist yet, a plain symlink is already atomic */
    if (symlink(target, name) == 0)
        return 1 ;

    if (errno != EEXIST)
        return 0 ;

    /* @name exists: build the new symlink under a temp name in the same dir,
    * then rename() it over @name so the swap is all-or-nothing. */
    size_t namelen = strlen(name) ;
    char tmp[namelen + 7 + 1] ;               /* "<name>.XXXXXX" + NUL */

    for (;;) {

        auto_strings(tmp, name, ".XXXXXX") ;

        int fd = mkstemp(tmp) ;               /* reserve a unique name atomically */
        if (fd < 0)
            return 0 ;

        close_fd(fd) ;

        if (unlink(tmp) < 0)                /* free the name for the symlink */
            return  0 ;

        if (symlink(target, tmp) == 0)
            break ;
        if (errno != EEXIST)                  /* race lost on EEXIST -> retry */
            return 0 ;
    }

    if (rename(tmp, name) < 0) {
        file_tryunlink(tmp) ;
        return 0 ;
    }

    return 1 ;
}
