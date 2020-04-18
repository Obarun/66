/* 
 * ss_resolve_graph.c
 * 
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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

#include <string.h>
#include <stdint.h>

#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/log.h>
#include <oblibs/sastr.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/utils.h>

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
	if (c[idx] == SS_GRAY) return 1 ;
	if (c[idx] == SS_WHITE)
	{
		c[idx] = SS_GRAY ;
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
		c[idx] = SS_BLACK ;
		if (!genalloc_insertb(ss_resolve_t, &graph->sorted, 0, &genalloc_s(ss_resolve_t,&graph->name)[idx],1))
			log_warnusys_return(LOG_EXIT_SYS,"genalloc") ;
	}
	end:
	return cycle ;
}

int ss_resolve_graph_sort(ss_resolve_graph_t *graph)
{
	unsigned int len = genalloc_len(ss_resolve_graph_ndeps_t,&graph->cp) ;
	visit c[len] ;
	unsigned int i, ename = 0, edeps = 0 ;
	for (i = 0 ; i < len; i++) c[i] = SS_WHITE ;
	if (!len) return 0 ;
	
	for (i = 0 ; i < len ; i++)
	{
		if ((c[i] == SS_WHITE) && ss_resolve_dfs(graph,genalloc_s(ss_resolve_graph_ndeps_t,&graph->cp)[i].idx,c,&ename,&edeps))
		{
			int data = genalloc_s(uint32_t,&genalloc_s(ss_resolve_graph_ndeps_t,&graph->cp)[ename].ndeps)[edeps] ;
			char *name = genalloc_s(ss_resolve_t,&graph->name)[ename].sa.s + genalloc_s(ss_resolve_t,&graph->name)[ename].name ;
			char *deps = genalloc_s(ss_resolve_t,&graph->name)[data].sa.s + genalloc_s(ss_resolve_t,&graph->name)[data].name ;
			log_warn_return(LOG_EXIT_LESSONE,"resolution of : ",name,": encountered a cycle involving service: ",deps) ;
		}
	}

	return 1 ;
}

int ss_resolve_graph_publish(ss_resolve_graph_t *graph,unsigned int reverse)
{
	int r, ret = 0 ;
	size_t a = 0 , b = 0 ;
	stralloc sa = STRALLOC_ZERO ;

	for (; a < genalloc_len(ss_resolve_t,&graph->name) ; a++)
	{
		ss_resolve_graph_ndeps_t rescp = RESOLVE_GRAPH_NDEPS_ZERO ;
		rescp.idx = a ;
		
		if (genalloc_s(ss_resolve_t,&graph->name)[a].ndeps)
		{
			sa.len = 0 ; 
			if (!sastr_clean_string(&sa, genalloc_s(ss_resolve_t,&graph->name)[a].sa.s +  genalloc_s(ss_resolve_t,&graph->name)[a].deps)) goto err ;
			for (b = 0 ; b < sa.len ; b += strlen(sa.s + b) + 1)
			{
				char *deps = sa.s + b ;
				r = ss_resolve_search(&graph->name,deps) ;
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
	
	stralloc_free(&sa) ;
	return 1 ;
	err:
		stralloc_free(&sa) ;
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
	stralloc sa = STRALLOC_ZERO ;
	ss_resolve_t res = RESOLVE_ZERO ;
	size_t dirlen = strlen(dir), pos = 0 ;
	
	char solve[dirlen + SS_DB_LEN + SS_SRC_LEN + 1] ;
	memcpy(solve,dir,dirlen) ;
	
	if (!what || what == 2)
	{
		memcpy(solve + dirlen, SS_SVC, SS_SVC_LEN) ;
		solve[dirlen + SS_SVC_LEN] = 0 ;
		if (!sastr_dir_get(&sa,solve,"",S_IFDIR)) goto err ;
	}
	if (what)
	{
		memcpy(solve + dirlen, SS_DB, SS_DB_LEN) ;
		memcpy(solve + dirlen + SS_DB_LEN, SS_SRC, SS_SRC_LEN) ;
		solve[dirlen + SS_DB_LEN + SS_SRC_LEN] = 0 ;
		if (!sastr_dir_get(&sa,solve,SS_MASTER + 1,S_IFDIR)) goto err ;
	}
	for (;pos < sa.len; pos += strlen(sa.s + pos) + 1)
	{
		char *name = sa.s + pos ;
		if (!ss_resolve_check(dir,name)) goto err ;
		if (!ss_resolve_read(&res,dir,name)) goto err ;
		if (!ss_resolve_graph_build(graph,&res,dir,reverse))
		{
			log_warnu("resolve dependencies of service: ",name) ;
			goto err ;
		 }
	}
	
	stralloc_free(&sa) ;
	ss_resolve_free(&res) ;
	return 1 ;
	err:
		stralloc_free(&sa) ;
		ss_resolve_free(&res) ;
		return 0 ;
}
