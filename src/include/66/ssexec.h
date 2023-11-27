/*
 * ssexec.h
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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

    stralloc scandir ;

    //char scandir[SS_MAX_SERVICE] ;
    //size_t scandirlen ;

    stralloc treename ;

    //char treename[SS_MAX_SERVICE] ;
    //size_t treenamelen ;

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

/** main */
extern ssexec_func_t ssexec_parse ;
extern ssexec_func_t ssexec_init ;
extern ssexec_func_t ssexec_enable ;
extern ssexec_func_t ssexec_disable ;
extern ssexec_func_t ssexec_start ;
extern ssexec_func_t ssexec_stop ;
extern ssexec_func_t ssexec_env ;
extern ssexec_func_t ssexec_reconfigure ;
extern ssexec_func_t ssexec_reload ;
extern ssexec_func_t ssexec_restart ;

/** PID1 and supervision */
extern ssexec_func_t ssexec_boot ;
extern ssexec_func_t ssexec_scandir_wrapper ;
extern ssexec_func_t ssexec_scandir_create ;
extern ssexec_func_t ssexec_scandir_remove ;
extern ssexec_func_t ssexec_scandir_signal ;

/** service */
extern ssexec_func_t ssexec_status ;
extern ssexec_func_t ssexec_resolve ;
extern ssexec_func_t ssexec_state ;
extern ssexec_func_t ssexec_remove ;
extern ssexec_func_t ssexec_signal ;

/** tree */
extern ssexec_func_t ssexec_tree_wrapper ;
extern ssexec_func_t ssexec_tree_signal ;
extern ssexec_func_t ssexec_tree_admin ;
extern ssexec_func_t ssexec_tree_status ;
extern ssexec_func_t ssexec_tree_resolve ;

/** shutdown */
extern ssexec_func_t ssexec_shutdown_wrapper ;

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
extern char const *usage_free ;
extern char const *help_free ;
extern char const *usage_status ;
extern char const *help_status ;
extern char const *usage_resolve ;
extern char const *help_resolve ;
extern char const *usage_state ;
extern char const *help_state ;
extern char const *usage_remove ;
extern char const *help_remove ;
extern char const *usage_signal ;
extern char const *help_signal ;

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
extern char const *usage_tree_start ;
extern char const *help_tree_start ;
extern char const *usage_tree_stop ;
extern char const *help_tree_stop ;
extern char const *usage_tree_unsupervise ;
extern char const *help_tree_unsupervise ;

extern char const *usage_scandir_wrapper ;
extern char const *help_scandir_wrapper ;
extern char const *usage_scandir_create ;
extern char const *help_scandir_create ;
extern char const *usage_scandir_remove ;
extern char const *help_scandir_remove ;
extern char const *usage_scandir_start ;
extern char const *help_scandir_start ;
extern char const *usage_scandir_stop ;
extern char const *help_scandir_stop ;
extern char const *usage_scandir_reconfigure ;
extern char const *help_scandir_reconfigure ;
extern char const *usage_scandir_rescan ;
extern char const *help_scandir_rescan ;
extern char const *usage_scandir_quit ;
extern char const *help_scandir_quit ;
extern char const *usage_scandir_halt ;
extern char const *help_scandir_halt ;
extern char const *usage_scandir_abort ;
extern char const *help_scandir_abort ;
extern char const *usage_scandir_nuke ;
extern char const *help_scandir_nuke ;
extern char const *usage_scandir_annihilate ;
extern char const *help_scandir_annihilate ;
extern char const *usage_scandir_zombies ;
extern char const *help_scandir_zombies ;

extern char const *usage_poweroff ;
extern char const *help_poweroff ;
extern char const *usage_reboot ;
extern char const *help_reboot ;
extern char const *usage_halt ;
extern char const *help_halt ;

#define OPTS_MAIN "hv:l:t:T:z"
#define OPTS_MAIN_LEN (sizeof OPTS_MAIN - 1)
#define OPTS_SUBSTART "hP"
#define OPTS_SUBSTART_LEN (sizeof OPTS_SUBSTART - 1)
#define OPTS_PARSE "hfFcmCI"
#define OPTS_PARSE_LEN (sizeof OPTS_PARSE - 1)
#define OPTS_INIT "h"
#define OPTS_INIT_LEN (sizeof OPTS_INIT - 1)
#define OPTS_ENABLE "hSP"
#define OPTS_ENABLE_LEN (sizeof OPTS_ENABLE - 1)
#define OPTS_DISABLE "hSFRP"
#define OPTS_DISABLE_LEN (sizeof OPTS_DISABLE - 1)
#define OPTS_START "hP"
#define OPTS_START_LEN (sizeof OPTS_START - 1)
#define OPTS_STOP "huP"
#define OPTS_STOP_LEN (sizeof OPTS_STOP - 1)
#define OPTS_STATUS "hno:grd:p:"
#define OPTS_STATUS_LEN (sizeof OPTS_STATUS - 1)
#define OPTS_SIGNAL "habqHkti12pcyrodDuUxOQw:P"
#define OPTS_SIGNAL_LEN (sizeof OPTS_SIGNAL - 1)
#define OPTS_ENV "hc:s:VLr:e:i:"
#define OPTS_ENV_LEN (sizeof OPTS_ENV - 1)
#define OPTS_STATE "h"
#define OPTS_STATE_LEN (sizeof OPTS_STATE - 1)
#define OPTS_RESOLVE "h"
#define OPTS_RESOVLE_LEN (sizeof OPTS_RESOLVE - 1)

#define OPTS_TREE_WRAPPER "h"
#define OPTS_TREE_WRAPPER_LEN (sizeof OPTS_TREE_WRAPPER - 1)
#define OPTS_TREE_STATUS "no:grd:"
#define OPTS_TREE_STATUS_LEN (sizeof OPTS_TREE_STATUS - 1)
#define OPTS_TREE_SIGNAL "f"
#define OPTS_TREE_SIGNAL_LEN (sizeof OPTS_TREE_SIGNAL - 1)
#define OPTS_TREE_ADMIN "co:EDRnadC:S:"
#define OPTS_TREE_ADMIN_LEN (sizeof OPTS_TREE_ADMIN - 1)

#define OPTS_BOOT "hms:e:d:b:l:"
#define OPTS_BOOT_LEN (sizeof OPTS_BOOT - 1)

#define OPTS_SCANDIR_WRAPPER "ho:"
#define OPTS_SCANCTL_WRAPPER_LEN (sizeof OPTS_SCANDIR_WRAPPER - 1)
#define OPTS_SCANDIR_SIGNAL "d:s:e:bB"
#define OPTS_SCANDIR_SIGNAL_LEN (sizeof OPTS_SCANDIR_SIGNAL - 1)
#define OPTS_SCANDIR_CREATE "bBs:cL:"
#define OPTS_SCANDIR_CREATE_LEN (sizeof OPTS_SCANDIR_CREATE - 1)

#define OPTS_SHUTDOWN_WRAPPER "hFfat:m:W"
#define OPTS_SHUTDOWN_WRAPPER_LEN (sizeof OPTS_SHUTDOWN_WRAPPER - 1)

#endif
