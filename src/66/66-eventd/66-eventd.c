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

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/opt.h>
#include <oblibs/sse.h>
#include <oblibs/sse_stream.h>
#include <oblibs/socket.h>
#include <oblibs/hash.h>
#include <oblibs/io.h>
#include <oblibs/fd.h>
#include <oblibs/files.h>
#include <oblibs/string.h>

#include <66/event.h>
#include <66/event_rule.h>
#include <66/status.h>
#include <66/constants.h>
#include <66/utils.h>

#include "66/config.h"
#include "lib/eventd.h"

#define EVENTD_SOCKET SS_EVENTD "/s"
#define EVENTD_MAXCLIENTS 64

typedef struct eventd_reactor_s eventd_reactor_t ;
struct eventd_reactor_s
{
    char name[SS_MAX_SERVICE_NAME] ; // hash key
    resolve_service_addon_event_t rule ; // loaded .event addon ; owns its sa
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

typedef struct eventd_conn_s eventd_conn_t ;
struct eventd_conn_s
{
    int fd ;
    sse_watcher_t w ;
    size_t blen ; // bytes buffered
    hash_node_t node ;
    char buf[1 + SS_MAX_SERVICE_NAME + 1] ; // <verb:1><name>
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
    hash_t conns ;          // eventd_conn_t*
} ;

static eventd_t eventd = {
    .epoll = SSE_EPOLL_ZERO,
    .wsignal = SSE_WATCHER_ZERO,
    .wserver = SSE_WATCHER_ZERO,
    .sfd = -1,
    .owner = -1,
    .sources = HASH_ZERO,
    .reactors = HASH_ZERO,
    .conns = HASH_ZERO
} ;

static char sysdir[SS_MAX_PATH_LEN + 1] ;

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

    switch (f->kind) {

        case EVENT_KIND_TRANSITION :
            log_info(s->name, ": ", status_state_to_string(f->state)) ;
            break ;

        case EVENT_KIND_SIGNAL : {
            char n[U8_FMT] ;
            n[u8_fmt(n, f->signo)] = 0 ;
            log_info(s->name, ": signal ", n) ;
            break ;
        }

        case EVENT_KIND_LIFECYCLE :
            log_info(s->name, ": ", f->phase == EVENT_LIFECYCLE_UP ? "supervisor up" : "supervisor down") ;
            break ;

        default :
            break ;
    }
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

    s->ag.len = 0 ; // malloc is not zeroed: the reassembler starts empty
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
    memcpy(re->name, name, len + 1) ;

    if (!hash_add(&eventd.reactors, re->name, len, re)) {
        eventd_rule_free(&re->rule) ;
        free(re) ;
        log_warnusys("register reactor: ", name) ;
        return ;
    }

    reactor_wire(re, 1) ;

    log_info("armed reactor: ", name) ;
}

static void eventd_disarm(char const *name)
{
    log_flow() ;

    eventd_reactor_t *re = hash_find(&eventd.reactors, name, strlen(name)) ;
    if (!re)
        return ; // idempotent : not armed

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

    // the pipeline (match + act) is not wired yet : report the routing for now
    char nb[U32_FMT] ;
    nb[u32_fmt(nb, s->refcount)] = 0 ;
    log_info("user event ", name, ": ", nb, " reactor(s)") ;
}

static void conn_dispatch(eventd_conn_t *conn)
{
    log_flow() ;

    if (!conn->blen) {
        log_warn("empty request on control socket") ;
        return ;
    }

    char verb = conn->buf[0] ;
    size_t namelen = conn->blen - 1 ;

    if (!namelen) {
        log_warn("request without a name on control socket") ;
        return ;
    }

    char name[namelen + 1] ;
    memcpy(name, conn->buf + 1, namelen) ;
    name[namelen] = 0 ;

    switch (verb) {

        case 'a' : eventd_arm(name) ; break ;
        case 'd' : eventd_disarm(name) ; break ;
        case 'e' : eventd_emit(name) ; break ;

        default :
            log_warn("unknown verb on control socket") ;
            break ;
    }
}

static void conn_destroy(eventd_conn_t *conn)
{
    log_flow() ;

    hash_del(&eventd.conns, conn) ;
    sse_free_io(&conn->w) ; // frees the watcher AND closes conn->fd
    free(conn) ;
}

static void conn_read_cb(sse_watcher_t *w, void *data, int revents)
{
    log_flow() ;

    (void)w ;

    eventd_conn_t *conn = data ;

    if (revents & SSE_ERROR) {
        log_warnusys("control socket connection") ;
        conn_destroy(conn) ;
        return ;
    }

    for (;;) {

        if (conn->blen == sizeof(conn->buf)) {
            log_warn("request too long on control socket") ;
            conn_destroy(conn) ;
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
            conn_dispatch(conn) ;
        else
            log_warnusys("read from control socket") ;

        conn_destroy(conn) ;
        return ;
    }
}

static int conn_create(int fd)
{
    log_flow() ;

    if (hash_count(&eventd.conns) >= EVENTD_MAXCLIENTS) {
        close_fd(fd) ;
        log_warn_return(LOG_EXIT_ZERO, "too many connections - refusing") ;
    }

    eventd_conn_t *conn = malloc(sizeof(*conn)) ;
    if (!conn) {
        close_fd(fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "allocate connection") ;
    }

    conn->fd = fd ;
    conn->w = (sse_watcher_t)SSE_WATCHER_ZERO ;
    conn->blen = 0 ;

    if (!sse_start_io(&eventd.epoll, &conn->w, conn_read_cb, conn, fd, SSE_READ, 0)) {
        close_fd(fd) ;
        free(conn) ;
        log_warnusys_return(LOG_EXIT_ZERO, "watch connection") ;
    }

    if (!hash_add(&eventd.conns, &conn->fd, sizeof(conn->fd), conn)) {
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

        conn_create(fd) ;
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

    /* runs after sse_poll returned, i.e. outside any handler dispatch, so
     * unsubscribing and freeing watchers is safe here. */
    eventd_conn_t *c, *tc ;
    HASH_FOREACH(&eventd.conns, c, tc)
        conn_destroy(c) ;

    eventd_reactor_t *re, *tre ;
    HASH_FOREACH(&eventd.reactors, re, tre)
        reactor_destroy(re) ;

    eventd_source_t *s, *ts ;
    HASH_FOREACH(&eventd.sources, s, ts)
        source_destroy(s) ;

    hash_free(&eventd.conns) ;
    hash_free(&eventd.reactors) ;
    hash_free(&eventd.sources) ;

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
        !hash_init(&eventd.conns, 0, offsetof(eventd_conn_t, node)))
        log_dieusys(LOG_EXIT_SYS, "initialize runtime tables") ;

    if (!eventd_init())
        log_dieu(LOG_EXIT_SYS, "initialize event daemon") ;

    log_info("event daemon watching scandir: ", scandir) ;

    if (io_write(notif, "\n", 1) < 0)
        log_dieusys(LOG_EXIT_SYS, "notify readiness") ;

    int r = sse_poll(&eventd.epoll, SSE_TIMEOUT_INFINITE) ;

    eventd_cleanup() ;

    return r ? 0 : LOG_EXIT_SYS ;
}
