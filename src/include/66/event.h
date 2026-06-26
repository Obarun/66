/*
 * event.h
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
 * @brief Layered event-reading engine over service supervise fifodirs.
 *
 * Event reading is split into three layers so the core never has to move:
 *
 *   - SOURCE   (transport): how a readable fd is obtained. Today `event_fifo`
 *     (this module); later a socket or an fdholder hand-off.
 *   - PUMP     (the rock): puts an fd on the oblibs SSE loop, reads it, and
 *     hands the raw bytes up. `event_reader`. Transport- and format-agnostic;
 *     it never interprets the bytes.
 *   - CONSUMER (format): gives the bytes meaning. Today `event_wait` (s6
 *     single-byte transitions); later eventd (rich framed payloads).
 *
 * This replaces the ftrigr / s6-ftrigrd client layer -- and the skalibs it
 * dragged in -- with plain libc plus oblibs. The s6 fifodir broadcast is kept:
 * every subscriber drops its own fifo in the service event directory and the
 * producer (s6-supervise) fans each message out to all of them.
 */

#ifndef SS_EVENT_H
#define SS_EVENT_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <oblibs/sse.h>
#include <oblibs/clock.h>

#include <66/config.h>

/**
 * @brief A service/supervisor event: the format-independent vocabulary.
 *
 * One single typed vocabulary for everything a waiter expresses AND everything
 * the producer emits. The first seven are the transitions 66-supervise actually
 * emits; their wire encoding (a single byte today) lives in `event_to_byte` /
 * `event_from_byte`, NOT here -- a future rich payload swaps the codec while this
 * enum and the matcher stay put. The two RESTART values are wait-only composites
 * (no transition byte): a waiter may want them, the producer never emits them.
 * `EVENT_NONE` is what `event_from_byte` returns for a byte it does not know.
 */
typedef enum event_e event_t ;
enum event_e
{
    EVENT_UP,             // process up, not ready           (byte 'u')
    EVENT_READY,          // process up and ready            (byte 'U')
    EVENT_DOWN,           // finishing / down                (byte 'd')
    EVENT_DOWN_READY,     // fully down, ready to start       (byte 'D')
    EVENT_NORESTART,      // will not be restarted            (byte 'O')
    EVENT_SUPERVISE_UP,   // supervisor started              (byte 's')
    EVENT_SUPERVISE_DOWN, // supervisor exiting              (byte 'x')
    EVENT_RESTART,        // wait-only: down then up          (no byte)
    EVENT_RESTART_READY,  // wait-only: down then up+ready     (no byte)
    EVENT_NONE            // unknown / not an event           (no byte)
} ;

/**
 * @brief Wire codec: convert between an event and its single transition byte.
 *
 * The byte is just today's serialization; isolating it here keeps the rest of
 * the engine format-independent (a rich-payload codec would sit beside these).
 *
 * @return event_to_byte: the transition byte for a wire event, or 0 for an event
 *         that never travels on the wire (RESTART, RESTART_READY, NONE).
 * @return event_from_byte: the event for a known byte, or EVENT_NONE otherwise.
 */
extern char event_to_byte(event_t e) ;
extern event_t event_from_byte(char c) ;

/** @brief event_match_feed verdicts. */
#define EVENT_MATCH_FAIL    (-1) // permanent failure ('O' while wanting up, or 'x')
#define EVENT_MATCH_PENDING   0  // not reached yet
#define EVENT_MATCH_OK        1  // wanted state reached

/**
 * @brief Subscriber fifo naming, byte-compatible with s6-supervise's fanout.
 *
 * A subscriber fifo is named `"ftrig1:" + TAI64N stamp + ":" + random suffix`.
 * s6-supervise's fanout filters directory entries on the `"ftrig1:"` prefix
 * (`EVENT_FIFO_PREFIXLEN` bytes) AND an exact total name length
 * (`EVENT_FIFO_NAMELEN`), so this layout must match it byte for byte; both
 * `event_fifo_subscribe` and `event_fifodir_clean` rely on it.
 *
 * - `EVENT_FIFO_PREFIX`    the literal `"ftrig1:"` prefix.
 * - `EVENT_FIFO_PREFIXLEN` its length without the NUL (7).
 * - `EVENT_FIFO_RANDLEN`   length of the random suffix (6 chars).
 * - `EVENT_FIFO_NAMELEN`   full entry-name length without the NUL (39):
 *                          prefix + TAI64N stamp + ':' separator + suffix.
 */
