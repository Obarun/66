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
 * except according to the terms contained in the LICENSE file.
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
#include <oblibs/string.h>
#include <oblibs/clock.h>
#include <oblibs/sse.h>
#include <oblibs/fd.h>
#include <oblibs/bits.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/state.h>
#include <66/status.h>
#include <66/enum_parser.h>
#include <66/svc.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/event.h>
#include <66/graph.h>
#include <66/selection.h>

typedef enum svc_event_type_e svc_event_type_t ;
enum svc_event_type_e
{
    SVC_EVENT_CHILD_SUCCESS,
    SVC_EVENT_CHILD_FAILED,
    SVC_EVENT_SHUTDOWN_REQUEST
} ;

typedef struct svc_event_msg_s svc_event_msg_t ;
struct svc_event_msg_s {
    svc_event_type_t type ;
    uint32_t id ;
    uint64_t timestamp ;
} ;

typedef struct launch_s launch_t ;
struct launch_s
{
    sse_epoll_t loop ;
    sse_watcher_t signalfd ;
    sse_watcher_t notifier ;
    sse_watcher_t deadline ;
    int notifd[2] ;

    svc_ctx_t *asvc ;
    uint32_t nsvc ;
    svc_request_t const *req ;
    selection_service_t ordered ;
    uint32_t *v2svc ;
    char const **passage ; // message of reason to not send anything to a member
    uint32_t npid ;
    uint32_t unordered ; // members out of the order not done yet: in a descent, the others wait for them

    bool failed ;
    int syserr ;
} ;

static launch_t *launch ;

static int launch_service(uint32_t id) ;
static int launch_classic(uint32_t id) ;
static int launch_oneshot(uint32_t id) ;
static void svc_wait_handler(event_reader_t *r, char const *buf, size_t len, void *data) ;
static void complete(uint32_t id, bool success) ;
static void svc_oneshot_result(void *data, uint8_t status, uint32_t wstat) ;
static void svc_oneshot_teardown(void *data) ;

static char const *name_of(svc_ctx_t const *svc)
{
    return svc->h->res.sa.s + svc->h->res.name ;
}

static resolve_service_t const *res_of(svc_ctx_t const *svc)
{
    return &svc->h->res ;
}

static void fail_sys(void)
{
    if (!launch->syserr)
        launch->syserr = errno ? errno : EIO ;

    launch->failed = true ;
    launch->loop.running = false ;
}

static char const *action_word(bool past)
{
    log_flow() ;

    if (launch->req->verb)
        return launch->req->verb ;

    switch (launch->req->action) {

        case SVC_ACTION_START :
            return past ? "started" : "start" ;

        case SVC_ACTION_STOP :
            return past ? "stopped" : "stop" ;

        default :
            return past ? "signaled" : "signal" ;
    }
}


static uint32_t get_asvc_id(uint32_t index)
{
    log_flow() ;
    return launch->v2svc[index] ;
}

static uint32_t lot_edges(uint32_t id, bool successors, uint32_t *list)
{
    log_flow() ;

    bool b = launch->req->action == SVC_ACTION_START ? successors : !successors ;
    return selection_service_edges(&launch->ordered, launch->asvc[id].index, list, b) ;
}

static bool deps_satisfied(uint32_t id)
{
    log_flow() ;

    svc_ctx_t *svc = &launch->asvc[id] ;
    int flag = launch->req->action == SVC_ACTION_STOP ? SVC_FLAGS_DOWN : SVC_FLAGS_UP ;

    // a member of the order waits until the members out of it are done
    if (launch->unordered && bitset32_isvalid(&launch->ordered.member, svc->index))
        return false ;

    if (!svc->ndepends)
        return true ;

    uint32_t edges[launch->ordered.member.count] ;
    uint32_t n = lot_edges(id, false, edges) ;

    for (uint32_t pos = 0 ; pos < n ; pos++) {

        uint32_t did = get_asvc_id(edges[pos]) ;
        svc_ctx_t *dep = &launch->asvc[did] ;

        if (!FLAGS_ISSET(dep->state, flag))
            return false ;
    }

    return true ;
}

