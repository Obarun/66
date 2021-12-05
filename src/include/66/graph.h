/*
 * graph.h
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef SS_GRAPH_H
#define SS_GRAPH_H

#include <stdint.h>

#include <oblibs/graph.h>

extern int graph_service_add_deps(graph_t *g, char const *service, char const *deps) ;
extern int graph_service_build_bytree(graph_t *g, char const *tree, uint8_t what) ;


#endif
