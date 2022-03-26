/*
 * service_frontend_src.c
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
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/utils.h>
#include <66/service.h>

int service_frontend_src(stralloc *sasrc, char const *name, char const *src)
{
    log_flow() ;

    int  insta, equal = 0, e = -1, r = 0, found = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    size_t pos = 0, dpos = 0 ;

    char const *exclude[1] = { 0 } ;

    if (!sastr_dir_get_recursive(&sa, src, exclude, S_IFREG|S_IFDIR, 1)) {
        stralloc_free(&sa) ;
        return e ;
    }
    size_t len = sa.len ;
    char tmp[len + 1] ;
    sastr_to_char(tmp, &sa) ;

    for (; pos < len ; pos += strlen(tmp + pos) + 1) {

        sa.len = 0 ;
        char *dname = tmp + pos ;

        struct stat st ;
        /** use stat instead of lstat to accept symlink */
        if (stat(dname,&st) == -1)
            goto err ;

        size_t dnamelen = strlen(dname) ;
        char bname[dnamelen + 1] ;

        if (!ob_basename(bname,dname))
            goto err ;

        equal = strcmp(name, bname) ;

        insta = instance_check(bname) ;

        if (insta > 0) {

            if (!instance_splitname(&sa, bname, insta, SS_INSTANCE_TEMPLATE))
                goto err ;

            equal = strcmp(sa.s,bname) ;
        }

        if (!equal) {

            found = 1 ;

            if (S_ISREG(st.st_mode)) {

                if (sastr_cmp(sasrc, dname) == -1)
                    if (!sastr_add_string(sasrc, dname))
                        goto err ;

                break ;

            } else if (S_ISDIR(st.st_mode)) {

                if (!insta) {
                    log_warn("invalid name format for: ", dname," -- directory instance name are case reserved") ;
                    goto err ;
                }

                int rd = service_endof_dir(dname, bname) ;
                if (rd == -1)
                    goto err ;

                if (!rd) {
                    /** user ask to deal which each frontend file
                     * inside the directory */
                    sa.len = 0 ;

                    if (!sastr_dir_get_recursive(&sa, dname, exclude, S_IFREG|S_IFDIR, 0))
                        goto err ;

                    /** directory may be empty. */
                    if (!sa.len)
                        found = 0 ;

                    dpos = 0 ;
                    FOREACH_SASTR(&sa, dpos) {

                        r = service_frontend_src(sasrc, sa.s + dpos, dname) ;
                        if (r < 0)
                            /** system error */
                            goto err ;
                        if (!r)
                            /** nothing found on empty sub-directory */
                            found = 0 ;
                    }

                    break ;

                }
            }
        }
    }

    e = found ;

    err:
        stralloc_free(&sa) ;
        return e ;
}
