/*
 * svc_launch.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <oblibs/io.h>
#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/environ.h>
#include <oblibs/string.h>
#include <oblibs/clock.h>
#include <oblibs/sse.h>
#include <oblibs/fd.h>
#include <oblibs/spawn.h>

#include <66/service.h>
#include <66/state.h>
#include <66/status.h>
#include <66/enum_parser.h>
#include <66/svc.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/event.h>

// Internal event types for coordination
enum svc_event_type_e
{
    SVC_EVENT_CHILD_SUCCESS,
    SVC_EVENT_CHILD_FAILED,
    SVC_EVENT_TIMEOUT,
    SVC_EVENT_SHUTDOWN_REQUEST
} ;
typedef enum svc_event_type_e svc_event_type_t ;

// Internal coordination message
struct svc_event_msg_s {
    svc_event_type_t type ; // type of the event
    uint32_t id ; // id of the service
    uint64_t timestamp ; // For debugging/logging
} ;
typedef struct svc_event_msg_s svc_event_msg_t ;

static svc_manager_t *pmanager ;
static uint32_t npid = 0 ;
static uint32_t *v2svc ;

// prototype
static int launch_service(uint32_t id) ;
static int launch_classic(uint32_t id) ;
static void child_cb(sse_watcher_t *w, void *cbdata, int event) ;
static void timeout_cb(sse_watcher_t *w, void *cbdata, int event) ;
static void wait_timeout_cb(sse_watcher_t *w, void *cbdata, int event) ;
static void svc_wait_handler(event_reader_t *r, char const *buf, size_t len, void *data) ;
static void complete(uint32_t id, bool success) ;

// helpers
static uint32_t get_asvc_id(vertex_t *v)
{
    log_flow() ;
    uint32_t id = v->index ;
    return v2svc[id] ;
}

static bool deps_satisfied(uint32_t id)
{
    log_flow() ;

    uint32_t pos = 0 ;
    svc_ctx_t *svc = &pmanager->asvc[id];
    int flag = !pmanager->operation ? SVC_FLAGS_UP : SVC_FLAGS_DOWN ;

    for (; pos < svc->ndepends ; pos++) {

        uint32_t did = get_asvc_id(svc->depends[pos]) ;
        svc_ctx_t *dep = &pmanager->asvc[did] ;

        if (!FLAGS_ISSET(dep->state, flag))
            return false ;
    }

    return true ;
}

static void wait_deps(uint32_t id)
{
    log_flow() ;

    uint32_t pos = 0 ;
    svc_ctx_t *svc = &pmanager->asvc[id];

    // Check all dependents of this service
    for (; pos < svc->nrequiredby ; pos++) {

        uint32_t did = get_asvc_id(svc->requiredby[pos]) ;
        svc_ctx_t *dep = &pmanager->asvc[did] ;

        if (dep->state == SVC_FLAGS_WAITING_DEPS && deps_satisfied(did))
            launch_service(did) ;
    }
}

static void propagate_failure(uint32_t id)
{
    log_flow() ;

    uint32_t pos = 0 ;
    svc_ctx_t *svc = &pmanager->asvc[id] ;

    for (; pos < svc->ndepends ; pos++) {

        uint32_t did = get_asvc_id(svc->depends[pos]) ;
        svc_ctx_t *dep = &pmanager->asvc[did];

        // Only propagate to services that were supposed to start
        if (dep->target_state == SVC_FLAGS_UP && (dep->state == SVC_FLAGS_WAITING_DEPS || dep->state == SVC_FLAGS_STARTING)) {

            // Stop any watchers that might be active */
            if (dep->pid > 0)
                kill(dep->pid, SIGTERM) ;

            // Recursively propagate
            if (pmanager->propagate)
                propagate_failure(did) ;
        }
    }
}

static inline int svc_send_event(svc_event_type_t type, uint32_t id)
{
    log_flow() ;

    svc_event_msg_t msg = {
        .type = type,
        .id = id,
        .timestamp = time(NULL)
    } ;

    ssize_t written = io_write(pmanager->notifd[1], (char *)&msg, sizeof(msg)) ;
    return (written == sizeof(msg)) ? 1 : 0 ;
}

