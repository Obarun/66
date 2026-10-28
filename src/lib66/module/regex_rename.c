/*
 * regex_rename.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
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
#include <unistd.h>
#include <stdio.h>//rename

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/environ.h>

#include <66/module.h>

int regex_rename(strbuf *list, resolve_service_addon_regex_t *rx, uint32_t element)
{
    log_flow() ;

    if (!element)
        return 1 ;

    size_t pos = 0, idx = 0 ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    _alloc_sbl_(stk, strlen(rx->sa.s + element)) ;

    if (!sbl_clean_string(&stk, rx->sa.s + element))
        return -1 ;

    FOREACH_SBL(&stk, pos) {

        idx = 0 ;
        char *line = stk.s + pos ;
        _alloc_sbl_(key, strlen(line) + 1) ;
        _alloc_sbl_(val, strlen(line) + 1) ;

        // a line of the frontend without its key=value form is its fault
        if (!environ_get_key(&key, line) || !environ_get_value(&val, line))
            log_warn_return(LOG_EXIT_ZERO, "invalid format of the line: ", line, " -- expected key=value") ;

        FOREACH_SBL(list, idx) {

            sa.len = 0 ;
            char *str = list->s + idx ;
            size_t len = strlen(str) ;
            char dname[len + 1] ;

            char bname[len + 1] ;

            if (!ob_dirname(dname, str) || !ob_basename(bname, str))
                return -1 ;

            if (!strbuf_copys(&sa, bname) || !strbuf_terminate(&sa) ||
                !sbl_replace(&sa, key.s, val.s) || !strbuf_terminate(&sa))
                    return -1 ;

            char new[len + sa.len + 1] ;
            auto_strings(new, dname, sa.s) ;

            /** do not try to rename the same directory */
            if (strcmp(str, new)) {

                // inside the temporary copy of the module, which nobody reads
                log_trace("rename: ", str, " to: ", new) ;
                if (rename(str, new) == -1)
                    return -1 ;

                break ;
            }
        }
    }

    return 1 ;
}
