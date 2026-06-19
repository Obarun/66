/*
 * fdholderd.c
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
 *
 * File descriptor holder daemon: stores named file descriptors, optionally
 * with an expiry, and hands them back over a Unix socket using SCM_RIGHTS.
 * Built solely on oblibs and libc.
 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/opt.h>
#include <oblibs/sse.h>
#include <oblibs/sse_stream.h>
#include <oblibs/stream_message.h>
#include <oblibs/io_rb.h>
#include <oblibs/hash2.h>
#include <oblibs/fd.h>
#include <oblibs/files.h>
#include <oblibs/socket.h>

#include <66/fdholder.h>

// one stored descriptor, keyed by name
typedef struct fdholder_entry_s fdholder_entry_t ;
struct fdholder_entry_s
{
    int fd ;
    time_t expire_abs ; // absolute expiry, 0 = never
    char name[FDHOLDER_NAME_MAX + 1] ;
    hash_node_t node ;
} ;

// one named pipe pair, keyed by name ; both ends held by the daemon
typedef struct fdholder_pipe_s fdholder_pipe_t ;
struct fdholder_pipe_s
{
    int rfd ; // read end  (p[0])
    int wfd ; // write end (p[1])
    char name[FDHOLDER_NAME_MAX + 1] ;
    hash_node_t node ;
} ;

// one connected client, keyed by socket fd
typedef struct fdholder_conn_s fdholder_conn_t ;
struct fdholder_conn_s
{
    int key ; // stream fd, hash key
    sse_stream_t *stream ;
    stream_message_t reader ;
    char *hdrbuf ;
    char *paybuf ;
    int *pclose ; // fds to close once the response has flushed
    size_t npclose ;
    size_t capclose ;
    bool closing ;
    hash_node_t node ;
} ;

typedef struct fdholder_daemon_s fdholder_daemon_t ;
struct fdholder_daemon_s
{
    sse_epoll_t epoll ;
    sse_watcher_t wserver ;
    sse_watcher_t wsignal ;
    sse_watcher_t wtimer ;
    bool timer_started ;
    int sfd ;
    char const *socket_path ;
    uid_t owner ;
    uint32_t maxfds ;
    hash_t entries ; // hash table by name
    hash_t pipes ; // hash table by name (named pipe pairs)
    hash_t conns ; // hash table by fd
} ;

static fdholder_daemon_t fdh = {
    .epoll = SSE_EPOLL_ZERO,
    .wserver = SSE_WATCHER_ZERO,
    .wsignal = SSE_WATCHER_ZERO,
    .wtimer = SSE_WATCHER_ZERO,
    .timer_started = false,
    .sfd = -1,
    .socket_path = NULL,
    .owner = 0,
    .maxfds = FDHOLDER_MAXFDS_DEFAULT,
    .entries = HASH_ZERO,
    .pipes = HASH_ZERO,
    .conns = HASH_ZERO
} ;

static opt_t const opts[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",    .arg = OPT_NONE,                          .help = "print this help" },
    { .id = 'v',         .shortname = 'v', .longname = "verbose", .arg = OPT_REQUIRED, .argname = "number", .help = "increase/decrease verbosity" },
    { .id = 'd',         .shortname = 'd', .longname = "notify",  .arg = OPT_REQUIRED, .argname = "fd",     .help = "notify readiness on file descriptor fd (>= 3)" },
    { .id = 'n',         .shortname = 'n', .longname = "maxfds",  .arg = OPT_REQUIRED, .argname = "number", .help = "maximum number of stored descriptors" },
    { .id = 'b',         .shortname = 'b', .longname = "backlog", .arg = OPT_REQUIRED, .argname = "number", .help = "listen backlog" },
} ;

static opt_cmd_t const cmd = {
    .name = "66-fdholderd",
    .operands = "socket",
    .opts = opts,
    .nopts = OPT_COUNT(opts),
} ;

// store

static fdholder_entry_t *entry_find(char const *name)
{
    return hash_find(&fdh.entries, name, strlen(name)) ;
}

/*
 * (Re)arm the single expiry timer to the earliest finite deadline currently
 * stored. A spurious early fire is harmless: the callback evicts only entries
 * that are actually due, then rearms.
 */
