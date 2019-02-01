/* 
 * tree.h
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
 
#ifndef TREE_H
#define TREE_H

#include <sys/types.h>

#include <skalibs/stralloc.h>

extern int tree_cmd_state(unsigned int verbosity,char const *cmd,char const *tree) ;
extern int tree_state(int argc, char const *const *argv) ;

extern int tree_copy(stralloc *dir, char const *tree,char const *treename) ;

extern int tree_copy_tmp(char const *workdir, char const *base, char const *live, char const *tree,char const *treename) ;

/** Set the tree to use as current for 66 tools
 * This is avoid to use the -t options for all 66 tools
 * Search on @base the directory current and append @tree
 * with the path.
 * @Return 1 on success
 * @Return 0 on fail */
extern int tree_find_current(stralloc *tree, char const *base) ;


extern int tree_get_permissions(char const *tree) ;

extern int tree_sethome(stralloc *tree, char const *base) ;

extern char *tree_setname(char const *tree) ;

extern int tree_switch_current(char const *base, char const *tree) ;

#endif
