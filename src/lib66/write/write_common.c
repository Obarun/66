/*
 * write_common.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <sys/types.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/directory.h>
#include <oblibs/strbuf.h>

#include <66/service.h>
#include <66/write.h>

int write_common(resolve_service_addon_execute_t *ex, strbuf const *copyfrom, char const *dst)
{
    log_flow() ;

    size_t pos = 0, dstlen = strlen(dst) ;

    /** down file */
    if (ex->down) {
        log_trace("create file: ", dst, "/down") ;
        if (!file_create_empty(dst, "down", 0644))
            return -1 ;
    }

    /** hierarchy copy: the sources the parse resolved and checked (parse_core) */
    while (pos < copyfrom->len) {

        char const *src = copyfrom->s + pos ;
        pos += strlen(src) + 1 ;
        char const *name = copyfrom->s + pos ;
        pos += strlen(name) + 1 ;
        char dest[dstlen + 1 + strlen(name) + 1] ;

        auto_strings(dest, dst, "/", name) ;

        log_trace("copy: ", src, " to: ", dest) ;
        if (!tree_copy(src, dest))
            return -1 ;
    }

    /** run file */
    if (write_execute_scripts("run", ex->sa.s + ex->run.run, dst, 0) < 0)
        return -1 ;

    /** finish file */
    if (ex->finish.run && write_execute_scripts("finish", ex->sa.s + ex->finish.run, dst, 0) < 0)
        return -1 ;

    return 1 ;
}
