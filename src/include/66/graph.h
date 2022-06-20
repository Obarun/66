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

extern int graph_build_g(graph_t *g, char const *base, char const *treename, uint8_t data_type, uint8_t general) ;
extern int graph_build_tree(graph_t *g,char const *base) ;
extern int graph_build_service(graph_t *g, char const *base, char const *treename, uint8_t general) ;
extern int graph_build_service_bytree(graph_t *g, char const *tree, uint8_t what) ;
extern int graph_build_service_bytree_from_src(graph_t *g, char const *src, uint8_t what) ;
extern int graph_add_deps(graph_t *g, char const *vertex, char const *edge, uint8_t requiredby) ;

#endif
