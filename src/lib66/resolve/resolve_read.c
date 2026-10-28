/*
 * resolve_read.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <sys/stat.h>
#include <errno.h>
#include <stdbool.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/resolve.h>
#include <66/constants.h>
#include <66/service.h>
#include <66/symlink.h>

int resolve_read(resolve_wrapper_t *wres, char const *base, char const *name)
{
    log_flow() ;

    char const *suffix = resolve_addon_suffix(wres->type) ;

    if (!suffix)
        return (errno = EINVAL, -1) ;

    int r ;
    bool tree = wres->type == DATA_TREE || wres->type == DATA_TREE_MASTER ;
    char path[SS_MAX_PATH_LEN] ;
    char lname[strlen(name) + strlen(suffix) + 1] ;

    if (tree)
        auto_strings(path, base, SS_SYSTEM) ;
    else
        auto_strings(path, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name) ;

    auto_strings(lname, name, suffix) ;

    // the open tells a missing file apart, no check made before it
    r = resolve_read_at(wres, path, lname) ;

    /** the link of the service stands and leads to something that is no
     * directory: its resolve file stands on disk and does not read (contract,
     * Invalid service; reconfigure -c repairs it). A base that is no directory
     * stays a system error */
    if (r < 0 && errno == ENOTDIR && !tree) {

        if (symlink_type(path))
            return (errno = EINVAL, 0) ;

        errno = ENOTDIR ;
    }

    return r ;
}
