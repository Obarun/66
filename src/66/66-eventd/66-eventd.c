/*
 * 66-eventd.c
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

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <dirent.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/opt.h>
#include <oblibs/sse.h>
#include <oblibs/sse_stream.h>
#include <oblibs/socket.h>
#include <oblibs/hash.h>
#include <oblibs/sbl.h>
#include <oblibs/io.h>
#include <oblibs/fd.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/clock.h>
#include <oblibs/process.h>

#include <66/event.h>
#include <66/event_rule.h>
#include <66/resolve.h>
#include <66/enum_parser.h>
#include <66/status.h>
#include <66/svc.h>
#include <66/constants.h>
#include <66/utils.h>

#include "66/config.h"
#include "lib/eventd.h"

#define EVENTD_SOCKET SS_EVENTD "/s"
#define EVENTD_MAXCLIENTS 64

#define EVENTD_MAX_TRIGGER 10
#define EVENTD_INTERVAL_MS 10000

typedef struct eventd_reactor_s eventd_reactor_t ;
struct eventd_reactor_s
{
    char name[SS_MAX_SERVICE_NAME] ; // hash key
    resolve_service_addon_event_t rule ;
    sse_watcher_t child ; // valid iff in_flight
    pid_t pid ; // action child pid ; 0 when idle
    uint8_t in_flight ; // own-reap latch: set at fork, cleared at reap
    uint8_t pending_free ; // a disarm arrived while in_flight: destroy at reap
    uint64_t fires[EVENTD_MAX_TRIGGER] ; // in-window firing timestamps (ms), oldest-first
    uint8_t nfires ; // count of in-window firings (sliding-window backstop)
    hash_node_t node ;
} ;

typedef struct eventd_source_s eventd_source_t ;
struct eventd_source_s
{
    char name[SS_MAX_SERVICE_NAME] ; // hash key
    event_fifo_t fifo ; // fifodir subscription ; used iff subscribed ; address must stay stable
    event_aggregator_t ag ;
    uint8_t type ; // event_source_t
    uint8_t subscribed ; // fifodir effectively subscribed
    uint32_t refcount ; // number of armed reactors referencing this source
    hash_node_t node ;
} ;

typedef struct eventd_client_s eventd_client_t ;
struct eventd_client_s
{
    int fd ;
    sse_watcher_t w ;
    size_t blen ; // bytes buffered
    hash_node_t node ;
    char buf[2 + SS_MAX_SERVICE_NAME + 1] ; // <verb:1><who:1><name>
} ;

typedef struct eventd_s eventd_t ;
struct eventd_s
{
    sse_epoll_t epoll ;
    sse_watcher_t wsignal ;
    sse_watcher_t wserver ; // control socket accept watcher
    int sfd ;               // control socket listening fd ; owned by wserver
    uid_t owner ;           // scandir owner ; only it may talk to the socket
    hash_t sources ;        // eventd_source_t*
    hash_t reactors ;       // eventd_reactor_t*
    hash_t clients ;          // eventd_client_t*
    strbuf emitq ;          // sbl of pending emit names (flattened emit recursion)
} ;

static eventd_t eventd = {
    .epoll = SSE_EPOLL_ZERO,
    .wsignal = SSE_WATCHER_ZERO,
    .wserver = SSE_WATCHER_ZERO,
    .sfd = -1,
    .owner = -1,
    .sources = HASH_ZERO,
    .reactors = HASH_ZERO,
    .clients = HASH_ZERO,
    .emitq = SBL_ZERO
} ;

static char sysdir[SS_MAX_PATH_LEN + 1] ;

static void reactor_run(char const *source, event_frame_t const *f) ;
static void reactor_reap_cb(sse_watcher_t *w, void *cbdata, int event) ;
static void reactor_destroy(eventd_reactor_t *re) ;
static void eventd_enqueue_emit(char const *name) ;
static void eventd_drain_emits(void) ;

static opt_t const opts[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",    .arg = OPT_NONE,                          .help = "print this help" },
    { .id = 'v',         .shortname = 'v', .longname = "verbosity", .arg = OPT_REQUIRED, .argname = "number", .help = "increase/decrease verbosity" },
    { .id = 'd',         .shortname = 'd', .longname = "notify",  .arg = OPT_REQUIRED, .argname = "number",     .help = "notify readiness on file descriptor fd (>= 3)" },
} ;

static opt_cmd_t const cmd = {
    .name = "66-eventd",
    .operands = "scandir",
    .opts = opts,
    .nopts = OPT_COUNT(opts),
} ;

static void eventd_on_frame(event_frame_t const *f, void *data)
{
    eventd_source_t *s = data ;

    reactor_run(s->name, f) ;
    eventd_drain_emits() ;
}

static void eventd_event_handler(event_reader_t *r, char const *buf, size_t len, void *data)
{
    log_flow() ;

    (void)r ;

    eventd_source_t *s = data ;

    if (!len)
        return ; // source closed: never happens on a fifo (write end held)

    event_aggregate(&s->ag, buf, len, &eventd_on_frame, s) ;
}

static void source_destroy(eventd_source_t *s)
{
    hash_del(&eventd.sources, s) ;
    if (s->subscribed)
        event_unsubscribe(&s->fifo) ;
    free(s) ;
}

static void source_ref(char const *name, size_t len, uint8_t type)
{
    log_flow() ;

    eventd_source_t *s = hash_find(&eventd.sources, name, len) ;
    if (s) {
        s->refcount++ ;
        return ;
    }

    s = malloc(sizeof(*s) + len + 1) ;
    if (!s) {
        log_warnusys("allocate source: ", name) ;
        return ;
    }

    s->ag.len = 0 ; // the reassembler starts empty
    s->type = type ;
    s->subscribed = 0 ;
    s->refcount = 1 ;
    memcpy(s->name, name, len + 1) ;

    if (!hash_add(&eventd.sources, s->name, len, s)) {
        log_warnusys("register source: ", name) ;
        free(s) ;
        return ;
    }

    if (type == EVENT_SOURCE_SERVICE || type == EVENT_SOURCE_SIGNAL) {

        char ev[len + SS_EVENTDIR_LEN + 1] ;
        auto_strings(ev, s->name, SS_EVENTDIR) ;

        struct stat st ;
        if (stat(ev, &st) < 0 || !S_ISDIR(st.st_mode)) {
            log_warn("source not supervised yet, deferring subscription: ", s->name) ;
            return ;
        }

        if (!event_subscribe(&s->fifo, &eventd.epoll, ev, &eventd_event_handler, s, 0)) {
            log_warnusys("subscribe to event fifodir: ", ev) ;
            return ;
        }

        s->subscribed = 1 ;

    } else if (type != EVENT_SOURCE_USER) {

        log_warn("tick source not activated yet: ", s->name) ; // timer/schedule/inotify : T3
    }
}

static void source_unref(char const *name, size_t len)
{
    log_flow() ;

    eventd_source_t *s = hash_find(&eventd.sources, name, len) ;
    if (!s)
        return ;

    if (--s->refcount)
        return ;

    source_destroy(s) ;
}

static void reactor_destroy(eventd_reactor_t *re)
{
    hash_del(&eventd.reactors, re) ;
    if (re->in_flight)
        sse_free_child(&re->child) ;
    eventd_rule_free(&re->rule) ;
    free(re) ;
}

static char const *reactor_source_list(resolve_service_addon_event_t const *r, uint32_t *n)
{
    if (r->type == EVENT_SOURCE_USER) {
        *n = r->non ;
        return r->sa.s + r->on ;
    }
    *n = r->nfrom ;
    return r->sa.s + r->from ;
}

static void reactor_wire(eventd_reactor_t *re, int arm)
{
    log_flow() ;

    uint32_t n ;
    char const *p = reactor_source_list(&re->rule, &n) ;
    uint8_t type = re->rule.type ;

    for (uint32_t i = 0 ; i < n ; i++) {

        ssize_t got = get_len_until(p, ' ') ;
        size_t plen = got < 0 ? strlen(p) : (size_t)got ; // last entry has no trailing space

        char nm[plen + 1] ;
        memcpy(nm, p, plen) ;
        nm[plen] = 0 ;

        if (arm)
            source_ref(nm, plen, type) ;
        else
            source_unref(nm, plen) ;

        p += plen + 1 ;
    }
}

static int reactor_listens_to(eventd_reactor_t const *re, char const *source)
{
    uint32_t n ;
    char const *p = reactor_source_list(&re->rule, &n) ;
    size_t slen = strlen(source) ;

    for (uint32_t i = 0 ; i < n ; i++) {

        ssize_t got = get_len_until(p, ' ') ;
        size_t plen = got < 0 ? strlen(p) : (size_t)got ;

        if (plen == slen && !memcmp(p, source, plen))
            return 1 ;

        p += plen + 1 ;
    }
    return 0 ;
}

// filter 1 -- On, conditional on source type
static int reactor_filter_on(eventd_reactor_t const *re, char const *source, event_frame_t const *f)
{
    if (re->rule.type == EVENT_SOURCE_SERVICE || re->rule.type == EVENT_SOURCE_SIGNAL)
        return f && eventd_rule_match(&re->rule, source, f) ;
    return 1 ;
}

// filter 2 -- reactor state x Do
static int reactor_state_allows(uint32_t state, uint32_t docmd)
{
    if (docmd == EVENT_DO_RECONFIGURE || docmd == EVENT_DO_FREE)
        return 1 ;

    switch (docmd) {

        case EVENT_DO_START :
            return state == STATUS_STATE_DOWN || state == STATUS_STATE_DONE
                || state == STATUS_STATE_FAILED || state == STATUS_STATE_WAITING ;

        case EVENT_DO_STOP :
        case EVENT_DO_RESTART :
        case EVENT_DO_RELOAD :
            return state == STATUS_STATE_UP ;

        default :
            return 0 ;
    }
}

static opt_cmd_t const *reactor_func(uint32_t docmd)
{
    switch (docmd) {
        case EVENT_DO_START :       return &cmd_start ;
        case EVENT_DO_STOP :        return &cmd_stop ;
        case EVENT_DO_RESTART :     return &cmd_restart ;
        case EVENT_DO_RELOAD :      return &cmd_reload ;
        case EVENT_DO_RECONFIGURE : return &cmd_reconfigure ;
        case EVENT_DO_FREE :        return &cmd_free ;
        default :                   return 0 ;
    }
}

static uint64_t eventd_now_ms(void)
{
    struct timespec ts ;
    clock_now_mono(&ts) ;
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000 ;
}

static int reactor_backstop(eventd_reactor_t *re)
{
    uint64_t now = eventd_now_ms() ;

    uint8_t k = 0 ;
    while (k < re->nfires && now - re->fires[k] >= EVENTD_INTERVAL_MS)
        k++ ;

    if (k) {
        memmove(re->fires, re->fires + k, (re->nfires - k) * sizeof(*re->fires)) ;
        re->nfires -= k ;
    }

    if (re->nfires >= EVENTD_MAX_TRIGGER)
        return 1 ;

    re->fires[re->nfires++] = now ;

    return 0 ;
}

static int reactor_check_backstop(eventd_reactor_t *re)
{
    if (!reactor_backstop(re))
        return 0 ;

    log_warn("reactor ", re->name, ": squelched -- rate limit exceeded, disarming") ;
    reactor_wire(re, 0) ;
    reactor_destroy(re) ;
    return 1 ;
}

static void reactor_act(eventd_reactor_t *re, char const *treename)
{
    log_flow() ;

    uint32_t docmd = re->rule.docmd ;
    opt_cmd_t const *cmd = reactor_func(docmd) ;
    char const *doname = event_do_to_string(docmd) ;

    if (!cmd)
        return ;

    // anti-loop
    if (reactor_check_backstop(re))
        return ;

    pid_t pid = fork() ;
    if (pid < 0) {
        log_warnusys("fork event action for reactor: ", re->name) ;
        return ;
    }

    if (!pid) {

        // dismantle the inherited context, and build a fresh one
        sse_free_signal(&eventd.wsignal) ;
        close(eventd.epoll.fd) ;
        close(eventd.sfd) ;

        ssexec_t info = SSEXEC_ZERO ;
        info.owner = eventd.owner ;
        info.ownerlen = uid_format(info.ownerstr, info.owner) ;
        info.ownerstr[info.ownerlen] = 0 ;

        if (!set_ownersysdir(&info.base, info.owner))
            log_dieusys(LOG_EXIT_SYS, "set owner system directory") ;

        if (!auto_strbuf(&info.treename, treename))
            log_die_nomem("strbuf") ;

        info.opt_tree = 1 ;

        set_info(&info) ;
        info.who = STATUS_WHO_EVENT ;

        // call the subcommand handler directly: argv is its operands (argv[0] is
        // the first positional), no options to parse -- no need for opt_dispatch
        char const *argv[] = { re->name, 0 } ;
        _exit(cmd->fn(1, argv, &info)) ;
    }

    re->pid = pid ;
    re->in_flight = 1 ;

    if (!sse_start_child(&eventd.epoll, &re->child, reactor_reap_cb, re, pid, 2, true)) {
        kill(pid, SIGKILL) ;
        int wstat ;
        process_wait(pid, &wstat) ;
        re->pid = 0 ;
        re->in_flight = 0 ;
        log_warnusys("watch event action child of reactor: ", re->name) ;
        return ;
    }

    log_info("reactor ", re->name, ": ", doname) ;
}

static int reactor_source_status(char const *svc, size_t svclen, event_frame_t *out, void *ctx)
{
    (void)ctx ;

    if (svclen >= SS_MAX_SERVICE_NAME)
        return 0 ;

    char name[SS_MAX_SERVICE_NAME] ;
    memcpy(name, svc, svclen) ;
    name[svclen] = 0 ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

    service_status_t st = STATUS_ZERO ;
    int ok = resolve_read(wres, sysdir, name) == 1 && svc_status(&res, &st) >= 0 ;

    resolve_free(wres) ;

    if (!ok)
        return 0 ;

    *out = (event_frame_t){ .kind = EVENT_KIND_TRANSITION, .state = st.state, .result = st.result, .code = st.code } ;
    return 1 ;
}

static void reactor_evaluate(eventd_reactor_t *re, char const *source, event_frame_t const *f)
{
    if (!reactor_filter_on(re, source, f))
        return ;

    // OnAll needed a T time
    if (!eventd_rule_onall(&re->rule, source, reactor_source_status, 0)) {
        log_info("reactor ", re->name, ": OnAll unmet across sources") ;
        return ;
    }

    uint32_t docmd = re->rule.docmd ;
    char const *doname = event_do_to_string(docmd) ;
    int has_emit = re->rule.emit != 0 ;

    if (!doname && !has_emit)
        return ; // neither Do nor Emit(should never happens)

    /** own-reap latch : a reactor whose action is in flight absorbs every
     * re-trigger until it reaps -- covers the status_read latency*/
    if (re->in_flight) {
        log_info("reactor ", re->name, ": absorbed (in flight)") ;
        return ;
    }

    // Emit-only reactor (Do=none)
    if (!doname) {

        if (reactor_check_backstop(re))
            return ;

        eventd_enqueue_emit(re->rule.sa.s + re->rule.emit) ;
        log_info("reactor ", re->name, ": emit ", re->rule.sa.s + re->rule.emit) ;
        return ;
    }

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    if (resolve_read(wres, sysdir, re->name) != 1) {
        resolve_free(wres) ;
        log_warnu("read resolve of reactor: ", re->name) ;
        return ;
    }

    service_status_t st = STATUS_ZERO ;
    int havest = svc_status(&res, &st) >= 0 ;

    int act = (docmd == EVENT_DO_RECONFIGURE || docmd == EVENT_DO_FREE) ? 1
            : !havest ? 0
            : reactor_state_allows(st.state, docmd) ;

    if (act)
        reactor_act(re, res.sa.s + res.treename) ;
    else
        log_info("reactor ", re->name, ": ", doname, " inhibited (state ", status_state_to_string(st.state), ")") ;

    resolve_free(wres) ;
}

