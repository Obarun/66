/* 
 * graph.c
 * 
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */
 
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
 
#include <oblibs/string.h>
#include <oblibs/error2.h>
#include <oblibs/stralist.h>
#include <oblibs/strakeyval.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/types.h>
#include <oblibs/strakeyval.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/direntry.h>
#include <skalibs/unix-transactional.h>//stat_at
#include <skalibs/lolstdio.h>

#include <66/graph.h>
#include <66/enum.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/resolve.h>
#include <66/ssexec.h>

#include <stdio.h>

graph_style graph_utf8 = {
	UTF_VR UTF_H,
	UTF_UR UTF_H,
	UTF_V " ",
	2
} ;

graph_style graph_default = {
	"|-",
	"`-",
	"|",
	2
} ;

static void print_text(char const *srctree,const char *sv, tdepth *depth, int last)
{
	ss_resolve_t res = RESOLVE_ZERO ;
		
	if (!ss_resolve_read(&res,srctree,sv)) return ;
	
	const char *tip = "" ;
	int level = 1 ;
	if(!sv)
		return ;
	
	if(depth->level > 0)
	{
		tip = last ? STYLE->last : STYLE->tip;
	
		while(depth->prev)
			depth = depth->prev;
		
	
		while(depth->next)
		{
			if (!bprintf(buffer_1,"%*s%-*s",STYLE->indent * (depth->level - level), "", STYLE->indent, STYLE->limb)) return ;
			level = depth->level + 1;
			depth = depth->next;
		} 
	}

	if(depth->level > 0)
	{
		if (!bprintf(buffer_1,"%*s%s(%i,%s) %s", STYLE->indent * (depth->level - level), "", tip, res.pid ? res.pid : 0,get_keybyid(res.type), sv)) return ;
		
	}
	else if (!bprintf(buffer_1,"%s(%i) %s: %s",tip,res.pid ? res.pid : 0, sv, get_keybyid(res.type))) return ;

	if (buffer_putsflush(buffer_1,"\n") < 0) return ; 

	ss_resolve_free(&res) ;
	
}

void graph_walk(char const *srctree, graph_t *g,genalloc *ga,unsigned int src,unsigned int nlen, tdepth *depth)
{
	
	unsigned int last, i, newdata ;

	unsigned int stacklen = genalloc_len(vertex_graph_t,ga) ;
	
	if(!stacklen || ((MAXDEPTH >= 0) && (depth->level > MAXDEPTH)))
		return ;

	char *string = g->string ;
	
	i = src ;
	last = newdata = 0 ;

	for(; i < stacklen && i < nlen ; i++ )
	{	
		for (unsigned int d = 0 ; d < genalloc_len(vertex_graph_t,&g->stack) ; d++)
		{
			if (genalloc_s(vertex_graph_t,ga)[i].idx == genalloc_s(vertex_graph_t,&g->stack)[d].idx)
			{
				newdata = genalloc_s(vertex_graph_t,&g->stack)[d].gaidx ;
				break ;
			}
			else 
			{
				newdata = genalloc_s(vertex_graph_t,ga)[i].gaidx ;
			}
		}
		
		int idx = genalloc_s(vertex_graph_t,&g->vertex)[newdata].name ;
		char *name = string + idx ;
		unsigned int dplen = genalloc_s(vertex_graph_t,&g->vertex)[newdata].ndeps ; 
	
		last =  i + 1 < depth->pndeps  ? 0 : 1 ;		
	
		print_text(srctree,name, depth, last) ;
	
		if (dplen)
		{
			genalloc data = genalloc_s(vertex_graph_t,&g->vertex)[newdata].dps ;
			
			tdepth d =
			{
				depth,
				NULL,
				depth->level + 1 ,
				depth->cndeps ,
				dplen 
				
			} ;
			depth->next = &d;
			
			if(last)
			{
				if(depth->prev)
				{
					depth->prev->next = &d;
					d.prev = depth->prev;
					depth = &d;
					depth->cndeps = 0 ;
					
				}
				else 
					d.prev = NULL;
			}
			graph_walk(srctree,g,&data,0,dplen,&d);
			depth->next = NULL;
			depth->cndeps = 0 ;
		}

	}
	
	
}

