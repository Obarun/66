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
#include <oblibs/linux.h>
#include <oblibs/types.h>
#include <oblibs/opt.h>
#include <oblibs/sse.h>
#include <oblibs/sse_stream.h>
#include <oblibs/stream_message.h>
#include <oblibs/io_rb.h>
#include <oblibs/hash.h>
#include <oblibs/fd.h>
#include <oblibs/files.h>
#include <oblibs/socket.h>
#include <oblibs/string.h>
#include <oblibs/clock.h>
#include <oblibs/environ.h>
#include <oblibs/spawn.h>

#include <66/oneshot.h>
#include <66/status.h>
#include <66/constants.h>
#include <66/utils.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/event.h>

typedef struct oneshot_job_s oneshot_job_t ;
struct oneshot_job_s
{
    sse_watcher_t wchild ;
    sse_watcher_t wtimer ; // the [Start] or [Stop] Timeout of the script, fd -1 when not armed
    bool timedout ; // the script was killed at its timeout
    uint32_t tstop ; // the [Stop] Timeout, between the SIGTERM and the SIGKILL of a way up that timed out
    pid_t pid ;
    struct oneshot_client_s *conn ; // NULL once the client is gone
    uint8_t down ;  // ran the finish (1) or run (0) script
    uint8_t who ;   // status_who_e to stamp into the status
    char servicedir[SS_MAX_PATH_LEN] ; // allocated inline with the job, used to write the status
} ;

typedef struct oneshot_client_s oneshot_client_t ;
struct oneshot_client_s
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
    { .id = 'v',         .shortname = 'v', .longname = "verbosity", .arg = OPT_REQUIRED, .argname = "number", .help = "increase/decrease verbosity" },
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

// status

static void oneshotd_write_status(char const *servicedir, uint8_t down, uint8_t who, uint8_t result, uint32_t code)
{
    log_flow() ;

    service_status_t st = STATUS_ZERO ;

    if (result == STATUS_RESULT_SUCCESS) {
        st.state = down ? STATUS_STATE_DOWN : STATUS_STATE_DONE ;
        st.result = STATUS_RESULT_SUCCESS ;
        // a oneshot reaching done is its transition to ready
        if (!down)
            clock_now(&st.readystamp) ;
    } else if (result == STATUS_RESULT_TIMEOUT_START || result == STATUS_RESULT_TIMEOUT_STOP) {
        st.state = STATUS_STATE_DOWN ;
        st.result = result ;
    } else {
        st.state = STATUS_STATE_FAILED ;
        st.result = result ;
        st.code = code ;
    }
    st.who = who ;
    clock_now(&st.stamp) ;

    char file[strlen(servicedir) + SS_SUPERVISEDIR_LEN + 1 + SS_STATUS_LEN + 1] ;
    auto_strings(file, servicedir, SS_SUPERVISEDIR, "/", SS_STATUS) ;

    if (!status_write(&st, file))
        log_warnusys("write runtime status of: ", servicedir) ;

    char ev[strlen(servicedir) + SS_EVENTDIR_LEN + 1] ;
    auto_strings(ev, servicedir, SS_EVENTDIR) ;

    if (!event_emit_transition(ev, st.state, st.result, st.who, st.code, 0, &st.stamp, 0))
        log_warnusys("emit the transition of: ", servicedir) ;
}

// responses

static void respond(oneshot_client_t *conn, uint8_t status, void const *payload, size_t paylen)
{
    char hdr[ONESHOT_HDR_SIZE] ;
    oneshot_hdr_pack(hdr, ONESHOT_CMD_RESPONSE, status, 0, (uint32_t)paylen) ;
    stream_message_send(conn->stream, hdr, ONESHOT_HDR_SIZE, payload, paylen) ;
}

static void conclude(oneshot_client_t *conn, char const *servicedir, uint8_t down, uint8_t who, uint8_t result, uint32_t code)
{
    log_flow() ;

    oneshotd_write_status(servicedir, down, who, result, code) ;

    if (!conn)
        return ;

    char pl[ONESHOT_VERDICT_LEN] ;
    pl[0] = (char)result ;
    u32_pack_big(pl + 1, code) ;
    respond(conn, ONESHOT_OK, pl, ONESHOT_VERDICT_LEN) ;
}

// child reaping