static void svc_runtime_write(svc_ctx_t *svc, bool success)
{
    log_flow() ;

    service_status_t st = STATUS_ZERO ;

    if (success) {
        st.state = pmanager->operation ? STATUS_STATE_DOWN : STATUS_STATE_DONE ;
        st.result = STATUS_RESULT_SUCCESS ;
    } else {
        st.state = STATUS_STATE_FAILED ;
        st.result = STATUS_RESULT_EXITED ;
        st.code = (uint32_t)svc->exitcode ;
    }
    st.who = STATUS_WHO_USER ;
    clock_now(&st.stamp) ;

    char const *supervisedir = svc->res->sa.s + svc->res->live.supervisedir ;
    char file[strlen(supervisedir) + 1 + SS_STATUS_LEN + 1] ;
    auto_strings(file, supervisedir, "/", SS_STATUS) ;

    if (!status_write(&st, file))
        log_warnusys("write runtime status of: ", svc->res->sa.s + svc->res->name) ;
}

// state = true > success
static void announce(uint32_t id, bool success)
{
    log_flow() ;

    int fd ;
    svc_ctx_t *svc = &pmanager->asvc[id] ;
    char const *name = svc->res->sa.s + svc->res->name ;
    char const *scandir = svc->res->sa.s + svc->res->live.scandir ;
    size_t scandirlen = strlen(scandir) ;
    char file[scandirlen +  6] ;

    auto_strings(file, scandir, "/down") ;

    if (svc->res->type != E_PARSER_TYPE_CLASSIC)
        svc_runtime_write(svc, success) ;

    if (success) {

        if (!svc->res->execute.down && svc->res->type == E_PARSER_TYPE_CLASSIC) {

            if (!pmanager->operation) {

                if (!access(scandir, F_OK)) {
                    log_trace("delete down file: ", file) ;
                    if (unlink(file) < 0 && errno != ENOENT)
                        log_warnusys("delete down file: ", file) ;
                }

            } else {

                fd = io_open_mode(file, O_WRONLY | O_NONBLOCK | O_TRUNC | O_CREAT, 0666) ;
                /** The directory and file may not exist. Typically,
                 * a service inside a module can not match the state
                 * of the module and may occur in case of crash of a
                 * previous user command invocation.
                 * Where are on stop process, so do not crash
                 * for a corrupted service.
                 * At start process, the live directory will be made
                 * from scratch anyway.*/
                if (fd < 0 && errno != ENOENT)
                    log_dieusys(LOG_EXIT_SYS, "create file: ", file) ;
                close_fd(fd) ;
            }
        }

        log_info("Successfully ", pmanager->cmdmsg ? pmanager->cmdmsg : pmanager->operation ? "stopped" : "started", pmanager->cmdmsg ? "ed" : "", " service: ", name) ;

        svc_send_event(SVC_EVENT_CHILD_SUCCESS, id) ;

    } else {

        if (svc->res->type == E_PARSER_TYPE_CLASSIC) {

            fd = io_open_mode(file, O_WRONLY | O_NONBLOCK | O_TRUNC | O_CREAT, 0666) ;
            if (fd < 0)
                log_dieusys(LOG_EXIT_SYS, "create file: ", scandir) ;
            close_fd(fd) ;
        }

        flog_1_warnu("%s service: %s -- exited with signal: %u", pmanager->cmdmsg ? pmanager->cmdmsg : pmanager->operation ? "stop" : "start",  name, svc->exitcode) ;

        svc_send_event(SVC_EVENT_CHILD_FAILED, id) ;
    }
}

static void svc_wait_teardown(void *data)
{
    log_flow() ;

    uint32_t id = (uint32_t)(uintptr_t)data ;
    svc_ctx_t *svc = &pmanager->asvc[id] ;

    if (svc->fifo.fifopath[0])
        event_fifo_unsubscribe(&svc->fifo) ;

    if (svc->timeout.fd > 0)
        sse_free_timer(&svc->timeout) ;
}

static void complete(uint32_t id, bool success)
{
    log_flow() ;

    svc_ctx_t *svc = &pmanager->asvc[id] ;

    if (svc->done)
        return ;

    svc->done = true ;
    announce(id, success) ;

    if (!sse_defer(&pmanager->loop, &svc_wait_teardown, (void *)(uintptr_t)id))
        log_warnusys("defer wait teardown for service: ", svc->res->sa.s + svc->res->name) ;
}