static void reactor_run(char const *source, event_frame_t const *f)
{
    log_flow() ;

    eventd_reactor_t *re, *tmp ;

    HASH_FOREACH(&eventd.reactors, re, tmp) {

        if (reactor_listens_to(re, source))
            reactor_evaluate(re, source, f) ;
    }
}

static void reactor_update(eventd_reactor_t *re)
{
    if (re->rule.type != EVENT_SOURCE_SERVICE)
        return ;

    uint32_t n ;
    char const *p = reactor_source_list(&re->rule, &n) ;

    for (uint32_t i = 0 ; i < n ; i++) {

        ssize_t got = get_len_until(p, ' ') ;
        size_t plen = got < 0 ? strlen(p) : (size_t)got ;

        event_frame_t frame ;
        if (reactor_source_status(p, plen, &frame, 0)) {

            char nm[plen + 1] ;
            memcpy(nm, p, plen) ;
            nm[plen] = 0 ;

            reactor_evaluate(re, nm, &frame) ;

            if (re->in_flight)
                break ; // fired: the latch absorbs the remaining sources
        }

        p += plen + 1 ;
    }
}

static void eventd_enqueue_emit(char const *name)
{
    if (!sbl_add(&eventd.emitq, name))
        log_warnusys("enqueue emit: ", name) ;
}