int graph_tree(char const *srctree,graph_t *g, char const *name, char const *tree,int reverse)
{
	unsigned int a = 0 ;
	unsigned int first = 0 ;
	unsigned int ndeps = g->nvertex ;
	if (!reverse) stack_reverse(&g->stack) ;
	if (*name) stack_reverse(&g->stack) ;
	if (*name)
	{
		char *string = g->string ;
		for (; a < g->nvertex ; a++)
		{
			char *gname = string + genalloc_s(vertex_graph_t,&g->stack)[a].name ;
			if (obstr_equal(name,gname))
			{
				first = a ;
				ndeps = first + 1 ;
				break ;
			}
		}
	}
	
	tdepth d = {
		NULL,
		NULL,
		1 ,
		0 ,
		ndeps
	} ;	
	graph_walk(srctree,g,&g->stack,first,ndeps,&d);
		
	return 1 ;
}

int stack_init(genalloc *st, unsigned int len)
{
	return genalloc_ready(vertex_graph_t,st,len) ;
}

int stack_push_at(genalloc *st,genalloc *data,unsigned int len, unsigned int idx)
{
	return genalloc_insertb(vertex_graph_t,st,idx,&genalloc_s(vertex_graph_t,data)[len],1) ;
}

int stack_push_start(genalloc *st, genalloc *data,unsigned int len)
{
	return stack_push_at(st,data,len,0) ;
}

void stack_reverse(genalloc *st)
{
	for (unsigned int i = 0 ; i < genalloc_len(vertex_graph_t,st) ; i++)
	{
		genalloc_reverse(vertex_graph_t,&genalloc_s(vertex_graph_t,st)[i].dps) ;
	} 
	genalloc_reverse(vertex_graph_t,st) ;
}

int stack_push(genalloc *st,genalloc *data,unsigned int len)
{
	return genalloc_append(vertex_graph_t,st,&genalloc_s(vertex_graph_t,data)[len]) ;
}

int dfs(graph_t *g, unsigned int idx, genalloc *stack, visit *c)
{
	int cycle = 0 ;
	unsigned int i, data ;
	unsigned int len = genalloc_s(vertex_graph_t,&g->vertex)[idx].ndeps ;
	
	if (c[idx] == GRAY) return 1 ;
	if (c[idx] == WHITE)
	{
		c[idx] = GRAY ;
		for (i = 0 ; i < len ; i++)
		{
			data = genalloc_s(vertex_graph_t,&genalloc_s(vertex_graph_t,&g->vertex)[idx].dps)[i].gaidx ;
			cycle = (cycle || dfs(g,data, stack, c)) ;
		}
		c[idx] = BLACK ;
		stack_push_start(stack,&g->vertex,idx) ;
	}
	return cycle ;
}

int graph_sort(graph_t *g)
{
	unsigned int len = g->nvertex ;
	unsigned int color = g->nedge ;
	visit c[color] ;
	for (unsigned int i = 0 ; i < color; i++) c[i] = WHITE ;
	if (!len) return 0 ;
/*	if (!stack_init(&g->stack,color))
	{
		VERBO3 strerr_warnwu1x("iniate stack") ;
		return 0;
	}*/ 
	for (unsigned int i = 0 ; i < len ; i++)
		if (c[i] == WHITE && dfs(g,i,&g->stack,c))
			return -1 ;
	
	return 1 ;
}

int graph_master(genalloc *ga, graph_t *g)
{
	unsigned int a ;
	char *string = g->string ;
	for (a = 0 ; a < genalloc_len(vertex_graph_t,&g->stack) ; a++)
		if (!stra_add(ga,string + genalloc_s(vertex_graph_t,&g->stack)[a].name)) return 0 ;
	 
	return 1 ;
}

