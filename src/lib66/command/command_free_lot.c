/*
 * command_free_lot.c
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

#include <oblibs/log.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/command.h>

int command_free_lot(selection_service_t const *sel, service_graph_resolve_t *store, ssexec_t const *info, strbuf *up, strbuf *down)
{
    log_flow() ;

    selection_service_t sup, lot ;

    if (selection_svc_supervised(&sup, sel, store) < 0 ||
        selection_svc_filter(&sup, &sup, store, 0, command_outside_boot) < 0 ||
        svc_lot(&lot, &sup, store, SVC_LOT_COMMAND) < 0)
            log_dieusys(LOG_EXIT_SYS, "select the supervised services of the selection") ;

    return command_lot_stop(&lot, store, info, SVC_TARGET_FREE, "-d", up, down) ;
}