static void eventd_drain_emits(void)
{
    size_t pos = 0 ;

    FOREACH_SBL(&eventd.emitq, pos) {

        char nm[SS_MAX_SERVICE_NAME] ;
        char const *q = eventd.emitq.s + pos ;

        if (strlen(q) >= sizeof(nm))
            continue ; // over-long: skip (guarded, never happens for a valid event name)

        // copy out: reactor_run may append (and reallocate) the list
        auto_strings(nm, q) ;
        reactor_run(nm, 0) ;
    }

    eventd.emitq.len = 0 ;
}

static void reactor_reap_cb(sse_watcher_t *w, void *cbdata, int event)
{
    log_flow() ;

    eventd_reactor_t *re = cbdata ;

    if (w->api_errno) {
        log_warn("event action child error for reactor: ", re->name) ;
        re->in_flight = 0 ;
        re->pid = 0 ;
        sse_free_child(&re->child) ;
        if (re->pending_free) {
            reactor_wire(re, 0) ;
            reactor_destroy(re) ;
        }
        return ;
    }

    if (!(event & (SSE_HUP | SSE_READ)))
        return ;

    sse_child_t *data = (sse_child_t *)w->sdata ;
    int wstat = data ? data->status : 0 ;

    re->in_flight = 0 ;
    re->pid = 0 ;
    sse_free_child(&re->child) ;

    log_info("reactor ", re->name, ": ", event_do_to_string(re->rule.docmd),
             !WIFSIGNALED(wstat) && !WEXITSTATUS(wstat) ? " done" : " failed") ;

    // a Do+Emit reactor emits once its Do has committed (copied before any teardown)
    if (re->rule.emit)
        eventd_enqueue_emit(re->rule.sa.s + re->rule.emit) ;

    if (re->pending_free) {
        reactor_wire(re, 0) ;
        reactor_destroy(re) ;
    }

    eventd_drain_emits() ;
}

