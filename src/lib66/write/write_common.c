/*
 * write_common.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/sastr.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/unix-transactional.h>

#include <66/service.h>
#include <66/write.h>
#include <66/constants.h>
#include <66/environ.h>
#include <66/enum.h>

int write_common(resolve_service_t *res, char const *dst)
{
    log_flow() ;

    /** down file */
    if (res->execute.down) {
        log_trace("create file: ", dst, "/down") ;
        if (!file_create_empty(dst, "down", 0644))
            log_warnusys_return(LOG_EXIT_ZERO, "create down file") ;
    }

    /** notification-fd */
    if (res->notify)
        if (!write_uint(dst, SS_NOTIFICATION, res->notify))
            log_warnusys_return(LOG_EXIT_ZERO, "write uint file", SS_NOTIFICATION) ;

    /** timeout family
     * Only write timeout file for classic service.
     * S6-supervise need it otherwise it's read directly
     * from the resolve file at start process. */
    if (res->execute.timeout.kill)
        if (!write_uint(dst, "timeout-kill", res->execute.timeout.kill))
            log_warnusys_return(LOG_EXIT_ZERO, "write uint file timeout-kill") ;

    if (res->execute.timeout.finish)
        if (!write_uint(dst, "timeout-finish", res->execute.timeout.finish))
            log_warnusys_return(LOG_EXIT_ZERO, "write uint file timeout-finish") ;

    /** max-death-tally */
    if (res->maxdeath)
        if (!write_uint(dst, SS_MAXDEATHTALLY, res->maxdeath))
            log_warnusys_return(LOG_EXIT_ZERO, "write uint file", SS_MAXDEATHTALLY) ;

    /** down-signal */
    if (res->execute.downsignal)
        if (!write_uint(dst, "down-signal", res->execute.downsignal))
            log_warnusys_return(LOG_EXIT_ZERO, "write uint file down-signal") ;

    /** environment for module is already written by the regex_configure() function */
    if (res->environ.env && res->type != TYPE_MODULE) {

        stralloc dst = STRALLOC_ZERO ;
        stralloc contents = STRALLOC_ZERO ;
        char name[strlen(res->sa.s + res->name) + 2] ;
        auto_strings(name, ".", res->sa.s + res->name) ;

        if (!env_prepare_for_write(&dst, &contents, res))
            log_warnusys_return(LOG_EXIT_ZERO, "prepare environment for: ", res->sa.s + res->name) ;

        if (!write_environ(name, contents.s, dst.s))
            log_warnusys_return(LOG_EXIT_ZERO, "write environment for: ", res->sa.s + res->name) ;

        stralloc_free(&dst) ;
        stralloc_free(&contents) ;
    }

    /** hierarchy copy */
    if (res->hiercopy) {

        int r ;
        size_t pos = 0 ;
        stralloc sa = STRALLOC_ZERO ;
        char *src = res->sa.s + res->path.frontend ;
        size_t srclen = strlen(src), dstlen = strlen(dst) ;
        char basedir[srclen + 1] ;

        if (!ob_dirname(basedir, src))
            log_warnusys_return(LOG_EXIT_ZERO, "get dirname of: ", src) ;

        if (!sastr_clean_string(&sa, res->sa.s + res->hiercopy))
            log_warnusys_return(LOG_EXIT_ZERO, "clean string") ;

        FOREACH_SASTR(&sa, pos) {

            char *what = sa.s + pos ;
            int fd ;
            size_t wlen = strlen(what) ;
            char tmp[SS_MAX_PATH_LEN + 1] ;
            char dest[dstlen + 1 + wlen + 1] ;
            char basename[SS_MAX_PATH_LEN + 1] ;

            if (what[0] == '/' ) {

                auto_strings(tmp, what) ;

                if (!ob_basename(basename, what))
                    log_warnusys_return(LOG_EXIT_ZERO, "get basename of: ", what) ;

                what = basename ;

            } else if (what[0] == '.' && ((what[1] == '/') || (what[1] == '.' && what[2] == '/'))) {
               /** distinction between .file and ./ ../ directory */
                char b[strlen(src) + 1]  ;

                if (!ob_dirname(b, src))
                    log_warnusys_return(LOG_EXIT_ZERO, "get dirname of: ", src) ;

                fd = open_read(".") ;
                if (fd < 0)
                    log_warnusys_return(LOG_EXIT_ZERO, "open current directory") ;

                if (chdir(b) < 0) {
                    fd_close(fd) ;
                    log_warnusys_return(LOG_EXIT_ZERO, "change directory") ;
                }

                char *p = realpath(what, tmp) ;
                if (!p) {
                    fd_close(fd) ;
                    log_warnusys_return(LOG_EXIT_ZERO, "get absolute path of: ", what) ;
                }

                if (fd_chdir(fd) < 0) {
                    fd_close(fd) ;
                    log_warnusys_return(LOG_EXIT_ZERO, "change directory") ;
                }

                fd_close(fd) ;

                if (!ob_basename(basename, what))
                    log_warnusys_return(LOG_EXIT_ZERO, "get basename of: ", what) ;

                what = basename ;

            } else {

                auto_strings(tmp, basedir, what) ;
            }

            r = scan_mode(tmp, S_IFDIR) ;
            if (r <= 0) {

                r = scan_mode(tmp, S_IFREG) ;
                if (!r)
                    log_warnusys_return(LOG_EXIT_ZERO, "find: ", tmp) ;
                else if (r < 0) {
                    errno = ENOTSUP ;
                    log_warnusys_return(LOG_EXIT_ZERO, "invalid format of: ", tmp) ;
                }
            }

            auto_strings(dest, dst, "/", what) ;

            log_trace("copy: ", tmp, " to: ", dest) ;
            if (!hiercopy(tmp, dest))
                log_warnusys_return(LOG_EXIT_ZERO, "copy: ", tmp, " to: ", dest) ;
        }

        stralloc_free(&sa) ;
    }

    return 1 ;
}
