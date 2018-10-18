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

/** USAGE "tree_cmd_switcher [ -v verbosity ] [ -b backup ] [ -s switch ] tree"
 * -b : look if current point to backup folder
 * 		@Return 0 if point to original source
 * 		@Return 1 if point to backup
 * -s : point the current symlink to backup/source
 * 	 	-s0 mean original source
 * 		-s1 mean backup source
 * @Return 1 on success
 * @Return -1 on fail 
 * for -b option : 	@Return 0 for source
 * 					@Return 1 for backup */
extern int tree_cmd_switcher(unsigned int verbosity, char const *cmd,char const *tree) ;
extern int tree_switcher(int argc, char const *const *argv) ;

extern int tree_copy(stralloc *dir, char const *tree,char const *treename) ;

/** Set the tree to use as current for 66 tools
 * This is avoid to use the -t options for all 66 tools
 * Search on @base the directory current and append @tree
 * with the path.
 * @Return 1 on success
 * @Return 0 on fail */
extern int tree_find_current(stralloc *tree, char const *base) ;

extern int tree_from_current(stralloc *sa, char const *tree) ;

extern int tree_get_permissions(char const *tree) ;

extern int tree_sethome(stralloc *tree, char const *base) ;

extern int tree_switch_current(char const *base, char const *tree) ;

extern int tree_switch_tobackup(char const *base, char const *treename, char const *tree, char const *livetree,char const *const *envp) ;

extern int tree_make_backup(char const *base, char const *tree,  char const *treename) ;

#endif
