/*
 * env_runtime_setdir.c
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

#include <sys/stat.h>
#include <sys/types.h>

#include <oblibs/files.h>
#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/environ.h>
#include <66/utils.h>

void env_runtime_setdir(strbuf *dir, strbuf *live, uid_t owner)
{
    log_flow() ;

    int r ;

    if (!strbuf_copy(dir, live) || !strbuf_uncounted(dir))
        log_die_nomem("strbuf") ;

    r = set_liveenviron(dir, owner) ;
    if (!r)
        log_die_nomem("strbuf") ;
    if (r < 0)
        log_die(LOG_EXIT_SYS, "live: ", live->s, " must be an absolute path") ;

    /** the directory is created and destroyed along with the scandir it belongs
     * to; 66 env never creates it, so its absence is a real error to report. */
    r = scan_mode(dir->s, S_IFDIR) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "conflicting format of: ", dir->s) ;
    if (!r)
        log_die(LOG_EXIT_USER, "no runtime environment directory: ", dir->s, " -- create a scandir first") ;
}
