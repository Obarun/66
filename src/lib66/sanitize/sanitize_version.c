/*
 * sanitize_version.c
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
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>

#include <66/constants.h>
#include <66/config.h>
#include <66/ssexec.h>
#include <66/utils.h>
#include <66/graph.h>
#include <66/command.h>
#include <66/sanitize.h>

static int version_read(strbuf *sa, char const *dst)
{
    sa->len = 0 ;

    if (strbuf_read_file(sa, dst))
        return 1 ;

    if (errno != ENOENT)
        log_warnusys_return(LOG_EXIT_LESSONE, "read system version file: ", dst) ;

    return 0 ;
}

int sanitize_version(ssexec_t *info)
{
    log_flow() ;

    int r ;
    _cleanup_strbuf_ strbuf version = STRBUF_ZERO ;
    char dst[info->base.len + SS_SYSTEM_LEN + 9 + 1] ;
    char tmp[TMPDIR_GETLEN(info->base.len) + 1] ;

    auto_strings(dst, info->base.s, SS_SYSTEM, "/.version") ;
    auto_strings(tmp, info->base.s, SS_TMPDIR + 1) ;

    r = version_read(&version, dst) ;
    if (r < 0)
        return -1 ;

    if (r && !version_compare(version.s, SS_VERSION))
        return 1 ;

    if (service_graph_write_begin(info->owner, info->base.s, SS_LIVE) < 0) {

        if (r)
            log_warnusys_return(LOG_EXIT_LESSONE, "lock the system") ;

        log_warnusys("lock the system") ;
        return 1 ;
    }

    r = version_read(&version, dst) ;
    if (r < 0) {
        int e = errno ;
        command_write_end(0) ;
        return (errno = e, -1) ;
    }

    if (!r) {

        /** Best-effort: a container boot skips sanitize, so the system
         * directory may not exist yet (it is created a bit later by the
         * spawned scandir/tree commands, or was never writable). Do not
         * abort here -- the marker is written once the state is set up. */
        log_trace("initialize system version file with version: ", SS_VERSION) ;
        if (!file_write_atomic_at(tmp, dst, SS_VERSION, strlen(SS_VERSION)))
            log_warnusys("write system version file: ", dst) ;

    } else if ((r = sanitize_migrate(info, version.s))) {

        if (r < 0) {
            int e = errno ;
            command_write_end(0) ;
            errno = e ;
            log_warnusys_return(LOG_EXIT_LESSONE, "migrate the system of: ", info->base.s) ;
        }

        log_trace("write system version file with version: ", SS_VERSION) ;
        if (!file_write_atomic_at(tmp, dst, SS_VERSION, strlen(SS_VERSION))) {
            int e = errno ;
            command_write_end(0) ;
            errno = e ;
            log_warnusys_return(LOG_EXIT_LESSONE, "write system version file: ", dst) ;
        }
    }

    command_write_end(0) ;

    return 1 ;
}