#define EVENT_FIFO_PREFIX     "ftrig1:"
#define EVENT_FIFO_PREFIXLEN  (sizeof(EVENT_FIFO_PREFIX) - 1)        /* 7 */
#define EVENT_FIFO_RANDLEN    6
#define EVENT_FIFO_NAMELEN    (EVENT_FIFO_PREFIXLEN + CLOCK_TAI64N_LEN + 1 + EVENT_FIFO_RANDLEN)  /* 39 */

typedef struct event_reader_s event_reader_t ;
typedef struct event_fifo_s event_fifo_t ;
typedef struct event_wait_s event_wait_t ;
typedef struct event_match_s event_match_t ;

/**
 * @struct event_match_s
 * @brief The transition interpreter: tracks (up, ready) and matches a wanted state.
 *
 * Feed it the raw transition bytes as they arrive; it maintains the decoded
 * `(up, ready)` pair and tells the caller when @wanted is reached or has
 * permanently failed. The same matcher backs every waiter (event_wait and the
 * svc launch/daemon waits), so the s6 wait semantics live in exactly one place.
 *
 * @param wanted       The service state being waited for.
 * @param up, ready    Current decoded process state (seed at init, updated on feed).
 * @param restart_done For a RESTART wait, set once the down phase has been seen.
 */
struct event_match_s
{
    event_t wanted ;
    unsigned char up ;
    unsigned char ready ;
    unsigned char restart_done ;
} ;

/**
 * @brief Initialize a matcher for @wanted, seeded with the currently known state.
 *
 * @param[out] m      Matcher to initialize.
 * @param[in]  wanted The state to wait for.
 * @param[in]  up     Current up bit (1 if the process is up), or 0 if unknown.
 * @param[in]  ready  Current ready bit, or 0 if unknown.
 *
 * @note A non-RESTART wait whose seed already satisfies @wanted is reported as
 *       matched on the first `event_match_feed` (including a zero-length feed).
 * @see event_match_feed
 */
extern void event_match_init(event_match_t *m, event_t wanted, unsigned char up, unsigned char ready) ;

/**
 * @brief Feed transition bytes and report whether @wanted has been reached.
 *
 * Updates the tracked `(up, ready)` from the bytes in @buf (decoding `d/D/u/U`,
 * handling the two-phase RESTART, and treating `O` while waiting up or `x` as a
 * permanent failure). Supervise waits match the exact `'s'`/`'x'` byte instead.
 * A `len == 0` call evaluates the current state without consuming input.
 *
 * @param[in,out] m   Initialized matcher.
 * @param[in]     buf Bytes read from the source (may be NULL when @len is 0).
 * @param[in]     len Number of bytes in @buf.
 *
 * @return `EVENT_MATCH_OK` (1) if @wanted is reached, `EVENT_MATCH_FAIL` (-1) on
 *         permanent failure, `EVENT_MATCH_PENDING` (0) otherwise. Once OK or FAIL
 *         is returned the matcher should not be fed further.
 * @see event_match_init
 */
extern int event_match_feed(event_match_t *m, char const *buf, size_t len) ;


