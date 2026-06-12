/*
 * ssexec_snapshot_list.c
 *
 * Copyright (c) 2024-2025 Eric Vidal <eric@obarun.org>
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
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>

#include <66/ssexec.h>
#include <66/constants.h>


int ssexec_snapshot_list(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    (void)argc ;
    (void)argv ;

    ssexec_t *info = data ;

    size_t pos = 0 ;
    char const *exclude[1] = { 0 } ;
    _alloc_strbuf_(snapdir, SS_MAX_PATH_LEN) ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    auto_strings(snapdir.s, info->base.s, SS_SNAPSHOT + 1) ;

    if (access(snapdir.s, F_OK) < 0) {
        log_info("There is no snapshot yet") ;
        return 0 ;
    }

    if (!sbl_dir_get(&sa, snapdir.s, exclude, S_IFDIR))
        log_dieusys(LOG_EXIT_SYS, "list snapshot from: ", snapdir.s) ;

    if (!sa.len) {
        log_info("There is no snapshot yet") ;
        return 0 ;
    }

    set_default_msg(0) ;
    set_clock_enable(0) ;
    FOREACH_SBL(&sa, pos)
        log_info(sa.s + pos) ;

    return 0 ;
}