int graph_rdepends(genalloc *ga,graph_t *g, char const *name, char const *src)
{
	unsigned int i,k ;
	int r ;

	char *string = g->string ;
	unsigned int type = graph_search(g,name) ;
	/** bundle , we need to check every service set onto it*/
	if (genalloc_s(vertex_graph_t,&g->vertex)[type].type == BUNDLE)
	{
		
		for (k = 0; k < genalloc_s(vertex_graph_t,&g->vertex)[type].ndeps; k++)
		{
			char *depname = g->string + genalloc_s(vertex_graph_t,&genalloc_s(vertex_graph_t,&g->vertex)[type].dps)[k].name ;
			
			if (!stra_cmp(ga,depname))
			{
				if (!stra_add(ga,depname)) 
				{	 
					VERBO3 strerr_warnwu3x("add: ",depname," as dependency to remove") ;
					return 0 ;
				}
			}
			r = graph_rdepends(ga,g,depname,src) ;
			if (!r) 
			{
				VERBO3 strerr_warnwu2x("find services depending on: ",name) ;
				return 0 ;
			}
			if(r == 2)
			{
				VERBO3 strerr_warnt2x("any services don't depends on: ",name) ;
				return 2 ;
			}
		}
	}
		
	for (i = 0 ; i < g->nvertex ; i++)
	{
		
		char *master = string + genalloc_s(vertex_graph_t,&g->vertex)[i].name ;
		if (obstr_equal(name,master)) continue ;
	
		if (genalloc_s(vertex_graph_t,&g->vertex)[i].ndeps)
		{
			for (k = 0; k < genalloc_s(vertex_graph_t,&g->vertex)[i].ndeps; k++)
			{
				char *depname = string + genalloc_s(vertex_graph_t,&genalloc_s(vertex_graph_t,&g->vertex)[i].dps)[k].name ;
			
				if (obstr_equal(name,depname))
				{
					if (!stra_cmp(ga,master))
					{
						if (!stra_add(ga,master)) 
						{	 
							VERBO3 strerr_warnwu3x("add: ",depname," as dependency to remove") ;
							return 0 ;
						}
						r = graph_rdepends(ga,g,master,src) ;
						if (!r)	return 0 ;		
					}
				}
			}
		}
	}
		
	if (!genalloc_len(stralist,ga)) return 2 ;
	
	//genalloc_reverse(stralist,ga) ;
	return 1 ;
}
/** what = 0 -> only classic
 * what = 1 -> only atomic
 * what = 2 -> both
 * @Return 0 on fail
 * @Return -1 for empty graph*/