static void expiry_timer_cb(sse_watcher_t *w, void *data, int revents) ;

static void daemon_rearm_timer(void)
{
    log_flow() ;

    time_t min = 0 ;
    bool have = false ;

    fdholder_entry_t *e, *t ;
    HASH_FOREACH(&fdh.entries, e, t) {
        if (!e->expire_abs)
            continue ;
        if (!have || e->expire_abs < min) {
            min = e->expire_abs ;
            have = true ;
        }
    }

    if (!have)
        return ;

    time_t now = time(NULL) ;
    int64_t secs = (int64_t)min - (int64_t)now ;
    if (secs < 0)
        secs = 0 ;
    // clamp so the relative millisecond delay never overflows an int (~24 days)
    int64_t ms = secs > 2000000 ? 2000000000 : secs * 1000 ;
    if (ms < 1)
        ms = 1 ;

    if (fdh.timer_started) {
        sse_modify_timer(&fdh.wtimer, (int)ms, 0) ;
    } else if (sse_start_timer(&fdh.epoll, &fdh.wtimer, expiry_timer_cb, NULL, (int)ms, 0, 1)) {
        fdh.timer_started = true ;
    } else {
        log_warnusys("arm expiry timer") ;
    }
}

static void expiry_timer_cb(sse_watcher_t *w, void *data, int revents)
{
    log_flow() ;
    (void)w ; (void)data ;

    if (revents & (SSE_ERROR | SSE_HUP)) {
        log_warnusys("expiry timer") ;
        return ;
    }

    time_t now = time(NULL) ;

    fdholder_entry_t *e, *t ;
    HASH_FOREACH(&fdh.entries, e, t) {
        if (e->expire_abs && e->expire_abs <= now) {
            flog_info("'%s' (fd %d) expired", e->name, e->fd) ;
            close_fd(e->fd) ;
            hash_del(&fdh.entries, e) ;
            free(e) ;
        }
    }

    daemon_rearm_timer() ;
}

static void close_all(int const *afd, int nfd)
{
    for (int i = 0 ; i < nfd ; i++)
        if (afd[i] >= 0)
            close_fd(afd[i]) ;
}

static int conn_pclose_add(fdholder_conn_t *conn, int fd)
{
    if (conn->npclose == conn->capclose) {
        size_t nc = conn->capclose ? conn->capclose + (conn->capclose >> 1) : 4 ;
        int *n = realloc(conn->pclose, nc * sizeof(int)) ;
        if (!n)
            return 0 ;
        conn->pclose = n ;
        conn->capclose = nc ;
    }
    conn->pclose[conn->npclose++] = fd ;
    return 1 ;
}

// responses

static void respond(fdholder_conn_t *conn, uint8_t status, void const *payload, size_t paylen, int fd)
{
    char hdr[FDHOLDER_HDR_SIZE] ;
    fdholder_hdr_pack(hdr, FDHOLDER_CMD_RESPONSE, status, 0, (uint32_t)paylen) ;

    if (fd >= 0)
        stream_message_send_with_fds(conn->stream, hdr, FDHOLDER_HDR_SIZE, payload, paylen, &fd, 1) ;
    else
        stream_message_send(conn->stream, hdr, FDHOLDER_HDR_SIZE, payload, paylen) ;
}

/*
 * Validate a name-only request (delete, retrieve): it must carry no descriptor
 * and a name of 1..FDHOLDER_NAME_MAX bytes. On success the NUL-terminated name
 * is copied into out (which must hold FDHOLDER_NAME_MAX + 1 bytes) and 1 is
 * returned. On a contract violation any stray descriptors are closed, a PROTO
 * answer is sent and 0 is returned.
 */
static inline int request_name(fdholder_conn_t *conn, char const *pl, size_t pll, int const *afd, int nfd, char *out)
{
    if (nfd != 0) {
        close_all(afd, nfd) ;
        respond(conn, FDHOLDER_PROTO, NULL, 0, -1) ;
        return 0 ;
    }

    if (pll == 0 || pll > FDHOLDER_NAME_MAX) {
        respond(conn, FDHOLDER_PROTO, NULL, 0, -1) ;
        return 0 ;
    }

    memcpy(out, pl, pll) ;
    out[pll] = 0 ;
    return 1 ;
}

