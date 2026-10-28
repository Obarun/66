/*
 * regex_replace.c
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

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>
#include <oblibs/environ.h>

#include <66/resolve.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/module.h>

int regex_replace(strbuf *filelist, resolve_service_addon_regex_t *rx, resolve_service_t *res)
{
    log_flow() ;

    int r ;
    size_t pos = 0, idx = 0 ;

    if (!rx->ninfiles)
        return 1 ;

    _cleanup_strbuf_ strbuf frontend = STRBUF_ZERO ;
    _alloc_sbl_(infiles, strlen(rx->sa.s + rx->infiles)) ;

    if (!sbl_clean_string(&infiles, rx->sa.s + rx->infiles))
        return -1 ;

    FOREACH_SBL(filelist, pos) {

        frontend.len = idx = 0 ;
        char *file = filelist->s + pos ;
        size_t len = strlen(file) ;
        char bname[len + 1] ;
        char dname[len + 1] ;

        if (!ob_basename(bname, file) || !ob_dirname(dname, file))
            return -1 ;

        // an empty file has nothing to replace
        r = read_svfile(&frontend, bname, dname) ;
        if (!r)
            return -1 ;
        else if (r == -1)
            continue ;

        {
            FOREACH_SBL(&infiles, idx) {

                uint32_t off = 0 ;
                char const *line = infiles.s + idx ;
                size_t linelen = strlen(line) ;
                _alloc_sbl_(filename, linelen + 1) ;
                _alloc_sbl_(key, linelen + 1) ;
                _alloc_sbl_(val, linelen + 1) ;

                if (linelen >= SS_MAX_PATH_LEN)
                    log_warn_return(LOG_EXIT_ZERO, "limit exceeded in service: ", res->sa.s + res->name) ;

                if ((line[0] != ':') || (get_sep_before(line + 1, ':', '=') < 0))
                    log_warn_return(LOG_EXIT_ZERO, "bad format in line: ", line, " of key InFiles field") ;

                // the format is checked above: what fails now is the system
                off = regex_get_file_name(&filename, line) ;
                if (!off || !environ_get_key(&key, line + off) || !environ_get_value(&val, line + off))
                    return -1 ;

                if (!strcmp(bname, filename.s) || !filename.len) {

                    // inside the temporary copy of the module, which nobody reads
                    if (!sbl_replace_nline(&frontend, key.s, val.s) ||
                        !file_write_at(dname, bname, frontend.s, frontend.len))
                            return -1 ;
                }
            }
        }
    }

    return 1 ;
}