static void eventd_arm(char const *name)
{
    log_flow() ;

    size_t len = strlen(name) ;

    if (hash_find(&eventd.reactors, name, len))
        return ; // idempotent : already armed

    resolve_service_addon_event_t rule = RESOLVE_SERVICE_ADDON_EVENT_ZERO ;
    int r = eventd_rule_load(sysdir, name, &rule) ;
    if (r < 0) {
        log_warnu("load event rule: ", name) ;
        return ;
    }
    if (!r) {
        log_warn("no event rule to arm: ", name) ;
        return ;
    }

    eventd_reactor_t *re = malloc(sizeof(*re) + len + 1) ;
    if (!re) {
        eventd_rule_free(&rule) ;
        log_warnusys("allocate reactor: ", name) ;
        return ;
    }

    re->rule = rule ; // transfer ownership of rule.sa to the node
    re->child = (sse_watcher_t)SSE_WATCHER_ZERO ;
    re->pid = 0 ;
    re->in_flight = 0 ;
    re->pending_free = 0 ;
    re->nfires = 0 ;
    memcpy(re->name, name, len + 1) ;

    if (!hash_add(&eventd.reactors, re->name, len, re)) {
        eventd_rule_free(&re->rule) ;
        free(re) ;
        log_warnusys("register reactor: ", name) ;
        return ;
    }

    reactor_wire(re, 1) ;

    log_info("armed reactor: ", name) ;

    // the sources may already hold the awaited condition: fire it now if so
    reactor_update(re) ;
    eventd_drain_emits() ;
}

