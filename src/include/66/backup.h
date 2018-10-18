/* 
 * backup.h
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
 
#ifndef BACKUP_H
#define BACKUP_H

#include <skalibs/stralloc.h>

extern int backup_make_new(char const *base, char const *tree, char const *treename,unsigned int type) ;

extern int backup_cmd_switcher(unsigned int verbosity,char const *cmd, char const *tree) ;
extern int backup_switcher(int argc, char const *const *argv) ;

extern int backup_realpath_sym(stralloc *sa,char const *sym,unsigned int type) ;
#endif
