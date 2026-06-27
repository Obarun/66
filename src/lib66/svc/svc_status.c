/*
 * svc_status.c
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

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <66/svc.h>
#include <66/status.h>
#include <66/state.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/enum_parser.h>

int svc_status(resolve_service_t *res, service_status_t *st)
{
    log_flow() ;

    *st = service_status_zero ;

    if (res->type == E_PARSER_TYPE_CLASSIC) {

        char const *supervisedir = res->sa.s + res->live.supervisedir ;
        char file[strlen(supervisedir) + 1 + SS_STATUS_LEN + 1] ;
        auto_strings(file, supervisedir, "/", SS_STATUS) ;

        int r = status_read(st, file) ;
        return r < 0 ? -1 : 1 ;
    }

    ss_state_t sta = STATE_ZERO ;
    if (!state_read(&sta, res))
        return 0 ;

    st->state = sta.isup == STATE_FLAGS_TRUE ? STATUS_STATE_UP : STATUS_STATE_DOWN ;
    return 1 ;
}
