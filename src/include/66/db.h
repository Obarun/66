/* 
 * db.h
 * 
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */
 
#ifndef DB_H
#define DB_H

#include <skalibs/stralloc.h>

extern int db_cmd_master(unsigned int verbosity,char const *cmd) ;
extern int db_update_master(int argc, char const *const *argv) ;

extern int db_compile(char const *workdir, char const *tree, char const *treename,char const *const *envp) ;

extern int db_find_compiled_state(char const *livetree, char const *treename) ;

extern int db_get_permissions(stralloc *uid, char const *tree) ;

extern int db_update(char const *newdb, char const *tree, char const *live,char const *const *envp) ;

extern int db_ok(char const *livetree, char const *treename) ;

extern int db_switch_to(char const *base, char const *livetree, char const *tree, char const *treename, char const *const *envp,unsigned int where) ;
#endif