// handlers

static void handle_store(fdholder_conn_t *conn, char const *pl, size_t pll, int const *afd, int nfd)
{
    log_flow() ;

    if (nfd != 1) {
        close_all(afd, nfd) ;
        respond(conn, FDHOLDER_PROTO, NULL, 0, -1) ;
        return ;
    }

    if (pll <= 8 || pll - 8 > FDHOLDER_NAME_MAX) {
        close_all(afd, nfd) ;
        respond(conn, FDHOLDER_PROTO, NULL, 0, -1) ;
        return ;
    }

    uint64_t expire ;
    u64_unpack_big(pl, &expire) ;
    size_t namelen = pll - 8 ;
    char name[FDHOLDER_NAME_MAX + 1] ;
    memcpy(name, pl + 8, namelen) ;
    name[namelen] = 0 ;

    if (hash_count(&fdh.entries) >= fdh.maxfds) {
        close_all(afd, nfd) ;
        respond(conn, FDHOLDER_FULL, NULL, 0, -1) ;
        return ;
    }

    if (entry_find(name)) {
        close_all(afd, nfd) ;
        respond(conn, FDHOLDER_EXISTS, NULL, 0, -1) ;
        return ;
    }

    fdholder_entry_t *e = malloc(sizeof(*e)) ;
    if (!e) {
        close_all(afd, nfd) ;
        respond(conn, FDHOLDER_ERR, NULL, 0, -1) ;
        return ;
    }

    e->fd = afd[0] ;
    e->expire_abs = expire ? time(NULL) + (time_t)expire : 0 ;
    memcpy(e->name, name, namelen + 1) ;
    hash_add(&fdh.entries, e->name, strlen(e->name), e) ;

    if (e->expire_abs)
        daemon_rearm_timer() ;

    respond(conn, FDHOLDER_OK, NULL, 0, -1) ;
    flog_info("stored '%s' as fd %d%s", e->name, e->fd, e->expire_abs ? "" : " (never expires)") ;
}

static void handle_retrieve(fdholder_conn_t *conn, uint8_t flags, char const *pl, size_t pll, int const *afd, int nfd)
{
    log_flow() ;

    char name[FDHOLDER_NAME_MAX + 1] ;
    if (!request_name(conn, pl, pll, afd, nfd, name))
        return ;

    fdholder_entry_t *e = entry_find(name) ;
    if (!e) {
        respond(conn, FDHOLDER_NOTFOUND, NULL, 0, -1) ;
        return ;
    }

    int fd = e->fd ;
    bool dodelete = (flags & FDHOLDER_FLAG_DELETE) != 0 ;

    /* the kernel duplicates fd at sendmsg time, so the number must stay valid
     * until the flush ; never close it synchronously here */
    respond(conn, FDHOLDER_OK, NULL, 0, fd) ;

    if (dodelete) {
        hash_del(&fdh.entries, e) ;
        if (!conn_pclose_add(conn, fd)) {
            log_warnusys("defer close of fd; closing now") ;
            close_fd(fd) ;
        }
        free(e) ;
        daemon_rearm_timer() ;
        flog_info("retrieved and removed '%s' (fd %d)", name, fd) ;
    } else {
        flog_info("retrieved '%s' (fd %d)", name, fd) ;
    }
}

static void handle_delete(fdholder_conn_t *conn, char const *pl, size_t pll, int const *afd, int nfd)
{
    log_flow() ;

    char name[FDHOLDER_NAME_MAX + 1] ;
    if (!request_name(conn, pl, pll, afd, nfd, name))
        return ;

    fdholder_entry_t *e = entry_find(name) ;
    if (!e) {
        respond(conn, FDHOLDER_NOTFOUND, NULL, 0, -1) ;
        return ;
    }

    int fd = e->fd ;
    hash_del(&fdh.entries, e) ;
    close_fd(fd) ;
    free(e) ;
    daemon_rearm_timer() ;

    respond(conn, FDHOLDER_OK, NULL, 0, -1) ;
    flog_info("deleted '%s' (fd %d)", name, fd) ;
}