static void wait_deps(uint32_t id)
{
    log_flow() ;

    svc_ctx_t *svc = &launch->asvc[id] ;

    if (!svc->nrequiredby)
        return ;

    uint32_t edges[launch->ordered.member.count] ;
    uint32_t n = lot_edges(id, true, edges) ;

    for (uint32_t pos = 0 ; pos < n ; pos++) {

        uint32_t did = get_asvc_id(edges[pos]) ;
        svc_ctx_t *dep = &launch->asvc[did] ;

        if (dep->state == SVC_FLAGS_WAITING_DEPS && deps_satisfied(did)) {

            dep->state = launch->req->action == SVC_ACTION_STOP ? SVC_FLAGS_STOPPING : SVC_FLAGS_STARTING ;
            launch_service(did) ;
        }
    }
}

static void unordered_done(uint32_t id)
{
    log_flow() ;

    svc_ctx_t *svc = &launch->asvc[id] ;

    if (!launch->unordered || bitset32_isvalid(&launch->ordered.member, svc->index))
        return ;

    if (--launch->unordered)
        return ;

    for (uint32_t pos = 0 ; pos < launch->nsvc ; pos++) {

        svc_ctx_t *dep = &launch->asvc[pos] ;

        if (dep->state == SVC_FLAGS_WAITING_DEPS && deps_satisfied(pos)) {

            dep->state = launch->req->action == SVC_ACTION_STOP ? SVC_FLAGS_STOPPING : SVC_FLAGS_STARTING ;
            launch_service(pos) ;
        }
    }
}

