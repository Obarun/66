/**
 * oneshot.h
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
 **/


/**
 * @file oneshot.h
 * @brief Wire protocol and client library for the 66-oneshotd daemon.
 *
 * `66-oneshotd` is a per-owner daemon that runs the `run` (up) or `finish` (down)
 * script of a service directory on behalf of a client and reports its verdict
 * back, the result and code it writes into the status of the service. A client connects to the daemon's Unix domain socket,
 * sends a single RUN request carrying the service directory path plus its own
 * `0`/`1`/`2` descriptors (passed out-of-band through `SCM_RIGHTS`), and blocks
 * until the daemon spawns the script, reaps it, and returns the verdict.
 *
 * The daemon spawns one child per RUN request with `spawn_path_full`: the child
 * leads its own session, gets the three forwarded descriptors as its `0`/`1`/`2`
 * and an empty signal mask, and execs the script. The daemon accepts a connection only from
 * a peer whose `SCM_CREDENTIALS` uid matches the daemon's effective uid; any other
 * peer is rejected at the socket layer (no protocol response is sent).
 *
 * This module provides two layers:
 *   - the wire framing helpers (`oneshot_hdr_pack`, `oneshot_parse_header`,
 *     `oneshot_status_str`),
 *   - the asynchronous request/response client (`oneshot_async_t`,
 *     `oneshot_async_send`, `oneshot_async_end`), multiplexed on a caller's loop.
 *
 * @note A response status is the PROTOCOL outcome (could the daemon run the script
 * at all), distinct from the verdict on the script. The verdict travels in the
 * response payload only when the status is `ONESHOT_OK`.
 */

#ifndef SS_ONESHOT_H
#define SS_ONESHOT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <oblibs/sse.h>
#include <oblibs/sse_stream.h>
#include <oblibs/stream_message.h>
#include <oblibs/io_rb.h>

/** @brief Wire protocol version. Stamped into header byte [0] by
 * `oneshot_hdr_pack` and rejected with `EPROTO` by `oneshot_parse_header` if a
 * received frame does not match. */
#define ONESHOT_VERSION            1

/** @brief Size in bytes of the fixed wire header (see the layout block below). */
#define ONESHOT_HDR_SIZE           8

/** @brief Maximum number of simultaneous client connections the daemon accepts.
 * Beyond this, `client_create` closes the freshly accepted socket and refuses the
 * connection. */
#define ONESHOT_MAXCLIENTS_DEFAULT 64

/** @brief Default `listen()` backlog for the daemon's server socket, used unless
 * overridden by the `-b` option. */
#define ONESHOT_BACKLOG_DEFAULT    64

/** @brief Largest payload that fits, together with the header, in one `io_rb`
 * message buffer (`IO_RB_BUFFER_SIZE - ONESHOT_HDR_SIZE`). `oneshot_parse_header`
 * rejects any frame announcing a longer payload with `EMSGSIZE`, and
 * `oneshot_async_send` rejects a longer service directory path with `EINVAL`. */
#define ONESHOT_PAYLOAD_MAX        (IO_RB_BUFFER_SIZE - ONESHOT_HDR_SIZE)

/** @brief Size in bytes of the verdict an `ONESHOT_OK` response carries: [0] the
 * `status_result_e` result, [1..4] its code as a big-endian `uint32` (exit code,
 * signal number or errno, read per result, 0 otherwise). */
#define ONESHOT_VERDICT_LEN        5

/**
 * @enum oneshot_cmd_e
 * @brief Wire command codes carried in header byte [1].
 *
 * A request carries `ONESHOT_CMD_RUN`; a response always carries
 * `ONESHOT_CMD_RESPONSE`. The daemon answers any request whose command is not
 * `ONESHOT_CMD_RUN` with a `ONESHOT_PROTO` response, and the client treats any
 * reply whose command is not `ONESHOT_CMD_RESPONSE` as `ONESHOT_ERR`.
 */
enum oneshot_cmd_e
{
    ONESHOT_CMD_RESPONSE = 0,   // response frame; status byte carries the outcome
    ONESHOT_CMD_RUN             // run the up/down script of a service directory
} ;

/**
 * @enum oneshot_status_e
 * @brief Response status codes carried in header byte [2] (responses only).
 *
 * This is the PROTOCOL outcome -- whether the daemon could run the script at all
 * -- not the verdict on the script. The verdict travels in the response payload
 * only when the status is `ONESHOT_OK`.
 */
enum oneshot_status_e
{
    ONESHOT_OK = 0,   // the request was carried out; the verdict is in the payload
    ONESHOT_ERR,      // daemon-side failure (child watcher, allocation, or a transport/connection error detected client-side)
    ONESHOT_PROTO     // malformed request: wrong command, missing/empty payload, path not starting with '/', path too long, wrong fd count, or a RUN already in flight on this connection
} ;