static void svc_wait_handler(event_reader_t *r, char const *buf, size_t len, void *data)
{
    log_flow() ;

    (void)r ;

    uint32_t id = (uint32_t)(uintptr_t)data ;
    svc_ctx_t *svc = &pmanager->asvc[id] ;

    if (svc->done)
        return ;

    int verdict = event_match_feed(&svc->match, buf, len) ;
    if (verdict == EVENT_MATCH_OK)
        complete(id, true) ;
    else if (verdict == EVENT_MATCH_FAIL)
        complete(id, false) ;
}

static void wait_timeout_cb(sse_watcher_t *w, void *cbdata, int event)
{
    log_flow() ;

    (void)event ;

    uint32_t id = (uint32_t)(uintptr_t)cbdata ;
    svc_ctx_t *svc = &pmanager->asvc[id] ;

    if (w->api_errno != 0)
        log_warn("timeout watcher error: ", strerror(w->api_errno)) ;

    if (svc->done)
        return ;

    log_warn("transition timeout for service: ", svc->res->sa.s + svc->res->name) ;
    complete(id, false) ;
}

/** Native CLASSIC launch: send the control command over supervise/control and,
 * when a wait was requested (-w), watch the event fifodir for the transition.
 * Owns its service's completion: it always returns 1 and reports success/failure
 * through complete()/announce() (so npid is decremented once, in notifier_cb). */
static int launch_classic(uint32_t id)
{
    log_flow() ;

    svc_ctx_t *svc = &pmanager->asvc[id] ;
    char *scandir = svc->res->sa.s + svc->res->live.scandir ;

    svc->native = true ;

    if (pmanager->woption) {

        char *eventdir = svc->res->sa.s + svc->res->live.eventdir ;

        // map the wait char to a wanted state (the s6 -w alphabet)
        event_t wanted ;
        switch (pmanager->wsignal[2]) {
            case 'u' : wanted = EVENT_UP ; break ;
            case 'U' : wanted = EVENT_READY ; break ;
            case 'd' : wanted = EVENT_DOWN ; break ;
            case 'D' : wanted = EVENT_DOWN_READY ; break ;
            case 'r' : wanted = EVENT_RESTART ; break ;
            case 'R' : wanted = EVENT_RESTART_READY ; break ;
            default :  wanted = EVENT_UP ; break ;
        }

        if (!svc->res->notify) {
            if (wanted == EVENT_READY) wanted = EVENT_UP ;
            else if (wanted == EVENT_DOWN_READY) wanted = EVENT_DOWN ;
            else if (wanted == EVENT_RESTART_READY) wanted = EVENT_RESTART ;
        }

        event_match_init(&svc->match, wanted, pmanager->operation ? 1 : 0, 0) ;

        // create the fifodir if missing, then subscribe BEFORE sending the command
        // so no transition is missed (mirrors s6-svlisten ordering)
        if (!event_fifodir_make(eventdir, getgid())) {
            log_warnusys("create event fifodir: ", eventdir) ;
            complete(id, false) ;
            return 1 ;
        }

        if (!event_fifo_subscribe(&svc->fifo, &pmanager->loop, eventdir, &svc_wait_handler, (void *)(uintptr_t)id, 0)) {
            log_warnusys("subscribe to event fifo: ", eventdir) ;
            complete(id, false) ;
            return 1 ;
        }

        uint64_t timeout = !pmanager->operation ? svc->res->execute.timeout.start : svc->res->execute.timeout.stop ;
        if (timeout) {
            if (!sse_start_timer(&pmanager->loop, &svc->timeout, wait_timeout_cb, (void *)(uintptr_t)id, timeout, 0, 1)) {
                log_warnusys("start timeout watcher for service: ", svc->res->sa.s + svc->res->name) ;
                complete(id, false) ;
                return 1 ;
            }
        }
    }

    log_trace("sending ", pmanager->signal + 1, " to: ", scandir) ;
    if (!svc_control_send(scandir, pmanager->signal + 1, strlen(pmanager->signal) - 1, STATUS_WHO_USER)) {
        complete(id, false) ;
        return 1 ;
    }

    if (!pmanager->woption)
        // fire-and-forget command: nothing to wait for, complete immediately
        complete(id, true) ;

    return 1 ;
}