static void job_release(oneshot_job_t *job)
{
    if (job->wtimer.fd >= 0)
        sse_free_timer(&job->wtimer) ;

    if (!sse_defer(&osd.epoll, &free, job)) {
        log_warnusys("defer the release of a job") ;
        free(job) ;
    }
}

static void oneshot_timeout_cb(sse_watcher_t *w, void *data, int event)
{
    log_flow() ;

    (void)event ;

    oneshot_job_t *job = data ;

    // freed by the reap or the close of the client earlier in this dispatch
    if (w->fd < 0)
        return ;

    if (w->api_errno != 0) {
        log_warn("timer watcher error: ", strerror(w->api_errno)) ;
        sse_free_timer(w) ;
        return ;
    }

    /** a way up that timed out goes down the way of a classic: SIGTERM, then
     * SIGKILL once the [Stop] Timeout has passed, 0 meaning without end */
    if (!job->down && !job->timedout) {

        job->timedout = true ;
        kill(-job->pid, SIGTERM) ;

        if (!job->tstop) {
            sse_free_timer(w) ;
            return ;
        }

        if (sse_modify_timer(w, (int)job->tstop, 0))
            return ;

        log_warnusys("arm the stop timeout of: ", job->servicedir, " -- killing its script now") ;
    }

    job->timedout = true ;
    sse_free_timer(w) ;
    kill(-job->pid, SIGKILL) ;
}

static void oneshot_child_cb(sse_watcher_t *w, void *data, int event)
{
    log_flow() ;

    oneshot_job_t *job = data ;

    if (w->api_errno != 0) {
        log_warn("child watcher error: ", strerror(w->api_errno)) ;
        sse_free_child(w) ;
        oneshotd_write_status(job->servicedir, job->down, job->who, STATUS_RESULT_EXITED, 0) ;
        if (job->conn) {
            job->conn->job = NULL ;
            respond(job->conn, ONESHOT_ERR, NULL, 0) ;
        }
        job_release(job) ;
        return ;
    }

    if (!(event & SSE_READ))
        return ;

    sse_child_t *cd = (sse_child_t *)w->sdata ;
    uint32_t wstat = cd ? (uint32_t)cd->status : 0 ;
    sse_free_child(w) ;

    uint8_t result = STATUS_RESULT_SUCCESS ;
    uint32_t code = 0 ;
    if (job->timedout) {
        result = job->down ? STATUS_RESULT_TIMEOUT_STOP : STATUS_RESULT_TIMEOUT_START ;
    } else if (WIFSIGNALED(wstat)) {
        result = STATUS_RESULT_SIGNALED ;
        code = (uint32_t)WTERMSIG(wstat) ;
    } else if (WEXITSTATUS(wstat)) {
        result = STATUS_RESULT_EXITED ;
        code = (uint32_t)WEXITSTATUS(wstat) ;
    }

    if (job->conn)
        job->conn->job = NULL ;

    conclude(job->conn, job->servicedir, job->down, job->who, result, code) ;

    job_release(job) ;
}

static void oneshot_timeouts(char const *servicedir, uint32_t *start, uint32_t *stop)
{
    log_flow() ;

    size_t len = strlen(servicedir) ;
    char name[len + 1] ;
    resolve_service_addon_execute_t ex = RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    resolve_wrapper_t wex = RESOLVE_WRAPPER(DATA_SERVICE_EXECUTE, &ex) ;

    *start = *stop = 0 ;

    if (!ob_basename(name, servicedir)) {
        log_warnusys("get the name of: ", servicedir, " -- its script is not timed") ;
        return ;
    }

    char exname[strlen(name) + SS_ADDON_EXECUTE_SUFFIX_LEN + 1] ;
    auto_strings(exname, name, SS_ADDON_EXECUTE_SUFFIX) ;

    if (resolve_read_at(&wex, servicedir, exname) <= 0)
        log_warnusys("read execute addon of: ", servicedir, " -- its script is not timed") ;
    else {
        *start = ex.timeout.start ;
        *stop = ex.timeout.stop ;
    }

    resolve_free_obj(&wex) ;
}

// handlers