static void eventd_disarm(char const *name)
{
    log_flow() ;

    eventd_reactor_t *re = hash_find(&eventd.reactors, name, strlen(name)) ;
    if (!re)
        return ;

    if (re->in_flight) {
        /** an action is running (e.g. a Do=free reactor freeing itself): defer the
         * teardown to reactor_reap_cb so the child finishes before the node dies*/
        re->pending_free = 1 ;
        log_info("disarm deferred, action in flight: ", name) ;
        return ;
    }

    reactor_wire(re, 0) ;
    reactor_destroy(re) ;

    log_info("disarmed reactor: ", name) ;
}

static void eventd_emit(char const *name)
{
    log_flow() ;

    eventd_source_t *s = hash_find(&eventd.sources, name, strlen(name)) ;
    if (!s || s->type != EVENT_SOURCE_USER) {
        log_warn("no reactor armed for user event: ", name) ;
        return ;
    }

    reactor_run(name, 0) ;
    eventd_drain_emits() ;
}

static void source_write_status(resolve_service_t *res, uint8_t up, uint8_t who)
{
    log_flow() ;

    service_status_t st = STATUS_ZERO ;
    st.state = up ? STATUS_STATE_DONE : STATUS_STATE_DOWN ;
    st.result = STATUS_RESULT_SUCCESS ;
    st.who = who ;
    clock_now(&st.stamp) ;

    char const *supervisedir = res->sa.s + res->live.supervisedir ;
    char file[strlen(supervisedir) + 1 + SS_STATUS_LEN + 1] ;
    auto_strings(file, supervisedir, "/", SS_STATUS) ;

    if (!status_write(&st, file))
        log_warnusys("write source status of: ", res->sa.s + res->name) ;
}

