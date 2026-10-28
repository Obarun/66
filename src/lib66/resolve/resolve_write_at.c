/*
 * resolve_write_at.c
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/fd.h>
#include <oblibs/files.h>

#include <66/resolve.h>
#include <66/constants.h>
#include <66/graph.h>

int resolve_write_at(resolve_wrapper_t *wres, char const *base, char const *name, char const *tmpdir)
{
    log_flow() ;

    char const *suffix = resolve_addon_suffix(wres->type) ;

    if (!suffix)
        return (errno = EINVAL, 0) ;

    size_t baselen = strlen(base), namelen = strlen(name) + strlen(suffix) ;
    char file[baselen + SS_RESOLVE_LEN + 1 + namelen + 1] ;
    char tmp[SS_MAX_PATH_LEN + 1] ;

    auto_strings(file, base, SS_RESOLVE, "/", name, suffix) ;

    int fd = file_tmp_open_at(tmp, sizeof tmp, tmpdir, 0600, (uid_t)-1, (gid_t)-1) ;
    if (fd < 0)
        return 0 ;

    if (!resolve_write_cdb(wres, fd)) {
        close_fd(fd) ;
        file_tryunlink(tmp) ;
        return 0 ;
    }

    return file_tmp_rename_at(fd, tmp, file) ;
}
