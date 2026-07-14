/*
 * oneshot_async.c
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

/* Deliver the outcome to the caller exactly once. Every path (response,
 * connection drop, transport error) funnels through here; `done` is the guard. */
static void async_deliver(oneshot_async_t *a, uint8_t status, uint32_t wstat)
{
    if (a->done)
        return ;

    a->done = true ;
    a->cb(a->data, status, wstat) ;
}

static void async_response_handler(io_rb_iovec_t *msg, void *data)
{
    log_flow() ;

    oneshot_async_t *a = data ;

    // the protocol never sends descriptors back ; close any stray ones
    for (int i = 0 ; i < msg->nfd ; i++)
        close_fd(msg->afd[i]) ;

    if (msg->niov < 1) {
        async_deliver(a, ONESHOT_ERR, 0) ;
        return ;
    }

    char const *hdr = msg->iov[0].iov_base ;
    uint8_t command = (uint8_t)hdr[1] ;
    uint8_t status = (uint8_t)hdr[2] ;

    if (command != ONESHOT_CMD_RESPONSE) {
        async_deliver(a, ONESHOT_ERR, 0) ;
        return ;
    }

    uint32_t wstat = 0 ;
    if (status == ONESHOT_OK && msg->niov > 1 && msg->iov[1].iov_len >= 4)
        u32_unpack_big(msg->iov[1].iov_base, &wstat) ;

    async_deliver(a, status, wstat) ;
}

static void async_read_cb(sse_stream_t *stream, void *data)
{
    log_flow() ;
    (void)stream ;

    oneshot_async_t *a = data ;

    for (;;) {
        int r = stream_message_read(&a->reader, oneshot_parse_header) ;
        if (r < 0) {
            log_warnusys("read response from oneshot daemon") ;
            async_deliver(a, ONESHOT_ERR, 0) ;
            return ;
        }
        if (!r)
            return ;
        if (a->done)
            return ;
    }
}

static void async_write_cb(sse_stream_t *stream, void *data)
{
    log_flow() ;
    (void)stream ; (void)data ;
}

static void async_close_cb(sse_stream_t *stream, void *data)
{
    log_flow() ;
    (void)stream ;

    // connection dropped: a still-pending request has failed
    async_deliver(data, ONESHOT_ERR, 0) ;
}

static void async_error_cb(sse_stream_t *stream, int error, void *data)
{
    log_flow() ;
    (void)stream ;

    errno = error ;
    log_warnusys("communicate with oneshot daemon") ;

    async_deliver(data, ONESHOT_ERR, 0) ;
}

int oneshot_async_send(oneshot_async_t *a, sse_epoll_t *loop, char const *socket, uint8_t down, uint8_t who, char const *servicedir, oneshot_async_cb_t *cb, void *data)
{
    log_flow() ;

    memset(a, 0, sizeof(*a)) ;
    a->cb = cb ;
    a->data = data ;

    size_t len = strlen(servicedir) ;
    if (!len || len > ONESHOT_PAYLOAD_MAX)
        return (errno = EINVAL, 0) ;

    int fd = sse_streamux_create_client(socket) ;
    if (fd < 0)
        log_warnusys("connect to oneshot daemon: ", socket) ;

    a->stream = sse_stream_new(fd, async_read_cb, async_write_cb, async_close_cb, async_error_cb, a) ;
    if (!a->stream) {
        close_fd(fd) ;
        log_warnusys("create stream") ;
    }

    a->hdrbuf = malloc(ONESHOT_HDR_SIZE) ;
    a->paybuf = malloc(IO_RB_BUFFER_SIZE) ;
    if (!a->hdrbuf || !a->paybuf) {
        log_warnusys("allocate client buffers") ;
        goto err ;
    }

    if (!stream_message_reader_init(&a->reader, a->stream, async_response_handler, a->hdrbuf, ONESHOT_HDR_SIZE, a->paybuf, IO_RB_BUFFER_SIZE, a)) {
        log_warnusys("initialize message reader") ;
        goto err ;
    }

    if (!sse_stream_attach(a->stream, loop, 0)) {
        log_warnusys("attach stream") ;
        goto err ;
    }

    uint8_t flags = down ? ONESHOT_FLAG_DOWN : 0 ;
    int fds[3] = { 0, 1, 2 } ;

    char hdr[ONESHOT_HDR_SIZE] ;
    oneshot_hdr_pack(hdr, ONESHOT_CMD_RUN, who, flags, (uint32_t)len) ;

    if (stream_message_send_with_fds(a->stream, hdr, ONESHOT_HDR_SIZE, servicedir, len, fds, 3) < 0) {
        log_warnusys("send request") ;
        goto err ;
    }

    return 1 ;

    err:
        free(a->hdrbuf) ;
        free(a->paybuf) ;
        a->hdrbuf = a->paybuf = NULL ;
        sse_stream_close(a->stream) ;
        a->stream = NULL ;

        return 0 ;
}

void oneshot_async_end(oneshot_async_t *a)
{
    log_flow() ;

    a->done = true ; // suppress any callback the close may trigger

    if (a->stream) {
        sse_stream_close(a->stream) ;
        a->stream = NULL ;
    }

    free(a->hdrbuf) ;
    a->hdrbuf = NULL ;
    free(a->paybuf) ;
    a->paybuf = NULL ;
}