static int launch_service(uint32_t id)
{
    log_flow() ;

    svc_ctx_t *svc = &pmanager->asvc[id] ;

    uint8_t type = svc->res->type ;

    if (type == E_PARSER_TYPE_CLASSIC) {

        return launch_classic(id) ;

    } else if (type == E_PARSER_TYPE_ONESHOT) {

        char *servicedir = svc->res->sa.s + svc->res->live.servicedir ;
        char *oneshotdir = svc->res->sa.s + svc->res->live.oneshotddir ;
        char *scandir = svc->res->sa.s + svc->res->live.scandir ;
        char oneshot[strlen(oneshotdir) + 2 + 1] ;
        auto_strings(oneshot, oneshotdir, "/s") ;

        char *newargv[5] ;
        unsigned int m = 0 ;
        newargv[m++] = SS_LIBEXECPREFIX "66-oneshot" ;
        newargv[m++] = oneshot ;
        newargv[m++] = !pmanager->operation ? "up" : "down" ;
        newargv[m++] = servicedir ;
        newargv[m++] = 0 ;

        log_trace("sending ", !pmanager->operation ? "start" : "stop", " to: ", scandir) ;

        svc->pid = spawn_path(newargv[0], (char const *const *)newargv, (char const *const *)environ) ;
        if (!svc->pid) {
            FLAGS_SET(svc->state, SVC_FLAGS_FAILED) ;
            log_warnusys_return(LOG_EXIT_ZERO, "spawn service: ", svc->res->sa.s + svc->res->name) ;
        }

    } else if (type == E_PARSER_TYPE_MODULE) {

        int r = svc_compute_ns(pmanager, id) ;
        announce(id, !r ? true : false) ;
        return r ? 0 : 1 ;
    }

    // Setup child watcher/
    if (!sse_start_child(&pmanager->loop, &svc->child, child_cb, (void *)(uintptr_t)id, svc->pid, 2, true)) {
        if (svc->pid)
            kill(svc->pid, SIGKILL);
        svc->state = SVC_FLAGS_FAILED ;
        log_warnusys_return(LOG_EXIT_ZERO, "start child watcher for service: ", svc->res->sa.s + svc->res->name) ;
    }

    // Setup timeout watcher if any
    uint64_t timeout = !pmanager->operation ? svc->res->execute.timeout.start : svc->res->execute.timeout.stop ;
    if (timeout) {
        if (!sse_start_timer(&pmanager->loop, &svc->timeout, timeout_cb, (void *)(uintptr_t)id, svc->res->execute.timeout.start, 0, 1))
            log_warnusys_return(LOG_EXIT_ZERO, "start timer watcher for service: ",  svc->res->sa.s + svc->res->name) ;
    }

    return 1 ;
}

// callback
static void signalfd_cb(sse_watcher_t *w, void *cbdata, int event)
{
    log_flow() ;

    (void)cbdata ;

    // Check for watcher errors first
    if (w->api_errno != 0) {
        log_warn("signalfd watcher error: ", strerror(w->api_errno)) ;
        sse_free_signal(w) ;
        pmanager->loop.running = false ;
        return ;
    }

    if (!(event & SSE_READ)) {
        log_warn("unexpected event on signalfd callback") ;
        sse_free_signal(w) ;
        pmanager->loop.running = false ;
        return ;
    }

    sse_signal_t *s = (sse_signal_t *)w->sdata;
    if (!s) {
        log_warn("signalfd sdata is NULL") ;
        sse_free_signal(w) ;
        pmanager->loop.running = false ;
        return ;
    }

    switch (s->si.ssi_signo) {

        case SIGTERM :
        case SIGKILL :
        case SIGINT :
            log_1_warn("received SIGTERM or SIGKILL or SIGINT, aborting transaction") ;
            svc_send_event(SVC_EVENT_SHUTDOWN_REQUEST, 0) ;
            break ;
        default :
            log_die(LOG_EXIT_SYS, "unexpected signal") ;
    }
}

static void deadline_cb(sse_watcher_t *w, void *cbdata, int event)
{
    log_flow() ;

    (void)cbdata ;
    (void)event ;

    // Check for watcher errors
    if (w->api_errno != 0) {
        log_warn("deadline watcher error: ", strerror(w->api_errno)) ;
        // Still trigger shutdown, then free
        svc_send_event(SVC_EVENT_SHUTDOWN_REQUEST, 0) ;
        sse_free_timer(w) ;
        return ;
    }

    log_warn("global deadline reached, shutting down") ;
    svc_send_event(SVC_EVENT_SHUTDOWN_REQUEST, 0) ;
    // one-shot timer
    sse_free_timer(w) ;
}

