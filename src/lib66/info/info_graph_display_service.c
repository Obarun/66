/*
 * info_graph_display_service.c
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
 * */

#include <stdint.h>
#include <unistd.h>

#include <oblibs/log.h>

#include <skalibs/lolstdio.h>
#include <skalibs/types.h>

#include <66/service.h>
#include <66/utils.h>
#include <66/resolve.h>
#include <66/config.h>
#include <66/constants.h>
#include <66/state.h>
#include <66/enum.h>

#include <s6/supervise.h>

int info_graph_display_service(char const *name)
{
    log_flow() ;

    int err = 0 ;
    uint8_t pid_color = 0 ;

    char str_pid[UINT_FMT] ;
    char *ppid ;

    ss_state_t sta = STATE_ZERO ;
    s6_svstatus_t status = S6_SVSTATUS_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    char base[SS_MAX_PATH_LEN + SS_SYSTEM_LEN + 1] ;

    if (!set_ownersysdir_stack(base, getuid()))
        log_warn_return(LOG_EXIT_ZERO, "set owner directory") ;

    if (resolve_read_g(wres, base, name) <= 0) {
        log_warnu("read resolve file of: ",name) ;
        goto freed ;
    }

    if (!state_read(&sta, &res)) {
        log_warnusys("read state of: ", name) ;
        goto freed ;
    }

    if (res.type == TYPE_CLASSIC) {

        s6_svstatus_read(res.sa.s + res.live.scandir ,&status) ;
        pid_color = !status.pid ? 1 : 2 ;
        str_pid[uint_fmt(str_pid, status.pid)] = 0 ;
        ppid = &str_pid[0] ;

    } else {

         if (service_is(&sta, STATE_FLAGS_ISSUPERVISED) == STATE_FLAGS_FALSE) {

            ppid = "unitialized" ;
            goto dis ;

        } else if (service_is(&sta, STATE_FLAGS_ISUP) == STATE_FLAGS_FALSE) {

            ppid = "down" ;
            pid_color = 1 ;

        } else {

            ppid = "up" ;
            pid_color = 2 ;

        }
    }

    dis:

    if (!bprintf(buffer_1,"%s (%s%s%s%s,%s%s%s%s,%s%s%s%s,%s%s%s%s)", \

        name, \

        "pid=",pid_color > 1 ? log_color->valid : pid_color ? log_color->error : log_color->warning, \
        ppid, \
        log_color->off, \

        "state=", service_is(&sta, STATE_FLAGS_ISENABLED) == STATE_FLAGS_TRUE ? log_color->valid : log_color->warning, \
        service_is(&sta, STATE_FLAGS_ISENABLED) == STATE_FLAGS_TRUE ? "Enabled" : "Disabled", \
        log_color->off, \

        "type=", log_color->blue, get_key_by_enum(ENUM_TYPE,res.type), log_color->off, \

        "tree=", log_color->magenta, res.sa.s + res.treename, log_color->off))
            goto freed ;

    err = 1 ;

    freed:
        resolve_free(wres) ;

    return err ;
}
