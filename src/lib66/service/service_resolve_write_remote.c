/*
 * service_resolve_write_remote.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
#include <unistd.h>
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/directory.h>

#include <66/service.h>
#include <66/constants.h>
#include <66/parse.h>

void service_resolve_write_remote(resolve_service_t *res, char const *dst, uint8_t force)
{
    log_flow() ;

    char *name = res->sa.s + res->name ;
    size_t dstlen = strlen(dst) ;
    char dest[dstlen + SS_RESOLVE_LEN + 1] ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    auto_strings(dest, dst, SS_RESOLVE) ;

    if (access(dest, F_OK) < 0) {
        log_trace("create directory: ", dest) ;
        if (!dir_create_parent(dest, 0755)) {
            parse_cleanup(res, dst, force) ;
            free(wres) ;
            log_dieusys(LOG_EXIT_SYS, "create directory: ", dest) ;
        }
    }

    dest[dstlen] = 0  ;

    log_trace("write resolve file: ", dest, SS_RESOLVE, "/", name) ;
    if (!resolve_write(wres, dest, name)) {
        parse_cleanup(res, dst, force) ;
        free(wres) ;
        log_dieusys(LOG_EXIT_SYS, "write resolve file: ", dest, SS_RESOLVE, "/", name) ;
    }

    free(wres) ;
}
