/*
 * environ.h
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

#ifndef SS_ENVIRON_H
#define SS_ENVIRON_H

#include <sys/types.h>
#include <stdint.h>

#include <skalibs/stralloc.h>

#include <66/parser.h>

extern int env_resolve_conf(stralloc *env,char const *svname,uid_t owner) ;
extern int env_make_symlink(sv_alltype *sv) ;
extern int env_compute(stralloc *result,sv_alltype *sv, uint8_t conf) ;
extern int env_clean_with_comment(stralloc *sa) ;
extern int env_prepare_for_write(stralloc *name, stralloc *dst, stralloc *contents, sv_alltype *sv,uint8_t conf) ;

/** version function */
extern int env_find_current_version(stralloc *sa,char const *svconf) ;
extern int env_check_version(stralloc *sa, char const *version) ;
extern int env_append_version(stralloc *saversion, char const *svconf, char const *version) ;
extern int env_import_version_file(char const *svname, char const *svconf, char const *sversion, char const *dversion,int svtype) ;

#endif
