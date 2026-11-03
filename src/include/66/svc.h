/*
 * svc.h
 *
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#ifndef SS_SVC_H
#define SS_SVC_H

#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include <66/service.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/ssexec.h>
#include <66/event.h>
#include <66/fdholder.h>
#include <66/oneshot.h>
#include <66/status.h>

#define DATASIZE 65

#define SVC_SIGNAL_MAP(macro) \
    macro('a', SIGALRM) \
    macro('b', SIGABRT) \
    macro('q', SIGQUIT) \
    macro('h', SIGHUP) \
    macro('k', SIGKILL) \
    macro('t', SIGTERM) \
    macro('i', SIGINT) \
    macro('1', SIGUSR1) \
    macro('2', SIGUSR2) \
    macro('p', SIGSTOP) \
    macro('c', SIGCONT) \
    macro('y', SIGWINCH)

typedef enum svc_target_e svc_target_t, *svc_target_t_ref ;
enum svc_target_e
{
    SVC_TARGET_NONE = 0, // nothing declared: the command picks its own
    SVC_TARGET_FREE,     // neither supervised nor running
    SVC_TARGET_DOWN,     // supervised, no process
    SVC_TARGET_UP,       // supervised and running
    SVC_TARGET_READY,    // able to serve: an ordinary service runs, a reactor arms and rests
    SVC_TARGET_KEEP      // no state change: deliver a signal and nothing else
} ;

static inline int svc_target_stops(uint8_t target)
{
    return target == SVC_TARGET_DOWN || target == SVC_TARGET_FREE ;
}

typedef enum svc_action_e svc_action_t, *svc_action_t_ref ;
enum svc_action_e
{
    SVC_ACTION_START = 0,
    SVC_ACTION_STOP,
    SVC_ACTION_SIGNAL // a signal that asks for no state: what it reaches first is what depends
} ;

typedef struct svc_request_s svc_request_t, *svc_request_t_ref ;
struct svc_request_s
{
    uint8_t action ;
    uint8_t target ;
    char const *signal ;
    char const *wsignal ;
    char const *verb ;
    char const *scandir ;
    uint64_t timeout ;
    uint8_t who ;
} ;

#define SVC_REQUEST_ZERO { .action = SVC_ACTION_START, .target = SVC_TARGET_NONE, .signal = 0, .wsignal = 0, .verb = 0, .scandir = 0, .timeout = 0, .who = 0 }

#define SVC_LOT_COMMAND 0 // outside the boot stage: an earlier member of a module is left to the boot
#define SVC_LOT_BOOT 1 // the boot stage, the scandir running: every member
#define SVC_LOT_EARLIER 2 // the boot stage before the scandir runs: the earlier members only

#define SVC_CONFLICT_RUNNING 0 // a conflicting service that runs (start)
#define SVC_CONFLICT_ENABLED 1 // a conflicting service that is enabled (enable)

extern int svc_issupervised(resolve_service_t const *res) ;
extern int svc_isrunning(struct resolve_hash_s const *h) ;
extern int svc_isrunning_supervised(struct resolve_hash_s const *h) ;

extern int svc_lot(selection_service_t *lot, selection_service_t const *sel, service_graph_resolve_t *store, uint8_t stage) ;

extern int svc_conflict(selection_service_t const *sel, service_graph_resolve_t *store, char const *base, uint8_t mode, uint32_t *member, uint32_t *other) ;
extern int svc_send(selection_service_t const *lot, service_graph_resolve_t *store, svc_request_t const *req, bitset32_t *reached) ;
extern int svc_unsupervise(selection_service_t const *lot, service_graph_resolve_t *store, uint8_t who) ;


/**
 *
 * The launcher of svc_send()
 *
 * */

#define SVC_FLAGS_DOWN (1 << 0)
#define SVC_FLAGS_UP (1 << 1)
#define SVC_FLAGS_STARTING (1 << 2)
#define SVC_FLAGS_STOPPING (1 << 3)
#define SVC_FLAGS_FAILED (1 << 4)
#define SVC_FLAGS_WAITING_DEPS (1 << 5)

typedef struct svc_ctx_s svc_ctx_t, *svc_ctx_t_ref ;
struct svc_ctx_s
{
    struct resolve_hash_s const *h ;
    oneshot_async_t oneshot ;

    event_fifo_t fifo ;
    event_aggregator_t ag ;
    event_state_t match ;
    bool native ;

    bool done ; // its completion is told, the event and the deadline race for it
    bool waiting ; // a reactor armed only, its Execute never ran
    bool mreactor ; // a member of a module reactor in the lot: armed and run with its module

    bool supervised ; // a supervisor held it when svc_init_ctx() read its state
    uint16_t state ; // SVC_FLAGS_*
    uint8_t target ; // svc_target_e it must reach

    uint32_t index ; // its vertex
    uint32_t ndepends ;
    uint32_t nrequiredby ;

    uint8_t result ; // status_result_e
    uint32_t code ; // read per result: the exit code (EXITED), the signal (SIGNALED)
} ;

#define SVC_CTX_ZERO { .h = 0, .oneshot = { 0 }, .native = false, .done = false, .waiting = false, .mreactor = false, .supervised = false, .state = 0, .target = SVC_TARGET_NONE, .index = 0, .ndepends = 0, .nrequiredby = 0, .result = STATUS_RESULT_SUCCESS, .code = 0 }

extern int svc_init_ctx(svc_ctx_t *asvc, selection_service_t const *lot, service_graph_resolve_t *store, svc_request_t const *req) ;
extern int svc_launch(svc_ctx_t *asvc, selection_service_t const *lot, svc_request_t const *req) ;
extern int svc_unsupervise_one(resolve_service_t *res, fdholder_client_t *a, bool *started, uint8_t who) ;

extern int svc_scandir_ok (char const *dir) ;
extern int svc_supervise_ok(char const *dir) ;
extern int svc_supervise_wait(char const *dir, int timeout_ms) ;
extern int svc_scandir_send(char const *scandir,char const *signal) ;
extern int svc_control_send(char const *scandir, char const *ops, size_t nops, uint8_t who) ;
extern int svcd_notify(char const *eventddir, char verb, uint8_t who, char const *name) ;

extern int svc_daemon_send(char const *dir, char const *control, uint8_t who, event_t wanted, int timeout_ms) ;
extern int svc_daemon_wait(char const *dir, int timeout_ms) ;
extern int svc_status_state(char const *dir, unsigned char *up, unsigned char *ready) ;
extern int svc_status(resolve_service_t const *res, service_status_t *st) ;
extern int svc_reactor_armed_idle(struct resolve_hash_s const *h) ;
extern uint8_t svc_status_effective(struct resolve_hash_s const *h, service_status_t const *st) ;

#endif
