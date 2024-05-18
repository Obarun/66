/*
 * environ.h
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

#ifndef SS_ENVIRON_H
#define SS_ENVIRON_H

#include <sys/types.h>
#include <stdint.h>

#include <skalibs/stralloc.h>

#include <66/parse.h>
#include <66/service.h>

extern int env_resolve_conf(stralloc *env, resolve_service_t *res) ;
extern int env_make_symlink(resolve_service_t *res) ;
extern int env_compute(stralloc *result, resolve_service_t *res) ;
extern int env_clean_with_comment(stralloc *sa) ;
extern int env_prepare_for_write(stralloc *dst, stralloc *contents, resolve_service_t *res) ;
extern int env_get_destination(stralloc *sa, resolve_service_t *res) ;

/** version function */
extern int env_find_current_version(stralloc *sa,char const *svconf) ;
extern int env_check_version(stack *stk, char const *version) ;
extern int env_append_version(stralloc *saversion, char const *svconf, char const *version) ;
extern int env_import_version_file(char const *svname, char const *svconf, char const *sversion, char const *dversion,int svtype) ;

#endif
