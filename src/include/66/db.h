/*
 * db.h
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

#ifndef SS_DB_H
#define SS_DB_H

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <66/ssexec.h>

extern int db_compile(char const *workdir, char const *tree, char const *treename,char const *const *envp) ;
extern int db_find_compiled_state(char const *livetree, char const *treename) ;
extern int db_update(char const *newdb, ssexec_t *info,char const *const *envp) ;
extern int db_ok(char const *livetree, char const *treename) ;
extern int db_switch_to(ssexec_t *info, char const *const *envp,unsigned int where) ;

#endif