static void propagate_failure(uint32_t id)
{
    log_flow() ;

    svc_ctx_t *svc = &launch->asvc[id] ;

    if (!svc->nrequiredby)
        return ;

    uint32_t edges[launch->ordered.member.count] ;
    uint32_t n = lot_edges(id, true, edges) ;

    for (uint32_t pos = 0 ; pos < n ; pos++) {

        uint32_t did = get_asvc_id(edges[pos]) ;
        svc_ctx_t *dep = &launch->asvc[did] ;

        if (!FLAGS_ISSET(dep->state, SVC_FLAGS_WAITING_DEPS))
            continue ;

        log_warnu(action_word(false), " service: ", name_of(dep), " -- a service it waits for failed") ;

        dep->state = 0 ;
        FLAGS_SET(dep->state, SVC_FLAGS_FAILED) ;

        launch->failed = true ;
        launch->npid-- ;

        propagate_failure(did) ;
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

    ssize_t written = io_write(launch->notifd[1], (char *)&msg, sizeof(msg)) ;
    return (written == sizeof(msg)) ? 1 : 0 ;
}

static void svc_runtime_write(svc_ctx_t *svc, bool success, bool armed)
{
    log_flow() ;

    service_status_t st = STATUS_ZERO ;
    resolve_service_t const *res = res_of(svc) ;

    char const *supervisedir = res->sa.s + res->live.supervisedir ;
    char file[strlen(supervisedir) + 1 + SS_STATUS_LEN + 1] ;
    auto_strings(file, supervisedir, "/", SS_STATUS) ;

    if (success) {
        st.state = armed ? STATUS_STATE_WAITING : (launch->req->action == SVC_ACTION_STOP ? STATUS_STATE_DOWN : STATUS_STATE_DONE) ;
        st.result = STATUS_RESULT_SUCCESS ;
    } else {
        st.state = STATUS_STATE_FAILED ;
        st.result = STATUS_RESULT_EXITED ;
        // the exit code only, as 66-oneshotd writes it: 0 for a script killed by a signal
        st.code = svc->result == STATUS_RESULT_EXITED ? svc->code : 0 ;
    }

    st.who = launch->req->who ;
    clock_now(&st.stamp) ;

    if (!svc->waiting && success && svc->target == SVC_TARGET_UP) {

        clock_now(&st.readystamp) ;

    } else if (armed) {

        service_status_t old = STATUS_ZERO ;

        if (svc_status(res, &old) < 0)
            log_warnusys("read runtime status of: ", name_of(svc)) ;

        st.readystamp = old.readystamp ;
    }

    if (!status_write(&st, file))
        log_warnusys("write runtime status of: ", name_of(svc)) ;
}

// state = true > success
static void announce(uint32_t id, bool success)
{
    log_flow() ;

    int fd ;
    svc_ctx_t *svc = &launch->asvc[id] ;
    resolve_service_t const *res = res_of(svc) ;
    char const *name = name_of(svc) ;
    char const *scandir = res->sa.s + res->live.scandir ;
    size_t scandirlen = strlen(scandir) ;
    char file[scandirlen +  6] ;

    auto_strings(file, scandir, "/down") ;

    bool armed = svc->target == SVC_TARGET_UP &&
                res->has_event &&
                (res->type == E_PARSER_TYPE_ONESHOT || res->type == E_PARSER_TYPE_MODULE) &&
                svc_reactor_armed_idle(svc->h) > 0 ;

    if ((armed || res->type == E_PARSER_TYPE_MODULE) && launch->req->action != SVC_ACTION_SIGNAL)
        svc_runtime_write(svc, success, armed) ;

    if (success) {

        if (!svc->h->execute.down && res->type == E_PARSER_TYPE_CLASSIC) {

            if (svc->target == SVC_TARGET_UP) {

                if (!access(scandir, F_OK)) {
                    log_trace("delete down file: ", file) ;
                    if (unlink(file) < 0 && errno != ENOENT)
                        log_warnusys("delete down file: ", file) ;
                }

            } else if (svc_target_stops(svc->target)) {

                fd = io_open_mode(file, O_WRONLY | O_NONBLOCK | O_TRUNC | O_CREAT, 0666) ;

                if (fd < 0 && errno != ENOENT) {
                    log_warnusys("create file: ", file) ;
                    fail_sys() ;
                }
                close_fd(fd) ;
            }
        }

        if (svc->waiting)
            log_info("Armed reactor -- waiting for its event: ", name) ;
        else
            log_info("Successfully ", action_word(true), launch->req->verb ? "ed" : "", " service: ", name) ;

        svc->state = 0 ;
        FLAGS_SET(svc->state, launch->req->action == SVC_ACTION_STOP ? SVC_FLAGS_DOWN : SVC_FLAGS_UP) ;

        svc_send_event(SVC_EVENT_CHILD_SUCCESS, id) ;

    } else {

        if (res->type == E_PARSER_TYPE_CLASSIC) {

            fd = io_open_mode(file, O_WRONLY | O_NONBLOCK | O_TRUNC | O_CREAT, 0666) ;
            if (fd < 0) {
                log_warnusys("create file: ", file) ;
                fail_sys() ;
            }
            close_fd(fd) ;
        }

        switch (svc->result) {

            case STATUS_RESULT_EXITED :
                flog_warnu("%s service: %s -- exited with code %u", action_word(false), name, svc->code) ;
                break ;

            case STATUS_RESULT_SIGNALED :
                flog_warnu("%s service: %s -- killed by signal %u", action_word(false), name, svc->code) ;
                break ;

            case STATUS_RESULT_TIMEOUT_START :
                log_warnu(action_word(false), " service: ", name, " -- not ready within its start timeout") ;
                break ;

            case STATUS_RESULT_TIMEOUT_STOP :
                log_warnu(action_word(false), " service: ", name, " -- killed at its stop timeout") ;
                break ;

            default :
                log_warnu(action_word(false), " service: ", name) ;
                break ;
        }

        svc->state = 0 ;
        FLAGS_SET(svc->state, SVC_FLAGS_FAILED) ;

        if (launch->req->action == SVC_ACTION_STOP && svc->result == STATUS_RESULT_TIMEOUT_STOP)
            FLAGS_SET(svc->state, SVC_FLAGS_DOWN) ;

        launch->failed = true ;

        svc_send_event(SVC_EVENT_CHILD_FAILED, id) ;
    }
}

static void svc_wait_teardown(void *data)
{
    log_flow() ;

    uint32_t id = (uint32_t)(uintptr_t)data ;
    svc_ctx_t *svc = &launch->asvc[id] ;

    if (svc->fifo.fifopath[0])
        event_unsubscribe(&svc->fifo) ;
}

static void complete(uint32_t id, bool success)
{
    log_flow() ;

    svc_ctx_t *svc = &launch->asvc[id] ;

    if (svc->done)
        return ;

    svc->done = true ;
    announce(id, success) ;

    if (!sse_defer(&launch->loop, &svc_wait_teardown, (void *)(uintptr_t)id))
        log_warnusys("defer wait teardown for service: ", name_of(svc)) ;
}

static void svc_wait_on_frame(event_frame_t const *f, void *data)
{
    log_flow() ;

    uint32_t id = (uint32_t)(uintptr_t)data ;
    svc_ctx_t *svc = &launch->asvc[id] ;

    if (svc->done)
        return ;

    int verdict = event_state_update(&svc->match, f) ;
    bool transition = f->kind == EVENT_KIND_TRANSITION ;

    if (verdict == EVENT_STATE_OK) {

        if (launch->req->action == SVC_ACTION_STOP && transition && f->result == STATUS_RESULT_TIMEOUT_STOP) {
            svc->result = STATUS_RESULT_TIMEOUT_STOP ;
            complete(id, false) ;
        } else
            complete(id, true) ;

    } else if (verdict == EVENT_STATE_FAIL) {

        if (transition && f->result == STATUS_RESULT_TIMEOUT_START)
            svc->result = STATUS_RESULT_TIMEOUT_START ;

        complete(id, false) ;
    }
}

static void svc_wait_handler(event_reader_t *r, char const *buf, size_t len, void *data)
{
    log_flow() ;

    (void)r ;

    uint32_t id = (uint32_t)(uintptr_t)data ;
    svc_ctx_t *svc = &launch->asvc[id] ;

    if (svc->done)
        return ;

    event_aggregate(&svc->ag, buf, len, &svc_wait_on_frame, data) ;
}

static int launch_classic(uint32_t id)
{
    log_flow() ;

    svc_ctx_t *svc = &launch->asvc[id] ;
    resolve_service_t const *res = res_of(svc) ;
    char const *scandir = res->sa.s + res->live.scandir ;
    char const *signal = launch->req->signal ;
    char const *wsignal = launch->req->wsignal ;
    int seeded = EVENT_STATE_PENDING ;

    svc->native = true ;

    if (wsignal) {

        char const *eventdir = res->sa.s + res->live.eventdir ;

        event_t wanted ;
        switch (wsignal[2]) {
            case 'u' : wanted = EVENT_UP ; break ;
            case 'U' : wanted = EVENT_UP_READY ; break ;
            case 'd' : wanted = EVENT_DOWN ; break ;
            case 'D' : wanted = EVENT_DOWN_READY ; break ;
            case 'r' : wanted = EVENT_RESTART ; break ;
            case 'R' : wanted = EVENT_RESTART_READY ; break ;
            default :  wanted = EVENT_UP ; break ;
        }

        if (!svc->h->execute.notify) {
            if (wanted == EVENT_UP_READY) wanted = EVENT_UP ;
            else if (wanted == EVENT_RESTART_READY) wanted = EVENT_RESTART ;
        }

        event_state_init(&svc->match, wanted, launch->req->action == SVC_ACTION_STOP ? 1 : 0, 0) ;
        svc->ag.len = 0 ;

        if (!event_fifo_make(eventdir, getgid())) {
            log_warnusys("create event fifodir: ", eventdir) ;
            complete(id, false) ;
            return 1 ;
        }

        if (!event_subscribe(&svc->fifo, &launch->loop, eventdir, &svc_wait_handler, (void *)(uintptr_t)id, 0)) {
            log_warnusys("subscribe to event fifo: ", eventdir) ;
            complete(id, false) ;
            return 1 ;
        }

        service_status_t st = STATUS_ZERO ;
        int r = svc_status(res, &st) ;
        if (r < 0) {
            log_warnusys("read status of service: ", name_of(svc)) ;
            complete(id, false) ;
            return 1 ;
        }

        if (r) {
            event_frame_t f = { .kind = EVENT_KIND_TRANSITION, .state = st.state } ;
            seeded = event_state_update(&svc->match, &f) ;
        }

    }

    log_trace("sending ", signal + 1, " to: ", scandir) ;
    if (!svc_control_send(scandir, signal + 1, strlen(signal) - 1, launch->req->who)) {
        complete(id, false) ;
        return 1 ;
    }

    if (!wsignal || seeded == EVENT_STATE_OK)
        complete(id, true) ;

    return 1 ;
}

static int launch_oneshot(uint32_t id)
{
    log_flow() ;

    svc_ctx_t *svc = &launch->asvc[id] ;
    resolve_service_t const *res = res_of(svc) ;
    char const *name = name_of(svc) ;
    bool stop = launch->req->action == SVC_ACTION_STOP ;

    char const *servicedir = res->sa.s + res->live.servicedir ;
    char const *oneshotdir = res->sa.s + res->live.oneshotddir ;
    char oneshot[strlen(oneshotdir) + 2 + 1] ;
    auto_strings(oneshot, oneshotdir, "/s") ;

    log_trace("sending ", action_word(false), " to: ", oneshot) ;

    if (!oneshot_async_send(&svc->oneshot, &launch->loop, oneshot, stop, launch->req->who, servicedir, &svc_oneshot_result, (void *)(uintptr_t)id)) {
        log_warnusys("request oneshot daemon for service: ", name) ;
        launch->npid-- ;
        announce(id, false) ;
        return 1 ;
    }

    return 1 ;
}

static int launch_service(uint32_t id)
{
    log_flow() ;

    svc_ctx_t *svc = &launch->asvc[id] ;
    resolve_service_t const *res = res_of(svc) ;
    uint8_t type = res->type ;

    if (launch->passage[id]) {

        log_warn(launch->passage[id], name_of(svc)) ;

        launch->npid-- ;

        svc->state = 0 ;
        FLAGS_SET(svc->state, launch->req->action == SVC_ACTION_STOP ? SVC_FLAGS_DOWN : SVC_FLAGS_UP) ;
        wait_deps(id) ;
        unordered_done(id) ;

        return 1 ;
    }

    if (svc->target == SVC_TARGET_READY) {

        svc->target = SVC_TARGET_UP ;

        if (svc_reactor_armed_idle(svc->h) > 0) {

            svc->native = true ;
            svc->waiting = true ;
            complete(id, true) ;
            return 1 ;
        }
    }

    switch (type) {

        case E_PARSER_TYPE_CLASSIC :

            return launch_classic(id) ;

        case E_PARSER_TYPE_ONESHOT :

            return launch_oneshot(id) ;

        case E_PARSER_TYPE_MODULE :

            svc->native = true ;
            complete(id, true) ;
            return 1 ;

        case E_PARSER_TYPE_EVENT :
        {
            char const *eventddir = res->sa.s + res->live.eventddir ;

            if (launch->req->action == SVC_ACTION_START && !svcd_notify(eventddir, 'a', launch->req->who, name_of(svc)))
                log_warnusys("arm event source: ", name_of(svc)) ;

            svc->native = true ;
            complete(id, true) ;
            return 1 ;
        }

        default :

            // a type 66 does not parse: nothing to drive it with
            log_warn("unknown type of service: ", name_of(svc)) ;
            svc->native = true ;
            complete(id, false) ;
            return 1 ;
    }
}

static int passage_of(svc_ctx_t const *svc, char const **why)
{
    log_flow() ;

    resolve_service_t const *res = res_of(svc) ;
    uint8_t type = res->type, action = launch->req->action, target = launch->req->target ;
    int r ;

    *why = 0 ;

    if (!svc->supervised) {
        *why = "skipping service without a supervisor: " ;
        return 1 ;
    }

    if (type == E_PARSER_TYPE_CLASSIC)
        return 1 ;

    if ((action == SVC_ACTION_SIGNAL && type == E_PARSER_TYPE_ONESHOT) ||
        (type == E_PARSER_TYPE_EVENT && target == SVC_TARGET_KEEP)) {
        *why = "skipping service with no process to signal: " ;
        return 1 ;
    }

    r = svc_reactor_armed_idle(svc->h) ;
    if (r < 0)
        return -1 ;

    if (action == SVC_ACTION_START && FLAGS_ISSET(svc->state, SVC_FLAGS_UP) && !r) {
        *why = "skipping already up service: " ;
    } else if (action == SVC_ACTION_STOP && FLAGS_ISSET(svc->state, SVC_FLAGS_DOWN)) {
        *why = "skipping already down service: " ;
    // only free disarms an event source, and svc_unsupervise() is what does it
    } else if (action == SVC_ACTION_STOP && type == E_PARSER_TYPE_EVENT && target != SVC_TARGET_FREE) {
        *why = "keeping armed event source, only free disarms it: " ;
    }
    return 1 ;
}

static void signalfd_cb(sse_watcher_t *w, void *cbdata, int event)
{
    log_flow() ;

    (void)cbdata ;

    if (w->api_errno != 0) {
        log_warn("signalfd watcher error: ", strerror(w->api_errno)) ;
        launch->failed = true ;
        sse_free_signal(w) ;
        launch->loop.running = false ;
        return ;
    }

    if (!(event & SSE_READ)) {
        log_warn("unexpected event on signalfd callback") ;
        launch->failed = true ;
        sse_free_signal(w) ;
        launch->loop.running = false ;
        return ;
    }

    sse_signal_t *s = (sse_signal_t *)w->sdata;
    if (!s) {
        log_warn("signalfd sdata is NULL") ;
        launch->failed = true ;
        sse_free_signal(w) ;
        launch->loop.running = false ;
        return ;
    }

    switch (s->si.ssi_signo) {

        case SIGTERM :
        case SIGKILL :
        case SIGINT :
            log_warn("received SIGTERM or SIGKILL or SIGINT, aborting transaction") ;
            break ;
        default :
            log_warn("unexpected signal, aborting transaction") ;
            break ;
    }

    launch->failed = true ;
    svc_send_event(SVC_EVENT_SHUTDOWN_REQUEST, 0) ;
}

static void deadline_cb(sse_watcher_t *w, void *cbdata, int event)
{
    log_flow() ;

    (void)cbdata ;
    (void)event ;

    launch->failed = true ;

    if (w->api_errno != 0)
        log_warn("deadline watcher error: ", strerror(w->api_errno)) ;
    else
        log_warnu("reach the target of the service selection within the given timeout") ;

    svc_send_event(SVC_EVENT_SHUTDOWN_REQUEST, 0) ;

    sse_free_timer(w) ;
}

static void notifier_cb(sse_watcher_t *w, void *cbdata, int event)
{
    log_flow() ;

    (void)cbdata ;
    svc_event_msg_t msg ;
    ssize_t n ;

    if (w->api_errno != 0) {
        log_warn("notifier watcher error: ", strerror(w->api_errno)) ;
        errno = w->api_errno ;
        fail_sys() ;
        return ;
    }

    if (!(event & SSE_READ)) {
        log_warn("unexpected event on notifier callback") ;
        errno = EIO ;
        fail_sys() ;
        return ;
    }

    while ((n = io_read(launch->notifd[0], (char *)&msg, sizeof(msg))) == sizeof(msg)) {

        if (msg.id >= launch->nsvc) {
            log_warn("invalid service id in notification") ;
            continue ;
        }

        svc_ctx_t *svc = &launch->asvc[msg.id] ;

        switch (msg.type) {

            case SVC_EVENT_CHILD_SUCCESS:
                if (svc->native)
                    launch->npid-- ;
                svc->state = 0 ;
                FLAGS_SET(svc->state, launch->req->action == SVC_ACTION_STOP ? SVC_FLAGS_DOWN : SVC_FLAGS_UP) ;
                wait_deps(msg.id) ;
                unordered_done(msg.id) ;
                break ;

            case SVC_EVENT_CHILD_FAILED:
                if (svc->native)
                    launch->npid-- ;
                svc->state = 0 ;
                FLAGS_SET(svc->state, SVC_FLAGS_FAILED) ;

                if (launch->req->action == SVC_ACTION_STOP && svc->result == STATUS_RESULT_TIMEOUT_STOP) {
                    FLAGS_SET(svc->state, SVC_FLAGS_DOWN) ;
                    wait_deps(msg.id) ;
                    unordered_done(msg.id) ;
                    break ;
                }

                /** -P narrows the selection, never the propagation of a failure:
                 * a waiter nobody releases makes the command hang on a count
                 * that never reaches zero */
                propagate_failure(msg.id) ;
                unordered_done(msg.id) ;
                break ;

            case SVC_EVENT_SHUTDOWN_REQUEST:
                launch->loop.running = false ;
                break ;

            default:
                log_warn("unexpected event type") ;
                break ;
        }

        if (!launch->npid)
            launch->loop.running = false ;
    }

    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        log_warnusys("read from notifier pipe") ;
        fail_sys() ;
    }
}

