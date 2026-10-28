/*
 * set_livetmp.c
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
#include <sys/types.h>
#include <sys/stat.h>

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/directory.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/config.h>
#include <66/constants.h>

int set_livetmp(strbuf *dir, uid_t owner, char const *live)
{
    log_flow() ;

    dir->len = 0 ;

    if (!auto_strbuf(dir, live, SS_LIVE_TMP))
        return -1 ;

    // made by 66 scandir create, at the boot of a machine booted with 66
    ssize_t r = scan_mode(dir->s, FLAGS_DIR) ;
    if (r > 0)
        return 1 ;

    if (!r && errno != ENOENT)
        return -1 ;

    /** otherwise made when it can be: root can, an account other than root
     * cannot write under @live, which belongs to root, and fails here. A
     * directory on another filesystem would not do: a rename does not cross
     * it */
    log_trace("create temporary directory: ", dir->s) ;

    if (!dir_create_parent(dir->s, 0755))
        return -1 ;

    // every account writes there: sticky, as 66 scandir create makes it; oblibs has no chmod primitive
    if (!owner && chmod(dir->s, S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO) < 0)
        return -1 ;

    return 1 ;
}
