/*
 * utils.h
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

#ifndef SS_UTILS_H
#define SS_UTILS_H

#include <sys/types.h>
#include <unistd.h> //getuid

#include <oblibs/strbuf.h>

#include <66/ssexec.h>

#define MYUID getuid()
#define YOURUID(passto,owner) youruid(passto,owner)
#define MYGID getgid()
#define YOURGID(passto,owner) yourgid(passto,owner)

/** ss_utils.c file */
extern char const *get_userhome(uid_t myuid) ;
extern int youruid(uid_t *passto,char const *owner) ;
extern int yourgid(gid_t *passto,uid_t owner) ;
extern int set_livedir(strbuf *live) ;
extern int set_livescan(strbuf *live,uid_t owner) ;
extern int set_livestate(strbuf *live,uid_t owner) ;
extern int set_ownerhome(strbuf *base,uid_t owner) ;
extern int set_ownersysdir(strbuf *base,uid_t owner) ;
extern int set_environment(strbuf *env,uid_t owner) ;
extern int read_svfile(strbuf *sasv,char const *name,char const *src) ;
extern void name_isvalid(char const *name) ;
extern int set_ownerhome_stack(char *store) ;
extern int set_ownersysdir_stack(char *base, uid_t owner) ;
extern int set_ownerhome_stack_byuid(char *store, uid_t owner) ;
extern void set_treeinfo(ssexec_t *info) ;
extern void set_info(ssexec_t *info) ;
extern int notifier_isvalid(const char *str) ;

/**
 * if a < b return -1
 * if a > b return 1
 * if a == b return 0
 * set errno to EINVAL and return -2 on system call failure
*/
extern int version_compare(char const  *a, char const *b) ;

/**
 * Identifier
*/

/**
 * @brief Compute the replacement string for one @identifier.
 * @param[out] store  Buffer receiving the NUL-terminated replacement value.
 * @param[in]  data   Context (the service name for @I); may be 0 at runtime.
 * @return 1 on success (store holds the value), 0 on error,
 *         2 to decline (identifier not applicable here; leave it untouched).
 */
typedef int identifier_func_t(char *store, const char *data) ;

identifier_func_t identifier_replace_instance ;
identifier_func_t identifier_replace_username ;
identifier_func_t identifier_replace_useruid ;
identifier_func_t identifier_replace_usergid ;
identifier_func_t identifier_replace_usergroup ;
identifier_func_t identifier_replace_home ;
identifier_func_t identifier_replace_shell ;
identifier_func_t identifier_replace_runtime ;

typedef struct identifier_table_s identifier_table_t, *identifier_table_t_ref ;
struct identifier_table_s
{
    const char *ident ;
    identifier_func_t *func;
} ;

extern identifier_table_t identifier_table[] ;
extern int identifier_replace(strbuf *sasv, char const *svname) ;

/**
 * @brief Expand every @identifier in a NUL-delimited value block, in place.
 *
 * Unlike identifier_replace, which treats its buffer as newline-delimited text
 * and returns a single C-string, this operates directly on a block of
 * NUL-terminated records (KEY=VALUE\0KEY=VALUE\0...), the exec-environment
 * format consumed by exec_path_merge. The record separators and the total
 * length are preserved.
 *
 * @param[in,out] block  NUL-delimited record block; substitutions applied in place.
 * @param[in]     svname Service name fed to the @I (instance) substitution;
 *                       pass 0 at runtime, where @I is not applicable and is left
 *                       untouched.
 * @return 1 on success (including an empty block), 0 on failure.
 */
extern int identifier_replace_block(strbuf *block, char const *svname) ;

#endif
