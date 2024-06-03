/*
 * service_graph_collect_list.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/stack.h>
#include <oblibs/lexer.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/sanitize.h>
#include <66/state.h>
#include <66/graph.h>
#include <66/enum.h>
#include <66/hash.h>

/** keep services from slist dependending of the flag passed.
 * STATE_FLAGS_TOPARSE -> call sanitize_source
 * STATE_FLAGS_TOPROPAGATE -> it build with the dependencies/requiredby services.
 * STATE_FLAGS_ISSUPERVISED -> only keep already supervised service*/
void service_graph_collect_list(graph_t *g, char const *slist, size_t slen, struct resolve_hash_s **hres, ssexec_t *info, uint32_t flag)
{
    log_flow () ;

    size_t pos = 0 ;

    for (; pos < slen ; pos += strlen(slist + pos) + 1)
        service_graph_collect(g, slist + pos, hres, info, flag) ;

}
