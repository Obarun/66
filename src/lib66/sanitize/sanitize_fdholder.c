/*
 * sanitize_fdholder.c
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
 */

#include <stdint.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/service.h>
#include <66/state.h>
#include <66/enum_parser.h>
#include <66/fdholder.h>

int sanitize_fdholder_start(fdholder_client_t *c, const char *socket)
{
    log_flow() ;

    _alloc_strbuf_(sock, strlen(socket) + 3) ;
    auto_strings(sock.s, socket, "/s") ;

    if (!fdholder_client_init(c, sock.s))
        log_warnusys_return(LOG_EXIT_ZERO, "connect to socket: ", sock.s) ;

    return 1 ;
}

/**
 * The log pipe of a service is now created on demand by 66-execute (the daemon's
 * get-or-create pipe op), and the daemon holds both ends so it survives a
 * restart of either side. There is therefore nothing to pre-seed: the only
 * lifecycle action left is to drop a service's pipe flow when it is removed.
 *
 * @flag STATE_FLAGS_FALSE -> delete the service's pipe flow ; other flags are no-ops.
 * @init kept for call-site compatibility (unused).
 */
int sanitize_fdholder(resolve_service_t *res, fdholder_client_t *c, ss_state_t *sta, uint32_t flag, uint8_t init)
{
    log_flow() ;

    (void)sta ;
    (void)init ;

    if (res->logger.want && res->type == E_PARSER_TYPE_CLASSIC) {

        if (FLAGS_ISSET(flag, STATE_FLAGS_FALSE)) {

            char *name = res->sa.s + res->logger.name ;

            log_trace("delete fdholder flow: ", name) ;
            if (!fdholder_pipe_delete(c, name, -1) && c->status != FDHOLDER_NOTFOUND)
                return 0 ;
        }
    }

    return 1 ;
}
