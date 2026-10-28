/*
 * symlink_atomic_at.c
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

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/files.h>

#include <66/symlink.h>
#include <66/graph.h>

int symlink_atomic_at(char const *tmpdir, char const *target, char const *name)
{
    log_flow() ;

    size_t len = 6 ;
    size_t dirlen = strlen(tmpdir) + 1 ;
    char tmp[dirlen + len + 1] ;

    auto_strings(tmp, tmpdir, "/") ;

    for (;;) {

        if (!file_tmpname(tmp + dirlen, len))
            return -1 ;

        tmp[dirlen + len] = 0 ;

        if (!symlink(target, tmp))
            break ;

        if (errno != EEXIST)
            return -1 ;
    }

    return file_tmp_rename(tmp, name) ? 1 : -1 ;
}
