/*
 * service_graph_g.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/stack.h>

#include <66/service.h>
#include <66/graph.h>
#include <66/ssexec.h>
#include <66/state.h>
#include <66/enum.h>

static void debug_flag(uint32_t flag)
{
    char req[8 + 10 + 11 + 15 + 9 + 10 + 14 + 6 + 13 + 8 + 10 + 11 + 1] ;

    memset(req, 0, sizeof(char) * 126);

    char *toinit = "toinit" ;
    char *toreload = "toreload" ;
    char *torestart = "torestart" ;
    char *tounsupervise = "tounsupervise" ;
    char *toparse = "toparse" ;
    char *isparsed = "isparsed" ;
    char *issupervised = "issupervised" ;
    char *isup = "isup" ;
    char *topropagate = "topropagate" ;
    char *wantup = "wantup" ;
    char *wantdown = "wantdown" ;
    char *isearlier = "isearlier" ;

    if (FLAGS_ISSET(flag, STATE_FLAGS_TOINIT))
        auto_strings(req, toinit) ;
    if (FLAGS_ISSET(flag, STATE_FLAGS_TORELOAD))
        auto_strings(req + strlen(req), " ", toreload) ;
    if (FLAGS_ISSET(flag, STATE_FLAGS_TORESTART))
        auto_strings(req + strlen(req), " ", torestart) ;
    if (FLAGS_ISSET(flag, STATE_FLAGS_TOUNSUPERVISE))
        auto_strings(req + strlen(req), " ", tounsupervise) ;
    if (FLAGS_ISSET(flag, STATE_FLAGS_TOPARSE))
        auto_strings(req + strlen(req), " ", toparse) ;
    if (FLAGS_ISSET(flag, STATE_FLAGS_ISPARSED))
        auto_strings(req + strlen(req), " ", isparsed) ;
    if (FLAGS_ISSET(flag, STATE_FLAGS_ISSUPERVISED))
        auto_strings(req + strlen(req), " ", issupervised) ;
    if (FLAGS_ISSET(flag, STATE_FLAGS_ISUP))
        auto_strings(req + strlen(req), " ", isup) ;
    if (FLAGS_ISSET(flag, STATE_FLAGS_TOPROPAGATE))
        auto_strings(req + strlen(req), " ", topropagate) ;
    if (FLAGS_ISSET(flag, STATE_FLAGS_WANTUP))
        auto_strings(req + strlen(req), " ", wantup) ;
    if (FLAGS_ISSET(flag, STATE_FLAGS_WANTDOWN))
        auto_strings(req + strlen(req), " ", wantdown) ;
    if (FLAGS_ISSET(flag, STATE_FLAGS_ISEARLIER))
        auto_strings(req + strlen(req), " ", isearlier) ;

    log_trace("requested flags to build the graph: ", req) ;
}

void service_graph_g(char const *slist, size_t slen, graph_t *graph, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info, uint32_t flag)
{
    log_flow() ;

    debug_flag(flag) ;

    service_graph_collect(graph, slist, slen, ares, areslen, info, flag) ;

    if (!*areslen) {
        /* avoid empty string */
        log_warn("no services matching the requirements at tree: ", info->treename.s) ;
        return ;
    }

    service_graph_build(graph, ares, (*areslen), flag) ;
}
