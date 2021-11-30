/*
 * ssexec.h
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

#ifndef SS_SSEXEC_H
#define SS_SSEXEC_H

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

typedef struct ssexec_s ssexec_t , *ssexec_t_ref ;
struct ssexec_s
{
    stralloc base ;
    stralloc live ;
    stralloc tree ;
    stralloc livetree ;
    stralloc scandir ;
    stralloc treename ;
    int treeallow ; //1 yes , 0 no
    uid_t owner ;
    uint32_t timeout ;
    char const *prog ;
    char const *help ;
    char const *usage ;
    // argument passed or not at commandline 0->no,1->yes
    uint8_t opt_verbo ;
    uint8_t opt_live ;
    uint8_t opt_tree ;
    uint8_t opt_timeout ;
    uint8_t opt_color ;
    // skip option definition 0->no,1-yes
    uint8_t skip_opt_tree ; // tree,treename, treeallow will not be set. Also, trees permissions is not checked.
} ;

#define SSEXEC_ZERO {   .base = STRALLOC_ZERO , \
                        .live = STRALLOC_ZERO , \
                        .tree = STRALLOC_ZERO , \
                        .livetree = STRALLOC_ZERO , \
                        .scandir = STRALLOC_ZERO , \
                        .treename = STRALLOC_ZERO , \
                        .treeallow = 0 , \
                        .owner = 0 , \
                        .timeout = 0 , \
                        .prog = 0 , \
                        .help = 0 , \
                        .usage = 0 , \
                        .opt_verbo = 0 , \
                        .opt_live = 0 , \
                        .opt_tree = 0 , \
                        .opt_timeout = 0 , \
                        .opt_color = 0 , \
                        .skip_opt_tree = 0 }

typedef int ssexec_func_t(int argc, char const *const *argv, char const *const *envp, ssexec_t *info) ;
typedef ssexec_func_t *ssexec_func_t_ref ;

extern void ssexec_free(ssexec_t *info) ;
extern ssexec_t const ssexec_zero ;
extern void set_ssinfo(ssexec_t *info) ;

extern ssexec_func_t ssexec_init ;
extern ssexec_func_t ssexec_enable ;
extern ssexec_func_t ssexec_disable ;
extern ssexec_func_t ssexec_start ;
extern ssexec_func_t ssexec_stop ;
extern ssexec_func_t ssexec_svctl ;
extern ssexec_func_t ssexec_dbctl ;
extern ssexec_func_t ssexec_env ;
extern ssexec_func_t ssexec_all ;
extern ssexec_func_t ssexec_tree ;

extern char const *usage_enable ;
extern char const *help_enable ;
extern char const *usage_disable ;
extern char const *help_disable ;
extern char const *usage_dbctl ;
extern char const *help_dbctl ;
extern char const *usage_svctl ;
extern char const *help_svctl ;
extern char const *usage_start ;
extern char const *help_start ;
extern char const *usage_stop ;
extern char const *help_stop ;
extern char const *usage_init ;
extern char const *help_init ;
extern char const *usage_env ;
extern char const *help_env ;
extern char const *usage_all ;
extern char const *help_all ;
extern char const *usage_tree ;
extern char const *help_tree ;

#define OPTS_INIT "cdb"
#define OPTS_INIT_LEN (sizeof OPTS_INIT - 1)
#define OPTS_ENABLE "fFSI"
#define OPTS_ENABLE_LEN (sizeof OPTS_ENABLE - 1)
#define OPTS_DISABLE "SFR"
#define OPTS_DISABLE_LEN (sizeof OPTS_DISABLE - 1)
#define OPTS_START "rR"
#define OPTS_START_LEN (sizeof OPTS_START - 1)
#define OPTS_STOP "uXK"
#define OPTS_STOP_LEN (sizeof OPTS_STOP - 1)
#define OPTS_SVCTL "n:urRdXK"
#define OPTS_SVCTL_LEN (sizeof OPTS_SVCTL - 1)
#define OPTS_DBCTL "udr"
#define OPTS_DBCTL_LEN (sizeof OPTS_DBCTL - 1)
#define OPTS_ENV "c:s:VLr:e:i:"
#define OPTS_ENV_LEN (sizeof OPTS_ENV - 1)
#define OPTS_ALL "f"
#define OPTS_ALL_LEN (sizeof OPTS_ALL - 1)
#define OPTS_TREE "na:d:cS:EDRC:o:"
#define OPTS_TREE_LEN (sizeof OPTS_TREE - 1)

extern int ssexec_main(int argc, char const *const *argv, char const *const *envp,ssexec_func_t *func,ssexec_t *info) ;
extern void ssexec_set_info(ssexec_t *info) ;
extern int ssexec_set_treeinfo(ssexec_t *info) ;

#endif
