/*
 * info_graph_display_service.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <oblibs/types.h>
#include <oblibs/stream.h>

#include <66/svc.h>
#include <66/service.h>
#include <66/utils.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/state.h>
#include <66/status.h>
#include <66/enum_parser.h>

int info_graph_display_service(char const *name)
{
    log_flow() ;

    int err = 0 ;
    uint8_t pid_color = 0 ;

    char str_pid[PID_FMT] ;
    char const *ppid ;

    ss_state_t sta = STATE_ZERO ;
    service_status_t st = STATUS_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    char base[SS_MAX_PATH_LEN + SS_SYSTEM_LEN + 1] ;

    if (!set_ownersysdir_stack(base, getuid()))
        log_warn_return(LOG_EXIT_ZERO, "set owner directory") ;

    if (resolve_read(wres, base, name) <= 0) {
        log_warnu("read resolve file of: ",name) ;
        goto freed ;
    }

    if (!state_read(&sta, &res)) {
        log_warnusys("read state of: ", name) ;
        goto freed ;
    }

    svc_status(&res, &st) ;

    if (st.pid > 0) {

        pid_color = 2 ;
        str_pid[pid_format(str_pid, st.pid)] = 0 ;
        ppid = &str_pid[0] ;

    } else switch (st.state) {

        /* the graph shows a compact state: up/done, failed, everything else as down */
        case STATUS_STATE_UP :
        case STATUS_STATE_DONE :   ppid = status_state_to_string(st.state) ; pid_color = 2 ; break ;
        case STATUS_STATE_FAILED : ppid = status_state_to_string(st.state) ; pid_color = 1 ; break ;
        default :                  ppid = status_state_to_string(STATUS_STATE_DOWN) ; pid_color = 1 ; break ;
    }

    if (!ostream_fmt(ostream_1,"%s (%s%s%s%s%s, %s%s%s%s%s, %s%s%s%s%s, %s%s%s%s%s)", \

        name, \

        pid_color > 1 ? log_color->valid : pid_color ? log_color->error : log_color->warning, \
        "pid", \
        log_color->off, \
        "=", \
        ppid, \

        res.enabled ? log_color->valid : log_color->warning, \
        "state", \
        log_color->off, \
        "=", \
        res.enabled ? "Enabled" : "Disabled", \

        log_color->blue, \
        "type",
        log_color->off, \
        "=", \
        enum_to_key(enum_list_parser_type,res.type), \

        log_color->magenta, \
        "tree", \
        log_color->off, \
        "=", \
        res.sa.s + res.treename))
            goto freed ;

    err = 1 ;

    freed:
        resolve_free(wres) ;

    return err ;
}
