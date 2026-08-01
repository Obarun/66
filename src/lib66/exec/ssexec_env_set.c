/*
 * ssexec_env_set.c
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

#include <oblibs/log.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/environ.h>
#include <66/ssexec.h>

int ssexec_env_set(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing variable=value pair") ;

    _cleanup_strbuf_ strbuf dir = STRBUF_ZERO ;

    env_runtime_setdir(&dir, &info->live, info->owner) ;

    for (int pos = 0 ; pos < argc ; pos++) {

        char const *pair = argv[pos] ;
        ssize_t klen = get_len_until(pair, '=') ;

        if (klen <= 0)
            log_die(LOG_EXIT_USER, "invalid variable=value pair: ", pair) ;

        // bound the name before it sizes the buffer below
        if (klen > SS_MAX_SERVICE_NAME)
            flog_die(LOG_EXIT_USER, "variable name is too long -- it can not exceed %d characters", SS_MAX_SERVICE_NAME) ;

        char key[klen + 1] ;
        memcpy(key, pair, klen) ;
        key[klen] = 0 ;

        if (!env_runtime_key_isvalid(key))
            log_dieusys(LOG_EXIT_USER, "invalid variable name: ", key) ;

        char const *value = pair + klen + 1 ;

        if (!*value)
            log_die(LOG_EXIT_USER, "empty value for variable: ", key) ;

        if (*value == SS_VAR_UNEXPORT)
            log_die(LOG_EXIT_USER, "value of variable: ", key, " starts with an exclamation mark -- the runtime environment is published verbatim") ;

        if (!env_runtime_publish(dir.s, key, value))
            log_dieusys(LOG_EXIT_SYS, "publish variable: ", key) ;

        log_info("Published variable: ", key) ;

        env_runtime_emit(info->scandir.s, info->who, SS_LIVEENV_EVENT, key) ;
    }

    return 0 ;
}
