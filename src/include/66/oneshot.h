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
 * @brief Wire protocol, client library and script executor for the 66-oneshotd daemon.
 *
 * `66-oneshotd` is a per-owner daemon that runs the `run` (up) or `finish` (down)
 * script of a service directory on behalf of a client and reports the script's
 * `waitpid` status back. A client connects to the daemon's Unix domain socket,
 * sends a single RUN request carrying the service directory path plus its own
 * `0`/`1`/`2` descriptors (passed out-of-band through `SCM_RIGHTS`), and blocks
 * until the daemon forks the script, reaps it, and returns the result.
 *
 * The daemon forks one child per RUN request, wires the three forwarded
 * descriptors onto the child's `0`/`1`/`2`, resets the signal mask and execs the
 * script through `oneshot_exec_script`. The daemon accepts a connection only from
 * a peer whose `SCM_CREDENTIALS` uid matches the daemon's effective uid; any other
 * peer is rejected at the socket layer (no protocol response is sent).
 *
 * This module provides three layers:
 *   - the wire framing helpers (`oneshot_hdr_pack`, `oneshot_parse_header`,
 *     `oneshot_status_str`),
 *   - the in-child script executor (`oneshot_exec_script`),
 *   - the synchronous request/response client (`oneshot_client_t`,
 *     `oneshot_client_init`, `oneshot_run`, `oneshot_client_end`).
 *
 * @note A response status is the PROTOCOL outcome (could the daemon run the script
 * at all), distinct from the script's own exit code. The script's full `waitpid`
 * status travels in the response payload only when the status is `ONESHOT_OK`.
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
 * Beyond this, `conn_create` closes the freshly accepted socket and refuses the
 * connection. */
#define ONESHOT_MAXCLIENTS_DEFAULT 64

/** @brief Default `listen()` backlog for the daemon's server socket, used unless
 * overridden by the `-b` option. */
#define ONESHOT_BACKLOG_DEFAULT    64

/** @brief Largest payload that fits, together with the header, in one `io_rb`
 * message buffer (`IO_RB_BUFFER_SIZE - ONESHOT_HDR_SIZE`). `oneshot_parse_header`
 * rejects any frame announcing a longer payload with `EMSGSIZE`, and `oneshot_run`
 * rejects a longer service directory path with `EINVAL`. */
#define ONESHOT_PAYLOAD_MAX        (IO_RB_BUFFER_SIZE - ONESHOT_HDR_SIZE)

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
 * -- not the script's own exit code. The script's full `waitpid` status travels
 * in the response payload only when the status is `ONESHOT_OK`.
 */
enum oneshot_status_e
{
    ONESHOT_OK = 0,   // script was forked and reaped; `waitpid` status is in the payload
    ONESHOT_ERR,      // daemon-side failure (fork, child watcher, allocation, or a transport/connection error detected client-side)
    ONESHOT_PROTO     // malformed request: wrong command, missing/empty payload, path not starting with '/', path too long, wrong fd count, or a RUN already in flight on this connection
} ;

/** @brief RUN-request header flag (header byte [3]): run the down/`finish`
 * script. When absent the daemon runs the up/`run` script. */
#define ONESHOT_FLAG_DOWN  0x01u

