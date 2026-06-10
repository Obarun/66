/*
 * regex_replace.c
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

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>
#include <oblibs/lexer.h>
#include <oblibs/environ.h>

#include <66/resolve.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/module.h>

void regex_replace(strbuf *filelist, resolve_service_t *res)
{
    log_flow() ;

    int r ;
    size_t pos = 0, idx = 0 ;

    if (!res->regex.ninfiles)
        return ;

    _cleanup_strbuf_ strbuf frontend = STRBUF_ZERO ;
    _alloc_sbl_(infiles, strlen(res->sa.s + res->regex.infiles)) ;

    if (!sbl_clean_string(&infiles, res->sa.s + res->regex.infiles))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    FOREACH_SBL(filelist, pos) {

        frontend.len = idx = 0 ;
        char *file = filelist->s + pos ;
        size_t len = strlen(file) ;
        char bname[len + 1] ;
        char dname[len + 1] ;

        if (!ob_basename(bname, file))
            log_dieu(LOG_EXIT_SYS, "get basename of: ", file) ;

        if (!ob_dirname(dname, file))
            log_dieu(LOG_EXIT_SYS, "get dirname of: ", file) ;

        //log_trace("read service file of: ", dname, bname) ;
        r = read_svfile(&frontend, bname, dname) ;
        if (!r)
            log_dieusys(LOG_EXIT_SYS, "read file: ", file) ;
        else if (r == -1)
            continue ;

        {
            FOREACH_SBL(&infiles, idx) {

                uint8_t r = 0 ;
                char const *line = infiles.s + idx ;
                size_t linelen = strlen(line) ;
                _alloc_sbl_(filename, linelen + 1) ;
                _alloc_sbl_(key, linelen + 1) ;
                _alloc_sbl_(val, linelen + 1) ;

                if (linelen >= SS_MAX_PATH_LEN)
                    log_die(LOG_EXIT_SYS, "limit exceeded in service: ", res->sa.s + res->name) ;

                if ((line[0] != ':') || (get_sep_before(line + 1, ':', '=') < 0))
                    log_die(LOG_EXIT_SYS, "bad format in line: ", line, " of key InFiles field") ;

                r = (int)regex_get_file_name(&filename, line) ;
                if (!r)
                    log_dieusys(LOG_EXIT_SYS, "get file name at line: ",  line) ;

                if (!environ_get_key(&key, line + r))
                    log_dieusys(LOG_EXIT_SYS, "get key at line: ",  line + r) ;

                if (!environ_get_value(&val, line + r))
                    log_dieusys(LOG_EXIT_SYS, "get value at line: ",  line + r) ;

                if (!strcmp(bname, filename.s) || !filename.len) {

                    if (!sbl_replace_nline(&frontend, key.s, val.s))
                        log_dieu(LOG_EXIT_SYS, "replace: ", key.s, " by: ", val.s, " in file: ", file) ;

                    if (!file_write_at(dname, bname, frontend.s, frontend.len))
                        log_dieusys(LOG_EXIT_SYS, "write: ", dname, "/", bname) ;
                }
            }
        }
    }
}