static void handle_list(fdholder_conn_t *conn)
{
    log_flow() ;

    char *buf = conn->paybuf ;
    size_t off = 0 ;

    fdholder_entry_t *e, *t ;
    HASH_FOREACH(&fdh.entries, e, t) {
        size_t l = strlen(e->name) + 1 ;
        if (off + l > FDHOLDER_PAYLOAD_MAX) {
            log_warn("list does not fit in a single message") ;
            respond(conn, FDHOLDER_ERR, NULL, 0, -1) ;
            return ;
        }
        memcpy(buf + off, e->name, l) ;
        off += l ;
    }

    respond(conn, FDHOLDER_OK, buf, off, -1) ;
}

/** Get-or-create one end of a named pipe pair. Atomic: the pipe() + insertion run
 * in a single dispatch, so two clients can never receive ends of different pipes.
 * The daemon keeps both ends (the kernel dups the requested one at send time), so
 * the pair survives either side restarting. Pipe ends never expire.
 */
static void handle_pipe(fdholder_conn_t *conn, char const *pl, size_t pll, int const *afd, int nfd)
{
    log_flow() ;

    if (nfd != 0) {
        close_all(afd, nfd) ;
        respond(conn, FDHOLDER_PROTO, NULL, 0, -1) ;
        return ;
    }

    if (pll < 2) {
        respond(conn, FDHOLDER_PROTO, NULL, 0, -1) ;
        return ;
    }

    uint8_t end = (uint8_t)pl[0] ;
    size_t nl = pll - 1 ;
    if ((end != FDHOLDER_END_READ && end != FDHOLDER_END_WRITE) || nl > FDHOLDER_NAME_MAX) {
        respond(conn, FDHOLDER_PROTO, NULL, 0, -1) ;
        return ;
    }

    char name[FDHOLDER_NAME_MAX + 1] ;
    memcpy(name, pl + 1, nl) ;
    name[nl] = 0 ;

    fdholder_pipe_t *fp = hash_find(&fdh.pipes, name, strlen(name)) ;

    if (!fp) {
        // each pair holds two descriptors ; bound against maxfds
        if (2 * hash_count(&fdh.pipes) + hash_count(&fdh.entries) + 2 > fdh.maxfds) {
            respond(conn, FDHOLDER_FULL, NULL, 0, -1) ;
            return ;
        }
        int p[2] ;
        if (pipe2(p, O_CLOEXEC) < 0) {
            log_warnusys("create pipe") ;
            respond(conn, FDHOLDER_ERR, NULL, 0, -1) ;
            return ;
        }
        fp = malloc(sizeof(*fp)) ;
        if (!fp) {
            close_fd(p[0]) ; close_fd(p[1]) ;
            respond(conn, FDHOLDER_ERR, NULL, 0, -1) ;
            return ;
        }
        fp->rfd = p[0] ;
        fp->wfd = p[1] ;
        memcpy(fp->name, name, nl + 1) ;
        hash_add(&fdh.pipes, fp->name, strlen(fp->name), fp) ;
        flog_info("created pipe flow '%s'", name) ;
    }

    /* kernel dups the end at send time ; the daemon keeps its own copy */
    respond(conn, FDHOLDER_OK, NULL, 0, end == FDHOLDER_END_READ ? fp->rfd : fp->wfd) ;
    flog_info("handed %s end of '%s'", end == FDHOLDER_END_READ ? "read" : "write", name) ;
}

static void handle_pipe_delete(fdholder_conn_t *conn, char const *pl, size_t pll, int const *afd, int nfd)
{
    log_flow() ;

    char name[FDHOLDER_NAME_MAX + 1] ;
    if (!request_name(conn, pl, pll, afd, nfd, name))
        return ;

    fdholder_pipe_t *fp = hash_find(&fdh.pipes, name, strlen(name)) ;
    if (!fp) {
        respond(conn, FDHOLDER_NOTFOUND, NULL, 0, -1) ;
        return ;
    }

    close_fd(fp->rfd) ;
    close_fd(fp->wfd) ;
    hash_del(&fdh.pipes, fp) ;
    free(fp) ;

    respond(conn, FDHOLDER_OK, NULL, 0, -1) ;
    flog_info("deleted pipe flow '%s'", name) ;
}