/**
 * Wire header layout (ONESHOT_HDR_SIZE bytes, explicit big-endian):
 *   [0] version
 *   [1] command
 *   [2] status   (responses only ; 0 in requests)
 *   [3] flags
 *   [4..7] payload_len (uint32, big-endian)
 * File descriptors travel out-of-band through SCM_RIGHTS, never in the payload.
 *
 *   request                                                      ancillary fds
 *   -------------------------------------------------------------------------
 *   RUN     +----------------------------+                          3 fds
 *           | servicedir (payload_len)   |   stdin/stdout/stderr to dup onto
 *           +----------------------------+   the script's 0/1/2 ; servicedir is
 *           op (up/down) lives in header flags (ONESHOT_FLAG_DOWN), NOT NUL-terminated
 *
 *   response  header.command = ONESHOT_CMD_RESPONSE, header.status = result.
 *   -------------------------------------------------------------------------
 *   RUN ok : payload = waitpid status (uint32, big-endian) ; 0 fds
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
 * @param[in] status       Status code (see `oneshot_status_e`); written verbatim.
 *                         Meaningful in responses only; pass 0 in requests.
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

// script executor, run in the daemon's forked child (oneshot_exec_script.c)

/**
 * @brief `chdir` into @p servicedir and `execve` its `run` (up) or `finish` (down)
 * script in the current process.
 *
 * Builds the script path as `<servicedir>/run` when @p down is 0, or
 * `<servicedir>/finish` when @p down is non-zero, `chdir`s into @p servicedir,
 * then `execve`s the script with argv `{ script, "run"|"finish", NULL }` and the
 * inherited `environ`. This is meant to run in the daemon's forked child, which
 * must already have wired the client's descriptors onto `0`/`1`/`2` and reset the
 * signal mask before calling.
 *
 * @param[in] servicedir  Absolute path to the service directory. Must not be NULL
 *                        (not checked). The daemon validates it (absolute, bounded,
 *                        nonempty) before forking.
 * @param[in] down        0 -> exec the `run` script; non-zero -> exec the `finish`
 *                        script.
 *
 * @return Does not return. On success control passes to the exec'd script and this
 * function never returns. On every failure it terminates the process via
 * `log_dieusys` rather than returning:
 *      - exits 111 (`LOG_EXIT_SYS`) if `chdir(servicedir)` fails.
 *      - exits 127 if `execve` fails with errno `ENOENT` (script not found).
 *      - exits 126 if `execve` fails with any other errno.
 *
 * @note The exit-code/no-return contract is the only contract: the function never
 * hands a status back to its caller. It never `_exit(0)`s for a missing `finish`
 * script -- an absent script is a failing `execve` (`ENOENT`) and exits 127.
 * @see oneshot_run
 */
extern void oneshot_exec_script(char const *servicedir, uint8_t down) ;

// client library (oneshot_client.c)

/**
 * @struct oneshot_client_s
 * @brief State of a synchronous oneshot client connection.
 *
 * Wraps an SSE event loop driving a single Unix-socket stream to the daemon. The
 * client is request/response and single-shot per call: `oneshot_run` queues one
 * RUN request, runs the loop until a reply arrives (or the connection drops, a
 * watched signal fires, or the guard timer expires), and records the outcome in
 * @c status / @c wstat. Initialize with `oneshot_client_init` and release with
 * `oneshot_client_end`. After a successful `oneshot_run` returning 1, read
 * @c status to learn the protocol outcome and, when it is `ONESHOT_OK`, @c wstat
 * for the script's `waitpid` status.
 *
 * @param epoll
 * The SSE epoll event loop driving the connection.
 *
 * @param stream
 * The buffered Unix-socket stream to the daemon. NULL once closed/released.
 *
 * @param reader
 * The message reader that reassembles framed responses from the stream.
 *
 * @param wsignal
 * Signal watcher. `SIGPIPE` is ignored; `SIGTERM` and `SIGINT` stop the loop.
 *
 * @param wtimer
 * Guard-timer watcher, armed by `oneshot_run` only when its timeout is > 0.
 *
 * @param hdrbuf
 * Heap buffer of `ONESHOT_HDR_SIZE` bytes backing the reader's header.
 *
 * @param paybuf
 * Heap buffer of `IO_RB_BUFFER_SIZE` bytes backing the reader's payload.
 *
 * @param timer_active
 * True while the guard timer is armed; managed internally.
 *
 * @param request_sent
 * True once a RUN request has been written to the stream.
 *
 * @param response_received
 * True once a complete response frame has been handled.
 *
 * @param status
 * The `oneshot_status_e` of the last response. Set to `ONESHOT_ERR` on any
 * transport failure, connection drop before reply, or non-`ONESHOT_CMD_RESPONSE`
 * reply. Valid after `oneshot_run` regardless of its return value.
 *
 * @param wstat
 * The script's raw `waitpid` status. Valid only when @c status is `ONESHOT_OK`;
 * 0 in every other case.
 */
typedef struct oneshot_client_s oneshot_client_t ;
struct oneshot_client_s
{
    sse_epoll_t epoll ;
    sse_stream_t *stream ;
    stream_message_t reader ;
    sse_watcher_t wsignal ;
    sse_watcher_t wtimer ;
    char *hdrbuf ;
    char *paybuf ;

    bool timer_active ;
    bool request_sent ;
    bool response_received ;
    uint8_t status ; // oneshot_status_e of the last response
    uint32_t wstat ; // raw waitpid status of the script (valid iff status == ONESHOT_OK)
} ;

