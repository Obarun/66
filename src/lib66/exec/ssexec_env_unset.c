/*
 * ssexec_env_unset.c
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

#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/constants.h>
#include <66/environ.h>
#include <66/ssexec.h>

int ssexec_env_unset(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing variable name") ;

    _cleanup_strbuf_ strbuf dir = STRBUF_ZERO ;

    env_runtime_setdir(&dir, &info->live, info->owner) ;

    for (int pos = 0 ; pos < argc ; pos++) {

        char const *key = argv[pos] ;

        if (!env_runtime_key_isvalid(key))
            log_dieusys(LOG_EXIT_USER, "invalid variable name: ", key) ;

        int r = env_runtime_withdraw(dir.s, key) ;

        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "withdraw variable: ", key) ;

        if (!r) {
            log_warn("variable: ", key, " is not published -- nothing to do") ;
            continue ;
        }

        log_info("Withdrew variable: ", key) ;

        /** a distinct name: a reactor woken on env.<key> would otherwise have no
         * way to tell an appearance from a disappearance. */
        env_runtime_emit(info->scandir.s, info->who, SS_LIVEENV_EVENT_GONE, key) ;
    }

    return 0 ;
}
