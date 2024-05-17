/*
 * regex_replace.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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

#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/files.h>

#include <skalibs/stralloc.h>

#include <66/resolve.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/module.h>

void regex_replace(stralloc *list, resolve_service_t *res)
{
    log_flow() ;

    int r ;
    size_t pos = 0, idx = 0 ;

    stralloc frontend = STRALLOC_ZERO ;
    stralloc sa = STRALLOC_ZERO ;

    if (!res->regex.ninfiles)
        return ;

    if (!sastr_clean_string(&sa, res->sa.s + res->regex.infiles))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    FOREACH_SASTR(list, pos) {

        frontend.len = idx = 0 ;
        char *str = list->s + pos ;
        size_t len = strlen(str) ;
        char bname[len + 1] ;
        char dname[len + 1] ;

        if (!ob_basename(bname, str))
            log_dieu(LOG_EXIT_SYS, "get basename of: ", str) ;

        if (!ob_dirname(dname, str))
            log_dieu(LOG_EXIT_SYS, "get dirname of: ", str) ;

        //log_trace("read service file of: ", dname, bname) ;
        r = read_svfile(&frontend, bname, dname) ;
        if (!r)
            log_dieusys(LOG_EXIT_SYS, "read file: ", str) ;
        else if (r == -1)
            continue ;

        {
            FOREACH_SASTR(&sa, idx) {

                int all = 0, fpos = 0 ;
                char const *line = sa.s + idx ;
                size_t linelen = strlen(line) ;
                char filename[SS_MAX_SERVICE_NAME + 1] ;
                char replace[linelen + 1] ;
                char regex[linelen + 1] ;

                if (linelen >= SS_MAX_PATH_LEN)
                    log_die(LOG_EXIT_SYS, "limit exceeded in service: ", res->sa.s + res->name) ;

                if ((line[0] != ':') || (get_sep_before(line + 1, ':', '=') < 0))
                    log_die(LOG_EXIT_SYS, "bad format in line: ", line, " of key @infiles field") ;

                memset(filename, 0, (SS_MAX_SERVICE_NAME + 1) * sizeof(char)); ;
                memset(replace, 0, (linelen + 1) * sizeof(char)) ;
                memset(regex, 0, (linelen + 1) * sizeof(char)) ;

                fpos = regex_get_file_name(filename, line) ;
                if (fpos < 3) all = 1 ;

                regex_get_replace(replace, line + fpos) ;

                regex_get_regex(regex, line + fpos) ;

                if (!strcmp(bname, filename) || all) {

                    if (!sastr_replace_all(&frontend, replace, regex))
                        log_dieu(LOG_EXIT_SYS, "replace: ", replace, " by: ", regex, " in file: ", str) ;

                    if (!stralloc_0(&frontend))
                        log_dieusys(LOG_EXIT_SYS, "stralloc") ;

                    frontend.len-- ;

                    if (!file_write_unsafe(dname, bname, frontend.s, frontend.len))
                        log_dieusys(LOG_EXIT_SYS, "write: ", dname, "/", bname) ;
                }
            }
        }
    }

    stralloc_free(&frontend) ;
    stralloc_free(&sa) ;
}