static void svc_oneshot_teardown(void *data)
{
    log_flow() ;

    uint32_t id = (uint32_t)(uintptr_t)data ;
    svc_ctx_t *svc = &launch->asvc[id] ;

    oneshot_async_end(&svc->oneshot) ;
}

static void svc_oneshot_result(void *data, uint8_t status, uint32_t wstat)
{
    log_flow() ;

    uint32_t id = (uint32_t)(uintptr_t)data ;
    svc_ctx_t *svc = &launch->asvc[id] ;

    launch->npid-- ;

    if (status == ONESHOT_OK) {

        if (WIFSIGNALED(wstat)) {

            service_status_t st = STATUS_ZERO ;

            svc->result = STATUS_RESULT_SIGNALED ;
            svc->code = (uint32_t)WTERMSIG(wstat) ;

            if (svc_status(res_of(svc), &st) < 0) {

                log_warnusys("read the status of service: ", name_of(svc)) ;

            } else if (st.result == STATUS_RESULT_TIMEOUT_START || st.result == STATUS_RESULT_TIMEOUT_STOP) {

                svc->result = st.result ;
            }

        } else if (WEXITSTATUS(wstat)) {

            svc->result = STATUS_RESULT_EXITED ;
            svc->code = (uint32_t)WEXITSTATUS(wstat) ;
        }

        announce(id, svc->result == STATUS_RESULT_SUCCESS) ;

    } else {
        announce(id, false) ;
    }

    if (!sse_defer(&launch->loop, &svc_oneshot_teardown, data))
        log_warnusys("defer oneshot teardown for service: ", name_of(svc)) ;
}

