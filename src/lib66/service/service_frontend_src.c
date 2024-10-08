/*
 * service_frontend_src.c
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
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/service.h>
#include <66/instance.h>

int service_frontend_src(stralloc *sasrc, char const *name, char const *src, char const **exclude)
{
    log_flow() ;

    int insta, equal = 0, e = -1, r = 0, found = 0 ;
    _alloc_sa_(sa) ;
    size_t pos = 0, dpos = 0, pathlen = strlen(src), namelen = strlen(name) ;
    char instaname[strlen(name) + 1] ;

    char path[pathlen + 1] ;

    auto_strings(path, src) ;

    /** avoid double slash */
    if (path[pathlen - 1] == '/')
        path[pathlen - 1] = 0 ;

    if (!sastr_dir_get_recursive(&sa, path, exclude, S_IFREG|S_IFDIR, 1))
        return e ;

    size_t len = sa.len ;
    char tmp[len + 1] ;
    sastr_to_char(tmp, &sa) ;

    insta = instance_check(name) ;
    if (!insta)
        log_warn_return(e,"invalid instance name: ", name) ;

    sa.len = 0 ;

    if (insta > 0) {
        /** search for the template name */
        memcpy(instaname, name, insta + 1) ;
        instaname[insta + 1] = 0 ;

    } else {

        auto_strings(instaname, name) ;
    }

    for (; pos < len ; pos += strlen(tmp + pos) + 1) {

        sa.len = 0 ;
        char *dname = tmp + pos ;

        struct stat st ;
        /** use stat instead of lstat to accept symlink */
        if (stat(dname,&st) == -1)
            return e ;

        size_t dnamelen = strlen(dname) ;
        char bname[dnamelen + 1] ;
        char srcname[dnamelen + 1] ;

        if (!ob_basename(bname,dname))
            return e ;

        if (!ob_dirname(srcname, dname))
            return e ;

        equal = strcmp(instaname, bname) ;

        if (!equal) {

            found = 1 ;

            if (S_ISREG(st.st_mode)) {

                char result[strlen(srcname) + namelen + 1] ;

                auto_strings(result, srcname, name) ;

                if (sastr_cmp(sasrc, result) == -1) {
                    if (!sastr_add_string(sasrc, result) ||
                        !stralloc_0(sasrc))
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

                    if (!sastr_dir_get_recursive(&sa, dname, exclude, S_IFREG|S_IFDIR, 0))
                        return e ;

                    /** directory may be empty. */
                    if (!sa.len)
                        found = 0 ;

                    dpos = 0 ;
                    FOREACH_SASTR(&sa, dpos) {

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
