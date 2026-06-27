/*
 * svc_is_up.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/string.h>

#include <66/svc.h>
#include <66/status.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/utils.h>

int svc_is_up(char const *name)
{
    log_flow() ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    int r, e = -1 ;
    char base[SS_MAX_PATH_LEN + SS_SYSTEM_LEN + 1] ;

    if (!set_ownersysdir_stack(base, getuid())) {
        log_warnusys("set owner directory") ;
        goto freed ;
    }

    size_t baselen = strlen(base) ;
    auto_strings(base + baselen, SS_SYSTEM) ;

    // no tree exists yet: nothing can be up
    if (!scan_mode(base, S_IFDIR)) {
        e = 0 ;
        goto freed ;
    }

    base[baselen] = 0 ;

    r = resolve_read_g(wres, base, name) ;
    if (r == -1)
        goto freed ;
    else if (!r) {
        e = 0 ;
        goto freed ;
    }

    service_status_t st = STATUS_ZERO ;
    if (svc_status(&res, &st) < 0)
        goto freed ;

    e = st.pid > 0 || st.state == STATUS_STATE_DONE ? 1 : 0 ;

    freed:
        resolve_free(wres) ;
        return e ;
}
