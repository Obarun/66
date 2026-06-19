/*
 * 66-oneshotd.c
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
 * Oneshot runner daemon: on request, forks the run/finish script of a service
 * directory with the client's 0/1/2 wired onto it, and hands the script's
 * waitpid status back over the Unix socket.
 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <limits.h> // INT_MAX

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

#include <66/oneshot.h>
#include <66/constants.h>

typedef struct oneshot_job_s oneshot_job_t ;
struct oneshot_job_s
{
    sse_watcher_t wchild ;
    pid_t pid ;
    struct oneshot_conn_s *conn ; // NULL once the client is gone
} ;

typedef struct oneshot_conn_s oneshot_conn_t ;
struct oneshot_conn_s
{
    int key ; // stream fd, hash key
    sse_stream_t *stream ;
    stream_message_t reader ;
    char *hdrbuf ;
    char *paybuf ;
    oneshot_job_t *job ; // in-flight RUN, NULL if none
    bool closing ;
    hash_node_t node ;
} ;

typedef struct oneshot_daemon_s oneshot_daemon_t ;
struct oneshot_daemon_s
{
    sse_epoll_t epoll ;
    sse_watcher_t wserver ;
    sse_watcher_t wsignal ;
    int sfd ;
    char const *socket_path ;
    uid_t owner ;
    hash_t conns ; // hash table by fd
} ;

static oneshot_daemon_t osd = {
    .epoll = SSE_EPOLL_ZERO,
    .wserver = SSE_WATCHER_ZERO,
    .wsignal = SSE_WATCHER_ZERO,
    .sfd = -1,
    .socket_path = NULL,
    .owner = 0,
    .conns = HASH_ZERO
} ;

static opt_t const opts[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",    .arg = OPT_NONE,                          .help = "print this help" },
    { .id = 'v',         .shortname = 'v', .longname = "verbose", .arg = OPT_REQUIRED, .argname = "number", .help = "increase/decrease verbosity" },
    { .id = 'd',         .shortname = 'd', .longname = "notify",  .arg = OPT_REQUIRED, .argname = "number", .help = "notify readiness on file descriptor fd (>= 3)" },
    { .id = 'b',         .shortname = 'b', .longname = "backlog", .arg = OPT_REQUIRED, .argname = "number", .help = "listen backlog" },
} ;

static opt_cmd_t const cmd = {
    .name = "66-oneshotd",
    .operands = "socket",
    .opts = opts,
    .nopts = OPT_COUNT(opts),
} ;

static void close_all(int const *afd, int nfd)
{
    for (int i = 0 ; i < nfd ; i++)
        if (afd[i] >= 0)
            close_fd(afd[i]) ;
}

// responses

static void respond(oneshot_conn_t *conn, uint8_t status, void const *payload, size_t paylen)
{
    char hdr[ONESHOT_HDR_SIZE] ;
    oneshot_hdr_pack(hdr, ONESHOT_CMD_RESPONSE, status, 0, (uint32_t)paylen) ;
    stream_message_send(conn->stream, hdr, ONESHOT_HDR_SIZE, payload, paylen) ;
}

// child reaping

static void oneshot_child_cb(sse_watcher_t *w, void *data, int event)
{
    log_flow() ;

    oneshot_job_t *job = data ;

    if (w->api_errno != 0) {
        log_warn("child watcher error: ", strerror(w->api_errno)) ;
        sse_free_child(w) ;
        if (job->conn) {
            job->conn->job = NULL ;
            respond(job->conn, ONESHOT_ERR, NULL, 0) ;
        }
        free(job) ;
        return ;
    }

    if (!(event & SSE_READ))
        return ;

    sse_child_t *cd = (sse_child_t *)w->sdata ;
    uint32_t wstat = cd ? (uint32_t)cd->status : 0 ;
    sse_free_child(w) ;

    oneshot_conn_t *conn = job->conn ;
    if (conn) {
        char pl[4] ;
        u32_pack_big(pl, wstat) ;
        respond(conn, ONESHOT_OK, pl, 4) ;
        conn->job = NULL ;
    }

    free(job) ;
}

// handlers

static void handle_run(oneshot_conn_t *conn, uint8_t flags, char const *pl, size_t pll, int const *afd, int nfd)
{
    log_flow() ;

    if (nfd != 3 || pll == 0 || pll >= SS_MAX_PATH_LEN || pl[0] != '/' || conn->job) {
        close_all(afd, nfd) ;
        respond(conn, ONESHOT_PROTO, NULL, 0) ;
        return ;
    }

    char servicedir[pll + 1] ;
    memcpy(servicedir, pl, pll) ;
    servicedir[pll] = 0 ;
    uint8_t down = (flags & ONESHOT_FLAG_DOWN) != 0 ;

    /* copy the received descriptors off the stack-borrowed afd array */
    int cfd[3] = { afd[0], afd[1], afd[2] } ;

    pid_t pid = fork() ;
    if (pid < 0) {
        log_warnusys("fork") ;
        close_all(cfd, 3) ;
        respond(conn, ONESHOT_ERR, NULL, 0) ;
        return ;
    }

    if (!pid) {

        /* reset the signal mask: the daemon blocks SIGTERM/SIGINT/SIGCHLD through
         * its signalfds, and the script must run with default signal handling */
        sigset_t empty ;
        sigemptyset(&empty) ;
        sigprocmask(SIG_SETMASK, &empty, 0) ;

        for (int i = 0 ; i < 3 ; i++) {
            if (dup2(cfd[i], i) < 0)
                _exit(126) ;
        }

        for (int i = 0 ; i < 3 ; i++) {
            if (cfd[i] >= 3)
                close(cfd[i]) ;
        }

        oneshot_exec_script(servicedir, down) ; // never returns on success

        _exit(126) ;
    }

    // parent: the child holds its own copies now
    close_all(cfd, 3) ;

    oneshot_job_t *job = malloc(sizeof(*job)) ;
    if (!job) {
        log_warnusys("allocate job") ;
        kill(pid, SIGKILL) ;
        waitpid(pid, NULL, 0) ;
        respond(conn, ONESHOT_ERR, NULL, 0) ;
        return ;
    }
    job->pid = pid ;
    job->conn = conn ;
    conn->job = job ;

    if (!sse_start_child(&osd.epoll, &job->wchild, oneshot_child_cb, job, pid, 2, true)) {
        log_warnusys("start child watcher") ;
        kill(pid, SIGKILL) ;
        waitpid(pid, NULL, 0) ;
        conn->job = NULL ;
        free(job) ;
        respond(conn, ONESHOT_ERR, NULL, 0) ;
        return ;
    }

    flog_info("running %s script of '%s' (pid %d)", down ? "finish" : "run", servicedir, (int)pid) ;
}

