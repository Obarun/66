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

#include <stdint.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include <66/config.h>

typedef struct ssexec_s ssexec_t , *ssexec_t_ref ;
struct ssexec_s
{
    stralloc base ;

    //char base[SS_MAX_SERVICE] ;
    //size_t baselen ;

    stralloc live ;

    //char live[SS_MAX_SERVICE] ;
    //size_t livelen ;

    stralloc tree ;

    //char tree[SS_MAX_SERVICE] ;
    //size_t treelen ;

    stralloc scandir ;

    //char scandir[SS_MAX_SERVICE] ;
    //size_t scandirlen ;

    stralloc treename ;

    //char treename[SS_MAX_SERVICE] ;
    //size_t treenamelen ;

    /**
     *
     * verifier pour les cast entre int et uint8_t
     * a son call
     *
     * */
    uint8_t treeallow ; //1 yes , 0 no
    uid_t owner ;
    char ownerstr[UID_FMT] ;
    size_t ownerlen ;
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

#define SSEXEC_ZERO {   .base = STRALLOC_ZERO, \
                        .live = STRALLOC_ZERO, \
                        .tree = STRALLOC_ZERO, \
                        .scandir = STRALLOC_ZERO, \
                        .treename = STRALLOC_ZERO, \
                        .treeallow = 0, \
                        .owner = 0, \
                        .ownerstr = { 0 }, \
                        .ownerlen = 0, \
                        .timeout = 0, \
                        .prog = 0, \
                        .help = 0, \
                        .usage = 0, \
                        .opt_verbo = 0, \
                        .opt_live = 0, \
                        .opt_tree = 0, \
                        .opt_timeout = 0, \
                        .opt_color = 0, \
                        .skip_opt_tree = 0 }

typedef int ssexec_func_t(int argc, char const *const *argv, ssexec_t *info) ;
typedef ssexec_func_t *ssexec_func_t_ref ;

extern void ssexec_free(ssexec_t *info) ;
extern void ssexec_copy(ssexec_t *dest, ssexec_t *src) ;
extern ssexec_t const ssexec_zero ;
extern void set_ssinfo(ssexec_t *info) ;

extern ssexec_func_t ssexec_parse ;
extern ssexec_func_t ssexec_init ;
extern ssexec_func_t ssexec_enable ;
extern ssexec_func_t ssexec_disable ;
extern ssexec_func_t ssexec_start ;
extern ssexec_func_t ssexec_stop ;
extern ssexec_func_t ssexec_svctl ;
extern ssexec_func_t ssexec_env ;
extern ssexec_func_t ssexec_treectl ;
extern ssexec_func_t ssexec_tree ;
extern ssexec_func_t ssexec_reconfigure ;
extern ssexec_func_t ssexec_reload ;
extern ssexec_func_t ssexec_restart ;
extern ssexec_func_t ssexec_inresolve ;
extern ssexec_func_t ssexec_resolve_service ;
extern ssexec_func_t ssexec_resolve_tree ;
extern ssexec_func_t ssexec_instate ;
extern ssexec_func_t ssexec_intree ;
extern ssexec_func_t ssexec_inservice ;
extern ssexec_func_t ssexec_boot ;
extern ssexec_func_t ssexec_scanctl ;
extern ssexec_func_t ssexec_scandir ;
extern ssexec_func_t ssexec_tree_wrapper ;
extern ssexec_func_t ssexec_service_wrapper ;
extern ssexec_func_t ssexec_service_admin ;

extern void info_help (char const *help,char const *usage) ;

extern char const *usage_66 ;
extern char const *help_66 ;

extern char const *usage_boot ;
extern char const *help_boot ;

extern char const *usage_enable ;
extern char const *help_enable ;

extern char const *usage_disable ;
extern char const *help_disable ;

extern char const *usage_start ;
extern char const *help_start ;

extern char const *usage_stop ;
extern char const *help_stop ;

extern char const *usage_env ;
extern char const *help_env ;

extern char const *usage_init ;
extern char const *help_init ;

extern char const *usage_parse ;
extern char const *help_parse ;

extern char const *usage_reconfigure ;
extern char const *help_reconfigure ;

extern char const *usage_reload ;
extern char const *help_reload ;