static void handle_run(oneshot_client_t *conn, uint8_t flags, uint8_t who, char const *pl, size_t pll, int const *afd, int nfd)
{
    log_flow() ;

    if (nfd != 3 || pll == 0 || pll >= SS_MAX_PATH_LEN || pl[0] != '/' || conn->job || who >= STATUS_WHO_ENDOFKEY) {
        close_all(afd, nfd) ;
        respond(conn, ONESHOT_PROTO, NULL, 0) ;
        return ;
    }

    char servicedir[pll + 1] ;
    memcpy(servicedir, pl, pll) ;
    servicedir[pll] = 0 ;
    uint8_t down = (flags & ONESHOT_FLAG_DOWN) != 0 ;

    char const *file = down ? "finish" : "run" ;
    char script[pll + 1 + 6 + 1] ;
    auto_strings(script, servicedir, "/", file) ;

    /* a oneshot with no finish script has nothing to bring down: report success
     * (DOWN status) without spawning, so no script exit code is fabricated */
    if (down && access(script, F_OK) < 0) {
        close_all(afd, nfd) ;
        conclude(conn, servicedir, down, who, STATUS_RESULT_SUCCESS, 0) ;
        return ;
    }

    uint32_t tstart, tstop ;
    oneshot_timeouts(servicedir, &tstart, &tstop) ;

    /** the script leads its own session, so that a kill of its process group reaches
     * every process it forks, and gets the received descriptors as its 0/1/2 */
    char const *argv[3] = { script, file, 0 } ;
    spawn_fa_t fa[3] = {
        { .type = SPAWN_FA_MOVE, .from = afd[0], .to = 0 },
        { .type = SPAWN_FA_MOVE, .from = afd[1], .to = 1 },
        { .type = SPAWN_FA_MOVE, .from = afd[2], .to = 2 }
    } ;

    pid_t pid = spawn_path_full(script, argv, (char const *const *)environ, 0, 0, SPAWN_FLAG_SETSID, fa, 3) ;
    int e = errno ;

    close_all(afd, nfd) ;

    if (!pid) {
        errno = e ;
        log_warnusys("spawn: ", script) ;
        conclude(conn, servicedir, down, who, STATUS_RESULT_EXEC_FAILED, (uint32_t)e) ;
        return ;
    }

    oneshot_job_t *job = malloc(sizeof(*job) + pll + 1) ;
    if (!job) {
        log_warnusys("allocate job") ;
        kill(-pid, SIGKILL) ;
        waitpid(pid, NULL, 0) ;
        oneshotd_write_status(servicedir, down, who, STATUS_RESULT_EXITED, 0) ;
        respond(conn, ONESHOT_ERR, NULL, 0) ;
        return ;
    }
    job->wchild = (sse_watcher_t)SSE_WATCHER_ZERO ;
    job->wtimer = (sse_watcher_t)SSE_WATCHER_ZERO ;
    job->timedout = false ;
    job->tstop = tstop ;
    job->pid = pid ;
    job->conn = conn ;
    job->down = down ;
    job->who = who ;
    memcpy(job->servicedir, servicedir, pll + 1) ;
    conn->job = job ;

    if (!sse_start_child(&osd.epoll, &job->wchild, oneshot_child_cb, job, pid, 2, true)) {
        log_warnusys("start child watcher") ;
        kill(-pid, SIGKILL) ;
        waitpid(pid, NULL, 0) ;
        conn->job = NULL ;
        free(job) ;
        oneshotd_write_status(servicedir, down, who, STATUS_RESULT_EXITED, 0) ;
        respond(conn, ONESHOT_ERR, NULL, 0) ;
        return ;
    }

    uint32_t timeout = down ? tstop : tstart ;
    if (timeout && !sse_start_timer(&osd.epoll, &job->wtimer, oneshot_timeout_cb, job, (int)timeout, 0, 1))
        log_warnusys("start the timer of: ", servicedir, " -- its script is not timed") ;

    flog_info("running %s script of '%s' (pid %d)", down ? "finish" : "run", servicedir, (int)pid) ;
}

static void handle_message(io_rb_iovec_t *msg, void *ctx)
{
    log_flow() ;

    oneshot_client_t *conn = ctx ;

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
    uint8_t who = (uint8_t)hdr[2] ;
    uint8_t flags = (uint8_t)hdr[3] ;
    char const *pl = msg->niov > 1 ? msg->iov[1].iov_base : NULL ;
    size_t pll = msg->niov > 1 ? msg->iov[1].iov_len : 0 ;

    if (command != ONESHOT_CMD_RUN) {
        close_all(msg->afd, msg->nfd) ;
        respond(conn, ONESHOT_PROTO, NULL, 0) ;
        return ;
    }

    handle_run(conn, flags, who, pl, pll, msg->afd, msg->nfd) ;
}

