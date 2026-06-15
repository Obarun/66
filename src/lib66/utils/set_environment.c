/*
 * set_environment.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
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
#include <stddef.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>

#include <66/constants.h>
#include <66/utils.h>

int set_environment(strbuf *sa, uid_t owner)
{
    log_flow() ;

    char ownerpack[UID_FMT] ;

    if (!owner) {

        if (!auto_strbuf(sa, SS_ENVIRONMENT_ADMDIR))
            log_warnsys_return(LOG_EXIT_ZERO,"strbuf") ;

    } else {

        size_t ownerlen = uid_format(ownerpack,owner) ;
        ownerpack[ownerlen] = 0 ;

        if (!set_ownerhome(sa, owner))
            log_warnusys_return(LOG_EXIT_ZERO, "set home directory") ;

        if (!auto_strbuf(sa, SS_ENVIRONMENT_USERDIR))
            log_die_nomem("strbuf") ;
    }

    return 1 ;
}