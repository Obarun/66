/*
 * graph.h
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

#ifndef SS_GRAPH_H
#define SS_GRAPH_H

#include <stdint.h>

#include <oblibs/graph.h>

#include <66/service.h>
#include <66/tree.h>
#include <66/ssexec.h>
#include <66/hash.h>

extern void graph_build_tree(graph_t *g, struct resolve_hash_tree_s **htres, char const *base, resolve_tree_master_enum_t field) ;
extern void graph_build_system(graph_t *g, struct resolve_hash_s **hres, ssexec_t *info, uint32_t flag) ;
extern void graph_build_arguments(graph_t *g, char const *const *argv, int argc, struct resolve_hash_s **hres, ssexec_t *info, uint32_t flag) ;
extern int graph_compute_dependencies(graph_t *g, char const *vertex, char const *edge, uint8_t requiredby) ;
extern void graph_compute_visit(struct resolve_hash_s hres, unsigned int *visit, unsigned int *list, graph_t *graph, unsigned int *nservice, uint8_t requiredby) ;
extern int graph_build_service_bytree(graph_t *g, char const *tree, uint8_t what,  uint8_t is_supervised) ;
extern int graph_build_service_bytree_from_src(graph_t *g, char const *src, uint8_t what) ;


/** possible remove of it */
extern int graph_build_service_from_sastr(graph_t *graph, stralloc *sa, char const *base) ;

#endif