static void notifier_cb(sse_watcher_t *w, void *cbdata, int event)
{
    log_flow() ;

    (void)cbdata ;
    svc_event_msg_t msg ;
    ssize_t n ;

    // Check for watcher errors
    if (w->api_errno != 0) {
        log_warn("notifier watcher error: ", strerror(w->api_errno)) ;
        pmanager->loop.running = false ;
        // simply return and let cleanup handle it
        return ;
    }

    if (!(event & SSE_READ)) {
        log_warn("unexpected event on notifier callback") ;
        pmanager->loop.running = false ;
        return ;
    }

    while ((n = io_read(pmanager->notifd[0], (char *)&msg, sizeof(msg))) == sizeof(msg)) {

        if (msg.id >= pmanager->nsvc) {
            log_warn("invalid service id in notification") ;
            continue ;
        }

        svc_ctx_t *svc = &pmanager->asvc[msg.id] ;

        switch (msg.type) {

            case SVC_EVENT_CHILD_SUCCESS:
                /* native CLASSIC services have no child watcher: account the
                 * completion here (the child path decrements in child_cb) */
                if (svc->native)
                    npid-- ;
                svc->state = 0 ;
                FLAGS_SET(svc->state, !pmanager->operation ? SVC_FLAGS_UP : SVC_FLAGS_DOWN) ;
                wait_deps(msg.id) ;
                break ;

            case SVC_EVENT_CHILD_FAILED:
                if (svc->native)
                    npid-- ;
                svc->state = 0 ;
                FLAGS_SET(svc->state, SVC_FLAGS_FAILED) ;

                if (pmanager->propagate)
                    propagate_failure(msg.id) ;
                break ;

            case SVC_EVENT_TIMEOUT:
                svc->state = 0 ;
                FLAGS_SET(svc->state, SVC_FLAGS_TIMEOUT) ;
                break ;

            case SVC_EVENT_SHUTDOWN_REQUEST:
                pmanager->loop.running = false ;
                break ;

            default:
                log_warn("unexpected event type") ;
                break ;
        }

        // Check if we're done
        if (!npid)
            pmanager->loop.running = false ;
    }

    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        log_warnusys("read from notifier pipe");
        pmanager->loop.running = false;
    }
}

static void child_cb(sse_watcher_t *w, void *cbdata, int event)
{
    log_flow() ;

    uint32_t id = (uint32_t)(uintptr_t)cbdata;
    svc_ctx_t *svc = &pmanager->asvc[id];

    if (w->api_errno != 0) {
        log_warn("child watcher error: ", strerror(w->api_errno)) ;
        npid--;
        svc->state = SVC_FLAGS_FAILED ;
        announce(id, false) ;
        sse_free_child(w) ;
        return ;
    }

    if (!(event & SSE_READ)) {
        log_warn("unexpected event on child callback") ;
        return ;
    }

    if (event & (SSE_HUP | SSE_READ)) {

        sse_child_t *data = (sse_child_t *)w->sdata;
        if (!data) {
            log_warn("child watcher sdata is NULL") ;
            sse_free_child(w) ;
            return ;
        }

        int wstat = data->status ;
        npid-- ;

        svc->exitcode = WEXITSTATUS(wstat) ;

        bool success = !WIFSIGNALED(wstat) && !WEXITSTATUS(wstat) ;
        announce(id, success) ;

        if (svc->timeout.fd > 0)
            sse_free_timer(&svc->timeout) ;

        sse_free_child(&svc->child) ;
    }
}

static void timeout_cb(sse_watcher_t *w, void *cbdata, int event)
{
    log_flow() ;

    uint32_t id = (uint32_t)(uintptr_t)cbdata ;
    svc_ctx_t *svc = &pmanager->asvc[id] ;
    (void)event ;
    if (w->api_errno != 0) {
        log_warn("timeout watcher error: ", strerror(w->api_errno)) ;
        sse_free_timer(w) ;
        return ;
    }

    if (!svc && svc->pid > 0) {
        // Kill the service
        log_warn("service timeout, killing: ", svc->res->sa.s + svc->res->name) ;
        kill(svc->pid, SIGTERM) ;

        svc_send_event(SVC_EVENT_TIMEOUT, id) ;
    }

    // one-shot timer
    sse_free_timer(w) ;
}