// connection

static void client_destroy(oneshot_client_t *conn)
{
    if (!conn || conn->closing)
        return ;

    conn->closing = true ;
    hash_del(&osd.conns, conn) ;

    /* detach the in-flight job and kill its script: oneshot_child_cb still owns
     * the watcher and will reap the child and free the job (conn now NULL) */
    if (conn->job) {
        conn->job->conn = NULL ;
        if (conn->job->wtimer.fd >= 0)
            sse_free_timer(&conn->job->wtimer) ;
        kill(-conn->job->pid, SIGKILL) ;
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

static void client_read_cb(sse_stream_t *stream, void *data)
{
    log_flow() ;
    (void)stream ;

    oneshot_client_t *conn = data ;
    if (!conn || conn->closing)
        return ;

    for (;;) {
        int r = stream_message_read(&conn->reader, oneshot_parse_header) ;
        if (r < 0) {
            flog_warnsys("read from client fd %d", conn->key) ;
            client_destroy(conn) ;
            return ;
        }
        if (r == 0)
            return ;
    }
}

static void client_write_cb(sse_stream_t *stream, void *data)
{
    log_flow() ;
    (void)stream ; (void)data ;
}

static void client_close_cb(sse_stream_t *stream, void *data)
{
    log_flow() ;
    (void)stream ;
    client_destroy(data) ;
}

static void client_error_cb(sse_stream_t *stream, int error, void *data)
{
    log_flow() ;
    (void)stream ;

    errno = error ;
    flog_warnusys("client fd %d", ((oneshot_client_t *)data)->key) ;
    client_destroy(data) ;
}

static int client_create(int fd)
{
    log_flow() ;

    if (hash_count(&osd.conns) >= ONESHOT_MAXCLIENTS_DEFAULT) {
        close_fd(fd) ;
        log_warnsys_return(LOG_EXIT_ZERO, "too many clients - refusing connection") ;
    }

    oneshot_client_t *conn = calloc(1, sizeof(*conn)) ;
    if (!conn) {
        close_fd(fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "allocate client") ;
    }

    conn->key = fd ;
    conn->stream = sse_stream_new(fd, client_read_cb, client_write_cb, client_close_cb, client_error_cb, conn) ;
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
    flog_trace("client connected on fd %d", fd) ;

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

        client_create(fd) ;
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

    if (!sse_attach_signal(&osd.wsignal, SIGTERM) ||
        !sse_attach_signal(&osd.wsignal, SIGINT)) {
        sse_free(&osd.epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "set signals") ;
    }

    int fdlock ;
    osd.sfd = sse_streamux_create_server(socket_path, backlog, &fdlock) ;
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

    oneshot_client_t *c, *tc ;
    HASH_FOREACH(&osd.conns, c, tc) {
        client_destroy(c) ;
    }
    hash_free(&osd.conns) ;

    sse_free_io(&osd.wserver) ;
    sse_free_signal(&osd.wsignal) ;
    sse_free(&osd.epoll) ;

    if (osd.socket_path)
        file_tryunlink(osd.socket_path) ;

    log_info("oneshot daemon stopped") ;
}

int main(int argc, char const *const *argv)
{
    log_flow() ;

    int notif = -1 ;
    int backlog = ONESHOT_BACKLOG_DEFAULT ;

    PROG = "66-oneshotd" ;

    // before anything is written: the catch-all fifo may have no reader yet
    if (!lx_signalfd_ignore(SIGPIPE))
        log_dieusys(LOG_EXIT_SYS, "ignore SIGPIPE") ;

    if (set_verbosity_from_env() < 0)
        log_warnusys("import VERBOSITY from the environment") ;

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

    if (!hash_init(&osd.conns, 0, offsetof(oneshot_client_t, node)))
        log_dieusys(LOG_EXIT_SYS, "initialize connection table") ;

    if (!server_init(argv[0], backlog))
        log_dieu(LOG_EXIT_SYS, "initialize oneshot daemon") ;

    if (io_write(notif, "\n", 1) < 0)
        log_dieusys(LOG_EXIT_SYS, "notify readiness") ;

    int r = sse_poll(&osd.epoll, SSE_TIMEOUT_INFINITE) ;

    server_cleanup() ;

    return r ? 0 : LOG_EXIT_SYS ;
}
