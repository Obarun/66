/*
 * ssexec_snapshot_remove.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/directory.h>

#include <66/ssexec.h>
#include <66/constants.h>


int ssexec_snapshot_remove(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    char const *snapname = 0 ;
    _alloc_strbuf_(snapdir, SS_MAX_PATH_LEN) ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing name argument") ;

    snapname = *argv ;

    auto_strings(snapdir.s, info->base.s, SS_SNAPSHOT + 1, "/", snapname) ;

    if (access(snapdir.s, F_OK) < 0)
        log_dieusys(LOG_EXIT_SYS, "find snapshot: ", snapdir.s) ;

    log_trace("delete directory: ", snapdir.s) ;
    if (!dir_destroy(snapdir.s))
        log_dieusys(LOG_EXIT_SYS, "delete snapshot: ", snapdir.s) ;

    log_info("Successfully removed snapshot: ", snapname) ;

    return 0 ;
}
