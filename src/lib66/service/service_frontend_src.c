/*
 * service_frontend_src.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <sys/stat.h>

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/service.h>
#include <66/instance.h>

int service_frontend_list(strbuf *list, char const *src, char const **exclude)
{
    log_flow() ;

    size_t pathlen = strlen(src) ;
    char path[pathlen + 1] ;

    auto_strings(path, src) ;

    /** avoid double slash */
    if (path[pathlen - 1] == '/')
        path[pathlen - 1] = 0 ;

    return sbl_dir_get_recursive(list, path, exclude, FLAGS_REG|FLAGS_DIR, 1) ? 1 : -1 ;
}

int service_frontend_search(strbuf *sasrc, char const *name, strbuf const *list, size_t start, size_t end, char const **exclude)
{
    log_flow() ;

    int insta, equal = 0, e = -1, r = 0, found = 0 ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    size_t pos = start, dpos = 0, namelen = strlen(name) ;
    char instaname[strlen(name) + 1] ;

    insta = instance_check(name) ;
    if (!insta)
        log_warn_return(e,"invalid instance name: ", name) ;

    if (insta > 0) {
        /** search for the template name */
        memcpy(instaname, name, insta + 1) ;
        instaname[insta + 1] = 0 ;

    } else {

        auto_strings(instaname, name) ;
    }

    for (; pos < end ; pos += strlen(list->s + pos) + 1) {

        sa.len = 0 ;
        char const *dname = list->s + pos ;

        struct stat st ;
        size_t dnamelen = strlen(dname) ;
        char bname[dnamelen + 1] ;
        char srcname[dnamelen + 1] ;

        if (!ob_basename(bname,dname))
            return e ;

        equal = strcmp(instaname, bname) ;

        if (!equal) {

            if (stat(dname,&st) == -1 || !ob_dirname(srcname, dname))
                return e ;

            found = 1 ;

            if (S_ISREG(st.st_mode)) {

                char result[strlen(srcname) + namelen + 1] ;

                auto_strings(result, srcname, name) ;

                if (sbl_search(sasrc, result) == -1) {
                    if (!sbl_add(sasrc, result) ||
                        !strbuf_terminate(sasrc))
                            return e ;
                    sasrc->len-- ;
                }

                break ;

            } else if (S_ISDIR(st.st_mode)) {

                if (!insta)
                    log_warn_return(e,"invalid name format for: ", dname," -- directory instance name are case reserved") ;

                int rd = service_endof_dir(dname, bname) ;
                if (rd == -1)
                    return e ;

                if (!rd) {
                    /** user ask to deal which each frontend file
                     * inside the directory */
                    sa.len = 0 ;

                    if (!sbl_dir_get_recursive(&sa, dname, exclude, FLAGS_REG|FLAGS_DIR, 0))
                        return e ;

                    /** directory may be empty. */
                    if (!sa.len)
                        found = 0 ;

                    dpos = 0 ;
                    FOREACH_SBL(&sa, dpos) {

                        r = service_frontend_src(sasrc, sa.s + dpos, dname, exclude) ;
                        if (r < 0)
                            /** system error */
                            return e ;
                        if (!r)
                            /** nothing found at empty sub-directory */
                            found = 0 ;
                    }

                    break ;

                }
            }
        }
    }

    e = found ;

    return e ;
}

int service_frontend_src(strbuf *sasrc, char const *name, char const *src, char const **exclude)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf list = STRBUF_ZERO ;

    if (service_frontend_list(&list, src, exclude) < 0)
        return -1 ;

    return service_frontend_search(sasrc, name, &list, 0, list.len, exclude) ;
}
