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
 * except according to the terms contained in the LICENSE file./
 */

#ifndef SS_SVC_H
#define SS_SVC_H

#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include <oblibs/sse.h>

#include <66/service.h>
#include <66/graph.h>
#include <66/ssexec.h>
#include <66/event.h>
#include <66/status.h>
#include <66/oneshot.h>

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

#define SVC_FLAGS_DOWN 1
#define SVC_FLAGS_UP (1 << 1)
#define SVC_FLAGS_PROCESSING (1 << 2)
#define SVC_FLAGS_STARTING (1 << 3)
#define SVC_FLAGS_STOPPING (1 << 4)
#define SVC_FLAGS_FAILED (1 << 5)
#define SVC_FLAGS_WAITING_DEPS (1 << 6)
#define SVC_FLAGS_TIMEOUT (1 << 7)

struct svc_ctx_s
{
    pid_t pid ; // Process ID when running
    resolve_service_t *res ; // Service resolution data
    resolve_service_addon_execute_t *execute ; // execute addon (down/timeout/notify)
    resolve_service_addon_dependencies_t *dependencies ; // dependencies addon (module contents)

    // Watchers
    sse_watcher_t timeout ; // Timeout watcher

    // ONESHOT services: the run/finish script is run by 66-oneshotd; this is the
    // async request/response state multiplexed on the manager loop
    oneshot_async_t oneshot ;

    // Native readiness wait (CLASSIC services: no child, transitions read from
    // the service event fifodir instead of a spawned external wait helper)
    event_fifo_t fifo ; // subscriber on the service event fifodir
    event_aggregator_t ag ; // per-source frame aggregator feeding the matcher
    event_state_t match ; // transition interpreter for this service's wait
    bool native ; // uses the native CLASSIC path (no child process)
    bool done ; // completion already emitted (guards event vs timeout)
    bool waiting ; // Do=start reactor armed-not-launched: report its status down

    // State management
    uint8_t state ; // Current state
    uint8_t target_state ; // Desired state

    // Dependencies
    uint32_t index ; // vertex index of the service
    vertex_t *depends[SS_MAX_SERVICE] ; // Services depends
    uint32_t ndepends ;
    vertex_t *requiredby[SS_MAX_SERVICE] ; // requiredby dependencies of the service
    uint32_t nrequiredby ;

    // Runtime data
    int exitcode ; // Last exit code
} ;
typedef struct svc_ctx_s svc_ctx_t ;

#define SVC_CTX_ZERO { \
    .pid = -1, \
    .res = NULL, \
    .execute = NULL, \
    .dependencies = NULL, \
    .timeout = {0}, \
    .oneshot = {0}, \
    .native = false, \
    .done = false, \
    .waiting = false, \
    .state = 0, \
    .target_state = 0, \
    .index = 0, \
    .depends = { NULL }, \
    .ndepends = 0, \
    .requiredby = { NULL }, \
    .nrequiredby = 0, \
    .exitcode = 0 \
}

struct svc_manager_s
{
    sse_epoll_t loop ; // Main event loop
    svc_ctx_t *asvc ; // Service array
    uint32_t nsvc ; // Number of services

    // Global watchers
    sse_watcher_t signalfd ; // Global signal handler
    sse_watcher_t notifier ; // Internal notifier pipe
    sse_watcher_t deadline ; // Global deadline timer if any

    // notifier mechanism
    int notifd[2] ; // Internal event notifier pipe

    // State
    bool shutdown_requested ; // Shutdown in progress

    // Configuration
    ssexec_t *info ;
    uint64_t timeout ; // Global operation timeout
    uint8_t operation ; // START/STOP operation
    bool propagate ; // Propagate failures to dependents

    char signal[DATASIZE + 1] ; // signal to sent
    char wsignal[4] ; // -w svc signal
    bool woption ; // -w is used or not
    char *cmdmsg ; // echo restart or reload
} ;
typedef struct svc_manager_s svc_manager_t ;

extern void svc_init_ctx(svc_ctx_t *asvc, service_graph_t *g, uint8_t requiredby, uint32_t flag) ;
extern int svc_launch(svc_ctx_t *asvc, uint32_t nsvc, uint8_t operation, ssexec_t *info, char const *wsignal, uint8_t woption, char const *signal, char *cmdmsg, uint8_t propagate) ;
extern int svc_compute_ns(svc_manager_t *mgr, uint32_t id) ;
extern int svc_scandir_ok (char const *dir) ;
extern int svc_scandir_send(char const *scandir,char const *signal) ;
extern int svc_control_send(char const *scandir, char const *ops, size_t nops, uint8_t who) ;
extern int svcd_notify(char const *eventddir, char verb, char const *name) ;
extern int svc_send(char const *const *argv, int argc, ssexec_t *info, char const *signal, char const *wsignal, uint8_t woption, uint8_t propagate) ;
extern void svc_unsupervise(service_graph_t *g) ;
extern void svc_send_daemon(char const *dir, char const *control, uint8_t who, event_t wanted, int timeout_ms) ;
extern int svc_status_state(char const *dir, unsigned char *up, unsigned char *ready) ;
extern int svc_status(resolve_service_t *res, service_status_t *st) ;
extern int svc_is_up(char const *name) ;

#endif
