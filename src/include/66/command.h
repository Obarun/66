/*
 * command.h
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#ifndef SS_COMMAND_H
#define SS_COMMAND_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include <oblibs/bits.h>
#include <oblibs/hash.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/svc.h>

#define COMMAND_READ (SERVICE_READ_DEPENDENCIES | SERVICE_READ_EVENT | SERVICE_READ_EXECUTE)

static inline selection_service_policy_t command_policy(uint8_t propagate, bool nopropagate, bool from, bool logger)
{
    selection_service_policy_t policy = {
        .propagate = nopropagate ? SELECTION_SERVICE_PROPAGATE_NONE : propagate,
        .from = from,
        .reactors = 0,
        .logger = logger
    } ;
    return policy ;
}

typedef struct command_graph_s command_graph_t, *command_graph_t_ref ;
struct command_graph_s
{
    service_graph_t g ;
    service_graph_resolve_t store ;
    bool reloaded ;
} ;

#define COMMAND_GRAPH_ZERO { .g = SERVICE_GRAPH_ZERO, .store = SERVICE_GRAPH_RESOLVE_ZERO, .reloaded = false }

static inline svc_request_t command_request(ssexec_t const *info, uint8_t action, uint8_t target, char const *signal)
{
    svc_request_t req = SVC_REQUEST_ZERO ;

    req.action = action ;
    req.target = target ;
    req.signal = signal ;
    req.scandir = info->scandir.s ;
    req.timeout = info->timeout ;
    req.who = info->who ;

    return req ;
}

extern void command_set_info(ssexec_t *info) ;
extern void command_write_end(service_graph_t *g) ;
extern int command_write_begin_checked(hash_t *hres, ssexec_t *info, strbuf *msg) ;
extern int command_argv_to_select(selection_service_t *sel, command_graph_t *d, char const *const *argv, int argc, ssexec_t const *info, selection_service_policy_t const *policy, strbuf *frontends, uint32_t refusals) ;
extern void command_refuse_scandir(ssexec_t const *info) ;
extern int command_parse_missing(service_graph_t *g, char const *const *argv, int argc, ssexec_t *info, uint32_t flags) ;
extern void command_graph_reload(command_graph_t *d, ssexec_t const *info, bool refuse) ;
extern void command_refuse_unknown(char const *name, char const *base, uid_t owner, bool literal) ;
extern void command_refuse_selection(selection_service_t const *sel, strbuf *frontends, uint32_t refusals) ;
extern int command_refuse_conflict(selection_service_t const *lot, command_graph_t *d, ssexec_t const *info, uint8_t mode) ;
extern void command_refuse_member(char const *const *argv, int argc, char const *verb) ;
extern void command_refuse_logger(service_graph_t const *g, bitset32_t const *named, char const *verb) ;
extern void command_refuse_unsupervised(selection_service_t *sup, selection_service_t const *lot, service_graph_resolve_t *store) ;
extern int command_resolve_load(command_graph_t *d, selection_service_t const *sel) ;
extern int command_put_back(strbuf *up, strbuf *down, service_graph_t const *g, ssexec_t *info, strbuf *frontends, bool nopropagate) ;
extern void command_drop_waiters(selection_service_t *sel, selection_service_t const *started, bitset32_t const *reached) ;
extern int command_leave_invalid(selection_service_t *sel, strbuf *frontends, bool up) ;
extern int command_outside_boot(struct resolve_hash_s const *h) ;
extern int command_free_unreadable(selection_service_t const *sel, ssexec_t *info, strbuf *up, strbuf *down) ;
extern int command_lot_start(selection_service_t const *lot, service_graph_resolve_t *store, ssexec_t const *info, uint8_t target, uint8_t stage, bitset32_t *reached) ;
extern int command_lot_stop(selection_service_t const *lot, service_graph_resolve_t *store, ssexec_t const *info, uint8_t target, char const *signal, strbuf *up, strbuf *down) ;
extern int command_free_lot(selection_service_t const *sel, service_graph_resolve_t *store, ssexec_t const *info, strbuf *up, strbuf *down) ;
extern int command_up(char const *const *argv, int argc, ssexec_t *info, strbuf *frontends, uint8_t target, bool nopropagate, uint8_t stage, bool tree, bool warned) ;
extern int command_down(char const *const *argv, int argc, ssexec_t *info, strbuf *frontends, bool nopropagate, bool unsupervise, bool tree, bool logger) ;

#endif
