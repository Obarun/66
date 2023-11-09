/*
 * sanitize_write.c
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
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <oblibs/log.h>
#include <oblibs/directory.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>

#include <skalibs/djbunix.h>
#include <skalibs/unix-transactional.h>

#include <66/sanitize.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/enum.h>
#include <66/tree.h>
#include <66/state.h>
#include <66/svc.h>

static void resolve_compare(resolve_service_t *res)
{
    log_flow() ;

    int r ;
    resolve_service_t fres = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &fres) ;
    ss_state_t sta = STATE_ZERO ;
    char *name = res->sa.s + res->name ;

    r = resolve_read_g(wres, res->sa.s + res->path.home, name) ;
    if (r < 0)
        log_dieu(LOG_EXIT_SYS, "read resolve file of: ", name) ;

    if (r) {

        if (!state_read(&sta, &fres))
            log_dieu(LOG_EXIT_SYS, "read state file of: ", name) ;

        if (sta.issupervised == STATE_FLAGS_TRUE) {

            if (fres.type != res->type)
                log_die(LOG_EXIT_SYS, "Detection of incompatible type format for supervised service: ", name, " -- current: ", get_key_by_enum(ENUM_TYPE, res->type), " previous: ", get_key_by_enum(ENUM_TYPE, fres.type), ". Please unsupervise it with '66 unsupervice ", name,"' before trying the conversion") ;
        }

        if (strcmp(res->sa.s + res->treename, fres.sa.s + fres.treename))
            tree_service_remove(fres.sa.s + fres.path.home, fres.sa.s + fres.treename, name) ;
    }

    resolve_free(wres) ;
}

static int preserve(resolve_service_t *res, uint8_t force)
{
    log_flow() ;

    int r = 0 ;

    char dir[strlen(res->sa.s + res->path.servicedir) + 1] ;

    auto_strings(dir, res->sa.s + res->path.servicedir) ;

    r = scan_mode(dir, S_IFDIR) ;
    if (r < 0)
        log_diesys(LOG_EXIT_SYS, "unvalid source: ", dir) ;

    if (r) {

        if (force) {

            resolve_compare(res) ;

        } else
            /** This info should only be executed with reconfigure process as long as the parse_frontend
             *  verify the service and prevents reaching this point if !force. */
            log_warn_return(0, "Ignoring: ", res->sa.s + res->name, " -- service already written") ;
    }

    return r ;
}

int sanitize_write(resolve_service_t *res, uint8_t force)
{
    log_flow() ;

    return preserve(res, force) ;
}

