/*
 * resolve.h
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
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
#include <sys/types.h>

#include <oblibs/cdb.h>

#include <oblibs/strbuf.h>

#include <66/enum.h>

#define DATA_TREE 1
#define DATA_TREE_MASTER 2
#define DATA_SERVICE 0
#define DATA_SERVICE_LIMIT 3 // autonomous addon of a service resolve
#define DATA_SERVICE_ENVIRON 4 // autonomous addon of a service resolve
#define DATA_SERVICE_IO 5 // autonomous addon of a service resolve
#define DATA_SERVICE_LOGGER 6 // autonomous addon of a service resolve
#define DATA_SERVICE_EXECUTE 7 // autonomous addon of a service resolve
#define DATA_SERVICE_DEPENDENCIES 8 // autonomous addon of a service resolve
#define DATA_SERVICE_REGEX 9 // autonomous addon of a service resolve

typedef struct resolve_wrapper_s resolve_wrapper_t, *resolve_wrapper_t_ref ;
struct resolve_wrapper_s
{
    uint8_t type ;
    void *obj ;
} ;

#ifndef RESOLVE_SET_SBWRES
#define RESOLVE_SET_SBWRES(wres) \
    strbuf_ref sbwres = 0 ; \
    if (wres->type == DATA_SERVICE) sbwres = (&((resolve_service_t *)wres->obj)->sa) ; \
    else if (wres->type == DATA_TREE) sbwres = (&((resolve_tree_t *)wres->obj)->sa) ; \
    else if (wres->type == DATA_TREE_MASTER) sbwres = (&((resolve_tree_master_t *)wres->obj)->sa) ; \
    else if (wres->type == DATA_SERVICE_LIMIT) sbwres = (&((resolve_service_addon_limit_t *)wres->obj)->sa) ; \
    else if (wres->type == DATA_SERVICE_ENVIRON) sbwres = (&((resolve_service_addon_environ_t *)wres->obj)->sa) ; \
    else if (wres->type == DATA_SERVICE_IO) sbwres = (&((resolve_service_addon_io_t *)wres->obj)->sa) ; \
    else if (wres->type == DATA_SERVICE_LOGGER) sbwres = (&((resolve_service_addon_logger_t *)wres->obj)->sa) ; \
    else if (wres->type == DATA_SERVICE_EXECUTE) sbwres = (&((resolve_service_addon_execute_t *)wres->obj)->sa) ; \
    else if (wres->type == DATA_SERVICE_DEPENDENCIES) sbwres = (&((resolve_service_addon_dependencies_t *)wres->obj)->sa) ; \
    else if (wres->type == DATA_SERVICE_REGEX) sbwres = (&((resolve_service_addon_regex_t *)wres->obj)->sa) ;
#endif

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
extern void resolve_init(resolve_wrapper_t *wres) ;

/** Frees the malloc'd wrapper itself, not the resolve payload it points to.
 *  Use with _cleanup_wres_ so early error returns do not leak the wrapper. */
extern void resolve_wrapper_free(resolve_wrapper_t **wres) ;
#define _cleanup_wres_ __attribute__((cleanup(resolve_wrapper_free)))

/**
 *
 * General API
 *
 * */

extern int resolve_check(resolve_wrapper_t *wres, char const *base, char const *name) ;
extern int resolve_open_cdb(int *fd, ocdb *c, const char *path, const char *name) ;
extern int resolve_read(resolve_wrapper_t *wres, char const *base, char const *name) ;
extern int resolve_write(resolve_wrapper_t *wres, char const *base, char const *name) ;
extern void resolve_remove(char const *base, char const *name, uint8_t data_type) ;
extern int resolve_get_field(strbuf *sa, resolve_wrapper_t_ref wres, char const *base, char const *name, resolve_enum_table_t table) ;
extern int resolve_modify_field(resolve_wrapper_t_ref wres, char const *base, char const *name, resolve_enum_table_t table, char const *value) ;
extern ssize_t resolve_add_string(resolve_wrapper_t *wres, char const *data) ;

/**
 *
 * Sub-functions
 *
 * */

extern int resolve_check_at(char const *base, char const *name) ;
extern int resolve_read_at(resolve_wrapper_t *wres, char const *base, char const *name) ;
extern int resolve_write_at(resolve_wrapper_t *wres, char const *base, char const *name) ;
extern void resolve_remove_at(char const *base, char const *name) ;
extern int resolve_get_field_from(strbuf *sa, resolve_wrapper_t_ref wres, resolve_enum_table_t table) ;
extern int resolve_modify_field_by(resolve_wrapper_t_ref wres, resolve_enum_table_t table, char const *by) ;
extern int resolve_read_cdb(resolve_wrapper_t *wres, const char *path, const char *name) ;
extern int resolve_write_cdb(resolve_wrapper_t *wres, const char *path, const char *name) ;
extern int resolve_add_cdb(ocdbmaker *c, char const *key, char const *str, uint32_t element, uint8_t check) ;
extern int resolve_add_cdb_uint(ocdbmaker *c, char const *key, uint32_t data) ;
extern int resolve_add_cdb_uint64(ocdbmaker *c, char const *key, uint64_t data) ;
extern int resolve_get_sa(strbuf *sa, const ocdb *c) ;
extern int resolve_get_key(const ocdb *c, const char *key, uint32_t *field) ;
extern uint32_t resolve_add_uint32(char const *data) ;
extern uint64_t resolve_add_uint64(char const *data) ;

#endif
