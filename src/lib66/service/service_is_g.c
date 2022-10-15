/*
 * service_is_g.c
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

#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/service.h>
#include <66/tree.h>
#include <66/resolve.h>
#include <66/state.h>
#include <66/constants.h>
#include <66/utils.h>

int service_is(ss_state_t *ste, uint32_t flag)
{

    switch (flag) {

        case STATE_FLAGS_TOINIT : return FLAGS_ISSET(ste->toinit, STATE_FLAGS_TRUE) ;

        case STATE_FLAGS_TORELOAD: return FLAGS_ISSET(ste->toreload, STATE_FLAGS_TRUE) ;

        case STATE_FLAGS_TORESTART : return FLAGS_ISSET(ste->torestart, STATE_FLAGS_TRUE) ;

        case STATE_FLAGS_TOUNSUPERVISE : return FLAGS_ISSET(ste->tounsupervise, STATE_FLAGS_TRUE) ;

        case STATE_FLAGS_ISDOWNFILE : return FLAGS_ISSET(ste->isdownfile, STATE_FLAGS_TRUE) ;

        case STATE_FLAGS_ISEARLIER : return FLAGS_ISSET(ste->isearlier, STATE_FLAGS_TRUE) ;

        case STATE_FLAGS_ISENABLED : return FLAGS_ISSET(ste->isenabled, STATE_FLAGS_TRUE) ;

        case STATE_FLAGS_ISPARSED : return  FLAGS_ISSET(ste->isparsed, STATE_FLAGS_TRUE) ; //always true framboise

        case STATE_FLAGS_ISSUPERVISED : return FLAGS_ISSET(ste->issupervised, STATE_FLAGS_TRUE) ;

        case STATE_FLAGS_ISUP : return FLAGS_ISSET(ste->isup, STATE_FLAGS_TRUE) ;

        default:
            break ;

    }

    return 0 ;
}

/*@Return :
 * -1 system error
 * 0 check fail
 * 1 check success */
int service_is_g(char *atree, char const *name, uint32_t flag)
{

    log_flow() ;

    ss_state_t ste = STATE_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    int e = -1, r = -1 ;
    char base[SS_MAX_PATH_LEN + SS_SYSTEM_LEN + 1] ;

    if (!set_ownersysdir_stack(base, getuid())) {

        log_warnusys("set owner directory") ;
        resolve_free(wres) ;
        return e ;
    }

    size_t baselen = strlen(base) ;
    //char tmp[baselen + SS_SYSTEM_LEN + 1] ;
    auto_strings(base + baselen, SS_SYSTEM) ;

    // no tree exist yet
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

    if (strlen(res.sa.s + res.treename) > SS_MAX_TREENAME) {
        errno = ENAMETOOLONG ;
        goto freed ;
    }

    auto_strings(atree, res.sa.s + res.treename) ;

    if (!state_read(&ste, res.sa.s + res.path.home, name)) {
        log_warnu("read state file of: ", name, " -- please make a bug report") ;
        goto freed ;
    }

    e = service_is(&ste, flag) ;

    freed:
        resolve_free(wres) ;
        return e ;
}