static void eventd_arm_source(resolve_service_t *res, uint8_t who)
{
    log_flow() ;

    source_write_status(res, 1, who) ;

    log_info("armed event source: ", res->sa.s + res->name) ;
}

static void eventd_disarm_source(resolve_service_t *res, uint8_t who)
{
    log_flow() ;

    source_write_status(res, 0, who) ;

    log_info("disarmed event source: ", res->sa.s + res->name) ;
}

/* At the very first use of repopulate, the scandir
 * should be empty. In any others case, best-efford to
 * recover the previous state.*/
static void repopulate(char const *scandir)
{
    log_flow() ;

    DIR *dir = opendir(".") ;
    if (!dir) {
        log_warnusys("opendir scandir for repopulation: ", scandir) ;
        return ;
    }

    struct dirent *d ;
    errno = 0 ;
    while ((d = readdir(dir))) {

        if (d->d_name[0] == '.'
         || !strcmp(d->d_name, SS_EVENTD)
         || !strcmp(d->d_name, SS_ONESHOTD)
         || !strcmp(d->d_name, SS_FDHOLDER)) {
            errno = 0 ;
            continue ;
        }

        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
        int armit = resolve_read(wres, sysdir, d->d_name) == 1 && res.has_event ;
        resolve_free(wres) ;

        if (armit)
            eventd_arm(d->d_name) ;

        errno = 0 ;
    }

    if (errno)
        log_warnusys("readdir scandir for repopulation: ", scandir) ;

    closedir(dir) ;
}

static void client_dispatch(eventd_client_t *conn)
{
    log_flow() ;

    if (conn->blen < 2) {
        log_warn("request too short on control socket") ;
        return ;
    }

    char verb = conn->buf[0] ;
    uint8_t who = (uint8_t)conn->buf[1] ;
    size_t namelen = conn->blen - 2 ;

    if (!namelen) {
        log_warn("request without a name on control socket") ;
        return ;
    }

    if (who >= STATUS_WHO_ENDOFKEY) {
        log_warn("invalid who on control socket") ;
        return ;
    }

    char name[namelen + 1] ;
    memcpy(name, conn->buf + 2, namelen) ;
    name[namelen] = 0 ;

    if (verb == 'a' || verb == 'd') {

        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;

        if (resolve_read(wres, sysdir, name) == 1 && res.type == E_PARSER_TYPE_EVENT) {

            if (verb == 'a')
                eventd_arm_source(&res, who) ;
            else
                eventd_disarm_source(&res, who) ;

            resolve_free(wres) ;
            return ;
        }

        resolve_free(wres) ;
    }

    switch (verb) {

        case 'a' : eventd_arm(name) ; break ;
        case 'd' : eventd_disarm(name) ; break ;
        case 'e' : eventd_emit(name) ; break ;

        default :
            log_warn("unknown verb on control socket") ;
            break ;
    }
}