int graph_type_src(genalloc *ga,char const *dir,unsigned int what)
{
	size_t dirlen = strlen(dir) ;
	char solve[dirlen + SS_DB_LEN + SS_SRC_LEN + 1] ;
	memcpy(solve,dir,dirlen) ;
	
	memcpy(solve + dirlen, SS_SVC, SS_SVC_LEN) ;
	solve[dirlen + SS_SVC_LEN] = 0 ;
	
	if (!what || what == 2)
	{
		if (!dir_get(ga,solve,"",S_IFDIR)) return 0 ;
		size_t dlen = strlen(solve) ;
		for(unsigned int i = 0 ; i < genalloc_len(stralist,ga) ; i++)
		{
			char *name = gaistr(ga,i) ;
			size_t namelen = gaistrlen(ga,i) ;
			char tmp[dlen + 1 + namelen + SS_LOG_SUFFIX_LEN +1] ;
			memcpy(tmp,solve,dlen) ;
			tmp[dlen] = '/' ;
			memcpy(tmp + dlen + 1,name,namelen) ;
			memcpy(tmp + dlen + 1 + namelen,"/log",SS_LOG_SUFFIX_LEN) ;
			tmp[dlen + 1 + namelen + SS_LOG_SUFFIX_LEN] = 0 ;
	
			if (scan_mode(tmp,S_IFDIR))
			{
				memcpy(tmp,gaistr(ga,i),namelen) ;
				memcpy(tmp + namelen,"-log",SS_LOG_SUFFIX_LEN) ;
				tmp[namelen + SS_LOG_SUFFIX_LEN] = 0 ;
				if (!stra_add(ga,tmp)) return 0 ;
			}
		}
	}
	
	memcpy(solve + dirlen, SS_DB, SS_DB_LEN) ;
	memcpy(solve + dirlen + SS_DB_LEN, SS_SRC, SS_SRC_LEN) ;
	solve[dirlen + SS_DB_LEN + SS_SRC_LEN] = 0 ;
	
	if (what)
		if (!dir_get(ga,solve,SS_MASTER + 1,S_IFDIR)) return 0 ;
	
	if (!genalloc_len(stralist,ga))
	{
		genalloc_deepfree(stralist,ga,stra_free) ;
		return -1 ;
	}
	return 1 ;
}
void vertex_graph_free(vertex_graph_t *vg)
{
	genalloc_free(vertex_graph_t,&vg->dps) ;	
}