// main API
static int svc_manager_init(svc_ctx_t *asvc, uint32_t nsvc, uint8_t operation, ssexec_t *info, char const *wsignal, uint8_t woption, char const *signal, char *cmdmsg, uint8_t propagate)
{
    log_flow() ;

    if (!asvc || nsvc == 0) {
        errno = EINVAL ;
        log_dieusys(LOG_EXIT_SYS, "bad parameter") ;
    }

    /* Initialize manager structure */
    pmanager->asvc = asvc ;
    pmanager->nsvc = nsvc ;
    pmanager->shutdown_requested = false ;
    pmanager->info = info ;
    pmanager->timeout = (uint64_t)info->timeout ;
    pmanager->propagate = propagate ? true : false ;
    pmanager->operation = operation ;
    pmanager->woption = woption ;
    auto_strings(pmanager->wsignal, wsignal) ;
    auto_strings(pmanager->signal, signal) ;
    pmanager->cmdmsg = cmdmsg ;

    if (!sse_new(&pmanager->loop, nsvc))
        log_dieusys(LOG_EXIT_SYS, "initiate events loop") ;

    pmanager->loop.running = true ;

    // general signal
    if (!sse_start_signal(&pmanager->loop, &pmanager->signalfd, signalfd_cb, NULL, 0))
        log_dieusys(LOG_EXIT_SYS, "start signal watcher") ;

    if (!sse_attach_signal(&pmanager->signalfd, SIGINT))
        log_dieusys(LOG_EXIT_SYS, "block signal SIGINT") ;

    if (!sse_attach_signal(&pmanager->signalfd, SIGKILL))
        log_dieusys(LOG_EXIT_SYS, "block signal SIGKILL") ;

    if (!sse_attach_signal(&pmanager->signalfd, SIGTERM))
        log_dieusys(LOG_EXIT_SYS, "block signal SIGTERM") ;

    if (!sse_ignore_signal(&pmanager->signalfd, SIGPIPE))
        log_dieusys(LOG_EXIT_SYS, "ignore signal SIGPIPE") ;

    // Create notifier pipe
    if (pipe(pmanager->notifd) < 0)
        log_dieusys(LOG_EXIT_SYS, "create notifier pipe") ;

    // Set pipes non-blocking
    if (!io_set_nonblock(pmanager->notifd[0]) || !io_set_nonblock(pmanager->notifd[1]))
        log_dieusys(LOG_EXIT_SYS, "set none blocking notifier pipe") ;

    // Setup notifier pipe watcher
    if (!sse_start_io(&pmanager->loop, &pmanager->notifier, notifier_cb, NULL, pmanager->notifd[0], SSE_READ, 0))
        log_dieusys(LOG_EXIT_SYS, "start notifier watcher") ;

    // Setup global deadline timer if timeout specified
    if (pmanager->timeout) {
        if (!sse_start_timer(&pmanager->loop, &pmanager->deadline, deadline_cb, NULL, pmanager->timeout, 0, 0))
            log_dieusys(LOG_EXIT_SYS, "start deadline watcher") ;
    }

    return 1;
}

static int svc_manager_start(void)
{
    log_flow() ;

    uint32_t pos = 0 ;

    for (; pos < pmanager->nsvc ; pos++) {

        svc_ctx_t *svc = &pmanager->asvc[pos] ;

        FLAGS_SET(svc->target_state, SVC_FLAGS_UP) ;

        if (FLAGS_ISSET(svc->state, SVC_FLAGS_UP)) {
            log_warn("skipping already up service: ", svc->res->sa.s + svc->res->name) ;
            continue ;
        }

        // Skip if service is already running
        if (FLAGS_ISSET(svc->state, SVC_FLAGS_STARTING | SVC_FLAGS_PROCESSING)) {
            log_warn("skipping already processing service: ", svc->res->sa.s + svc->res->name) ;
            continue ;
        }

        npid++ ;

        // Check if we can start this service now
        if (deps_satisfied(pos)) {

            svc->state = SVC_FLAGS_STARTING ;
            log_trace("initiate start process for service: ", svc->res->sa.s + svc->res->name) ;

            if (!launch_service(pos)) {

                log_warn("failed to start service: ", svc->res->sa.s + svc->res->name);
                svc->state = SVC_FLAGS_FAILED ;
                if (svc->pid)
                    kill(svc->pid, SIGKILL) ;
                svc->pid = 0 ;

                // Propagate failure to dependents if enabled
                if (pmanager->propagate)
                    propagate_failure(pos) ;
            }

        } else {
            // Mark as waiting for dependencies
            svc->state = SVC_FLAGS_WAITING_DEPS ;
            log_trace("service waiting for dependencies: ", svc->res->sa.s + svc->res->name) ;
        }
    }

    return 1 ;
}