/**
 * @brief Consumer callback invoked by the pump with each chunk read from a source.
 *
 * The pump calls this with the bytes exactly as they arrived from the fd: a
 * single read may carry several bytes (`1 <= len <= 256`, the pump's internal
 * buffer size), and the handler may be called repeatedly within one wakeup as
 * the pump drains the fd until it would block. The pump stays AGNOSTIC to the
 * wire format -- interpreting the bytes is the consumer's job. Today the payload
 * is the s6 single-byte transitions (one byte = one event); a future consumer
 * can reassemble length-prefixed frames that straddle reads without touching the
 * pump.
 *
 * A call with `len == 0` (and `buf` possibly NULL) signals the SOURCE HAS CLOSED
 * (EOF / hangup): no more data will ever arrive on this reader. The pump raises
 * it on `SSE_ERROR`/`SSE_HUP`, or when a read returns 0 bytes (surfaced as
 * `EPIPE` by the read layer). On the fifo source this NEVER fires, because the
 * source holds the write end open; it is the close path for the future socket
 * and fdholder sources.
 *
 * @param[in] r    The pump itself, with stable identity, handed back so the
 *                 handler can correlate the call (e.g. to schedule a deferred
 *                 teardown). MUST NOT be detached from inside this callback.
 * @param[in] buf  The bytes read, valid only for the duration of the call. NULL
 *                 is possible when `len == 0`.
 * @param[in] len  Number of valid bytes in `buf`; `0` means the source closed.
 * @param[in] data The opaque cookie registered at attach time, passed back
 *                 verbatim. The pump makes no assumption about its contents.
 *
 * @return Nothing.
 *
 * @note The handler MUST NOT detach its own reader (`event_reader_detach`,
 *       `event_fifo_unsubscribe`): that would free the watcher currently being
 *       dispatched. To stop the loop, clear the epoll's `running` flag instead
 *       (`r` reaches the epoll, or the consumer keeps its own handle). Any
 *       teardown of this reader must be deferred until the dispatch has
 *       returned.
 */
typedef void (event_handler_t)(event_reader_t *r, char const *buf, size_t len, void *data) ;

/**
 * @struct event_reader_s
 * @brief The pump: one readable fd on the SSE loop, drained into a handler.
 *
 * Owns a single readable fd attached to an `sse_epoll_t`, reads it as bytes
 * arrive, and hands each raw chunk to `handler`. Transport- and format-agnostic.
 * Its address must stay stable between `event_reader_attach` and
 * `event_reader_detach`: the loop hashes `&watcher` for lookup, and the
 * watcher's `cbdata` points back at the enclosing `event_reader_t`, which the
 * pump hands to the handler as `r`.
 *
 * @param watcher
 * The SSE io watcher. It owns the read fd, which is closed by `sse_free_io`
 * during `event_reader_detach`.
 *
 * @param handler
 * The consumer callback invoked with each chunk of bytes read (and with
 * `len == 0` on source close). NULL after detach.
 *
 * @param data
 * The opaque cookie passed back to `handler` verbatim. NULL after detach.
 */
struct event_reader_s
{
    sse_watcher_t watcher ;      // io watcher; owns the read fd (closed by sse_free_io)
    event_handler_t *handler ;   // called with each chunk of bytes read
    void *data ;                 // opaque cookie passed back to handler
} ;

/**
 * @brief Attach an already-open readable fd to the loop and start pumping it.
 *
 * Records `handler`/`data` in @r, then registers @r as an io watcher on @ep for
 * read events at @priority. On success the watcher takes OWNERSHIP of @fd: the
 * fd is from then on closed by `event_reader_detach` (via `sse_free_io`), and
 * the caller must not close it. On failure the watcher is not registered and the
 * caller STILL OWNS @fd (the function does not close it).
 *
 * @param[in,out] r        Pump to initialize. Must keep a STABLE ADDRESS until
 *                         `event_reader_detach` (the loop hashes `&r->watcher`).
 * @param[in]     ep       The SSE event loop to register with.
 * @param[in]     fd       An already-open readable fd; ownership transfers to
 *                         @r on success only.
 * @param[in]     handler  Consumer callback invoked with each chunk read.
 * @param[in]     data     Opaque cookie handed back to @handler verbatim.
 * @param[in]     priority Event-processing priority for the watcher.
 *
 * @return 1 on success (@r owns @fd and is active on @ep).
 * @return 0 on failure; errno is set to the `sse_start_io` errno (`EINVAL` for
 *         invalid parameters, `ENOMEM` on allocation failure, or another
 *         registration error). On failure the caller still owns @fd.
 *
 * @see event_reader_detach
 */
extern int event_reader_attach(event_reader_t *r, sse_epoll_t *ep, int fd, event_handler_t *handler, void *data, int priority) ;

/**
 * @brief Detach the pump from the loop and close its fd.
 *
 * Removes @r's watcher from the loop and closes the owned read fd (via
 * `sse_free_io`), then clears `handler` and `data` to NULL. A NULL @r is a
 * no-op.
 *
 * @param[in,out] r The pump to detach. May be NULL (no-op).
 *
 * @return Nothing.
 *
 * @note MUST NOT be called from within the handler: it frees the very watcher
 *       being dispatched. A handler that wants to stop reading should clear the
 *       epoll's `running` flag instead, and let the teardown happen after the
 *       dispatch returns.
 * @see event_reader_attach
 */
