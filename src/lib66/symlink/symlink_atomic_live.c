/*
 * symlink_atomic_live.c
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

#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/symlink.h>
#include <66/utils.h>

int symlink_atomic_live(uid_t owner, char const *live, char const *target, char const *name)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf tmpdir = STRBUF_ZERO ;

    if (set_livetmp(&tmpdir, owner, live) < 0)
        return -1 ;

    return symlink_atomic_at(tmpdir.s, target, name) ;
}