static void client_destroy(eventd_client_t *conn)
{
    log_flow() ;

    hash_del(&eventd.clients, conn) ;
    sse_free_io(&conn->w) ; // frees the watcher AND closes conn->fd
    free(conn) ;
}

static void client_read_cb(sse_watcher_t *w, void *data, int revents)
{
    log_flow() ;

    (void)w ;

    eventd_client_t *conn = data ;

    if (revents & SSE_ERROR) {
        log_warnusys("control socket connection") ;
        client_destroy(conn) ;
        return ;
    }

    for (;;) {

        if (conn->blen == sizeof(conn->buf)) {
            log_warn("request too long on control socket") ;
            client_destroy(conn) ;
            return ;
        }

        ssize_t r = io_read_result(io_read(conn->fd, conn->buf + conn->blen, sizeof(conn->buf) - conn->blen)) ;

        if (r > 0) {
            conn->blen += (size_t)r ;
            continue ;
        }

        if (!r)
            return ; // would block: wait for the next readable event

        // r < 0 : EPIPE marks the client's EOF, anything else is a real error
        if (errno == EPIPE)
            client_dispatch(conn) ;
        else
            log_warnusys("read from control socket") ;

        client_destroy(conn) ;
        return ;
    }
}

static int client_create(int fd)
{
    log_flow() ;

    if (hash_count(&eventd.clients) >= EVENTD_MAXCLIENTS) {
        close_fd(fd) ;
        log_warn_return(LOG_EXIT_ZERO, "too many connections - refusing") ;
    }

    eventd_client_t *conn = malloc(sizeof(*conn)) ;
    if (!conn) {
        close_fd(fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "allocate connection") ;
    }

    conn->fd = fd ;
    conn->w = (sse_watcher_t)SSE_WATCHER_ZERO ;
    conn->blen = 0 ;

    if (!sse_start_io(&eventd.epoll, &conn->w, client_read_cb, conn, fd, SSE_READ, 0)) {
        close_fd(fd) ;
        free(conn) ;
        log_warnusys_return(LOG_EXIT_ZERO, "watch connection") ;
    }

    if (!hash_add(&eventd.clients, &conn->fd, sizeof(conn->fd), conn)) {
        sse_free_io(&conn->w) ; // closes fd
        free(conn) ;
        log_warnusys_return(LOG_EXIT_ZERO, "register connection") ;
    }

    return 1 ;
}

static void server_accept_cb(sse_watcher_t *w, void *data, int revents)
{
    log_flow() ;

    (void)w ; (void)data ;

    if (revents & (SSE_ERROR | SSE_HUP)) {
        log_warnusys("control socket") ;
        eventd.epoll.running = false ;
        return ;
    }

    if (revents & SSE_READ) {

        int fd = sse_streamux_accept(eventd.sfd) ;
        if (fd < 0) {
            log_warnusys("accept connection") ;
            return ;
        }

        struct ucred cred ;
        if (socketunix_getucred(fd, &cred) < 0) {
            log_warnusys("get peer credentials") ;
            close_fd(fd) ;
            return ;
        }

        if (cred.uid != eventd.owner) {
            flog_warn("rejecting connection from uid %d", (int)cred.uid) ;
            close_fd(fd) ;
            return ;
        }

        client_create(fd) ;
    }
}

static void signal_cb(sse_watcher_t *w, void *data, int revents)
{
    log_flow() ;

    (void)data ;

    if (revents & (SSE_ERROR | SSE_HUP)) {
        log_warnusys("signal watcher") ;
        eventd.epoll.running = false ;
        return ;
    }

    sse_signal_t *sig = (sse_signal_t *)w->sdata ;
    flog_info("received signal %d - shutting down", sig->si.ssi_signo) ;
    eventd.epoll.running = false ;
}