static void handle_message(io_rb_iovec_t *msg, void *ctx)
{
    log_flow() ;

    fdholder_conn_t *conn = ctx ;

    if (!conn || conn->closing) {
        close_all(msg->afd, msg->nfd) ;
        return ;
    }

    if (msg->niov < 1) {
        close_all(msg->afd, msg->nfd) ;
        respond(conn, FDHOLDER_PROTO, NULL, 0, -1) ;
        return ;
    }

    char const *hdr = msg->iov[0].iov_base ;
    uint8_t command = (uint8_t)hdr[1] ;
    uint8_t flags = (uint8_t)hdr[3] ;
    char const *pl = msg->niov > 1 ? msg->iov[1].iov_base : NULL ;
    size_t pll = msg->niov > 1 ? msg->iov[1].iov_len : 0 ;

    switch (command) {
        case FDHOLDER_CMD_STORE :
            handle_store(conn, pl, pll, msg->afd, msg->nfd) ;
            break ;
        case FDHOLDER_CMD_RETRIEVE :
            handle_retrieve(conn, flags, pl, pll, msg->afd, msg->nfd) ;
            break ;
        case FDHOLDER_CMD_DELETE :
            handle_delete(conn, pl, pll, msg->afd, msg->nfd) ;
            break ;
        case FDHOLDER_CMD_LIST :
            close_all(msg->afd, msg->nfd) ;
            handle_list(conn) ;
            break ;
        case FDHOLDER_CMD_PIPE :
            handle_pipe(conn, pl, pll, msg->afd, msg->nfd) ;
            break ;
        case FDHOLDER_CMD_PIPE_DELETE :
            handle_pipe_delete(conn, pl, pll, msg->afd, msg->nfd) ;
            break ;
        default :
            close_all(msg->afd, msg->nfd) ;
            respond(conn, FDHOLDER_PROTO, NULL, 0, -1) ;
            break ;
    }
}

// connection

static void conn_destroy(fdholder_conn_t *conn)
{
    if (!conn || conn->closing)
        return ;

    conn->closing = true ;
    hash_del(&fdh.conns, conn) ;

    for (size_t i = 0 ; i < conn->npclose ; i++)
        close_fd(conn->pclose[i]) ;
    free(conn->pclose) ;

    if (conn->stream) {
        io_rb_close_fds(&conn->stream->io.fdin.buf) ;
        sse_stream_close(conn->stream) ;   /* may re-enter close_cb, guarded by closing */
    }

    free(conn->hdrbuf) ;
    free(conn->paybuf) ;
    free(conn) ;
}

static void conn_read_cb(sse_stream_t *stream, void *data)
{
    log_flow() ;
    (void)stream ;

    fdholder_conn_t *conn = data ;
    if (!conn || conn->closing)
        return ;

    for (;;) {
        int r = stream_message_read(&conn->reader, fdholder_parse_header) ;
        if (r < 0) {
            flog_warnsys("read from client fd %d", conn->key) ;
            conn_destroy(conn) ;
            return ;
        }
        if (r == 0)
            return ;
    }
}

static void conn_write_cb(sse_stream_t *stream, void *data)
{
    log_flow() ;

    fdholder_conn_t *conn = data ;
    if (!conn || conn->closing)
        return ;

    /* only release deferred fds once everything queued has actually been sent */
    if (sse_stream_pending_out(stream) || sse_stream_pending_fdout(stream))
        return ;

    for (size_t i = 0 ; i < conn->npclose ; i++)
        close_fd(conn->pclose[i]) ;
    conn->npclose = 0 ;
}

static void conn_close_cb(sse_stream_t *stream, void *data)
{
    log_flow() ;
    (void)stream ;
    conn_destroy(data) ;
}

static void conn_error_cb(sse_stream_t *stream, int error, void *data)
{
    log_flow() ;
    (void)stream ;

    errno = error ;
    flog_warnusys("client fd %d", ((fdholder_conn_t *)data)->key) ;
    conn_destroy(data) ;
}

