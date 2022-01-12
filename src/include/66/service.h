/*
 * service.h
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

#ifndef SS_SERVICE_H
#define SS_SERVICE_H

#include <stdint.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/cdb.h>
#include <skalibs/cdbmake.h>

#include <66/parser.h>
#include <66/ssexec.h>


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
    genalloc name ;//resolve_service_t
    genalloc cp ; //ss_resolve_graph_ndeps_t
    genalloc sorted ;//resolve_service_t
} ;
#define RESOLVE_GRAPH_ZERO { GENALLOC_ZERO , GENALLOC_ZERO , GENALLOC_ZERO }

typedef enum visit_e visit ;
enum visit_e
{
    SS_WHITE = 0,
    SS_GRAY,
    SS_BLACK
} ;


#define DATA_SERVICE 0

typedef struct resolve_service_s resolve_service_t, *resolve_service_t_ref ;
struct resolve_service_s
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
    uint32_t depends ; // for module -> list of s6-rc service
    uint32_t requiredby ;
    uint32_t optsdeps ; //optional dependencies
    uint32_t extdeps ; //external dependencies
    uint32_t contents ; // module -> list of s6-rc and s6 service
    uint32_t src ;  //frontend source
    uint32_t srconf ; //configuration file source
    uint32_t live ; //run/66
    uint32_t runat ; //livetree->longrun,scandir->svc
    uint32_t tree ; //var/lib/66/system/tree
    uint32_t treename ;
    uint32_t state ; //run/66/state/uid/treename/
    uint32_t exec_run ;
    uint32_t exec_log_run ;
    uint32_t real_exec_run ;
    uint32_t real_exec_log_run ;
    uint32_t exec_finish ;
    uint32_t real_exec_finish ;

    uint32_t type ;
    uint32_t ndepends ;
    uint32_t nrequiredby ;
    uint32_t noptsdeps ;
    uint32_t nextdeps ;
    uint32_t ncontents ;
    uint32_t down ;
    uint32_t disen ;//disable->0,enable->1
} ;
#define RESOLVE_SERVICE_ZERO { 0,STRALLOC_ZERO,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }

typedef enum resolve_service_enum_e resolve_service_enum_t, *resolve_service_enum_t_ref;
enum resolve_service_enum_e
{
    SERVICE_ENUM_NAME = 0,
    SERVICE_ENUM_DESCRIPTION,
    SERVICE_ENUM_VERSION,
    SERVICE_ENUM_LOGGER,
    SERVICE_ENUM_LOGREAL,
    SERVICE_ENUM_LOGASSOC,
    SERVICE_ENUM_DSTLOG,
    SERVICE_ENUM_DEPENDS,
    SERVICE_ENUM_REQUIREDBY,
    SERVICE_ENUM_OPTSDEPS,
    SERVICE_ENUM_EXTDEPS,
    SERVICE_ENUM_CONTENTS,
    SERVICE_ENUM_SRC,
    SERVICE_ENUM_SRCONF,
    SERVICE_ENUM_LIVE,
    SERVICE_ENUM_RUNAT,
    SERVICE_ENUM_TREE,
    SERVICE_ENUM_TREENAME,
    SERVICE_ENUM_STATE,
    SERVICE_ENUM_EXEC_RUN,
    SERVICE_ENUM_EXEC_LOG_RUN,
    SERVICE_ENUM_REAL_EXEC_RUN,
    SERVICE_ENUM_REAL_EXEC_LOG_RUN,
    SERVICE_ENUM_EXEC_FINISH,
    SERVICE_ENUM_REAL_EXEC_FINISH,
    SERVICE_ENUM_TYPE,
    SERVICE_ENUM_NDEPENDS,
    SERVICE_ENUM_NREQUIREDBY,
    SERVICE_ENUM_NOPTSDEPS,
    SERVICE_ENUM_NEXTDEPS,
    SERVICE_ENUM_NCONTENTS,
    SERVICE_ENUM_DOWN,
    SERVICE_ENUM_DISEN,
    SERVICE_ENUM_ENDOFKEY
} ;

typedef struct resolve_service_field_table_s resolve_service_field_table_t, *resolve_service_field_table_t_ref ;
struct resolve_service_field_table_s
{
    char *field ;
} ;

extern resolve_service_field_table_t resolve_service_field_table[] ;

extern int service_isenabled(char const *sv) ;
extern int service_isenabledat(stralloc *tree, char const *sv) ;
extern int service_frontend_src(stralloc *sasrc, char const *name, char const *src) ;
extern int service_frontend_path(stralloc *sasrc,char const *sv, uid_t owner,char const *directory_forced) ;
extern int service_endof_dir(char const *dir, char const *name) ;
extern int service_cmp_basedir(char const *dir) ;
extern int service_intree(stralloc *svtree, char const *svname, char const *tree) ;

/**
 *
 * Resolve API
 *
 * */

extern int service_read_cdb(cdb *c, resolve_service_t *res) ;
extern int service_write_cdb(cdbmaker *c, resolve_service_t *sres) ;
extern int service_resolve_copy(resolve_service_t *dst, resolve_service_t *res) ;
extern int service_resolve_sort_bytype(stralloc *list, char const *src) ;
extern int service_resolve_setnwrite(sv_alltype *services, ssexec_t *info, char const *dst) ;
extern int service_resolve_setlognwrite(resolve_service_t *sv, char const *dst) ;
extern int service_resolve_master_create(char const *base, char const *treename) ;
extern int service_resolve_master_write(ssexec_t *info, ss_resolve_graph_t *graph, char const *dir, unsigned int reverse) ;
extern int service_resolve_modify_field(resolve_service_t *res, resolve_service_enum_t field, char const *data) ;
extern int service_resolve_field_tosa(stralloc *sa, resolve_service_t *res, resolve_service_enum_t field) ;
/**
 *
 * obsolete function
 *
 *
 * */
extern int service_resolve_add_deps(genalloc *tokeep, resolve_service_t *res, char const *src) ;
extern int service_resolve_add_rdeps(genalloc *tokeep, resolve_service_t *res, char const *src) ;
extern int service_resolve_add_logger(genalloc *ga,char const *src) ;



/** obsolete Graph function */
extern void ss_resolve_graph_ndeps_free(ss_resolve_graph_ndeps_t *graph) ;
extern void ss_resolve_graph_free(ss_resolve_graph_t *graph) ;
extern int ss_resolve_graph_src(ss_resolve_graph_t *graph, char const *dir, unsigned int reverse, unsigned int what) ;
extern int ss_resolve_graph_build(ss_resolve_graph_t *graph,resolve_service_t *res,char const *src,unsigned int reverse) ;
extern int ss_resolve_graph_sort(ss_resolve_graph_t *graph) ;
extern int ss_resolve_dfs(ss_resolve_graph_t *graph, unsigned int idx, visit *c,unsigned int *ename,unsigned int *edeps) ;
extern int ss_resolve_graph_publish(ss_resolve_graph_t *graph,unsigned int reverse) ;

#endif