extern void event_reader_detach(event_reader_t *r) ;

/**
 * @struct event_fifo_s
 * @brief A subscriber fifo in a service event fifodir, pumped through a reader.
 *
 * Wraps the pump with the fifo-specific state: a write end kept open so reads
 * never see EOF, and the published fifo path to unlink on teardown. The fifo is
 * created under a hidden `"."`-prefixed name and renamed into place only once
 * both ends are open, so the producer never sees a visible fifo without a
 * reader.
 *
 * @param reader
 * The pump driving this fifo's read end.
 *
 * @param wfd
 * The write end, held open by us so the fifo always has a writer and reads
 * return EAGAIN instead of EOF when idle. `-1` when not held.
 *
 * @param fifopath
 * The published fifo path, unlinked on `event_fifo_unsubscribe`. An empty string
 * (first byte `'\0'`) means no fifo is currently owned.
 */
struct event_fifo_s
{
    event_reader_t reader ;      // the pump
    int wfd ;                    // write end held open so reads never see EOF
    char fifopath[SS_MAX_PATH] ; // our fifo, unlinked on unsubscribe (empty = none)
} ;

/**
 * @brief Drop our own subscriber fifo into a service event fifodir and pump it.
 *
 * Creates a fifo named per the s6 `ftrig1` convention (so s6-supervise's fanout
 * writes to it) inside @eventdir, then pumps it into @handler / @data. The fifo
 * is first created under a hidden `"."`-prefixed name; the read end is opened
 * `O_RDONLY | O_NONBLOCK | O_CLOEXEC`, its mode forced to `0622` with `fchmod`
 * (regardless of umask, so a same-group s6-supervise can open it for writing),
 * then a write end is opened `O_WRONLY | O_NONBLOCK | O_CLOEXEC` and held open;
 * only then is the fifo renamed into place. The read end is handed to the pump,
 * which owns it from there. On a transient name clash (`EEXIST`) the loop retries
 * with a fresh random suffix.
 *
 * On ANY failure the function fully unwinds: the hidden (or published) fifo is
 * unlinked, both fds are closed, `wfd` is reset to `-1`, and `fifopath` is
 * emptied, so @f is left clean and need not be unsubscribed.
 *
 * @param[out] f        Fifo source to initialize (zeroed on entry). Must keep a
 *                      STABLE ADDRESS until `event_fifo_unsubscribe` (the pump's
 *                      watcher is hashed by address).
 * @param[in]  ep       The SSE event loop to register the read end with.
 * @param[in]  eventdir Service event directory to drop the fifo into. Must be
 *                      short enough that the hidden fifo path fits in
 *                      `SS_MAX_PATH` (see ENAMETOOLONG below).
 * @param[in]  handler  Consumer callback invoked with each chunk read.
 * @param[in]  data     Opaque cookie handed back to @handler verbatim.
 * @param[in]  priority Event-processing priority for the read-end watcher.
 *
 * @return 1 on success (@f owns the read end via its pump and the held write end).
 * @return 0 on failure; errno is set and @f is left clean. The errno per path is:
 *         - `ENAMETOOLONG` if the hidden fifo path would not fit `SS_MAX_PATH`.
 *         - the `clock_gettime` errno if reading the wall clock fails.
 *         - the `getrandom` errno if the random suffix cannot be generated.
 *         - the `mkfifo` errno (other than the retried `EEXIST`) on fifo creation.
 *         - the `open` errno when opening the read end fails.
 *         - the `fchmod` errno when forcing the fifo mode fails.
 *         - the `open` errno when opening the write end fails.
 *         - the `rename` errno when publishing the fifo fails.
 *         - the `sse_start_io` errno when attaching the reader fails.
 *
 * @note On the fifo source the handler never sees a `len == 0` close, because
 *       this function holds the write end open for the lifetime of @f.
 * @see event_fifo_unsubscribe
 */
extern int event_fifo_subscribe(event_fifo_t *f, sse_epoll_t *ep, char const *eventdir, event_handler_t *handler, void *data, int priority) ;

