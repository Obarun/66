/*
 * fdholder_client.c
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 *
 * Synchronous request/response client built on top of the asynchronous SSE
 * stream machinery: a request is queued, the event loop flushes and reads the
 * reply, and a guard timer bounds the wait.
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/sse.h>
#include <oblibs/sse_stream.h>
#include <oblibs/stream_message.h>
#include <oblibs/io_rb.h>
#include <oblibs/fd.h>

#include <66/fdholder.h>

static void client_response_handler(io_rb_iovec_t *msg, void *data)
{
    log_flow() ;

    fdholder_client_t *c = data ;
    c->received_fd = -1 ;
    c->resp_payload_len = 0 ;

    if (msg->niov < 1) {
        c->status = FDHOLDER_ERR ;
        goto done ;
    }

    char const *hdr = msg->iov[0].iov_base ;
    uint8_t command = (uint8_t)hdr[1] ;
    uint8_t status = (uint8_t)hdr[2] ;

    if (command != FDHOLDER_CMD_RESPONSE) {
        for (int i = 0 ; i < msg->nfd ; i++)
            close(msg->afd[i]) ;
        c->status = FDHOLDER_ERR ;
        goto done ;
    }

    c->status = status ;

    if (msg->nfd > 0) {
        c->received_fd = msg->afd[0] ;
        for (int i = 1 ; i < msg->nfd ; i++)   /* protocol hands out at most one */
            close(msg->afd[i]) ;
    }

    /* a list reply leaves its NUL-separated names sitting in paybuf */
    if (msg->niov > 1 && msg->iov[1].iov_len > 0)
        c->resp_payload_len = msg->iov[1].iov_len ;

 done:
    c->response_received = true ;
    c->epoll.running = false ;
}

