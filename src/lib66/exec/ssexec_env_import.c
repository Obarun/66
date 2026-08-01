/*
 * ssexec_env_import.c
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

#include <stdlib.h> // getenv

#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/constants.h>
#include <66/environ.h>
#include <66/ssexec.h>

int ssexec_env_import(int argc, char const *const *argv, void *data)
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

        /** transport only: the value comes from the environment of the caller,
         * verbatim. Nothing is computed, guessed or checked for reachability. */
        char const *value = getenv(key) ;

        if (!value) {
            log_warn("variable: ", key, " is not set in the environment -- ignored") ;
            continue ;
        }

        if (!*value) {
            log_warn("variable: ", key, " is empty -- ignored") ;
            continue ;
        }

        if (*value == SS_VAR_UNEXPORT)
            log_die(LOG_EXIT_USER, "value of variable: ", key, " starts with an exclamation mark -- the runtime environment is published verbatim") ;

        if (!env_runtime_publish(dir.s, key, value))
            log_dieusys(LOG_EXIT_SYS, "publish variable: ", key) ;

        log_info("Published variable: ", key) ;

        env_runtime_emit(info->scandir.s, info->who, SS_LIVEENV_EVENT, key) ;
    }

    return 0 ;
}