/** @brief RUN-request header flag (header byte [3]): run the down/`finish`
 * script. When absent the daemon runs the up/`run` script. */
#define ONESHOT_FLAG_DOWN  0x01u

/**
 * Wire header layout (ONESHOT_HDR_SIZE bytes, explicit big-endian):
 *   [0] version
 *   [1] command
 *   [2] status (responses) / who (requests)
 *   [3] flags
 *   [4..7] payload_len (uint32, big-endian)
 * File descriptors travel out-of-band through SCM_RIGHTS, never in the payload.
 *
 * Byte [2] is direction-dependent: a response carries the protocol status there,
 * while a RUN request carries the `status_who_e` provenance (who) to stamp into
 * the status the daemon writes for the oneshot.
 *
 *   request                                                      ancillary fds
 *   -------------------------------------------------------------------------
 *   RUN     +----------------------------+                          3 fds
 *           | servicedir (payload_len)   |   stdin/stdout/stderr to dup onto
 *           +----------------------------+   the script's 0/1/2 ; servicedir is
 *           op (up/down) lives in header flags (ONESHOT_FLAG_DOWN), NOT NUL-terminated ;
 *           who lives in header byte [2]
 *
 *   response  header.command = ONESHOT_CMD_RESPONSE, header.status = result.
 *   -------------------------------------------------------------------------
 *   RUN ok : payload = verdict (ONESHOT_VERDICT_LEN bytes) ; 0 fds
 *   others : empty payload, 0 fds (status carries the outcome)
 */

// wire helpers (oneshot_wire.c)

/**
 * @brief Serialize the 8-byte wire header into `hdr`.
 *
 * Writes byte [0] = `ONESHOT_VERSION`, [1] = @p command, [2] = @p status,
 * [3] = @p flags, and [4..7] = @p payload_len as a big-endian `uint32`
 * (via `u32_pack_big`). Performs no validation and has no failure path.
 *
 * @param[out] hdr         Buffer of at least `ONESHOT_HDR_SIZE` bytes. Must not be
 *                         NULL (not checked; a NULL pointer is undefined behaviour).
 * @param[in] command      Command code (see `oneshot_cmd_e`); written verbatim.
 * @param[in] status       Byte [2] value, written verbatim: a `oneshot_status_e`
 *                         status code in a response, or the `status_who_e` who in
 *                         a RUN request.
 * @param[in] flags        Header flags (e.g. `ONESHOT_FLAG_DOWN`); written verbatim.
 * @param[in] payload_len  Number of payload bytes that will follow the header.
 *
 * @return Nothing. Always succeeds.
 */
extern void oneshot_hdr_pack(char *hdr, uint8_t command, uint8_t status, uint8_t flags, uint32_t payload_len) ;

/**
 * @brief Framing callback: validate the fixed header and extract the payload length.
 *
 * Intended as the `stream_message_read` header parser. It performs framing
 * validation only -- version check and payload-length bound -- never per-command
 * validation, which is the message handlers' responsibility. The header is not
 * consumed until all `ONESHOT_HDR_SIZE` bytes are present.
 *
 * @param[in] header       Pointer to the received header bytes. Read-only.
 * @param[in] header_len   Number of header bytes currently available.
 * @param[out] payload_len Set to the announced payload length only when the
 *                         function returns `ONESHOT_HDR_SIZE`. Untouched otherwise.
 * @param[in] data         Caller context; ignored by this parser.
 *
 * @return Framing result:
 *      - `ONESHOT_HDR_SIZE` (8) on success: the header is complete and valid, and
 *        @p payload_len has been set.
 *      - 0 when fewer than `ONESHOT_HDR_SIZE` bytes are available yet (more input
 *        is needed); errno is not touched.
 *      - -1 on a malformed header (errno set).
 *
 * @retval -1 errno is set to `EPROTO` if header byte [0] does not equal
 *         `ONESHOT_VERSION`.
 * @retval -1 errno is set to `EMSGSIZE` if the announced payload length exceeds
 *         `ONESHOT_PAYLOAD_MAX`.
 */
extern int oneshot_parse_header(void const *header, size_t header_len, size_t *payload_len, void *data) ;

/**
 * @brief Return a static human-readable label for a response status code.
 *
 * @param[in] status  A `oneshot_status_e` value.
 *
 * @return A pointer to a statically allocated, NUL-terminated string. Never NULL.
 *      - `ONESHOT_OK`     -> "success"
 *      - `ONESHOT_PROTO`  -> "protocol error"
 *      - any other value (including `ONESHOT_ERR`) -> "error"
 *
 * @note The returned string is owned by the library; the caller must not free or
 * modify it. Always succeeds.
 */
