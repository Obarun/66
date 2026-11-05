/*
 * migrate_0920_eventdir.c
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

#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/types.h>

#include <66/constants.h>
#include <66/enum_parser.h>
#include <66/event.h>
#include <66/migrate.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/utils.h>

static int migrate_one(char const *base, char const *name)
{
    int r ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = { .type = DATA_SERVICE, .obj = &res } ;

    r = resolve_read(&w, base, name) ;

    if (r > 0 && res.type == E_PARSER_TYPE_ONESHOT && !access(res.sa.s + res.live.servicedir, F_OK)) {

        log_trace("create fifo: ", res.sa.s + res.live.eventdir) ;
        if (!event_fifo_make(res.sa.s + res.live.eventdir, getgid()))
            r = -1 ;
    }

    resolve_free_obj(&w) ;

    return r < 0 ? -1 : 1 ;
}

int migrate_0920_eventdir(void)
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
