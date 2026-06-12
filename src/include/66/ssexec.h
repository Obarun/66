/*
 * ssexec.h
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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

#include <oblibs/types.h>

#include <oblibs/strbuf.h>
#include <oblibs/opt.h>

#include <66/config.h>

typedef struct ssexec_s ssexec_t , *ssexec_t_ref ;
struct ssexec_s
{
    strbuf base ;
    strbuf live ;
    strbuf scandir ;
    strbuf treename ;
    strbuf environment ;

    uint8_t treeallow ; //1 yes , 0 no
    uid_t owner ;
    char ownerstr[UID_FMT] ;
    size_t ownerlen ;
    uint64_t timeout ;
    // argument passed or not at commandline 0->no,1->yes
    uint8_t opt_verbo ;
    uint8_t opt_live ;
    uint8_t opt_tree ;
    uint8_t opt_timeout ;
    uint8_t opt_color ;
    // skip option definition 0->no,1-yes
    uint8_t skip_opt_tree ; // tree,treename, treeallow will not be set. Also, trees permissions is not checked.
} ;

#define SSEXEC_ZERO {   .base = STRBUF_ZERO, \
                        .live = STRBUF_ZERO, \
                        .scandir = STRBUF_ZERO, \
                        .treename = STRBUF_ZERO, \
                        .environment = STRBUF_ZERO, \
                        .treeallow = 0, \
                        .owner = 0, \
                        .ownerstr = { 0 }, \
                        .ownerlen = 0, \
                        .timeout = 0, \
                        .opt_verbo = 0, \
                        .opt_live = 0, \
                        .opt_tree = 0, \
                        .opt_timeout = 0, \
                        .opt_color = 0, \
                        .skip_opt_tree = 0 }

extern void ssexec_free(ssexec_t *info) ;
extern void ssexec_copy(ssexec_t *dest, ssexec_t *src) ;
extern ssexec_t const ssexec_zero ;

/** The leaf handler matches
 *  opt_cmd_fn (data is the ssexec_t *); the exported cmd_<name> node carries its
 *  option table and is the entry point both for the top dispatcher and for any
 *  internal re-dispatch (reconfigure, restart, free, ...). */
extern opt_cmd_fn ssexec_start ;
extern opt_cmd_fn ssexec_stop ;
extern opt_cmd_fn ssexec_enable ;
extern opt_cmd_fn ssexec_disable ;
extern opt_cmd_fn ssexec_parse ;
extern opt_cmd_fn ssexec_reload ;
extern opt_cmd_fn ssexec_restart ;
extern opt_cmd_fn ssexec_reconfigure ;
extern opt_cmd_fn ssexec_configure ;
extern opt_cmd_fn ssexec_resolve ;
extern opt_cmd_fn ssexec_state ;
extern opt_cmd_fn ssexec_remove ;
extern opt_cmd_fn ssexec_status ;
extern opt_cmd_fn ssexec_signal ;

extern opt_cmd_t const cmd_start ;
extern opt_cmd_t const cmd_stop ;
extern opt_cmd_t const cmd_free ;
extern opt_cmd_t const cmd_enable ;
extern opt_cmd_t const cmd_disable ;
extern opt_cmd_t const cmd_parse ;
extern opt_cmd_t const cmd_reload ;
extern opt_cmd_t const cmd_restart ;
extern opt_cmd_t const cmd_reconfigure ;
extern opt_cmd_t const cmd_configure ;
extern opt_cmd_t const cmd_resolve ;
extern opt_cmd_t const cmd_state ;
extern opt_cmd_t const cmd_remove ;
extern opt_cmd_t const cmd_status ;
extern opt_cmd_t const cmd_signal ;

/** wrapper sub-command handlers (opt_dispatch leaves) */
extern opt_cmd_fn ssexec_scandir_create ;
extern opt_cmd_fn ssexec_scandir_remove ;
extern opt_cmd_fn ssexec_scandir_signal ;
extern opt_cmd_fn ssexec_tree_admin ;
extern opt_cmd_fn ssexec_tree_signal ;
extern opt_cmd_fn ssexec_tree_status ;
extern opt_cmd_fn ssexec_tree_resolve ;
extern opt_cmd_fn ssexec_tree_init ;
extern opt_cmd_fn ssexec_snapshot_create ;
extern opt_cmd_fn ssexec_snapshot_restore ;
extern opt_cmd_fn ssexec_snapshot_remove ;
extern opt_cmd_fn ssexec_snapshot_list ;

/** wrapper command nodes (sub-trees) */
extern opt_cmd_t const cmd_scandir ;
extern opt_cmd_t const cmd_tree ;
extern opt_cmd_t const cmd_snapshot ;
extern opt_cmd_t const cmd_poweroff ;
extern opt_cmd_t const cmd_reboot ;
extern opt_cmd_t const cmd_halt ;

/** PID1 and supervision */
extern opt_cmd_fn ssexec_boot ;
extern opt_cmd_t const cmd_boot ;

/** Top-level dispatcher: builds the 66 command tree and routes argv to the
 *  selected (sub)command. 66.c is just owner setup + this call. */
extern int ssexec_main(int argc, char const *const *argv, ssexec_t *info) ;

#endif