extern char const *oneshot_status_str(uint8_t status) ;

// async client library (oneshot_async.c)

/**
 * @brief Result callback: delivered exactly once per in-flight RUN request.
 *
 * @param[in] data    Opaque caller context handed to `oneshot_async_send`.
 * @param[in] status  Protocol outcome (`oneshot_status_e`). `ONESHOT_OK` means the
 *                    daemon carried the request out; any other value (including
 *                    a connection drop before reply) is a failure.
 * @param[in] result  The `status_result_e` result the daemon wrote into the status
 *                    of the service; valid only when @p status is `ONESHOT_OK`, 0
 *                    otherwise.
 * @param[in] code    The code of @p result (exit code, signal number or errno), 0
 *                    when @p result carries none or @p status is not `ONESHOT_OK`.
 */
typedef void oneshot_async_cb_t(void *data, uint8_t status, uint8_t result, uint32_t code) ;

/**
 * @struct oneshot_async_s
 * @brief State of a single asynchronous RUN request multiplexed on a caller's loop.
 *
 * Unlike the daemon's own server or a blocking client, this carries no private
 * event loop: `oneshot_async_send` attaches the stream to a loop the caller already
 * runs, so many requests progress concurrently on one `sse_epoll_t`. The result is
 * delivered through `oneshot_async_cb_t` from within that loop; `done` guards the
 * single-delivery guarantee across the response, connection-drop and error paths.
 *
 * @param stream   Buffered Unix-socket stream to the daemon. NULL once released.
 * @param reader   Message reader reassembling the framed response.
 * @param hdrbuf   Heap buffer of `ONESHOT_HDR_SIZE` bytes backing the reader header.
 * @param paybuf   Heap buffer of `IO_RB_BUFFER_SIZE` bytes backing the reader payload.
 * @param cb       Result callback; called at most once.
 * @param data     Opaque context forwarded to @c cb.
 * @param done     True once the result has been delivered (or delivery suppressed by
 *                 `oneshot_async_end`); blocks any further callback.
 */
typedef struct oneshot_async_s oneshot_async_t ;
struct oneshot_async_s
{
    sse_stream_t *stream ;
    stream_message_t reader ;
    char *hdrbuf ;
    char *paybuf ;
    oneshot_async_cb_t *cb ;
    void *data ;
    bool done ;
} ;

/**
 * @brief Connect to the daemon, attach to @p loop, and send one RUN request.
 *
 * Zeroes @p a, connects (non-blocking) to the daemon socket at @p socket, creates
 * the stream and attaches it to @p loop, then sends one RUN request whose payload
 * is @p servicedir (not NUL-terminated on the wire), whose header byte [2] carries
 * @p who and whose flags carry @p down, forwarding the caller's `0`/`1`/`2` through
 * `SCM_RIGHTS`. The request is fire-and-forget: control returns immediately and the
 * outcome arrives later through @p cb as the caller runs @p loop. On any immediate
 * failure everything allocated is unwound and @p cb is NOT called.
 *
 * @param[out] a          Request state to initialize. Must not be NULL (not checked).
 * @param[in] loop        Event loop the caller runs; the stream is attached to it.
 * @param[in] socket      Filesystem path of the daemon's Unix domain socket.
 * @param[in] down        0 -> up/`run` script; non-zero -> down/`finish` script.
 * @param[in] who         `status_who_e` provenance carried in header byte [2].
 * @param[in] servicedir  Service directory path. Nonempty, at most `ONESHOT_PAYLOAD_MAX`.
 * @param[in] cb          Result callback, invoked once when the outcome is known.
 * @param[in] data        Opaque context forwarded to @p cb.
 *
 * @return Status code:
 *      - 1 the request is in flight; @p cb will fire later and @p a must be released
 *        with `oneshot_async_end`.
 *      - 0 immediate failure (errno set); @p a is already torn down and @p cb will
 *        NOT be called. `EINVAL` if @p servicedir is empty or too long; otherwise
 *        the errno of the failing connect/alloc/send step.
 */
extern int oneshot_async_send(oneshot_async_t *a, sse_epoll_t *loop, char const *socket, uint8_t down, uint8_t who, char const *servicedir, oneshot_async_cb_t *cb, void *data) ;

/**
 * @brief Close the stream and free the buffers of @p a.
 *
 * Marks @p a done (suppressing any pending callback), closes the stream -- which
 * signals the daemon to `SIGKILL` a still-running script -- and frees and NULLs the
 * header/payload buffers. Safe to call once after the result is delivered, or to
 * abort a request still in flight (e.g. on a caller-side timeout).
 *
 * @param[in,out] a  Request to release. Must not be NULL (not checked).
 *
 * @return Nothing. Always succeeds.
 */
extern void oneshot_async_end(oneshot_async_t *a) ;

#endif
