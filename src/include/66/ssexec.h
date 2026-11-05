/*
 * ssexec.h
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#ifndef SS_SSEXEC_H
#define SS_SSEXEC_H

#include <stdint.h>
#include <stdbool.h>

#include <oblibs/types.h>

#include <oblibs/strbuf.h>
#include <oblibs/opt.h>

#include <66/config.h>
#include <66/status.h>

#define SSEXEC_MSG_UNKNOWN(name) "service: ", (name), " is unknown: no frontend and no service answers to the name -- is its provider enabled?"
#define SSEXEC_MSG_UNPARSED(name) "service: ", (name), " is not parsed -- parse it with 66 parse ", (name)
#define SSEXEC_MSG_MEMBER(name, module, verb) "service: ", (name), " is part of a module and cannot be ", (verb), "d alone -- please ", (verb), " the entire module instead using '66 ", (verb), " ", (module), "'"
#define SSEXEC_MSG_LOGGER(name, service, verb) "service: ", (name), " is a logger and is ", (verb), "d with its service -- please use '66 ", (verb), " ", (service), "'"

typedef struct ssexec_s ssexec_t , *ssexec_t_ref ;
struct ssexec_s
{
    strbuf base ;
    strbuf live ;
    strbuf scandir ;
    strbuf treename ;
    strbuf environment ;

    uid_t owner ;
    char ownerstr[UID_FMT] ;
    size_t ownerlen ;
    uint64_t timeout ;
    // argument passed or not at commandline 0->no,1->yes
    uint8_t opt_tree ;
    uint8_t opt_color ;
    // skip option definition 0->no,1-yes
    uint8_t skip_opt_tree ; // tree,treename will not be set. Also, trees permissions is not checked.
    uint8_t who ; // status_who_e: provenance of the command
    uint8_t target ; // svc_target_e a caller demands of the command it re-enters, SVC_TARGET_NONE to let the command decide
    uint8_t opt_react ; // set by 66-eventd on a fire-time start: do not pull a reactor's From establishment sources
} ;

#define SSEXEC_ZERO {   .base = STRBUF_ZERO, \
                        .live = STRBUF_ZERO, \
                        .scandir = STRBUF_ZERO, \
                        .treename = STRBUF_ZERO, \
                        .environment = STRBUF_ZERO, \
                        .owner = 0, \
                        .ownerstr = { 0 }, \
                        .ownerlen = 0, \
                        .timeout = 0, \
                        .opt_tree = 0, \
                        .opt_color = 0, \
                        .skip_opt_tree = 0, \
                        .who = STATUS_WHO_SELF, \
                        .target = 0, \
                        .opt_react = 0 }

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
extern opt_cmd_fn ssexec_runstate ;
extern opt_cmd_fn ssexec_remove ;
extern opt_cmd_fn ssexec_status ;
extern opt_cmd_fn ssexec_signal ;
extern opt_cmd_fn ssexec_log ;
extern opt_cmd_fn ssexec_emit ;

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
extern opt_cmd_t const cmd_runstate ;
extern opt_cmd_t const cmd_remove ;
extern opt_cmd_t const cmd_status ;
extern opt_cmd_t const cmd_signal ;
extern opt_cmd_t const cmd_log ;
extern opt_cmd_t const cmd_emit ;

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
extern opt_cmd_fn ssexec_fdholder_store ;
extern opt_cmd_fn ssexec_fdholder_retrieve ;
extern opt_cmd_fn ssexec_fdholder_list ;
extern opt_cmd_fn ssexec_internal_signal ;
extern opt_cmd_fn ssexec_internal_status ;
extern opt_cmd_fn ssexec_env_import ;
extern opt_cmd_fn ssexec_env_set ;
extern opt_cmd_fn ssexec_env_unset ;
extern opt_cmd_fn ssexec_env_list ;
extern opt_cmd_fn do_tree_start ;
extern opt_cmd_fn do_tree_stop ;
extern opt_cmd_fn do_scandir_start ;
extern opt_cmd_fn do_scandir_quit ;

extern opt_on_option_fn on_scandir_signal ;

/** wrapper command nodes (sub-trees) */
extern opt_cmd_t const cmd_scandir ;
extern opt_cmd_t const cmd_env ;
extern opt_cmd_t const cmd_tree ;
extern opt_cmd_t const cmd_snapshot ;
extern opt_cmd_t const cmd_fdholder ;
extern opt_cmd_t const cmd_internal ;
extern opt_cmd_t const cmd_poweroff ;
extern opt_cmd_t const cmd_reboot ;
extern opt_cmd_t const cmd_halt ;
extern opt_cmd_t const cmd_suspend ;
extern opt_cmd_t const cmd_hibernate ;

/** PID1 and supervision */
extern opt_cmd_fn ssexec_boot ;
extern opt_cmd_t const cmd_boot ;

/** Top-level dispatcher: builds the 66 command tree and routes argv to the
 *  selected (sub)command. 66.c is just owner setup + this call. */
extern int ssexec_main(int argc, char const *const *argv, ssexec_t *info) ;

#endif