/**
 * @brief Unsubscribe: unlink our fifo, detach the pump, close the write end.
 *
 * Tears the source down in the order that avoids a producer racing onto a
 * reader-less fifo: it unlinks `fifopath` FIRST (so the published name is gone
 * before the read end closes, never reaching `ENXIO`), then detaches the pump
 * (which closes the read end), then closes the write end and resets `wfd` to
 * `-1`. A NULL @f is a no-op; an already-clean @f (empty `fifopath`, `wfd == -1`)
 * unlinks nothing and closes nothing.
 *
 * @param[in,out] f The fifo source to tear down. May be NULL (no-op).
 *
 * @return Nothing.
 *
 * @note MUST NOT be called from within the handler: it detaches the pump and so
 *       frees the watcher being dispatched. Defer it until after the dispatch
 *       returns.
 * @see event_fifo_subscribe
 */
extern void event_fifo_unsubscribe(event_fifo_t *f) ;

/**
 * @brief Create (or normalize) a service event fifodir. Replaces ftrigw_fifodir_make.
 *
 * Creates @path as a directory `0700` (with umask forced to 0 for the `mkdir`).
 * If @path already exists, it must be a real directory OWNED BY THE CALLER (a
 * symlink is rejected -- the existence check uses `lstat`, not `stat`, so a
 * planted symlink is not followed); the call then RE-APPLIES the ownership and
 * permissions below, so the postcondition holds however the directory was
 * created (force=1, idempotent).
 *
 * Permissions: if @gid is not `(gid_t)-1`, the directory is `chown`ed to that
 * group and `chmod`ed `03730` (setgid + group-write, so a same-group
 * s6-supervise can fan out into it); otherwise it is `chmod`ed `01733`.
 *
 * @param[in] path Directory path to create or normalize.
 * @param[in] gid  Group to own the directory, or `(gid_t)-1` to skip the chown
 *                 and use the more restrictive `01733` mode.
 *
 * @return 1 on success (directory exists with the intended owner and mode).
 * @return 0 on failure; errno is set and per path is:
 *         - the `mkdir` errno (other than `EEXIST`) when creation fails.
 *         - the `lstat` errno when stat-ing an existing @path fails.
 *         - `EACCES` if an existing @path is not owned by the caller's uid.
 *         - `ENOTDIR` if an existing @path is not a directory (e.g. a symlink
 *           or a regular file).
 *         - the `chown` errno when the group chown fails.
 *         - the `chmod` errno when setting the directory mode fails.
 *
 * @note Idempotent: safe to call repeatedly; an existing owned directory has its
 *       perms re-applied each time.
 * @see event_fifodir_clean
 */
extern int event_fifodir_make(char const *path, gid_t gid) ;

/**
 * @brief Sweep orphan subscriber fifos from a fifodir. Replaces ftrigw_clean.
 *
 * Scans @path and, for every entry matching the subscriber naming (the
 * `EVENT_FIFO_PREFIX` prefix AND the exact `EVENT_FIFO_NAMELEN` length),
 * probes it by opening `O_WRONLY | O_NONBLOCK`. An orphan fifo with no reader
 * rejects that open with `ENXIO`; such an entry is unlinked. An entry that opens
 * (a live reader) is closed and left alone, and any other open error is ignored;
 * the sweep only fails on an `unlink` that itself fails, or on a `readdir`
 * error. Errors are deferred past `closedir` so the directory stream is never
 * leaked, and the FIRST error is the one reported.
 *
 * @param[in] path Fifodir to sweep.
 *
 * @return 1 on success (all orphans that were found were swept; no fatal error).
 * @return 0 on failure; errno is set and per path is:
 *         - the `opendir` errno if @path cannot be opened.
 *         - the `unlink` errno if removing an orphan fifo fails (first such).
 *         - the `readdir` errno if iterating the directory fails (first error
 *           wins; a clean end of directory is not an error).
 *
 * @note A `O_WRONLY | O_NONBLOCK` open failing with anything other than `ENXIO`
 *       does NOT cause failure and does NOT unlink the entry; only `ENXIO`
 *       (no reader) triggers an unlink.
 * @see event_fifodir_make
 */
extern int event_fifodir_clean(char const *path) ;

