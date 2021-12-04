/*
 * resolve.h
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

#ifndef SS_RESOLVE_H
#define SS_RESOLVE_H

#include <stddef.h>
#include <stdint.h>

#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/types.h>
#include <skalibs/cdb.h>
#include <skalibs/cdbmake.h>
#include <skalibs/gccattributes.h>

#include <66/ssexec.h>
#include <66/parser.h>
#include <66/tree.h>
#include <66/service.h>


#define SS_RESOLVE "/.resolve"
#define SS_RESOLVE_LEN (sizeof SS_RESOLVE - 1)
#define SS_RESOLVE_LIVE 0
#define SS_RESOLVE_SRC 1
#define SS_RESOLVE_BACK 2
#define SS_RESOLVE_STATE 3
#define SS_NOTYPE -1
#define SS_SIMPLE 0
#define SS_DOUBLE 1

typedef struct resolve_wrapper_s resolve_wrapper_t, *resolve_wrapper_t_ref ;
struct resolve_wrapper_s
{
    uint8_t type ;
    void *obj ;
} ;

#define RESOLVE_SET_SAWRES(wres) \
    stralloc_ref sawres ; \
    if (wres->type == SERVICE_STRUCT) sawres = (&((resolve_service_t *)wres->obj)->sa) ; \
    else if (wres->type == TREE_STRUCT) sawres = (&((resolve_tree_t *)wres->obj)->sa) ;

/**
 *
 * General API
 *
 * */

extern resolve_wrapper_t *resolve_set_struct(uint8_t type, void *s) ;
extern int resolve_init(resolve_wrapper_t *wres) ;
extern int resolve_read(resolve_wrapper_t *wres, char const *src, char const *name) ;
extern int resolve_write(resolve_wrapper_t *wres, char const *dst, char const *name) ;
extern int resolve_check(char const *src, char const *name) ;
extern int resolve_append(genalloc *ga, resolve_wrapper_t *wres) ;
extern int resolve_search(genalloc *ga, char const *name, uint8_t type) ;
extern int resolve_cmp(genalloc *ga, char const *name, uint8_t type) ;
extern void resolve_rmfile(char const *src,char const *name) ;
extern ssize_t resolve_add_string(resolve_wrapper_t *wres, char const *data) ;

/**
 *
 * Freed
 *
 * */

extern void resolve_free(resolve_wrapper_t *wres) ;
extern void resolve_deep_free(uint8_t type, genalloc *g) ;

/**
 *
 * CDB
 *
 * */

extern int resolve_read_cdb(resolve_wrapper_t *wres, char const *name) ;
extern int resolve_write_cdb(resolve_wrapper_t *wres, char const *dst, char const *name) ;
extern int resolve_add_cdb(cdbmaker *c, char const *key, char const *data) ;
extern int resolve_add_cdb_uint(cdbmaker *c, char const *key, uint32_t data) ;
extern int resolve_find_cdb(stralloc *result, cdb const *c, char const *key) ;

/*
extern ss_resolve_t const ss_resolve_zero ;
extern void ss_resolve_init(ss_resolve_t *res) ;
extern void ss_resolve_free(ss_resolve_t *res) ;

extern int ss_resolve_pointo(stralloc *sa,ssexec_t *info,int type, unsigned int where) ;
extern int ss_resolve_src_path(stralloc *sasrc,char const *sv, uid_t owner,char const *directory_forced) ;
extern int ss_resolve_module_path(stralloc *sdir, stralloc *mdir, char const *sv,char const *src_frontend, uid_t owner) ;
extern int ss_resolve_src(stralloc *sasrc, char const *name, char const *src,int *found) ;
extern int ss_resolve_service_isdir(char const *dir, char const *name) ;
extern ssize_t ss_resolve_add_string(ss_resolve_t *res,char const *data) ;
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
extern int ss_resolve_modify_field(ss_resolve_t *res, ss_resolve_enum_t field, char const *data) ;
extern int ss_resolve_put_field_to_sa(stralloc *sa,ss_resolve_t *res, ss_resolve_enum_t field) ;
*/

/** cdb
extern int ss_resolve_read_cdb(ss_resolve_t *dres, char const *name) ;
extern int ss_resolve_write_cdb(ss_resolve_t *res, char const *dst, char const *name) ;
extern int ss_resolve_add_cdb(cdbmaker *c,char const *key,char const *data) ;
extern int ss_resolve_add_cdb_uint(cdbmaker *c, char const *key,uint32_t data) ;
extern int ss_resolve_find_cdb(stralloc *result, cdb const *c,char const *key) ;
*/

#endif