static void handle_message(io_rb_iovec_t *msg, void *ctx)
{
    log_flow() ;

    oneshot_conn_t *conn = ctx ;

    if (!conn || conn->closing) {
        close_all(msg->afd, msg->nfd) ;
        return ;
    }

    if (msg->niov < 1) {
        close_all(msg->afd, msg->nfd) ;
        respond(conn, ONESHOT_PROTO, NULL, 0) ;
        return ;
    }

    char const *hdr = msg->iov[0].iov_base ;
    uint8_t command = (uint8_t)hdr[1] ;
    uint8_t flags = (uint8_t)hdr[3] ;
    char const *pl = msg->niov > 1 ? msg->iov[1].iov_base : NULL ;
    size_t pll = msg->niov > 1 ? msg->iov[1].iov_len : 0 ;

    if (command != ONESHOT_CMD_RUN) {
        close_all(msg->afd, msg->nfd) ;
        respond(conn, ONESHOT_PROTO, NULL, 0) ;
        return ;
    }

    handle_run(conn, flags, pl, pll, msg->afd, msg->nfd) ;
}

// connection

static void conn_destroy(oneshot_conn_t *conn)
{
    if (!conn || conn->closing)
        return ;

    conn->closing = true ;
    hash_del(&osd.conns, conn) ;

    /* detach the in-flight job and kill its script: oneshot_child_cb still owns
     * the watcher and will reap the child and free the job (conn now NULL) */
    if (conn->job) {
        conn->job->conn = NULL ;
        kill(conn->job->pid, SIGKILL) ;
        conn->job = NULL ;
    }

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

    oneshot_conn_t *conn = data ;
    if (!conn || conn->closing)
        return ;

    for (;;) {
        int r = stream_message_read(&conn->reader, oneshot_parse_header) ;
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
    (void)stream ; (void)data ;
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
    flog_warnusys("client fd %d", ((oneshot_conn_t *)data)->key) ;
    conn_destroy(data) ;
}

static int conn_create(int fd)
{
    log_flow() ;

    if (hash_count(&osd.conns) >= ONESHOT_MAXCLIENTS_DEFAULT) {
        close_fd(fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "too many clients - refusing connection") ;
    }

    oneshot_conn_t *conn = calloc(1, sizeof(*conn)) ;
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

    conn->hdrbuf = malloc(ONESHOT_HDR_SIZE) ;
    conn->paybuf = malloc(IO_RB_BUFFER_SIZE) ;
    if (!conn->hdrbuf || !conn->paybuf) {
        free(conn->hdrbuf) ;
        free(conn->paybuf) ;
        sse_stream_close(conn->stream) ;
        free(conn) ;
        log_warnusys_return(LOG_EXIT_ZERO, "allocate client buffers") ;
    }

    if (!stream_message_reader_init(&conn->reader, conn->stream, handle_message, conn->hdrbuf, ONESHOT_HDR_SIZE, conn->paybuf, IO_RB_BUFFER_SIZE, conn)) {
        sse_stream_close(conn->stream) ;
        free(conn->hdrbuf) ;
        free(conn->paybuf) ;
        free(conn) ;
        log_warnusys_return(LOG_EXIT_ZERO, "initialize message reader") ;
    }

    if (!sse_stream_attach(conn->stream, &osd.epoll, 0)) {
        sse_stream_close(conn->stream) ;
        free(conn->hdrbuf) ;
        free(conn->paybuf) ;
        free(conn) ;
        log_warnusys_return(LOG_EXIT_ZERO, "attach client stream") ;
    }

    hash_add(&osd.conns, &conn->key, sizeof conn->key, conn) ;
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
        osd.epoll.running = false ;
        return ;
    }

    if (revents & SSE_READ) {
        int fd = sse_streamux_accept(osd.sfd) ;
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

        if (cred.uid != osd.owner) {
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
        osd.epoll.running = false ;
        return ;
    }

    sse_signal_t *sig = (sse_signal_t *)w->sdata ;
    flog_info("received signal %d - shutting down", sig->si.ssi_signo) ;
    osd.epoll.running = false ;
}

// daemon

static int server_init(char const *socket_path, int backlog)
{
    log_flow() ;

    if (!sse_new(&osd.epoll, ONESHOT_MAXCLIENTS_DEFAULT))
        log_warnusys_return(LOG_EXIT_ZERO, "create event loop") ;

    if (!sse_start_signal(&osd.epoll, &osd.wsignal, server_signal_cb, NULL, 10)) {
        sse_free(&osd.epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "start signal watcher") ;
    }

    if (!sse_ignore_signal(&osd.wsignal, SIGPIPE) ||
        !sse_attach_signal(&osd.wsignal, SIGTERM) ||
        !sse_attach_signal(&osd.wsignal, SIGINT)) {
        sse_free(&osd.epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "set signals") ;
    }

    osd.sfd = sse_streamux_create_server(socket_path, backlog) ;
    if (osd.sfd < 0) {
        sse_free(&osd.epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "create server socket: ", socket_path) ;
    }

    if (!sse_start_io(&osd.epoll, &osd.wserver, server_accept_cb, NULL, osd.sfd, SSE_READ, 0)) {
        close_fd(osd.sfd) ;
        sse_free(&osd.epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "start server watcher") ;
    }

    osd.socket_path = socket_path ;
    log_info("oneshot daemon listening on ", socket_path) ;

    return 1 ;
}

static void server_cleanup(void)
{
    log_flow() ;

    oneshot_conn_t *c, *tc ;
    HASH_FOREACH(&osd.conns, c, tc) {
        conn_destroy(c) ;
    }
    hash_free(&osd.conns) ;

    sse_free_io(&osd.wserver) ;
    sse_free_signal(&osd.wsignal) ;
    sse_free(&osd.epoll) ;

    if (osd.socket_path)
        file_tryunlink(osd.socket_path) ;

    log_info("oneshot daemon stopped") ;
}

static int notifier_isvalid(const char *str)
{
	uint32_t u ;

	if (!u32_scan_strict(str, &u) || u > INT_MAX)
		log_die(LOG_EXIT_USER, "invalid notification file descriptor: ", str) ;

	if (u < 3)
		log_die(LOG_EXIT_USER, "file descriptor must be 3 or more") ;

	if (fcntl(u, F_GETFD) < 0)
		log_diesys(LOG_EXIT_USER, "invalid file descriptor") ;

	return (int)u ;
}

int main(int argc, char const *const *argv)
{
    log_flow() ;

    int notif = -1 ;
    int backlog = ONESHOT_BACKLOG_DEFAULT ;

    PROG = "66-oneshotd" ;

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
                    notif = notifier_isvalid(st.arg) ;
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

    osd.owner = geteuid() ;

    if (!hash_init(&osd.conns, 0, offsetof(oneshot_conn_t, node)))
        log_dieusys(LOG_EXIT_SYS, "initialize connection table") ;

    if (!server_init(argv[0], backlog))
        log_dieu(LOG_EXIT_SYS, "initialize oneshot daemon") ;

    if (notif >= 0 && write(notif, "\n", 1) < 0)
        log_dieusys(LOG_EXIT_SYS, "notify readiness") ;

    int r = sse_poll(&osd.epoll, SSE_TIMEOUT_INFINITE) ;

    server_cleanup() ;

    return r ? 0 : LOG_EXIT_SYS ;
}