static void launch_free(void)
{
    log_flow() ;

    launch->loop.running = false ;

    sse_free(&launch->loop) ;

    if (launch->notifd[0] >= 0)
        close_fd(launch->notifd[0]) ;

    if (launch->notifd[1] >= 0)
        close_fd(launch->notifd[1]) ;
}


static int launch_init(void)
{
    log_flow() ;

    if (!sse_new(&launch->loop, launch->nsvc))
        return -1 ;

    launch->loop.running = true ;

    if (!sse_start_signal(&launch->loop, &launch->signalfd, signalfd_cb, NULL, 0)
        || !sse_attach_signal(&launch->signalfd, SIGINT)
        || !sse_attach_signal(&launch->signalfd, SIGKILL)
        || !sse_attach_signal(&launch->signalfd, SIGTERM)
        || !sse_ignore_signal(&launch->signalfd, SIGPIPE))
            goto err ;

    if (pipe(launch->notifd) < 0) {
        launch->notifd[0] = launch->notifd[1] = -1 ;
        goto err ;
    }

    if (!io_set_nonblock(launch->notifd[0]) || !io_set_nonblock(launch->notifd[1]))
        goto err ;

    if (!sse_start_io(&launch->loop, &launch->notifier, notifier_cb, NULL, launch->notifd[0], SSE_READ, 0))
        goto err ;

    if (launch->req->timeout && !sse_start_timer(&launch->loop, &launch->deadline, deadline_cb, NULL, launch->req->timeout, 0, 0))
        goto err ;

    return 1 ;

    err:
    {
        int e = errno ;
        launch_free() ;
        errno = e ;
        return -1 ;
    }
}

