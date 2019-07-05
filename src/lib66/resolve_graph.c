/* 
 * resolve_graph.c
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
 
#include <66/resolve.h>

#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/error2.h>

#include <skalibs/genalloc.h>

#include <66/constants.h>
#include <66/utils.h>

ss_resolve_graph_style graph_utf8 = {
	UTF_VR UTF_H,
	UTF_UR UTF_H,
	UTF_V " ",
	2
} ;

ss_resolve_graph_style graph_default = {
	"|-",
	"`-",
	"|",
	2
} ;

void ss_resolve_graph_ndeps_free(ss_resolve_graph_ndeps_t *graph)
{
	genalloc_free(uint32_t,&graph->ndeps) ;
}

void ss_resolve_graph_free(ss_resolve_graph_t *graph)
{
	genalloc_deepfree(ss_resolve_t,&graph->name,ss_resolve_free) ;
	genalloc_deepfree(ss_resolve_graph_ndeps_t,&graph->cp,ss_resolve_graph_ndeps_free) ;
	genalloc_free(ss_resolve_t,&graph->sorted) ;
}

int ss_resolve_dfs(ss_resolve_graph_t *graph, unsigned int idx, visit *c,unsigned int *ename,unsigned int *edeps)
{
	int cycle = 0 ;
	unsigned int i, data ;
	unsigned int len = genalloc_len(uint32_t,&genalloc_s(ss_resolve_graph_ndeps_t,&graph->cp)[idx].ndeps) ;
	if (c[idx] == GRAY) return 1 ;
	if (c[idx] == WHITE)
	{
		c[idx] = GRAY ;
		for (i = 0 ; i < len ; i++)
		{
			data = genalloc_s(uint32_t,&genalloc_s(ss_resolve_graph_ndeps_t,&graph->cp)[idx].ndeps)[i] ;
			cycle = (cycle || ss_resolve_dfs(graph,data,c,ename,edeps)) ;
			if (cycle)
			{ 
				if (!*ename)
				{
					(*ename) = idx ; 
					(*edeps) =  i ; 
				}
				goto end ; 
			} 
		}
		c[idx] = BLACK ;
		genalloc_insertb(ss_resolve_t, &graph->sorted, 0, &genalloc_s(ss_resolve_t,&graph->name)[idx],1) ; 
	}
	end:
	return cycle ;
}

int ss_resolve_graph_sort(ss_resolve_graph_t *graph)
{
	unsigned int len = genalloc_len(ss_resolve_graph_ndeps_t,&graph->cp) ;
	visit c[len] ;
	unsigned int i, ename = 0, edeps = 0 ;
	for (i = 0 ; i < len; i++) c[i] = WHITE ;
	if (!len) return 0 ;

	for (i = 0 ; i < len ; i++)
	{
		if (c[i] == WHITE && ss_resolve_dfs(graph,genalloc_s(ss_resolve_graph_ndeps_t,&graph->cp)[i].idx,c,&ename,&edeps))
		{
			int data = genalloc_s(uint32_t,&genalloc_s(ss_resolve_graph_ndeps_t,&graph->cp)[ename].ndeps)[edeps] ;
			char *name = genalloc_s(ss_resolve_t,&graph->name)[ename].sa.s + genalloc_s(ss_resolve_t,&graph->name)[ename].name ;
			char *deps = genalloc_s(ss_resolve_t,&graph->name)[data].sa.s + genalloc_s(ss_resolve_t,&graph->name)[data].name ;
			VERBO3 strerr_warnw4x("resolution of : ",name,": encountered a cycle involving service: ",deps) ;
			return -1 ;
		}
	}
	
	return 1 ;
}

int ss_resolve_graph_publish(ss_resolve_graph_t *graph,unsigned int reverse)
{
	int ret = 0 ;
	genalloc gatmp = GENALLOC_ZERO ;

	for (unsigned int a = 0 ; a < genalloc_len(ss_resolve_t,&graph->name) ; a++)
	{
		ss_resolve_graph_ndeps_t rescp = RESOLVE_GRAPH_NDEPS_ZERO ;
		rescp.idx = a ;
		
		if (genalloc_s(ss_resolve_t,&graph->name)[a].ndeps)
		{
			genalloc_deepfree(stralist,&gatmp,stra_free) ;
			if (!clean_val(&gatmp, genalloc_s(ss_resolve_t,&graph->name)[a].sa.s +  genalloc_s(ss_resolve_t,&graph->name)[a].deps)) goto err ;
			for (unsigned int b = 0 ; b < genalloc_len(stralist,&gatmp) ; b++)
			{
				char *deps = gaistr(&gatmp,b) ;
				int r = ss_resolve_search(&graph->name,deps) ;
				if (r >= 0)
				{
					if (!genalloc_append(uint32_t,&rescp.ndeps,&r)) goto err ;
				}else continue ;
			}
		}
		if (!genalloc_append(ss_resolve_graph_ndeps_t,&graph->cp,&rescp)) goto err ;
	}
		
	if (ss_resolve_graph_sort(graph) < 0) { ret = -1 ; goto err ; }
	if (!reverse) genalloc_reverse(ss_resolve_t,&graph->sorted) ;
	
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	return 1 ;
	err:
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		return ret ;
}

int ss_resolve_graph_build(ss_resolve_graph_t *graph,ss_resolve_t *res,char const *src,unsigned int reverse)
{
	char *string = res->sa.s ;
	char *name = string + res->name ;
		
	int r = ss_resolve_search(&graph->name,name) ;
	if (r < 0)
	{
		if (!obstr_equal(name,SS_MASTER+1))
		{
			if(!ss_resolve_append(&graph->name,res)) goto err ;
		}		
		if (!reverse)
		{
			if (!ss_resolve_add_deps(&graph->name,res,src)) goto err ;
		}
		else
		{ 
			if (!ss_resolve_add_rdeps(&graph->name,res,src)) goto err ;
		}
	}
		
	return 1 ;
	err:
		return 0 ;
}
/** what = 0 -> only classic
 * what = 1 -> only atomic
 * what = 2 -> both
 * @Return 0 on fail*/
