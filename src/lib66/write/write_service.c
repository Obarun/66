/*
 * write_service.c
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
#include <stdint.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/enum.h>
#include <66/write.h>
#include <66/constants.h>

/** @Return 0 on fail
 * @Return 1 on success
 * @Return 2 if the service is ignored
 *
 * @workdir -> /var/lib/66/system/<tree>/servicedirs/
 * */
int write_services(resolve_service_t *res, char const *workdir, uint8_t force)
{
    log_flow() ;

    int r ;
    size_t workdirlen = strlen(workdir) ;
    char *name = res->sa.s + res->name ;
    size_t namelen = strlen(name) ;
    int type = res->type ;

    {
        resolve_service_t fres = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &fres) ;
        ss_state_t ste = STATE_ZERO ;
        if (resolve_check(workdir, name)) {
            if (!resolve_read(wres, workdir, name))
                log_dieu(LOG_EXIT_SYS, "read resolve file of: ", name) ;

            if (state_check(fres.sa.s + fres.path.home, name)) {

                if (!state_read(&ste, fres.sa.s + fres.path.home, name))
                    log_dieu(LOG_EXIT_SYS, "read state file of: ", name) ;

                if (fres.type != type && FLAGS_ISSET(ste.isenabled, STATE_FLAGS_TRUE))
                    log_die(LOG_EXIT_SYS, "Detection of incompatible type format for: ", name, " -- current: ", get_key_by_enum(ENUM_TYPE, type), " previous: ", get_key_by_enum(ENUM_TYPE, fres.type)) ;
            }
        }
        resolve_free(wres) ;
    }
    /**
     *
     *
     * please pass through a temporary or backup
     *
     * just need to switch the /run/66/state/0/<service> symlink
     * with atomic_symlink
     *
     *
     *
     * */
    char wname[workdirlen + SS_SVC_LEN + 1 + namelen + 1] ;
    auto_strings(wname, workdir, SS_SVC, "/", name) ;

    r = scan_mode(wname, S_IFDIR) ;
    if (r < 0)
        log_die(LOG_EXIT_SYS, "unvalide source: ", wname) ;

    if ((r && force) || !r) {

        if (!dir_rm_rf(wname))
            log_dieusys(LOG_EXIT_SYS, "remove: ", wname) ;

        if (!dir_create_parent(wname, 0755))
            log_dieusys(LOG_EXIT_SYS, "create ", wname) ;

    } else if (r && !force) {

        log_info("Ignoring: ", name, " service: already written") ;
        return 2 ;
    }

    log_trace("Write service ", name, " ...") ;

    switch(type) {

        case TYPE_MODULE:
        case TYPE_BUNDLE:
            break ;

        case TYPE_CLASSIC:

            write_classic(res, wname, force) ;
            break ;

        case TYPE_ONESHOT:

            write_oneshot(res, wname) ;
            break ;

        default:
            log_die(LOG_EXIT_SYS, "unkown type: ", get_key_by_enum(ENUM_TYPE, type)) ;
    }

    return 1 ;
}




