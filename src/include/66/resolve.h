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
#include <skalibs/cdb.h>
#include <skalibs/cdbmake.h>

#include <66/graph.h>

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

#ifndef RESOLVE_SET_SAWRES
#define RESOLVE_SET_SAWRES(wres) \
    stralloc_ref sawres = 0 ; \
    if (wres->type == DATA_SERVICE) sawres = (&((resolve_service_t *)wres->obj)->sa) ; \
    else if (wres->type == DATA_SERVICE_MASTER) sawres = (&((resolve_service_master_t *)wres->obj)->sa) ; \
    else if (wres->type == DATA_TREE) sawres = (&((resolve_tree_t *)wres->obj)->sa) ; \
    else if (wres->type == DATA_TREE_MASTER) sawres = (&((resolve_tree_master_t *)wres->obj)->sa) ;
#endif

typedef struct resolve_field_table_s resolve_field_table_t, *resolve_field_table_t_ref ;
struct resolve_field_table_s
{
    char *field ;
} ;

/**
 *
 * General API
 *
 * */

extern resolve_wrapper_t *resolve_set_struct(uint8_t type, void *s) ;
extern int resolve_init(resolve_wrapper_t *wres) ;
extern int resolve_check(char const *src, char const *name) ;
extern int resolve_read(resolve_wrapper_t *wres, char const *src, char const *name) ;
/* convenient API: do a resolve_check then a resolve_read */
extern int resolve_read_g(resolve_wrapper_t *wres, char const *src, char const *name) ;
extern int resolve_write(resolve_wrapper_t *wres, char const *dst, char const *name) ;
extern int resolve_append(genalloc *ga, resolve_wrapper_t *wres) ;
extern int resolve_search(genalloc *ga, char const *name, uint8_t type) ;
extern int resolve_cmp(genalloc *ga, char const *name, uint8_t type) ;
extern void resolve_rmfile(char const *src,char const *name) ;
extern ssize_t resolve_add_string(resolve_wrapper_t *wres, char const *data) ;
extern int resolve_get_field_tosa_g(stralloc *sa, char const *base, char const *treename, char const *element, uint8_t data_type, uint8_t field) ;
extern int resolve_get_field_tosa(stralloc *sa, resolve_wrapper_t_ref wres, uint8_t field) ;
extern int resolve_modify_field(resolve_wrapper_t_ref wres, uint8_t field, char const *by) ;
extern int resolve_modify_field_g(resolve_wrapper_t_ref wres, char const *base, char const *element, uint8_t field, char const *value) ;
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

#endif