/**
 * @brief Fan a message out to every subscriber fifo in a fifodir. The producer
 * side of the broadcast, byte-compatible with s6's ftrigw fanout.
 *
 * Scans @path and, for every entry matching the subscriber naming (the
 * `EVENT_FIFO_PREFIX` prefix AND the exact `EVENT_FIFO_NAMELEN` length), opens it
 * `O_WRONLY | O_NONBLOCK` and writes the @len bytes at @s into it. This is how
 * 66-supervise emits a transition byte (`EVENT_* event`) to all current readers.
 *
 * The producer NEVER blocks on a bad subscriber: a fifo with no reader (`ENXIO`)
 * or whose reader has gone (`EPIPE`) is unlinked, and a full fifo (`EAGAIN`) or
 * any short write is dropped. Errors are deferred past `closedir` so the
 * directory stream is never leaked, and the FIRST error is the one reported.
 *
 * @param[in] path Fifodir to fan out into.
 * @param[in] s    Bytes to write to each subscriber (typically one `EVENT_* event`).
 * @param[in] len  Number of bytes at @s.
 *
 * @return 1 on success (message delivered to every live subscriber; orphans
 *         swept).
 * @return 0 on failure; errno is set and per path is:
 *         - the `opendir` errno if @path cannot be opened.
 *         - the `unlink` errno if removing an orphan/broken fifo fails (first
 *           such).
 *         - the `readdir` errno if iterating the directory fails (first error
 *           wins; a clean end of directory is not an error).
 *
 * @note A short or failed write that is NOT `EPIPE` (e.g. `EAGAIN` on a full
 *       fifo) is dropped silently and does NOT cause failure; the producer must
 *       not stall on a slow reader.
 * @see event_fifodir_clean
 */
extern int event_fifodir_notify(char const *path, char const *s, size_t len) ;

/**
 * @brief Typed producer-side emit: fan @n events out to a fifodir in one message.
 *
 * Encodes each of @ev to its transition byte (`event_to_byte`) and hands the
 * resulting bytes to `event_fifodir_notify` in a single write, so a multi-event
 * transition (e.g. DOWN then DOWN_READY) reaches every subscriber atomically.
 * This is how 66-supervise emits, in the typed vocabulary rather than raw bytes.
 *
 * @param[in] path Fifodir to fan out into.
 * @param[in] ev   Events to emit (must be wire events; RESTART/NONE encode to 0).
 * @param[in] n    Number of events (1 or more).
 *
 * @return 1 on success, 0 on failure (the `event_fifodir_notify` errno).
 * @see event_fifodir_notify
 * @see event_to_byte
 */
extern int event_fifodir_emit(char const *path, event_t const *ev, size_t n) ;

/**
 * @struct event_wait_s
 * @brief A wait_and over a set of service event fifodirs.
 *
 * Subscribes to each given event directory and blocks until EVERY source has
 * seen the wanted transition byte, or a deadline fires. Each source pumps into
 * `event_wait`'s own handler with a PER-SOURCE cookie (the `data` idiom): the
 * cookie carries the back-pointer to this struct and an "already matched" flag,
 * so no shared index table is needed. This is a consumer of the pump that gives
 * the bytes meaning (the s6 transition bytes).
 *
 * @param epoll
 * The SSE event loop driving all sources and the deadline timer.
 *
 * @param fifos
 * Heap array of @n fifo sources, held at a stable address (the pump hashes each
 * watcher by address). NULL when @n is 0.
 *
 * @param slots
 * Heap array of @n per-source cookies. Its element layout is PRIVATE to
 * event_wait.c (a `void *` here on purpose); callers must not dereference it.
 *
 * @param timer
 * The deadline-timer watcher, registered only when a positive timeout is given.
 *
 * @param n
 * Number of sources / cookies. Zero is a valid degenerate set (nothing to wait
 * for).
 *
 * @param triggered
 * Count of sources that have already seen @wanted. The wait completes when
 * `triggered == n`.
 *
 * @param wanted
 * The service state every source is waiting for (interpreted by event_match).
 *
 * @param failed
 * Set when a source reported a permanent failure (`O`/`x`): the wait ended early
 * and `event_wait_run` returns 0, distinct from a clean timeout.
 *
 * @param timer_active
 * Non-zero while the deadline timer is registered on @epoll.
 */
