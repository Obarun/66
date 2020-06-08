/* 
 * resolve.h
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
 
#ifndef SS_RESOLVE_H
#define SS_RESOLVE_H

#include <stddef.h>
#include <stdint.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/types.h>
#include <66/ssexec.h>
#include <66/parser.h>

#define SS_RESOLVE "/.resolve"
#define SS_RESOLVE_LEN (sizeof SS_RESOLVE - 1)
#define SS_RESOLVE_LIVE 0
#define SS_RESOLVE_SRC 1
#define SS_RESOLVE_BACK 2
#define SS_RESOLVE_STATE 3
#define SS_NOTYPE -1
#define SS_SIMPLE 0
#define SS_DOUBLE 1

typedef struct ss_resolve_s ss_resolve_t, *ss_resolve_t_ref ;
struct ss_resolve_s
{
	uint32_t salen ;
	stralloc sa ;
	
	uint32_t name ;
	uint32_t description ;
	uint32_t version ;
	uint32_t logger ;
	uint32_t logreal ;
	uint32_t logassoc ;
	uint32_t dstlog ;
	uint32_t deps ; // for module -> list of s6-rc service
	uint32_t optsdeps ; //optional dependencies
	uint32_t extdeps ; //external dependencies
	uint32_t contents ; // module -> list of s6-rc and s6 service 
	uint32_t src ;	//frontend source
	uint32_t srconf ; //configuration file source
	uint32_t live ; //run/66
	uint32_t runat ; //livetree->longrun,scandir->svc
	uint32_t tree ;	//var/lib/66/system/tree
	uint32_t treename ;
	uint32_t state ; //run/66/state/uid/treename/
	uint32_t exec_run ;
	uint32_t exec_log_run ;
	uint32_t real_exec_run ;
	uint32_t real_exec_log_run ;
	uint32_t exec_finish ;
	uint32_t real_exec_finish ;
	
	uint32_t type ;
	uint32_t ndeps ;
	uint32_t noptsdeps ;
	uint32_t nextdeps ;
	uint32_t ncontents ;
	uint32_t down ;
	uint32_t disen ;//disable->0,enable->1
} ;
#define RESOLVE_ZERO { 0,STRALLOC_ZERO,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }

/** Graph struct */
typedef struct ss_resolve_graph_ndeps_s ss_resolve_graph_ndeps_t ;
struct ss_resolve_graph_ndeps_s
{ 
	uint32_t idx ;//uint32_t
	genalloc ndeps ;//uint32_t
} ;
#define RESOLVE_GRAPH_NDEPS_ZERO { 0 , GENALLOC_ZERO }

typedef struct ss_resolve_graph_s ss_resolve_graph_t, *ss_resolve_graph_t_ref ;
struct ss_resolve_graph_s
{
	genalloc name ;//ss_resolve_t
	genalloc cp ; //ss_resolve_graph_ndeps_t
	genalloc sorted ;//ss_resolve_t
} ;
#define RESOLVE_GRAPH_ZERO { GENALLOC_ZERO , GENALLOC_ZERO , GENALLOC_ZERO } 

typedef enum visit_e visit ;
enum visit_e
{
	SS_WHITE = 0, 
	SS_GRAY, 
	SS_BLACK
} ; 

extern ss_resolve_t const ss_resolve_zero ;
extern void ss_resolve_init(ss_resolve_t *res) ;
extern void ss_resolve_free(ss_resolve_t *res) ;

extern int ss_resolve_pointo(stralloc *sa,ssexec_t *info,int type, unsigned int where) ;
extern int ss_resolve_src_path(stralloc *sasrc,char const *sv, uid_t owner,char const *directory_forced) ;
extern int ss_resolve_module_path(stralloc *sdir, stralloc *mdir, char const *sv, uid_t owner) ;
extern int ss_resolve_src(stralloc *sasrc, char const *name, char const *src,int *found) ;
extern int ss_resolve_service_isdir(char const *dir, char const *name) ;
extern int ss_resolve_add_uint32(stralloc *sa, uint32_t data) ;
extern uint32_t ss_resolve_add_string(ss_resolve_t *res,char const *data) ;
extern int ss_resolve_pack(stralloc *sa,ss_resolve_t *res) ;
extern int ss_resolve_write(ss_resolve_t *res,char const *dst,char const *name) ;
extern int ss_resolve_read(ss_resolve_t *res,char const *src,char const *name) ;
extern int ss_resolve_check(char const *src, char const *name) ;
extern int ss_resolve_setnwrite(sv_alltype *services,ssexec_t *info,char const *dst) ;
extern int ss_resolve_setlognwrite(ss_resolve_t *sv, char const *dst,ssexec_t *info) ;
extern void ss_resolve_rmfile(char const *src,char const *name) ;
extern int ss_resolve_cmp(genalloc *ga,char const *name) ;
extern int ss_resolve_add_deps(genalloc *tokeep,ss_resolve_t *res, char const *src) ;
extern int ss_resolve_add_rdeps(genalloc *tokeep, ss_resolve_t *res, char const *src) ;
extern int ss_resolve_add_logger(genalloc *ga,char const *src) ;
extern int ss_resolve_copy(ss_resolve_t *dst,ss_resolve_t *res) ;
extern int ss_resolve_append(genalloc *ga,ss_resolve_t *res) ;
extern int ss_resolve_create_live(ssexec_t *info) ;
extern int ss_resolve_search(genalloc *ga,char const *name) ;
extern int ss_resolve_check_insrc(ssexec_t *info, char const *name) ;
extern int ss_resolve_write_master(ssexec_t *info,ss_resolve_graph_t *graph,char const *dir, unsigned int reverse) ;
extern int ss_resolve_sort_bytype(genalloc *gares,stralloc *list,char const *src) ;
extern int ss_resolve_cmp_service_basedir(char const *dir) ;
extern int ss_resolve_sort_byfile_first(stralloc *sort, char const *src) ;
extern int ss_resolve_svtree(stralloc *svtree,char const *svname,char const *tree) ;
/** Graph function */
extern void ss_resolve_graph_ndeps_free(ss_resolve_graph_ndeps_t *graph) ;
extern void ss_resolve_graph_free(ss_resolve_graph_t *graph) ;

extern int ss_resolve_graph_src(ss_resolve_graph_t *graph, char const *dir, unsigned int reverse, unsigned int what) ;
extern int ss_resolve_graph_build(ss_resolve_graph_t *graph,ss_resolve_t *res,char const *src,unsigned int reverse) ;
extern int ss_resolve_graph_sort(ss_resolve_graph_t *graph) ;
extern int ss_resolve_dfs(ss_resolve_graph_t *graph, unsigned int idx, visit *c,unsigned int *ename,unsigned int *edeps) ;
extern int ss_resolve_graph_publish(ss_resolve_graph_t *graph,unsigned int reverse) ;

#endif