void graph_free(graph_t *graph)
{
	genalloc_deepfree(vertex_graph_t,&graph->vertex,vertex_graph_free) ;
	genalloc_free(vertex_graph_t,&graph->stack) ;
}
int graph_build(graph_t *g, stralloc *sagraph, genalloc *tokeep,char const *dir)
{
	
	ss_resolve_t res = RESOLVE_ZERO ;
	genalloc gatmp = GENALLOC_ZERO ;//stralist
		
	graph_t gtmp = GRAPH_ZERO ;
		
	unsigned int nvertex = 0 ;
	unsigned int nedge = 0 ;
	
	
	for (unsigned int i = 0 ; i < genalloc_len(stralist,tokeep) ; i++)
	{
		vertex_graph_t gsv = VERTEX_GRAPH_ZERO ; 
		vertex_graph_t gdeps = VERTEX_GRAPH_ZERO ;
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		res.sa.len = 0 ;
		char const *name = gaistr(tokeep,i) ;
		size_t namelen = gaistrlen(tokeep,i) ;
			
		if (!ss_resolve_read(&res,dir,name))
		{
			VERBO3 strerr_warnwu2sys("read resolve file of: ",name) ;
			goto err ;
		}
			
		if (res.ndeps )
		{
			
			if (!clean_val(&gatmp,res.sa.s + res.deps))
			{
				VERBO3 strerr_warnwu2x("clean val: ",res.sa.s + res.deps) ;
				goto err ;
			}
			
		}
		
		gsv.type = res.type ;
		gsv.idx = nedge ;
		nedge++ ;
		gsv.gaidx = genalloc_len(vertex_graph_t, &gtmp.vertex) ;
		gsv.name = sagraph->len ;
		if (!stralloc_catb(sagraph,name,namelen + 1)) goto err ;
		
		if (genalloc_len(stralist,&gatmp))
		{	
			gsv.ndeps = genalloc_len(stralist,&gatmp) ;
			for (unsigned int i = 0 ; i < genalloc_len(stralist,&gatmp);i++)
			{ 
				gdeps.idx = nedge++ ;
				gdeps.name = sagraph->len ;
				if (!stralloc_catb(sagraph,gaistr(&gatmp,i),gaistrlen(&gatmp,i) + 1)) goto err ;
				gdeps.gaidx = genalloc_len(vertex_graph_t,&gsv.dps) ;
				gdeps.ndeps = 0 ;
				gdeps.dps.len = 0 ;
				if (!genalloc_append(vertex_graph_t,&gsv.dps,&gdeps)) goto err ;
			}
		}
		gtmp.nedge = nedge ;
		gtmp.nvertex = ++nvertex ;
		if (!genalloc_append(vertex_graph_t,&gtmp.vertex,&gsv)) goto err ;
	}
	
	/** second pass */
	
	char *string = sagraph->s ;
	for (unsigned int a = 0 ; a < gtmp.nvertex ; a++)
	{
		vertex_graph_t gsv = VERTEX_GRAPH_ZERO ; 
		vertex_graph_t gdeps = VERTEX_GRAPH_ZERO ;
				
		int idxa = genalloc_s(vertex_graph_t,&gtmp.vertex)[a].idx ;
		gsv.idx = idxa ;
		gsv.type = genalloc_s(vertex_graph_t,&gtmp.vertex)[a].type ;
		gsv.name = genalloc_s(vertex_graph_t,&gtmp.vertex)[a].name ;
		gsv.ndeps = genalloc_s(vertex_graph_t,&gtmp.vertex)[a].ndeps ;
		gsv.gaidx = genalloc_s(vertex_graph_t,&gtmp.vertex)[a].gaidx ;
		
		for (unsigned int b = 0 ; b < genalloc_s(vertex_graph_t,&gtmp.vertex)[a].ndeps ; b++)
		{
			int idxb = genalloc_s(vertex_graph_t,&genalloc_s(vertex_graph_t,&gtmp.vertex)[a].dps)[b].idx ;
			int nb = genalloc_s(vertex_graph_t,&genalloc_s(vertex_graph_t,&gtmp.vertex)[a].dps)[b].name ;
			char *nameb = string + nb ;
			gdeps.gaidx = genalloc_s(vertex_graph_t,&genalloc_s(vertex_graph_t,&gtmp.vertex)[a].dps)[b].gaidx ;
			gdeps.ndeps = 0 ;
			gdeps.dps.len = 0 ;
			gdeps.idx = idxb ;
			gdeps.name = nb ;
			for (unsigned int c = 0 ; c < gtmp.nvertex ; c++)
			{
				int idxc = genalloc_s(vertex_graph_t,&gtmp.vertex)[c].idx ;
				if (idxa == idxc) continue ;
				int nc = genalloc_s(vertex_graph_t,&gtmp.vertex)[c].name ;
				char *namec = string + nc ;
				
				if (obstr_equal(namec,nameb))
				{
					gdeps.idx = idxc ;
					gdeps.name = nc ;
					gdeps.gaidx = genalloc_s(vertex_graph_t,&gtmp.vertex)[c].gaidx ;
					gdeps.ndeps = genalloc_s(vertex_graph_t,&gtmp.vertex)[c].ndeps ;
					gdeps.type = genalloc_s(vertex_graph_t,&gtmp.vertex)[c].type ;
					break ;
				}
			}
			if (!genalloc_append(vertex_graph_t,&gsv.dps,&gdeps)) goto err ;
				
		}
		if (!genalloc_append(vertex_graph_t,&g->vertex,&gsv)) goto err ;
	}
	
	g->string = sagraph->s ;
	g->nvertex = gtmp.nvertex ;
	g->nedge = gtmp.nedge ;
	graph_free(&gtmp) ;
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	ss_resolve_free(&res) ;
	return 1 ;
	
	err:
		graph_free(&gtmp) ;
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		ss_resolve_free(&res) ;
		return 0 ;
}

/** return idx of @name in @g 
 * @Return -1 if not found*/
int graph_search(graph_t *g, char const *name)
{
	unsigned int i ;
	char *string = g->string ;
	for (i = 0 ; i < g->nvertex ; i++)
	{
		char *master = string + genalloc_s(vertex_graph_t,&g->vertex)[i].name ;
		if (obstr_equal(name,master)) return i ;// return genalloc_s(vertex_graph_t,&g->vertex)[i].gaidx ;
		
	}
	
	return -1 ;
}