static void launch_dispatch(void)
{
    log_flow() ;

    for (uint32_t pos = 0 ; pos < launch->nsvc ; pos++) {

        svc_ctx_t *svc = &launch->asvc[pos] ;

        launch->npid++ ;

        if (deps_satisfied(pos)) {

            svc->state = launch->req->action == SVC_ACTION_STOP ? SVC_FLAGS_STOPPING : SVC_FLAGS_STARTING ;
            log_trace("initiate ", action_word(false), " process for service: ", name_of(svc)) ;

            launch_service(pos) ;

        } else {
            svc->state = SVC_FLAGS_WAITING_DEPS ;
            log_trace("service waiting for dependencies: ", name_of(svc)) ;
        }
    }
}

static int launch_run(void)
{
    log_flow() ;

    launch->loop.running = true ;

    while (launch->loop.running && launch->npid) {

        if (!sse_run(&launch->loop, SSE_TIMEOUT_INFINITE))
            return -1 ;

        if (!launch->npid)
            launch->loop.running = false ;
    }

    return 1 ;
}

int svc_launch(svc_ctx_t *asvc, selection_service_t const *lot, svc_request_t const *req)
{
    log_flow() ;

    uint32_t n = lot->member.count, nvertex = service_graph_nvertex(lot->g) ;
    uint32_t v2svc[nvertex + 1] ;
    char const *passage[n + 1] ;
    launch_t l = { .notifd = { -1, -1 }, .asvc = asvc, .nsvc = n, .req = req, .ordered = SELECTION_SERVICE_ZERO, .v2svc = v2svc, .passage = passage, .npid = 0, .unordered = 0, .failed = false, .syserr = 0 } ;
    int r ;

    if (!n || !req->signal || req->target == SVC_TARGET_NONE)
        return (errno = EINVAL, -1) ;

    launch = &l ;

    selection_service_ordered(&l.ordered, lot) ;
    l.unordered = req->action == SVC_ACTION_START ? 0 : lot->member.count - l.ordered.member.count ;

    for (uint32_t pos = 0 ; pos < n ; pos++) {

        asvc[pos].target = req->target ;
        v2svc[asvc[pos].index] = pos ;

        if (passage_of(&asvc[pos], &passage[pos]) < 0) {
            launch = 0 ;
            return -1 ;
        }
    }

    if (launch_init() < 0) {
        launch = 0 ;
        return -1 ;
    }

    launch_dispatch() ;

    r = l.syserr ? -1 : launch_run() ;

    // the error of the loop itself, kept across the release of the loop
    int e = errno ;

    launch_free() ;

    if (l.syserr) {
        errno = l.syserr ;
        r = -1 ;
    } else if (r < 0)
        errno = e ;
    else
        r = l.failed ? 0 : 1 ;

    launch = 0 ;

    return r ;
}
