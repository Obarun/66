/*
 * module_search_service.c
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

#include <sys/types.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/enum.h>
#include <66/utils.h>

int module_search_service(char const *src, genalloc *gares, char const *name,uint8_t *found, char module_name[256])
{
    log_flow() ;

    int e = 0 ;
    size_t srclen = strlen(src), pos = 0, deps = 0 ;
    stralloc list = STRALLOC_ZERO ;
    stralloc tmp = STRALLOC_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    char const *exclude[2] = { SS_MASTER + 1, 0 } ;

    char t[srclen + SS_RESOLVE_LEN + 1] ;
    auto_strings(t,src,SS_RESOLVE) ;

    if (!sastr_dir_get(&list,t,exclude,S_IFREG)) goto err ;

    for (;pos < list.len ; pos += strlen(list.s + pos) + 1)
    {
        char *dname = list.s + pos ;
        if (!resolve_read(wres,src,dname)) goto err ;
        if (res.type == TYPE_MODULE && res.dependencies.depends)
        {
            if (!sastr_clean_string(&tmp,res.sa.s + res.dependencies.depends)) goto err ;
            for (deps = 0 ; deps < tmp.len ; deps += strlen(tmp.s + deps) + 1)
            {
                if (!strcmp(name,tmp.s + deps))
                {
                    (*found)++ ;
                    if (strlen(dname) > 255) log_1_warn_return(LOG_EXIT_ZERO,"module name too long") ;
                    auto_strings(module_name,dname) ;
                    goto end ;
                }
            }
        }
    }
    end:
    /** search if the service is on the commandline
     * if not we crash */
    for(pos = 0 ; pos < genalloc_len(resolve_service_t,gares) ; pos++)
    {
        resolve_service_t_ref pres = &genalloc_s(resolve_service_t, gares)[pos] ;
        char *str = pres->sa.s ;
        char *name = str + pres->name ;
        if (!strcmp(name,module_name)) {
            (*found) = 0 ;
            break ;
        }
    }

    e = 1 ;

    err:
        stralloc_free(&list) ;
        stralloc_free(&tmp) ;
        resolve_free(wres) ;
        return e ;
}
