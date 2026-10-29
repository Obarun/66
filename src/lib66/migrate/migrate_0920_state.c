/*
 * migrate_0920_state.c
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
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/sbl.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/types.h>

#include <66/constants.h>
#include <66/migrate.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/state.h>
#include <66/utils.h>

#define STATE_STATE_SIZE_0912 28

static int rewrite(char const *file)
{
    log_flow() ;

    char pack[STATE_STATE_SIZE_0912 + 1] ;
    ssize_t r = file_read(file, pack, sizeof(pack)) ;

    if (r < 0)
        return errno == ENOENT ? 1 : -1 ;

    if (r == STATE_STATE_SIZE)
        return 1 ;

    if (r != STATE_STATE_SIZE_0912) {
        log_warn("state file: ", file, " has an unknown size -- left as it is") ;
        return 1 ;
    }

    memcpy(pack + 8, pack + 12, 4) ;
    memcpy(pack + 12, pack + 24, 4) ;

    log_trace("drop the unused fields of: ", file) ;

    return file_write_atomic(file, pack, STATE_STATE_SIZE) ? 1 : -1 ;
}

static int migrate_one(char const *base, char const *name)
{
    int r ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = { .type = DATA_SERVICE, .obj = &res } ;

    r = resolve_read(&w, base, name) ;

    if (r > 0) {

        char const *statedir = res.sa.s + res.live.statedir ;
        char live[strlen(statedir) + 1 + SS_STATUS_LEN + 1] ;

        auto_strings(live, statedir, "/", SS_STATUS) ;

        r = rewrite(res.sa.s + res.live.status) ;
        if (r > 0)
            r = rewrite(live) ;
    }

    resolve_free_obj(&w) ;

    return r < 0 ? -1 : 1 ;
}

int migrate_0920_state(void)
{
    log_flow() ;

    int r = 1 ;
    size_t pos = 0 ;
    char const *exclude[2] = { SS_PROVIDE + 1, 0 } ;
    _cleanup_strbuf_ strbuf base = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (!set_ownersysdir(&base, getuid()))
        return -1 ;

    char dir[base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1] ;

    auto_strings(dir, base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;

    if (!sbl_dir_get_recursive(&sa, dir, exclude, FLAGS_LNK, 0))
        return -1 ;

    FOREACH_SBL(&sa, pos) {

        r = migrate_one(base.s, sa.s + pos) ;
        if (r < 0)
            break ;
    }

    return r ;
}