struct event_wait_s
{
    sse_epoll_t epoll ;
    event_fifo_t *fifos ; // n fifo sources held at a stable address
    void *slots ; // n per-source cookies (private layout, event_wait.c)
    sse_watcher_t timer ;
    size_t n ;
    size_t triggered ; // sources that have reached `wanted`
    event_t wanted ; // service state every source waits for
    int failed ; // a source reported permanent failure (O/x)
    int timer_active ;
} ;

/**
 * @brief Subscribe to every event directory, each waiting for the byte @wanted.
 *
 * Zero-initializes @w, creates its event loop, and -- when @n is non-zero --
 * allocates the @n fifo sources and their per-source cookies and subscribes a
 * fifo to each of @eventdirs (via `event_fifo_subscribe`). If any subscription
 * fails, every already-subscribed source is unsubscribed, all allocations are
 * freed, and the loop is released, so @w is left clean and need not be freed.
 * With @n == 0 the loop is created and the function returns success with no
 * sources.
 *
 * @param[out] w         Wait set to initialize. Must keep a STABLE ADDRESS until
 *                       `event_wait_free` (sources and timer are hashed by
 *                       address).
 * @param[in]  eventdirs Array of @n service event directories to subscribe to.
 *                       Read only when @n > 0.
 * @param[in]  n         Number of directories.
 * @param[in]  wanted    The service state every source must reach.
 *
 * @return 1 on success (all @n sources subscribed, or @n == 0).
 * @return 0 on failure; errno is set and per path is:
 *         - the `sse_new` errno if the event loop cannot be created.
 *         - `ENOMEM` if the sources array or the cookies array cannot be
 *           allocated.
 *         - the `event_fifo_subscribe` errno of the source that failed (see that
 *           function for the full list).
 *
 * @note The caller must trigger the events (e.g. reload the scandir) AFTER this
 *       returns and BEFORE `event_wait_run`, so all subscriptions are in place
 *       before the producer starts emitting.
 * @see event_wait_run
 * @see event_wait_free
 */
extern int event_wait_init(event_wait_t *w, char const *const *eventdirs, size_t n, event_t wanted) ;

/**
 * @brief Block until every subscribed source has seen its byte, or the deadline.
 *
 * Runs the event loop until either every source has matched @wanted (the handler
 * clears `running` when `triggered == n`) or, if @timeout_ms is positive, a
 * one-shot deadline timer fires and clears `running`. A non-positive @timeout_ms
 * installs no timer and the wait blocks indefinitely until all sources match.
 * The timer, if installed, is freed before returning. With `w->n == 0` it
 * returns success immediately without entering the loop.
 *
 * @param[in,out] w          Initialized wait set.
 * @param[in]     timeout_ms Whole-wait deadline in milliseconds; `<= 0` means no
 *                           deadline (wait forever).
 *
 * @return 1 if every source saw @wanted (`triggered == n`, including the
 *         degenerate `n == 0`).
 * @return 0 on timeout: the loop ended cleanly (the deadline fired) but not all
 *         sources matched. errno is NOT meaningfully set on this path.
 * @return -1 on loop error; errno is set to the `sse_poll` errno (e.g. `EINVAL`
 *         for an invalid loop, or another `epoll_wait` error). EINTR is retried
 *         internally by the loop and does not surface here.
 *
 * @note Distinguish 0 (timeout, partial match) from -1 (loop failure) by the
 *       return value, not by errno.
 * @see event_wait_init
 */
extern int event_wait_run(event_wait_t *w, int timeout_ms) ;

/**
 * @brief Unsubscribe every source and release the loop.
 *
 * Unsubscribes each of the @w->n fifo sources, frees the sources and cookies
 * arrays, releases the event loop, and resets `n` and `triggered` to 0. Safe to
 * call on a wait set with no sources.
 *
 * @param[in,out] w The wait set to release.
 *
 * @return Nothing.
 *
 * @note MUST NOT be called from within a handler (it unsubscribes sources, which
 *       free watchers being dispatched). Does not free @w itself (caller owns it).
 * @see event_wait_init
 */
extern void event_wait_free(event_wait_t *w) ;

#endif
