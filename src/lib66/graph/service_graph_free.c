/*
 * service_graph_free.c
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

#include <oblibs/strbuf.h>

#include <66/graph.h>

void service_graph_free(service_graph_t *g)
{
    strbuf_free(&g->vertex) ;
    strbuf_free(&g->sort) ;
    strbuf_free(&g->depends) ;
    strbuf_free(&g->requiredby) ;
    strbuf_free(&g->dorigin) ;
    strbuf_free(&g->rorigin) ;
    strbuf_free(&g->strings) ;
}
