/*
 * oneshot_client.c
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

#include <66/oneshot.h>

static void client_response_handler(io_rb_iovec_t *msg, void *data)
{
    log_flow() ;

    oneshot_client_t *c = data ;
    c->wstat = 0 ;

    // the protocol never sends descriptors back ; close any stray ones
    for (int i = 0 ; i < msg->nfd ; i++)
        close_fd(msg->afd[i]) ;

    if (msg->niov < 1) {
        c->status = ONESHOT_ERR ;
        goto done ;
    }

    char const *hdr = msg->iov[0].iov_base ;
    uint8_t command = (uint8_t)hdr[1] ;
    uint8_t status = (uint8_t)hdr[2] ;

    if (command != ONESHOT_CMD_RESPONSE) {
        c->status = ONESHOT_ERR ;
        goto done ;
    }

    c->status = status ;

    if (status == ONESHOT_OK && msg->niov > 1 && msg->iov[1].iov_len >= 4) {
        uint32_t w ;
        u32_unpack_big(msg->iov[1].iov_base, &w) ;
        c->wstat = w ;
    }

 done:
    c->response_received = true ;
    c->epoll.running = false ;
}

static void client_read_cb(sse_stream_t *stream, void *data)
{
    log_flow() ;

    oneshot_client_t *c = data ;

    for (;;) {
        int r = stream_message_read(&c->reader, oneshot_parse_header) ;
        if (r < 0) {
            log_warnusys("read response from daemon") ;
            c->status = ONESHOT_ERR ;
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

    oneshot_client_t *c = data ;
    if (c->request_sent && !c->response_received) {
        log_warn("connection closed before a response was received") ;
        c->status = ONESHOT_ERR ;
    }
    c->epoll.running = false ;
}

static void client_error_cb(sse_stream_t *stream, int error, void *data)
{
    log_flow() ;
    (void)stream ;

    oneshot_client_t *c = data ;
    errno = error ;
    log_warnusys("communicate with oneshot daemon") ;
    c->status = ONESHOT_ERR ;
    c->epoll.running = false ;
}

static void client_signal_cb(sse_watcher_t *w, void *data, int revents)
{
    log_flow() ;
    (void)revents ;

    oneshot_client_t *c = data ;
    sse_signal_t *sig = (sse_signal_t *)w->sdata ;
    flog_info("received signal %d", sig->si.ssi_signo) ;
    c->epoll.running = false ;
}

static void client_timeout_cb(sse_watcher_t *w, void *data, int revents)
{
    log_flow() ;
    (void)w ; (void)revents ;

    oneshot_client_t *c = data ;
    log_warn("timeout for the oneshot daemon") ;
    c->epoll.running = false ;
}

int oneshot_client_init(oneshot_client_t *c, char const *socket)
{
    log_flow() ;

    memset(c, 0, sizeof(*c)) ;

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
        log_warnusys_return(LOG_EXIT_ZERO, "connect to oneshot daemon: ", socket) ;
    }

    c->stream = sse_stream_new(fd, client_read_cb, client_write_cb, client_close_cb, client_error_cb, c) ;
    if (!c->stream) {
        close_fd(fd) ;
        sse_free(&c->epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "create stream") ;
    }

    c->hdrbuf = malloc(ONESHOT_HDR_SIZE) ;
    c->paybuf = malloc(IO_RB_BUFFER_SIZE) ;
    if (!c->hdrbuf || !c->paybuf) {
        free(c->hdrbuf) ; free(c->paybuf) ; c->hdrbuf = c->paybuf = NULL ;
        sse_stream_close(c->stream) ; c->stream = NULL ;
        sse_free(&c->epoll) ;
        log_warnusys_return(LOG_EXIT_ZERO, "allocate client buffers") ;
    }

    if (!stream_message_reader_init(&c->reader, c->stream, client_response_handler, c->hdrbuf, ONESHOT_HDR_SIZE, c->paybuf, IO_RB_BUFFER_SIZE, c)) {
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

void oneshot_client_end(oneshot_client_t *c)
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

static int client_wait(oneshot_client_t *c, int timeout)
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

int oneshot_run(oneshot_client_t *c, uint8_t down, char const *servicedir, int timeout)
{
    log_flow() ;

    size_t len = strlen(servicedir) ;
    if (!len || len > ONESHOT_PAYLOAD_MAX)
        return (errno = EINVAL, 0) ;

    uint8_t flags = down ? ONESHOT_FLAG_DOWN : 0 ;
    int fds[3] = { 0, 1, 2 } ;

    char hdr[ONESHOT_HDR_SIZE] ;
    oneshot_hdr_pack(hdr, ONESHOT_CMD_RUN, 0, flags, (uint32_t)len) ;

    if (stream_message_send_with_fds(c->stream, hdr, ONESHOT_HDR_SIZE, servicedir, len, fds, 3) < 0)
        log_warnusys_return(LOG_EXIT_ZERO, "send request") ;

    c->request_sent = true ;
    c->response_received = false ;

    if (!client_wait(c, timeout)) {
        c->status = ONESHOT_ERR ;
        return 0 ;
    }

    return 1 ;
}
