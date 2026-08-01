/*
 * env_runtime_publish.c
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
#include <sys/stat.h>

#include <oblibs/environ.h>
#include <oblibs/files.h>
#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/constants.h>
#include <66/environ.h>

int env_runtime_publish(char const *dir, char const *key, char const *value)
{
    log_flow() ;

    if (!env_runtime_key_isvalid(key))
        return 0 ;

    if (!value || !*value)
        return (errno = EINVAL, 0) ;

    /** the runtime directory is merged after environ_clean_unexport, so an
     * exclamation mark would reach the service as part of the value instead of
     * unexporting the key. */
    if (*value == SS_VAR_UNEXPORT)
        return (errno = EINVAL, 0) ;

    size_t dirlen = strlen(dir) ;

    {
        char const *exclude[] = { 0 } ;
        _cleanup_strbuf_ strbuf list = STRBUF_ZERO ;

        if (!sbl_dir_get(&list, dir, exclude, S_IFREG))
            return 0 ;

        // the cap only bites for a new key: replacing one leaves the count where it was
        if (sbl_search(&list, key) < 0 && sbl_count(&list) >= MAXFILE) {
            errno = E2BIG ;
            flog_warn_return(LOG_EXIT_ZERO, "runtime environment directory: %s is full -- it can not hold more than %d variables", dir, MAXFILE) ;
        }
    }

    _cleanup_strbuf_ strbuf out = STRBUF_ZERO ;

    if (!environ_untrim_kv(&out, key, value))
        return 0 ;

    char tmpdir[dirlen + 1] ;

    if (!ob_dirname(tmpdir, dir))
        return 0 ;

    size_t len = strlen(key) ;
    char dst[dirlen + 1 + len + 1] ;
    auto_strings(dst, dir, "/", key) ;

    return file_write_atomic_at(tmpdir, dst, out.s, out.len) ;
}
