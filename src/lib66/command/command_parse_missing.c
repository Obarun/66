/*
 * command_parse_missing.c
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
#include <stddef.h>

#include <oblibs/log.h>
#include <oblibs/hash.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/parse.h>
#include <66/graph.h>
#include <66/service.h>
#include <66/command.h>

int command_parse_missing(service_graph_t *g, char const *const *argv, int argc, ssexec_t *info, uint32_t flags)
{
    log_flow() ;

    int r ;
    hash_t hres = HASH_ZERO ;
    _cleanup_strbuf_ strbuf msg = STRBUF_ZERO ;

    if (!hash_init(&hres, 0, offsetof(struct resolve_hash_s, node)))
        log_dieusys(LOG_EXIT_SYS, "initialize hash table") ;

    // in memory, then checked under the lock: a refusal writes nothing
    r = parse_names(&hres, g, argv, argc, info, PARSE_MISSING | flags, &msg) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "parse the services") ;

    if (!r)
        log_die(LOG_EXIT_USER, msg.s) ;

    if (!hash_count(&hres)) {
        resolve_hash_free(&hres) ;
        return 0 ;
    }

    if (!command_write_begin_checked(&hres, info, &msg))
        log_die(LOG_EXIT_USER, msg.s) ;

    if (parse_write(&hres, info) <= 0)
        log_dieusys(LOG_EXIT_SYS, "write the services") ;

    command_write_end(g) ;

    resolve_hash_free(&hres) ;

    return 1 ;
}