extern char const *usage_restart ;
extern char const *help_restart ;

extern char const *usage_unsupervise ;
extern char const *help_unsupervise ;

extern char const *usage_svctl ;
extern char const *help_svctl ;

extern char const *usage_tree_wrapper ;
extern char const *help_tree_wrapper ;
extern char const *usage_tree_create ;
extern char const *help_tree_create ;
extern char const *usage_tree_admin ;
extern char const *help_tree_admin ;
extern char const *usage_tree_remove ;
extern char const *help_tree_remove ;
extern char const *usage_tree_enable ;
extern char const *help_tree_enable ;
extern char const *usage_tree_disable ;
extern char const *help_tree_disable ;
extern char const *usage_tree_current ;
extern char const *help_tree_current ;
extern char const *usage_tree_resolve ;
extern char const *help_tree_resolve ;
extern char const *usage_tree_status ;
extern char const *help_tree_status ;
extern char const *usage_tree_up ;
extern char const *help_tree_up ;
extern char const *usage_tree_down ;
extern char const *help_tree_down ;
extern char const *usage_tree_unsupervise ;
extern char const *help_tree_unsupervise ;

extern char const *usage_service_wrapper ;
extern char const *help_service_wrapper ;
extern char const *usage_service_status ;
extern char const *help_service_status ;
extern char const *usage_service_resolve ;
extern char const *help_service_resolve ;
extern char const *usage_service_state ;
extern char const *help_service_state ;
extern char const *usage_service_remove ;
extern char const *help_service_remove ;

extern char const *usage_scanctl ;
extern char const *help_scanctl ;

extern char const *usage_scandir ;
extern char const *help_scandir ;

#define OPTS_SUBSTART "hP"
#define OPTS_SUBSTART_LEN (sizeof OPTS_SUBSTART - 1)
#define OPTS_PARSE "hfFcmCI"
#define OPTS_PARSE_LEN (sizeof OPTS_PARSE - 1)
#define OPTS_ENABLE "hfFSI"
#define OPTS_ENABLE_LEN (sizeof OPTS_ENABLE - 1)
#define OPTS_DISABLE "hS"
#define OPTS_DISABLE_LEN (sizeof OPTS_DISABLE - 1)
#define OPTS_START "hP"
#define OPTS_START_LEN (sizeof OPTS_START - 1)
#define OPTS_STOP "huP"
#define OPTS_STOP_LEN (sizeof OPTS_STOP - 1)
#define OPTS_SVCTL "habqHkti12pcyroduxOw:P"
#define OPTS_SVCTL_LEN (sizeof OPTS_SVCTL - 1)
#define OPTS_ENV "hc:s:VLr:e:i:"
#define OPTS_ENV_LEN (sizeof OPTS_ENV - 1)
#define OPTS_TREECTL "f"
#define OPTS_TREECTL_LEN (sizeof OPTS_TREECTL - 1)
#define OPTS_TREE "hco:EDRnadC:S:"
#define OPTS_TREE_LEN (sizeof OPTS_TREE - 1)
#define OPTS_INTREE "no:grd:l:"
#define OPTS_INTREE_LEN (sizeof OPTS_INTREE - 1)
#define OPTS_INSERVICE "no:grd:t:p:"
#define OPTS_INSERVICE_LEN (sizeof OPTS_INSERVICE - 1)
#define OPTS_BOOT "hms:e:d:b:l:"
#define OPTS_BOOT_LEN (sizeof OPTS_BOOT - 1)
#define OPTS_SCANCTL "ho:d:t:e:"
#define OPTS_SCANCTL_LEN (sizeof OPTS_SCANCTL - 1)
#define OPTS_SCANDIR "hbl:s:o:L:cB"
#define OPTS_SCANDIR_LEN (sizeof OPTS_SCANDIR - 1)

#define OPTS_SERVICE_WRAPPER ""
#define OPTS_SERVICE_WRAPPER_LEN (sizeof OPTS_SERVICE_WRAPPER - 1)
#define OPTS_SERVICE_ADMIN ""
#define OPTS_SERVICE_ADMIN_LEN (sizeof OPTS_SERVICE_ADMIN - 1)

#endif
