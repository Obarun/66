/*
 * command_resolve_load.c
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

#include <66/service.h>
#include <66/selection.h>
#include <66/command.h>

int command_resolve_load(command_graph_t *d, selection_service_t const *sel)
{
    log_flow() ;

    int r = service_graph_resolve_load(&d->store, sel, COMMAND_READ) ;

    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "read the services of the selection") ;

    if (!r && d->reloaded)
        log_dieu(LOG_EXIT_SYS, "read the services of the selection -- the system changed under the command") ;

    return r ;
}
