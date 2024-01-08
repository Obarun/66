/*
 * resolve.h
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

#ifndef SS_RESOLVE_H
#define SS_RESOLVE_H

#include <stddef.h>
#include <stdint.h>

#include <skalibs/stralloc.h>
#include <skalibs/cdb.h>
#include <skalibs/cdbmake.h>

#define DATA_TREE 1
#define DATA_TREE_MASTER 2
#define DATA_SERVICE 0

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
 * Freed
 *
 * */

extern void resolve_free(resolve_wrapper_t *wres) ;

/**
 *
 * Initiate
 *
 * */

extern resolve_wrapper_t *resolve_set_struct(uint8_t type, void *s) ;
extern int resolve_init(resolve_wrapper_t *wres) ;

/**
 *
 * General API
 *
 * */

extern int resolve_check_g(resolve_wrapper_t *wres, char const *base, char const *name) ;
extern int resolve_read_g(resolve_wrapper_t *wres, char const *base, char const *name) ;
extern int resolve_write_g(resolve_wrapper_t *wres, char const *base, char const *name) ;
extern void resolve_remove_g(char const *base, char const *name, uint8_t data_type) ;
extern int resolve_get_field_tosa_g(stralloc *sa, char const *base, char const *name, uint8_t data_type, uint8_t field) ;
extern int resolve_modify_field_g(resolve_wrapper_t_ref wres, char const *base, char const *name, uint8_t field, char const *value) ;
extern ssize_t resolve_add_string(resolve_wrapper_t *wres, char const *data) ;

/**
 *
 * Sub-functions
 *
 * */

extern int resolve_check(char const *base, char const *name) ;
extern int resolve_read(resolve_wrapper_t *wres, char const *base, char const *name) ;
extern int resolve_write(resolve_wrapper_t *wres, char const *base, char const *name) ;
extern void resolve_remove(char const *base, char const *name) ;
extern int resolve_get_field_tosa(stralloc *sa, resolve_wrapper_t_ref wres, uint8_t field) ;
extern int resolve_modify_field(resolve_wrapper_t_ref wres, uint8_t field, char const *by) ;
extern int resolve_read_cdb(resolve_wrapper_t *wres, char const *file) ;
extern int resolve_write_cdb(resolve_wrapper_t *wres, char const *file) ;
extern int resolve_add_cdb(cdbmaker *c, char const *key, char const *str, uint32_t element, uint8_t check) ;
extern int resolve_add_cdb_uint(cdbmaker *c, char const *key, uint32_t data) ;
extern int resolve_find_cdb(stralloc *result, cdb const *c, char const *key) ;

#endif
