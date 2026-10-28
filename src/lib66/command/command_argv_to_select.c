/*
 * command_argv_to_select.c
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

#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/bits.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/command.h>

int command_argv_to_select(selection_service_t *sel, command_graph_t *d, char const *const *argv, int argc, ssexec_t const *info, selection_service_policy_t const *policy, strbuf *frontends, uint32_t refusals)
{
    log_flow() ;

    int unknown = 0 ;
    bitset32_t named ;

    *sel = (selection_service_t)SELECTION_SERVICE_ZERO ;

    int r = selection_service_names(&named, &d->g, info->base.s, argv, argc, true, &unknown) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "find the services named") ;

    if (!r)
        command_refuse_unknown(argv[unknown], info->base.s, info->owner, false) ;

    selection_service_propagate(sel, &d->g, &named, policy) ;

    if (service_graph_resolve_init(&d->store, &d->g, info->base.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "initialize the store of services") ;

    if (!command_resolve_load(d, sel))
        return 0 ;

    if (refusals)
        command_refuse_selection(sel, frontends, refusals) ;

    return 1 ;
}
