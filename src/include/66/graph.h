/* 
 * graph.h
 * 
 * Copyright (c) 2018-2019 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */
 
#ifndef GRAPH_H
#define GRAPH_H

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>


extern stralloc SAGRAPH ;

typedef enum visit_e visit ;
enum visit_e
{
	WHITE = 0, 
	GRAY, 
	BLACK
} ; 

typedef struct vertex_graph_s vertex_graph_t, *vertex_graph_t_ref ;
struct vertex_graph_s
{
	unsigned int type ; //type of the service
	unsigned int idx ; //idx of the service
	unsigned int name ; //pos in associated stralloc, name of the service
	unsigned int ndeps ; //number of dependencies
	unsigned int gaidx ; //pos of deps in genalloc dps
	genalloc dps ; // type vertex_graph_t
} ;

typedef struct graph_s graph_t, *graph_t_ref ;
struct graph_s
{
	unsigned int nvertex ; //number of vertex
	unsigned int nedge ; //number of edge
	char *string ; // associated stralloc
	genalloc vertex ; //type vertex_graph_t
	genalloc stack ; //vertex_graph_t, topological sorted
} ;

#define VERTEX_GRAPH_ZERO { 0, 0, 0, 0, 0, GENALLOC_ZERO }
#define GRAPH_ZERO { 0, 0, 0, GENALLOC_ZERO, GENALLOC_ZERO }

static vertex_graph_t const vertex_graph_zero = VERTEX_GRAPH_ZERO ;
static graph_t const graph_zero = GRAPH_ZERO ;


typedef struct tdepth_s tdepth ;
struct tdepth_s
{
	tdepth *prev;
	tdepth *next;
	int level;
	unsigned int cndeps ;
	unsigned int pndeps ;
} ;

typedef struct graph_style_s graph_style ;
struct graph_style_s
{
	const char *tip;
	const char *last;
	const char *limb;
	int indent;
} ;

#define UTF_V   "\342\224\202"  /* U+2502, Vertical line drawing char */
#define UTF_VR  "\342\224\234"  /* U+251C, Vertical and right */
#define UTF_H   "\342\224\200"  /* U+2500, Horizontal */
#define UTF_UR  "\342\224\224"  /* U+2514, Up and right */

extern unsigned int MAXDEPTH ;
extern graph_style *STYLE ; 
extern graph_style graph_utf8 ;
extern graph_style graph_default ;

/** what = 0 -> only classic
 * what = 1 -> only atomic
 * what = 2 -> both*/
extern int graph_type_src(genalloc *ga,char const *tree,unsigned int what) ;

extern int graph_build(graph_t *g, stralloc *sagraph, genalloc *tokeep,char const *tree) ;

extern int graph_master(genalloc *ga, graph_t *g) ;

extern int graph_sort(graph_t *g) ;

extern int graph_search(graph_t *g, char const *name) ;

extern int graph_tree(char const *srctree, graph_t *g, char const *name, char const *tree,int reverse) ;

extern void stack_reverse(genalloc *st) ;

extern void vertex_graph_free(vertex_graph_t *vgraph) ;

extern void graph_free(graph_t *graph) ;

#endif
