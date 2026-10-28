/*
 * parse_compute_list.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <stdint.h>
#include <unistd.h> // getuid

#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/module.h>

/**
 * @opts -> 1 : build list removing commented optional deps
 * */
int parse_compute_list(resolve_wrapper_t_ref wres, strbuf *store, uint32_t *res, uint8_t opts)
{
    log_flow() ;

    if (!store->len)
        return resolve_add_string(wres, "") ;

    int r, found = 0 ;
    size_t pos = 0 ;
     uint32_t off ;
    char const *exclude[2] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1 } ;
    _cleanup_strbuf_ strbuf tmp = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf kept = STRBUF_ZERO ;

    FOREACH_SBL(store, pos) {

        if (store->s[pos] == '#')
            continue ;

        if (opts) {

            tmp.len = 0 ;

            r = service_frontend_path(&tmp, store->s + pos, getuid(), 0, exclude, 2) ;
            if (r < 0)
                return -1 ;

            if (!r)
                continue ;

            found++ ;

        }

        if (!sbl_add(&kept, store->s + pos))
            return -1 ;

        (*res)++ ;

        if (found)
            break ;
    }

    if (!(*res))
        return 0 ;

    return resolve_add_list(wres, &kept, &off) < 0 ? -1 : (int)off ;
}