static int conn_create(int fd)
{
    log_flow() ;

    if (hash_count(&fdh.conns) >= FDHOLDER_MAXCLIENTS_DEFAULT) {
        close_fd(fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "too many clients - refusing connection") ;
    }

    fdholder_conn_t *conn = calloc(1, sizeof(*conn)) ;
    if (!conn) {
        close_fd(fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "allocate client") ;
    }

    conn->key = fd ;
    conn->stream = sse_stream_new(fd, conn_read_cb, conn_write_cb, conn_close_cb, conn_error_cb, conn) ;
    if (!conn->stream) {
        close_fd(fd) ;
        free(conn) ;
        log_warnusys_return(LOG_EXIT_ZERO, "create client stream") ;
    }

    conn->hdrbuf = malloc(FDHOLDER_HDR_SIZE) ;
    conn->paybuf = malloc(IO_RB_BUFFER_SIZE) ;
    if (!conn->hdrbuf || !conn->paybuf) {
        free(conn->hdrbuf) ;
        free(conn->paybuf) ;
        sse_stream_close(conn->stream) ;
        free(conn) ;
        log_warnusys_return(LOG_EXIT_ZERO, "allocate client buffers") ;
    }

    if (!stream_message_reader_init(&conn->reader, conn->stream, handle_message,
                                    conn->hdrbuf, FDHOLDER_HDR_SIZE,
                                    conn->paybuf, IO_RB_BUFFER_SIZE, conn)) {
        sse_stream_close(conn->stream) ;
        free(conn->hdrbuf) ;
        free(conn->paybuf) ;
        free(conn) ;
        log_warnusys_return(LOG_EXIT_ZERO, "initialize message reader") ;
    }

    if (!sse_stream_attach(conn->stream, &fdh.epoll, 0)) {
        sse_stream_close(conn->stream) ;
        free(conn->hdrbuf) ;
        free(conn->paybuf) ;
        free(conn) ;
        log_warnusys_return(LOG_EXIT_ZERO, "attach client stream") ;
    }

    hash_add(&fdh.conns, &conn->key, sizeof conn->key, conn) ;
    flog_info("client connected on fd %d", fd) ;

    return 1 ;
}

// server callbacks

static void server_accept_cb(sse_watcher_t *w, void *data, int revents)
{
    log_flow() ;
    (void)w ; (void)data ;

    if (revents & (SSE_ERROR | SSE_HUP)) {
        log_warnusys("server socket") ;
        fdh.epoll.running = false ;
        return ;
    }

    if (revents & SSE_READ) {
        int fd = sse_streamux_accept(fdh.sfd) ;
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

        if (cred.uid != fdh.owner) {
            flog_warn("rejecting connection from uid %d", (int)cred.uid) ;
            close_fd(fd) ;
            return ;
        }

        conn_create(fd) ;
    }
}

static void server_signal_cb(sse_watcher_t *w, void *data, int revents)
{
    log_flow() ;
    (void)data ;

    if (revents & (SSE_ERROR | SSE_HUP)) {
        log_warnusys("signal watcher") ;
        fdh.epoll.running = false ;
        return ;
    }

    sse_signal_t *sig = (sse_signal_t *)w->sdata ;
    flog_info("received signal %d - shutting down", sig->si.ssi_signo) ;
    fdh.epoll.running = false ;
}

// daemon

static int server_init(char const *socket_path, int backlog)
{
    log_flow() ;

    if (!sse_new(&fdh.epoll, FDHOLDER_MAXCLIENTS_DEFAULT))
        log_warnusys_return(0, "create event loop") ;

    if (!sse_start_signal(&fdh.epoll, &fdh.wsignal, server_signal_cb, NULL, 10)) {
        sse_free(&fdh.epoll) ;
        log_warnusys_return(0, "start signal watcher") ;
    }

    if (!sse_ignore_signal(&fdh.wsignal, SIGPIPE) ||
        !sse_attach_signal(&fdh.wsignal, SIGTERM) ||
        !sse_attach_signal(&fdh.wsignal, SIGINT)) {
        sse_free(&fdh.epoll) ;
        log_warnusys_return(0, "set signals") ;
    }

    fdh.sfd = sse_streamux_create_server(socket_path, backlog) ;
    if (fdh.sfd < 0) {
        sse_free(&fdh.epoll) ;
        log_warnusys_return(0, "create server socket: ", socket_path) ;
    }

    if (!sse_start_io(&fdh.epoll, &fdh.wserver, server_accept_cb, NULL, fdh.sfd, SSE_READ, 0)) {
        close_fd(fdh.sfd) ;
        sse_free(&fdh.epoll) ;
        log_warnusys_return(0, "start server watcher") ;
    }

    fdh.socket_path = socket_path ;
    log_info("fdholder daemon listening on ", socket_path) ;

    return 1 ;
}