static void client_read_cb(sse_stream_t *stream, void *data)
{
    log_flow() ;

    fdholder_client_t *c = data ;

    for (;;) {
        int r = stream_message_read(&c->reader, fdholder_parse_header) ;
        if (r < 0) {
            log_warnusys("read response from daemon") ;
            c->status = FDHOLDER_ERR ;
            c->response_received = true ;
            c->epoll.running = false ;
            sse_stream_close(stream) ;
            return ;
        }
        if (!r)
            return ;
        if (c->response_received)
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

    fdholder_client_t *c = data ;
    if (c->request_sent && !c->response_received) {
        log_warn("connection closed before a response was received") ;
        c->status = FDHOLDER_ERR ;
    }
    c->epoll.running = false ;
}

static void client_error_cb(sse_stream_t *stream, int error, void *data)
{
    log_flow() ;
    (void)stream ;

    fdholder_client_t *c = data ;
    errno = error ;
    log_warnusys("communicate with fdholder daemon") ;
    c->status = FDHOLDER_ERR ;
    c->epoll.running = false ;
}

static void client_signal_cb(sse_watcher_t *w, void *data, int revents)
{
    log_flow() ;
    (void)revents ;

    fdholder_client_t *c = data ;
    sse_signal_t *sig = (sse_signal_t *)w->sdata ;
    flog_info("received signal %d", sig->si.ssi_signo) ;
    c->epoll.running = false ;
}

static void client_timeout_cb(sse_watcher_t *w, void *data, int revents)
{
    log_flow() ;
    (void)w ; (void)revents ;

    fdholder_client_t *c = data ;
    log_warn("timeout for the fdholder daemon") ;
    c->epoll.running = false ;
}

int fdholder_client_init(fdholder_client_t *c, char const *socket)
{
    log_flow() ;

    memset(c, 0, sizeof(*c)) ;
    c->received_fd = -1 ;

    if (!sse_new(&c->epoll, SSE_MAX_EVENTS))
        log_warnusys_return(LOG_EXIT_ZERO, "create event loop") ;

    if (!sse_start_signal(&c->epoll, &c->wsignal, client_signal_cb, c, 10)) {
        sse_free(&c->epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "start signal watcher") ;
    }

    if (!sse_ignore_signal(&c->wsignal, SIGPIPE) ||
        !sse_attach_signal(&c->wsignal, SIGTERM) ||
        !sse_attach_signal(&c->wsignal, SIGINT)) {
        sse_free(&c->epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "set signals") ;
    }

    int fd = sse_streamux_create_client(socket) ;
    if (fd < 0) {
        sse_free(&c->epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "connect to fdholder daemon: ", socket) ;
    }

    c->stream = sse_stream_new(fd, client_read_cb, client_write_cb, client_close_cb, client_error_cb, c) ;
    if (!c->stream) {
        close_fd(fd) ;
        sse_free(&c->epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "create stream") ;
    }

    c->hdrbuf = malloc(FDHOLDER_HDR_SIZE) ;
    c->paybuf = malloc(IO_RB_BUFFER_SIZE) ;
    if (!c->hdrbuf || !c->paybuf) {
        free(c->hdrbuf) ; free(c->paybuf) ; c->hdrbuf = c->paybuf = NULL ;
        sse_stream_close(c->stream) ; c->stream = NULL ;
        sse_free(&c->epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "allocate client buffers") ;
    }

    if (!stream_message_reader_init(&c->reader, c->stream, client_response_handler, c->hdrbuf, FDHOLDER_HDR_SIZE, c->paybuf, IO_RB_BUFFER_SIZE, c)) {
        sse_stream_close(c->stream) ; c->stream = NULL ;
        free(c->hdrbuf) ; free(c->paybuf) ; c->hdrbuf = c->paybuf = NULL ;
        sse_free(&c->epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "initialize message reader") ;
    }

    if (!sse_stream_attach(c->stream, &c->epoll, 0)) {
        sse_stream_close(c->stream) ; c->stream = NULL ;
        free(c->hdrbuf) ; free(c->paybuf) ; c->hdrbuf = c->paybuf = NULL ;
        sse_free(&c->epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "attach stream") ;
    }

    return 1 ;
}

void fdholder_client_end (fdholder_client_t *c)
{
    log_flow() ;

    if (c->timer_active) {
        sse_free_timer(&c->wtimer) ;
        c->timer_active = false ;
    }
    if (c->stream) {
        sse_stream_close(c->stream) ;
        c->stream = NULL ;
    }
    sse_free_signal(&c->wsignal) ;
    sse_free(&c->epoll) ;
    free(c->hdrbuf) ; c->hdrbuf = NULL ;
    free(c->paybuf) ; c->paybuf = NULL ;
}

static int client_wait (fdholder_client_t *c, int timeout)
{
    if (timeout > 0 && sse_start_timer(&c->epoll, &c->wtimer, client_timeout_cb, c, timeout, 0, 5))
        c->timer_active = true ;

    sse_poll(&c->epoll, SSE_TIMEOUT_INFINITE) ;

    if (c->timer_active) {
        sse_free_timer(&c->wtimer) ;
        c->timer_active = false ;
    }

    return c->response_received ? 1 : 0 ;
}

int fdholder_client_request (fdholder_client_t *c, uint8_t cmd, uint8_t flags, void const *payload, size_t paylen, int const *fds, int nfd, int timeout)
{
    log_flow() ;

    char hdr[FDHOLDER_HDR_SIZE] ;
    fdholder_hdr_pack(hdr, cmd, 0, flags, (uint32_t)paylen) ;

    int r = nfd > 0
        ? stream_message_send_with_fds(c->stream, hdr, FDHOLDER_HDR_SIZE, payload, paylen, fds, nfd)
        : stream_message_send(c->stream, hdr, FDHOLDER_HDR_SIZE, payload, paylen) ;
    if (r < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "send request") ;

    c->request_sent = true ;
    c->response_received = false ;

    if (!client_wait(c, timeout)) {
        c->status = FDHOLDER_ERR ;
        return 0 ;
    }

    return 1 ;
}

int fdholder_store (fdholder_client_t *c, char const *name, int fd, uint64_t expire, int timeout)
{
    log_flow() ;

    size_t nl = strlen(name) ;
    if (!nl || nl > FDHOLDER_NAME_MAX)
        return (errno = EINVAL, 0) ;

    char pl[8 + FDHOLDER_NAME_MAX] ;
    u64_pack_big(pl, expire) ;
    memcpy(pl + 8, name, nl) ;

    int afd[1] = { fd } ;
    if (!fdholder_client_request(c, FDHOLDER_CMD_STORE, 0, pl, 8 + nl, afd, 1, timeout))
        return 0 ;

    return c->status == FDHOLDER_OK ;
}

int fdholder_retrieve (fdholder_client_t *c, char const *name, bool dodelete, int timeout)
{
    log_flow() ;

    size_t nl = strlen(name) ;
    if (!nl || nl > FDHOLDER_NAME_MAX)
        return (errno = EINVAL, 0) ;

    uint8_t flags = dodelete ? FDHOLDER_FLAG_DELETE : 0 ;
    if (!fdholder_client_request(c, FDHOLDER_CMD_RETRIEVE, flags, name, nl, NULL, 0, timeout))
        return 0 ;

    return c->status == FDHOLDER_OK ;
}

int fdholder_delete (fdholder_client_t *c, char const *name, int timeout)
{
    log_flow() ;

    size_t nl = strlen(name) ;
    if (!nl || nl > FDHOLDER_NAME_MAX)
        return (errno = EINVAL, 0) ;

    if (!fdholder_client_request(c, FDHOLDER_CMD_DELETE, 0, name, nl, NULL, 0, timeout))
        return 0 ;

    return c->status == FDHOLDER_OK ;
}

int fdholder_list (fdholder_client_t *c, int timeout)
{
    log_flow() ;

    if (!fdholder_client_request(c, FDHOLDER_CMD_LIST, 0, NULL, 0, NULL, 0, timeout))
        return 0 ;

    return c->status == FDHOLDER_OK ;
}

int fdholder_pipe (fdholder_client_t *c, char const *name, uint8_t end, int timeout)
{
    log_flow() ;

    size_t nl = strlen(name) ;
    if (!nl || nl > FDHOLDER_NAME_MAX)
        return (errno = EINVAL, 0) ;
    if (end != FDHOLDER_END_READ && end != FDHOLDER_END_WRITE)
        return (errno = EINVAL, 0) ;

    char pl[1 + FDHOLDER_NAME_MAX] ;
    pl[0] = (char)end ;
    memcpy(pl + 1, name, nl) ;

    if (!fdholder_client_request(c, FDHOLDER_CMD_PIPE, 0, pl, 1 + nl, NULL, 0, timeout))
        return 0 ;

    return c->status == FDHOLDER_OK ;    /* requested end in c->received_fd */
}

int fdholder_pipe_delete (fdholder_client_t *c, char const *name, int timeout)
{
    log_flow() ;

    size_t nl = strlen(name) ;
    if (!nl || nl > FDHOLDER_NAME_MAX)
        return (errno = EINVAL, 0) ;

    if (!fdholder_client_request(c, FDHOLDER_CMD_PIPE_DELETE, 0, name, nl, NULL, 0, timeout))
        return 0 ;

    return c->status == FDHOLDER_OK ;
}