/**
 * @brief Connect a client to the daemon and initialize @p c for use.
 *
 * Zeroes @p c, creates the SSE event loop, installs a signal watcher (ignoring
 * `SIGPIPE`, attaching `SIGTERM` and `SIGINT`), connects to the daemon's Unix
 * socket at @p socket, creates the stream, allocates the header/payload buffers,
 * initializes the message reader and attaches the stream to the loop. On any
 * failure it unwinds everything allocated so far before returning, so @p c is left
 * in a released state and must not be passed to `oneshot_client_end`.
 *
 * @param[out] c       Client to initialize. Must not be NULL (not checked).
 * @param[in] socket   Filesystem path of the daemon's Unix domain socket. Must not
 *                     be NULL (forwarded to the connect helper).
 *
 * @return Status code:
 *      - 1 on success; @p c is ready for `oneshot_run` and must be released with
 *        `oneshot_client_end`.
 *      - 0 on failure; the failing step has already logged a diagnostic. No
 *        dedicated errno is set by this function: it forwards the errno left by
 *        the underlying failing call (`sse_new`, the signal-watcher setup,
 *        `sse_streamux_create_client` connecting to @p socket, `sse_stream_new`,
 *        the buffer `malloc`s, the reader init, or `sse_stream_attach`).
 *
 * @note On a 0 return @p c has been fully torn down internally; do not call
 * `oneshot_client_end` on it.
 */
extern int oneshot_client_init(oneshot_client_t *c, char const *socket) ;

/**
 * @brief Release every resource held by @p c.
 *
 * Disarms the guard timer if active, closes the stream, frees the signal watcher
 * and the event loop, and frees and NULLs the header/payload buffers. Call exactly
 * once on a client that `oneshot_client_init` returned 1 for.
 *
 * @param[in,out] c  Client to release. Must not be NULL (not checked). Pointers
 *                   it owns are set to NULL.
 *
 * @return Nothing. Always succeeds.
 */
extern void oneshot_client_end(oneshot_client_t *c) ;

/**
 * @brief Ask the daemon to run the up or down script of @p servicedir, forwarding
 * the caller's `0`/`1`/`2` to the script, and block until the result is known.
 *
 * Sends one RUN request whose payload is @p servicedir (not NUL-terminated on the
 * wire) and whose ancillary data carries the caller's current descriptors `0`,
 * `1` and `2`, then runs the event loop until a response frame is handled, the
 * connection drops, a watched signal (`SIGTERM`/`SIGINT`) fires, or the guard timer
 * expires. When @p timeout is > 0 it arms a guard timer of that many milliseconds;
 * when <= 0 no guard timer is set and the call can block indefinitely. On a
 * received `ONESHOT_OK` response the script's `waitpid` status is stored in
 * @c c->wstat. The protocol outcome is always available in @c c->status after the
 * call.
 *
 * @param[in,out] c       Initialized client (from `oneshot_client_init`). Must not
 *                        be NULL (not checked). Updated in place: @c status,
 *                        @c wstat, @c request_sent, @c response_received.
 * @param[in] down        0 -> request the up/`run` script; non-zero -> request the
 *                        down/`finish` script (sets `ONESHOT_FLAG_DOWN`).
 * @param[in] servicedir  Service directory path. Must be nonempty and at most
 *                        `ONESHOT_PAYLOAD_MAX` bytes (its length, NUL excluded).
 * @param[in] timeout     Guard-timer duration in milliseconds; <= 0 disables it.
 *
 * @return Status code:
 *      - 1 if a response was received from the daemon. Inspect @c c->status for
 *        the protocol outcome (`ONESHOT_OK`, `ONESHOT_ERR`, `ONESHOT_PROTO`),
 *        and @c c->wstat when @c c->status is `ONESHOT_OK`.
 *      - 0 if no response was obtained; @c c->status is forced to `ONESHOT_ERR`.
 *        This covers a rejected argument, a send failure, a timeout, a watched
 *        signal, and a connection drop before reply.
 *
 * @retval 0 errno is set to `EINVAL` if @p servicedir is empty or longer than
 *         `ONESHOT_PAYLOAD_MAX`.
 * @retval 0 if `stream_message_send_with_fds` fails, the failure is logged and
 *         errno reflects that underlying send error.
 * @retval 0 if the loop ends without a response (timeout, watched signal, or
 *         connection drop): @c c->status is `ONESHOT_ERR` and errno is not set to
 *         any dedicated value by this function.
 *
 * @note A return of 1 does NOT mean the script succeeded; it means the daemon
 * answered. The script's success/failure is encoded in @c c->wstat (a `waitpid`
 * status) and is meaningful only when @c c->status is `ONESHOT_OK`.
 * @see oneshot_status_str
 */
extern int oneshot_run(oneshot_client_t *c, uint8_t down, char const *servicedir, int timeout) ;

#endif