static void server_cleanup(void)
{
    log_flow() ;

    fdholder_entry_t *e, *te ;
    HASH_FOREACH(&fdh.entries, e, te) {
        hash_del(&fdh.entries, e) ;
        close_fd(e->fd) ;
        free(e) ;
    }
    hash_free(&fdh.entries) ;

    fdholder_pipe_t *fp, *tfp ;
    HASH_FOREACH(&fdh.pipes, fp, tfp) {
        hash_del(&fdh.pipes, fp) ;
        close_fd(fp->rfd) ;
        close_fd(fp->wfd) ;
        free(fp) ;
    }
    hash_free(&fdh.pipes) ;

    fdholder_conn_t *c, *tc ;
    HASH_FOREACH(&fdh.conns, c, tc) {
        conn_destroy(c) ;
    }
    hash_free(&fdh.conns) ;

    if (fdh.timer_started)
        sse_free_timer(&fdh.wtimer) ;

    sse_free_io(&fdh.wserver) ;
    sse_free_signal(&fdh.wsignal) ;
    sse_free(&fdh.epoll) ;

    if (fdh.socket_path)
        file_tryunlink(fdh.socket_path) ;

    log_info("fdholder daemon stopped") ;
}

int main(int argc, char const *const *argv)
{
    log_flow() ;

    int notif = -1 ;
    int backlog = FDHOLDER_BACKLOG_DEFAULT ;

    PROG = "66-fdholderd" ;

    // keep 0/1/2 reserved so accepted client sockets never land on them
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
                {
                    uint32_t f ;
                    /* 0/1/2 are reserved by the daemon (stdio + logging) ; the
                     * readiness fd is an out-of-band channel, always >= 3 */
                    if (!u32_scan_strict(st.arg, &f) || f < 3)
                        return opt_emit_usage(cmd.name, &cmd) ;
                    notif = (int)f ;
                    break ;
                }
                case 'n' :
                    if (!u32_scan_strict(st.arg, &fdh.maxfds))
                        return opt_emit_usage(cmd.name, &cmd) ;
                    break ;
                case 'b' :
                {
                    uint32_t b ;
                    if (!u32_scan_strict(st.arg, &b))
                        return opt_emit_usage(cmd.name, &cmd) ;
                    backlog = (int)b ;
                    break ;
                }
                default :
                    return opt_emit_error(cmd.name, &cmd, o, &st) ;
            }
        }
        argc -= st.ind ; argv += st.ind ;
    }

    if (argc < 1)
        return opt_emit_usage(cmd.name, &cmd) ;

    if (!fdh.maxfds)
        fdh.maxfds = FDHOLDER_MAXFDS_DEFAULT ;

    fdh.owner = geteuid() ;

    if (!hash_init(&fdh.entries, 0, offsetof(fdholder_entry_t, node))
     || !hash_init(&fdh.pipes, 0, offsetof(fdholder_pipe_t, node))
     || !hash_init(&fdh.conns, 0, offsetof(fdholder_conn_t, node)))
        log_dieusys(LOG_EXIT_SYS, "initialize hash tables") ;

    if (!server_init(argv[0], backlog))
        log_dieu(LOG_EXIT_SYS, "initialize fdholder daemon") ;

    if (notif >= 0 && write(notif, "\n", 1) < 0)
        log_dieusys(LOG_EXIT_SYS, "notify readiness") ;

    int r = sse_poll(&fdh.epoll, SSE_TIMEOUT_INFINITE) ;

    server_cleanup() ;

    return r ? 0 : LOG_EXIT_SYS ;
}
