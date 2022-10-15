/*
 * write_common.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */


#include <string.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/sastr.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/service.h>
#include <66/write.h>
#include <66/constants.h>
#include <66/environ.h>
#include <66/enum.h>

void write_common(resolve_service_t *res, char const *dst)
{
    log_flow() ;

    /** down file */
    if (res->execute.down)
        if (!file_create_empty(dst, "down", 0644))
            log_dieusys(LOG_EXIT_SYS, "create down file") ;

    /** notification-fd */
    if (res->notify)
        write_uint(dst, SS_NOTIFICATION, res->notify) ;

    /** timeout family
     *
     * Only write timeout file for classic service.
     * All others services are read directly through
     * the resolve file at start process. */
    if (res->execute.timeout.kill)
        write_uint(dst, "timeout-kill", res->execute.timeout.kill) ;

    if (res->execute.timeout.finish)
        write_uint(dst, "timeout-finish", res->execute.timeout.finish) ;

    /** max-death-tally */
    if (res->maxdeath)
        write_uint(dst, SS_MAXDEATHTALLY, res->maxdeath) ;

    /** down-signal */
    if (res->execute.downsignal)
        write_uint(dst, "down-signal", res->execute.downsignal) ;

    /** environment
     * environment for module is already written by the parse_module() function */
    if (res->environ.env && res->type != TYPE_MODULE) {

        stralloc dst = STRALLOC_ZERO ;
        stralloc contents = STRALLOC_ZERO ;
        char name[strlen(res->sa.s + res->name) + 2] ;
        auto_strings(name, ".", res->sa.s + res->name) ;

        if (!env_prepare_for_write(&dst, &contents, res))
            log_dieu(LOG_EXIT_SYS, "prepare environment for: ", res->sa.s + res->name) ;

        write_environ(name, contents.s, dst.s) ;

        stralloc_free(&dst) ;
        stralloc_free(&contents) ;
    }

    /** hierarchy copy */
    if (res->hiercopy) {

        int r ;
        size_t pos = 0 ;
        stralloc sa = STRALLOC_ZERO ;
        char *src = res->sa.s + res->path.frontend ;
        size_t srclen = strlen(src) ;

        if (!sastr_clean_string(&sa, res->sa.s + res->hiercopy))
            log_dieu(LOG_EXIT_SYS, "clean string") ;

        FOREACH_SASTR(&sa, pos) {

            char *what = sa.s + pos ;
            char tmp[SS_MAX_PATH_LEN + 1] ;
            char basedir[srclen + 1] ;

            if (!ob_dirname(basedir, src))
                log_dieu(LOG_EXIT_SYS, "get dirname of: ", src) ;

            if (what[0] == '.') {

                if (!dir_beabsolute(tmp, src))
                    log_dieusys(LOG_EXIT_SYS, "find absolute path of: ", what) ;

            } else {

                auto_strings(tmp, what) ;
            }

            r = scan_mode(tmp, S_IFDIR) ;
            if (r <= 0) {

                r = scan_mode(tmp, S_IFREG) ;
                if (!r)
                    log_dieusys(LOG_EXIT_SYS, "find: ", tmp) ;
                if (r < 0) {
                    errno = ENOTSUP ;
                    log_diesys(LOG_EXIT_SYS, "invalid format of: ", tmp) ;
                }
            }

            if (!hiercopy(tmp, dst))
                log_dieusys(LOG_EXIT_SYS, "copy: ", tmp, " to: ", dst) ;
        }
    }
}