static int svc_manager_stop(void)
{
    log_flow() ;

    uint32_t pos = 0 ;

    for (; pos < pmanager->nsvc ; pos++) {

        svc_ctx_t *svc = &pmanager->asvc[pos] ;

        FLAGS_SET(svc->target_state, SVC_FLAGS_DOWN) ;

        if (FLAGS_ISSET(svc->state, SVC_FLAGS_DOWN)) {
            log_warn("skipping already down service: ", svc->res->sa.s + svc->res->name) ;
            continue ;
        }

        if (FLAGS_ISSET(svc->state, SVC_FLAGS_STOPPING | SVC_FLAGS_PROCESSING)) {
            log_warn("skipping already processing service: ", svc->res->sa.s + svc->res->name) ;
            continue ;
        }

        npid++ ;

        // Check if we can start this service now
        if (deps_satisfied(pos)) {

            svc->state = SVC_FLAGS_STOPPING ;
            log_trace("initiate stop process for service: ", svc->res->sa.s + svc->res->name) ;

            if (!launch_service(pos)) {

                log_warn("failed to stop service: ", svc->res->sa.s + svc->res->name) ;
                svc->state = SVC_FLAGS_FAILED ;
                if (svc->pid)
                    kill(svc->pid, SIGKILL) ;
                svc->pid = 0 ;
            }

        } else {
            // Mark as waiting for dependencies
            svc->state = SVC_FLAGS_WAITING_DEPS ;
            log_trace("service waiting for dependencies: ", svc->res->sa.s + svc->res->name) ;
        }
    }

    return 1 ;
}

static int svc_manager_run(void)
{
    log_flow() ;

    pmanager->loop.running = true ;

    while (pmanager->loop.running && npid) {

        if (!sse_run(&pmanager->loop, SSE_TIMEOUT_INFINITE))
            return 0 ;

        // Check if we are done
        if (!npid)
            pmanager->loop.running = false ;
    }

    return 1 ;
}

static void svc_manager_free(void)
{
    log_flow() ;

    pmanager->loop.running = false ;

    sse_free(&pmanager->loop) ;

    if (pmanager->notifd[0])
        close_fd(pmanager->notifd[0]) ;
    if (pmanager->notifd[1])
        close_fd(pmanager->notifd[1]) ;
}

int svc_launch(svc_ctx_t *asvc, uint32_t nsvc, uint8_t operation, ssexec_t *info, char const *wsignal, uint8_t woption, char const *signal, char *cmdmsg, uint8_t propagate)
{
    log_flow() ;

    svc_manager_t manager ;
    svc_manager_t *saved_manager = pmanager ;
    pmanager = &manager ;

    uint32_t vertex_to_asvc[SS_MAX_SERVICE] ;
    uint32_t *saved_v2svc = v2svc ;

    npid = 0 ;

    if (!svc_manager_init(asvc, nsvc, operation, info, wsignal, woption, signal, cmdmsg, propagate))
        log_dieusys(LOG_EXIT_SYS, "initiate manager") ;

    // table mapping for depends array
    for (uint32_t pos = 0 ; pos < nsvc ; pos++){

        uint32_t idx = asvc[pos].index ;
        if (idx >= SS_MAX_SERVICE)
            log_dieusys(LOG_EXIT_SYS, "build correspondence table for dependencies") ;

        vertex_to_asvc[idx] = pos ;
    }

    v2svc = vertex_to_asvc ;

    int result ;
    if (!operation) {
        result = svc_manager_start() ;
    } else {
        result = svc_manager_stop() ;
    }

    if (result) {
        result = svc_manager_run() ;
    }

    svc_manager_free() ;
    /** svc_compute_ns call svc_launch and ovewritte the
     * pmanager global pointer. Be sure to reassign to the
     * original one. */
    pmanager = saved_manager ;
    v2svc = saved_v2svc ;
    return !result ? 1 : 0 ;
}