int ss_resolve_graph_src(ss_resolve_graph_t *graph, char const *dir, unsigned int reverse, unsigned int what)
{
	genalloc gatmp = GENALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	size_t dirlen = strlen(dir) ;
	
	char solve[dirlen + SS_DB_LEN + SS_SRC_LEN + 1] ;
	memcpy(solve,dir,dirlen) ;
	
	if (!what || what == 2)
	{
		memcpy(solve + dirlen, SS_SVC, SS_SVC_LEN) ;
		solve[dirlen + SS_SVC_LEN] = 0 ;
		if (!dir_get(&gatmp,solve,"",S_IFDIR)) goto err ;
	}
	if (what)
	{
		memcpy(solve + dirlen, SS_DB, SS_DB_LEN) ;
		memcpy(solve + dirlen + SS_DB_LEN, SS_SRC, SS_SRC_LEN) ;
		solve[dirlen + SS_DB_LEN + SS_SRC_LEN] = 0 ;
		if (!dir_get(&gatmp,solve,SS_MASTER + 1,S_IFDIR)) goto err ;
	}
	
	for(unsigned int i = 0 ; i < genalloc_len(stralist,&gatmp) ; i++)
	{
		char *name = gaistr(&gatmp,i) ;
		if (!ss_resolve_check(dir,name)) goto err ;
		if (!ss_resolve_read(&res,dir,name)) goto err ;
		if (!ss_resolve_graph_build(graph,&res,dir,reverse)) goto err ;
	}
	
	genalloc_deepfree(stralist,&gatmp,stra_free) ;
	ss_resolve_free(&res) ;
	return 1 ;
	err:
		genalloc_deepfree(stralist,&gatmp,stra_free) ;
		ss_resolve_free(&res) ;
		return 0 ;
}
