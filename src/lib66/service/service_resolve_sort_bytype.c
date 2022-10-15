/*
 * service_resolve_sort_bytype.c
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

#include <oblibs/log.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/resolve.h>
#include <66/enum.h>
#include <66/service.h>
/***
 *
 *
 *
 * may not be used anymore
 *
 *
 *
 *
 * */
int service_resolve_sort_bytype(stralloc *list, char const *src)
{
    log_flow() ;

    size_t pos = 0, len = list->len ;
    int e = 0 ;

    char tmp[len + 1] ;

    sastr_to_char(tmp, list) ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    size_t classic_list = 0, module_list = 0 ;

    list->len = 0 ;

    for (; pos < len ; pos += strlen(tmp + pos) + 1) {

        size_t nlen = strlen(tmp + pos) + 1 ;
        char *name = tmp + pos ;

        if (!resolve_read(wres, src, name))
            log_warnu_return(LOG_EXIT_ZERO,"read resolve file of: ", src, name) ;

        switch (res.type) {

            case TYPE_CLASSIC:

                if (!stralloc_insertb(list, 0, name, strlen(name) + 1))
                    goto err ;

                classic_list += nlen  ;
                module_list = classic_list ;

                break ;

            case TYPE_MODULE:

                if (!stralloc_insertb(list, classic_list, name, strlen(name) + 1))
                    goto err ;

                module_list += nlen ;

                break ;

            case TYPE_BUNDLE:
            case TYPE_ONESHOT:

                if (!stralloc_insertb(list, module_list, name, strlen(name) + 1))
                    goto err ;

                break ;

            default: log_warn_return(LOG_EXIT_ZERO,"unknown action") ;
        }
    }

    e = 1 ;

    err:
        resolve_free(wres) ;
        return e ;
}
