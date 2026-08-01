/*
 * ssexec_env_list.c
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

#include <oblibs/environ.h>
#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/stream.h>

#include <66/environ.h>
#include <66/ssexec.h>

int ssexec_env_list(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    (void)argv ;

    ssexec_t *info = data ;

    if (argc)
        log_die(LOG_EXIT_USER, "too many arguments") ;

    _cleanup_strbuf_ strbuf dir = STRBUF_ZERO ;

    env_runtime_setdir(&dir, &info->live, info->owner) ;

    _cleanup_strbuf_ strbuf env = STRBUF_ZERO ;

    if (!environ_merge_dir(&env, dir.s))
        log_dieusys(LOG_EXIT_SYS, "merge environment directory: ", dir.s) ;

    size_t pos = 0 ;
    FOREACH_SBL(&env, pos)
        if (!ostream_puts(ostream_1, env.s + pos) || !ostream_put(ostream_1, "\n", 1))
            log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

    if (!ostream_flush(ostream_1))
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;

    return 0 ;
}
