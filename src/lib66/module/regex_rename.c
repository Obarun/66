/*
 * regex_rename.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <unistd.h>
#include <stdio.h>//rename

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/lexer.h>
#include <oblibs/environ.h>

#include <skalibs/djbunix.h>

#include <66/module.h>
#include <66/constants.h>

void regex_rename(strbuf *list, resolve_service_t *res, uint32_t element)
{
    log_flow() ;

    if (!element)
        return ;

    size_t pos = 0, idx = 0 ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    _alloc_sbl_(stk, strlen(res->sa.s + element)) ;

    if (!sbl_clean_string(&stk, res->sa.s + element))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    FOREACH_SBL(&stk, pos) {

        idx = 0 ;
        char *line = stk.s + pos ;
        _alloc_sbl_(key, strlen(line) + 1) ;
        _alloc_sbl_(val, strlen(line) + 1) ;

        if (!environ_get_key(&key, line))
            log_dieusys(LOG_EXIT_SYS, "get key at line: ",  line) ;

        if (!environ_get_value(&val, line))
            log_dieusys(LOG_EXIT_SYS, "get value at line: ",  line) ;

        FOREACH_SBL(list, idx) {

            sa.len = 0 ;
            char *str = list->s + idx ;
            size_t len = strlen(str) ;
            char dname[len + 1] ;

            if (!ob_dirname(dname, str))
                log_dieu(LOG_EXIT_SYS, "get dirname of: ", str) ;

            char bname[len + 1] ;
            if (!ob_basename(bname, str))
                log_dieu(LOG_EXIT_SYS, "get basename of: ", str) ;

            if (!strbuf_copys(&sa, bname) || !strbuf_terminate(&sa))
                log_die_nomem("strbuf") ;

            if (!sbl_replace(&sa, key.s, val.s))
                log_dieu(LOG_EXIT_SYS, "replace: ", key.s, " by: ", val.s, " in file: ", str) ;

            if (!strbuf_terminate(&sa))
                log_die_nomem("strbuf") ;

            char new[len + sa.len + 1] ;
            auto_strings(new, dname, sa.s) ;

            /** do not try to rename the same directory */
            if (strcmp(str, new)) {

                log_trace("rename: ", str, " to: ", new) ;
                if (rename(str, new) == -1)
                    //log_warnusys( "rename: ", str, " to: ", new) ;
                    log_dieusys(LOG_EXIT_SYS, "rename: ", str, " to: ", new) ;

                break ;
            }
        }
    }
}