static int eventd_init(void)
{
    log_flow() ;

    if (!sse_new(&eventd.epoll, SSE_MAX_EVENTS))
        log_warnusys_return(LOG_EXIT_ZERO, "create event loop") ;

    if (!sse_start_signal(&eventd.epoll, &eventd.wsignal, signal_cb, NULL, 10)) {
        sse_free(&eventd.epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "start signal watcher") ;
    }

    if (!sse_ignore_signal(&eventd.wsignal, SIGPIPE) ||
        !sse_attach_signal(&eventd.wsignal, SIGTERM) ||
        !sse_attach_signal(&eventd.wsignal, SIGINT)) {
        sse_free(&eventd.epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "set signals") ;
    }

    eventd.sfd = sse_streamux_create_server(EVENTD_SOCKET, 0) ;
    if (eventd.sfd < 0) {
        sse_free(&eventd.epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "create control socket: ", EVENTD_SOCKET) ;
    }

    if (!sse_start_io(&eventd.epoll, &eventd.wserver, server_accept_cb, NULL, eventd.sfd, SSE_READ, 0)) {
        close_fd(eventd.sfd) ;
        sse_free(&eventd.epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "watch control socket") ;
    }

    return 1 ;
}

static void eventd_cleanup(void)
{
    log_flow() ;

    eventd_client_t *c, *tc ;
    HASH_FOREACH(&eventd.clients, c, tc)
        client_destroy(c) ;

    eventd_reactor_t *re, *tre ;
    HASH_FOREACH(&eventd.reactors, re, tre)
        reactor_destroy(re) ;

    eventd_source_t *s, *ts ;
    HASH_FOREACH(&eventd.sources, s, ts)
        source_destroy(s) ;

    hash_free(&eventd.clients) ;
    hash_free(&eventd.reactors) ;
    hash_free(&eventd.sources) ;
    strbuf_free(&eventd.emitq) ;

    sse_free_io(&eventd.wserver) ; // closes eventd.sfd
    sse_free_signal(&eventd.wsignal) ;
    sse_free(&eventd.epoll) ;

    file_tryunlink(EVENTD_SOCKET) ;

    log_info("event daemon stopped") ;
}

int main(int argc, char const *const *argv)
{
    log_flow() ;

    int notif = -1 ;

    PROG = "66-eventd" ;

    if (!ensure_stdfds())
        log_dieusys(LOG_EXIT_SYS, "ensure standard descriptors") ;

    {
        opt_scan_t st = OPT_SCAN_ZERO ;

        for (;;) {

            int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
            if (o == OPT_END)
                break ;

            switch (o) {

                case OPT_ID_HELP :
                    return opt_emit_help(cmd.name, &cmd) ;

                case 'v' :
                    if (!u32_scan_strict(st.arg, &VERBOSITY))
                        return opt_emit_usage(cmd.name, &cmd) ;
                    break ;

                case 'd' :
                    notif = notifier_isvalid(st.arg) ;
                    break ;

                default :
                    return opt_emit_error(cmd.name, &cmd, o, &st) ;
            }
        }
        argc -= st.ind ; argv += st.ind ;
    }

    if (argc < 1)
        return opt_emit_usage(cmd.name, &cmd) ;

    if (argv[0][0] != '/')
        log_die(LOG_EXIT_SYS, "scandir must be an absolute path: ", argv[0]) ;

    char const *scandir = argv[0] ;

    if (!set_ownersysdir_stack(sysdir, getuid()))
        log_dieusys(LOG_EXIT_SYS, "set owner system directory") ;

    if (chdir(scandir) < 0)
        log_dieusys(LOG_EXIT_SYS, "chdir to scandir: ", scandir) ;

    eventd.owner = geteuid() ;

    if (mkdir(SS_EVENTD, 0700) < 0 && errno != EEXIST)
        log_dieusys(LOG_EXIT_SYS, "create daemon directory: ", SS_EVENTD) ;

    if (!hash_init(&eventd.sources, 0, offsetof(eventd_source_t, node)) ||
        !hash_init(&eventd.reactors, 0, offsetof(eventd_reactor_t, node)) ||
        !hash_init(&eventd.clients, 0, offsetof(eventd_client_t, node)))
        log_dieusys(LOG_EXIT_SYS, "initialize runtime tables") ;

    if (!eventd_init())
        log_dieu(LOG_EXIT_SYS, "initialize event daemon") ;

    log_info("event daemon watching scandir: ", scandir) ;

    repopulate(scandir) ;

    if (io_write(notif, "\n", 1) < 0)
        log_dieusys(LOG_EXIT_SYS, "notify readiness") ;

    int r = sse_poll(&eventd.epoll, SSE_TIMEOUT_INFINITE) ;

    eventd_cleanup() ;

    return r ? 0 : LOG_EXIT_SYS ;
}
