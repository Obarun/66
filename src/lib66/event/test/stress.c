/* stress.c — ADVERSARIAL stress harness for the 66 `event` module. Its job is to
 * try to BREAK the implemented layers under load (100+ events): infinite loops,
 * buffer overruns, leaks, and — the core ask — PRODUCER STARVATION on the fanout.
 *
 * It complements test_event.c (per-behaviour correctness) by hammering the same
 * code with hostile input at scale and asserting by EFFECT:
 *   - every valid frame in a storm is decoded exactly once, in order (no loss/dup);
 *   - the decoder does BOUNDED work and never over-reads its 31-byte buffer (ASan);
 *   - the matcher tracks the LAST absolute state (overwrite, not cumulative) and a
 *     consumer latch ignores everything after OK/FAIL;
 *   - the producer NEVER blocks: dead subscribers (ENXIO) are unlinked, full/slow
 *     ones (EAGAIN) are dropped, and a HEALTHY reader wedged between them still
 *     receives every frame — with zero fd/inode leak;
 *   - event_wait over many sources triggers each exactly once and frees cleanly.
 *
 * Hardened: ASan + UBSan + LSan, with a process alarm() anti-hang (any wedge dies
 * as exit 124 = FAILURE, never a hang).
 */
#include "ctest.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include <oblibs/sse.h>
#include <oblibs/log.h>
#include <oblibs/clock.h>
#include <oblibs/types.h>

#include <66/event.h>
#include <66/status.h>
#include "helpers.h"

/* anti-hang: any wedge (a producer that blocks, a decoder loop) dies here. */
static void on_alarm(int sig) { (void)sig ; _exit(124) ; }

static char *mkdir_scratch(char *tmpl)
{
    char *p = t_tmpdir(tmpl) ;
    T_ASSERT(p != NULL, "mkdtemp") ;
    return p ;
}

static void rm_rf(char const *dir)
{
    DIR *d = opendir(dir) ;
    if (d) {
        struct dirent *e ;
        char path[1024] ;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue ;
            snprintf(path, sizeof(path), "%s/%s", dir, e->d_name) ;
            unlink(path) ;
        }
        closedir(d) ;
    }
    rmdir(dir) ;
}

static struct timespec fixed_stamp(void)
{
    struct timespec ts ;
    ts.tv_sec = (time_t)0x1122334455LL ;
    ts.tv_nsec = 123456789L ;
    return ts ;
}

/* number of open fds in this process (minus the opendir handle itself). Used to
 * prove the fanout / wait paths leak neither read nor write ends. */
static int count_fds(void)
{
    DIR *d = opendir("/proc/self/fd") ;
    if (!d) return -1 ;
    int n = 0 ;
    struct dirent *e ;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue ;
        n++ ;
    }
    closedir(d) ;
    return n - 1 ;
}

/* ================================================================== */
/* a frame descriptor: what we packed, so we can verify what decoded.  */
/* ================================================================== */

typedef struct {
    uint8_t kind ;
    uint8_t state, result, who, signo, phase, flags ;
    uint32_t code, pid ;
} fdesc_t ;

/* deterministic per-index frame: cycles the three kinds, distinct payloads. */
static void frame_params(size_t i, fdesc_t *d)
{
    memset(d, 0, sizeof(*d)) ;
    switch (i % 3) {
        case 0 :
            d->kind = EVENT_KIND_TRANSITION ;
            d->state = STATUS_STATE_UP ;
            d->result = STATUS_RESULT_SUCCESS ;
            d->who = STATUS_WHO_SELF ;
            d->code = (uint32_t)(i * 2654435761u) ;   // scrambled, catches field swaps
            d->pid = (uint32_t)(1000 + i) ;
            d->flags = 0 ;
            break ;
        case 1 :
            d->kind = EVENT_KIND_SIGNAL ;
            d->signo = (uint8_t)((i % 63) + 1) ;
            d->who = STATUS_WHO_USER ;
            break ;
        default :
            d->kind = EVENT_KIND_LIFECYCLE ;
            d->phase = (i & 1) ? EVENT_LIFECYCLE_DOWN : EVENT_LIFECYCLE_UP ;
            break ;
    }
}

static size_t pack_desc(char *out, fdesc_t const *d, struct timespec const *ts)
{
    switch (d->kind) {
        case EVENT_KIND_TRANSITION :
            return event_frame_pack_transition(out, d->state, d->result, d->who, d->code, d->pid, ts, d->flags) ;
        case EVENT_KIND_SIGNAL :
            return event_frame_pack_signal(out, d->signo, d->who, ts) ;
        default :
            return event_frame_pack_lifecycle(out, d->phase, ts) ;
    }
}

static void verify_desc(event_frame_t const *f, fdesc_t const *d, size_t i)
{
    char msg[64] ;
    snprintf(msg, sizeof(msg), "frame %zu kind", i) ;
    T_ASSERT_EQ(d->kind, f->kind, msg) ;
    switch (d->kind) {
        case EVENT_KIND_TRANSITION :
            T_ASSERT_EQ(d->state, f->state, "transition state") ;
            T_ASSERT_EQ(d->result, f->result, "transition result") ;
            T_ASSERT_EQ(d->who, f->who, "transition who") ;
            T_ASSERT_EQ((long long)d->code, (long long)f->code, "transition code") ;
            T_ASSERT_EQ((long long)d->pid, (long long)f->pid, "transition pid") ;
            T_ASSERT_EQ(d->flags, f->flags, "transition flags") ;
            break ;
        case EVENT_KIND_SIGNAL :
            T_ASSERT_EQ(d->signo, f->signo, "signal signo") ;
            T_ASSERT_EQ(d->who, f->who, "signal who") ;
            break ;
        default :
            T_ASSERT_EQ(d->phase, f->phase, "lifecycle phase") ;
            break ;
    }
}

/* a frame sink for the decoder: records every frame plus the running count. `n`
 * keeps incrementing PAST the array cap so a loss/dup is caught by the count. */
enum { SINK_CAP = 512 } ;
typedef struct {
    event_aggregator_t ag ;
    event_frame_t f[SINK_CAP] ;
    size_t n ;
} sink_t ;

static void sink_cb(event_frame_t const *f, void *data)
{
    sink_t *s = data ;
    if (s->n < SINK_CAP)
        s->f[s->n] = *f ;
    s->n++ ;
}

static void verify_stream(sink_t *s, fdesc_t const *exp, size_t nframes)
{
    T_ASSERT_EQ((long long)nframes, (long long)s->n, "exact frame count: none lost, none duplicated") ;
    for (size_t i = 0 ; i < nframes ; i++)
        verify_desc(&s->f[i], &exp[i], i) ;
}

/* pack `nframes` mixed valid frames back to back; record each in `exp`. */
static size_t build_stream(char *buf, fdesc_t *exp, size_t nframes, struct timespec const *ts)
{
    size_t off = 0 ;
    for (size_t i = 0 ; i < nframes ; i++) {
        frame_params(i, &exp[i]) ;
        off += pack_desc(buf + off, &exp[i], ts) ;
    }
    return off ;
}

/* ================================================================== */
/* SCENARIO 1: decoder storm — 100+ frames, raw adversarial bytes       */
/* ================================================================== */

enum { STORM_N = 150 } ;   // > 100 frames

/* 1a: the whole valid stream in ONE chunk -> each frame decoded once, in order. */
static void test_storm_onechunk(void)
{
    struct timespec ts = fixed_stamp() ;
    static char buf[STORM_N * EVENT_FRAME_MAX] ;
    static fdesc_t exp[STORM_N] ;
    size_t len = build_stream(buf, exp, STORM_N, &ts) ;

    sink_t s = {0} ;
    event_aggregate(&s.ag, buf, len, sink_cb, &s) ;
    verify_stream(&s, exp, STORM_N) ;
    T_ASSERT(s.ag.len == 0, "decoder buffer fully drained after a clean stream") ;
}

/* 1b: the same stream delivered in chunks of 1, 7, 256, 4096 bytes -> identical
 * result, whatever the chunking. The reassembler must be chunk-invariant. */
static void test_storm_varied_chunks(void)
{
    struct timespec ts = fixed_stamp() ;
    static char buf[STORM_N * EVENT_FRAME_MAX] ;
    static fdesc_t exp[STORM_N] ;
    size_t len = build_stream(buf, exp, STORM_N, &ts) ;

    size_t sizes[] = { 1, 7, 256, 4096 } ;
    for (size_t si = 0 ; si < sizeof(sizes) / sizeof(sizes[0]) ; si++) {
        size_t step = sizes[si] ;
        sink_t s = {0} ;
        for (size_t off = 0 ; off < len ; off += step) {
            size_t chunk = step ;
            if (chunk > len - off) chunk = len - off ;
            event_aggregate(&s.ag, buf + off, chunk, sink_cb, &s) ;
        }
        char msg[64] ;
        snprintf(msg, sizeof(msg), "chunk size %zu: all frames", step) ;
        T_ASSERT_EQ((long long)STORM_N, (long long)s.n, msg) ;
        verify_stream(&s, exp, STORM_N) ;
        T_ASSERT(s.ag.len == 0, "buffer drained regardless of chunk size") ;
    }
}

/* 1c: split the whole stream at EVERY interior offset -> both halves reassemble
 * to the full stream, exactly once each. */
static void test_storm_straddle_every_offset(void)
{
    struct timespec ts = fixed_stamp() ;
    static char buf[STORM_N * EVENT_FRAME_MAX] ;
    static fdesc_t exp[STORM_N] ;
    size_t len = build_stream(buf, exp, STORM_N, &ts) ;

    for (size_t cut = 1 ; cut < len ; cut++) {
        sink_t s = {0} ;
        event_aggregate(&s.ag, buf, cut, sink_cb, &s) ;
        event_aggregate(&s.ag, buf + cut, len - cut, sink_cb, &s) ;
        T_ASSERT_EQ((long long)STORM_N, (long long)s.n, "split stream: exact frame count at every cut") ;
        /* spot-check the first and last frame fields survive the split */
        verify_desc(&s.f[0], &exp[0], 0) ;
        verify_desc(&s.f[STORM_N - 1], &exp[STORM_N - 1], STORM_N - 1) ;
    }
}

/* build a stream of corrupt blocks interleaved with valid frames. Each corrupt
 * block is crafted to SELF-CONSUME (never eat the following valid frame) in the
 * correct decoder; in a decoder whose payload_len bound was removed, the huge/24
 * plen headers make the 31-byte buffer overflow (ASan). */
static size_t build_resync_storm(unsigned char *buf, fdesc_t *exp, size_t nvalid, struct timespec const *ts)
{
    size_t off = 0 ;
    for (size_t i = 0 ; i < nvalid ; i++) {

        switch (i % 6) {

            case 0 : // a run of non-version garbage bytes: dropped one by one
                for (int k = 0 ; k < 9 ; k++) buf[off++] = (unsigned char)(0x02 + k) ;
                break ;

            case 1 : // bad version starts: never anchor
                buf[off++] = 0xFF ; buf[off++] = 0xEE ; buf[off++] = 0x7E ;
                break ;

            case 2 : // version ok, payload_len = 0xFFFFFFFF (>> bound): resync
                buf[off++] = EVENT_VERSION ; buf[off++] = 0x00 ; buf[off++] = 0x00 ; buf[off++] = 0x00 ;
                buf[off++] = 0xFF ; buf[off++] = 0xFF ; buf[off++] = 0xFF ; buf[off++] = 0xFF ;
                break ;

            case 3 : // version ok, payload_len = 24 (exactly one over the 23 bound): resync
                buf[off++] = EVENT_VERSION ; buf[off++] = 0x00 ; buf[off++] = 0x00 ; buf[off++] = 0x00 ;
                buf[off++] = 0x00 ; buf[off++] = 0x00 ; buf[off++] = 0x00 ; buf[off++] = 24 ;
                break ;

            case 4 : // version ok, unknown kind, plen=15 in-bounds: consumed, no cb
                buf[off++] = EVENT_VERSION ; buf[off++] = 0x7F ; buf[off++] = 0x00 ; buf[off++] = 0x00 ;
                buf[off++] = 0x00 ; buf[off++] = 0x00 ; buf[off++] = 0x00 ; buf[off++] = 15 ;
                for (int k = 0 ; k < 15 ; k++) buf[off++] = 0x00 ;
                break ;

            default : // TRANSITION kind but plen=13 (too short for its 11-byte body): consumed, no cb
                buf[off++] = EVENT_VERSION ; buf[off++] = EVENT_KIND_TRANSITION ; buf[off++] = 0x00 ; buf[off++] = 0x00 ;
                buf[off++] = 0x00 ; buf[off++] = 0x00 ; buf[off++] = 0x00 ; buf[off++] = 13 ;
                for (int k = 0 ; k < 13 ; k++) buf[off++] = 0x00 ;
                break ;
        }

        frame_params(i, &exp[i]) ;
        off += pack_desc((char *)buf + off, &exp[i], ts) ;
    }
    return off ;
}

/* 1d: resync-storm. Every valid frame is recovered after each corrupt block, and
 * the decoder never over-reads its buffer (ASan) nor loops (alarm). */
static void test_storm_resync_recovers_all(void)
{
    struct timespec ts = fixed_stamp() ;
    static unsigned char buf[STORM_N * (EVENT_FRAME_MAX + 32)] ;
    static fdesc_t exp[STORM_N] ;
    size_t len = build_resync_storm(buf, exp, STORM_N, &ts) ;

    /* whole storm in one chunk */
    sink_t s = {0} ;
    event_aggregate(&s.ag, (char const *)buf, len, sink_cb, &s) ;
    verify_stream(&s, exp, STORM_N) ;
    T_ASSERT(s.ag.len <= EVENT_FRAME_MAX, "decoder buffer stayed within bound after storm") ;

    /* and byte-by-byte, the harshest chunking for resync */
    sink_t s2 = {0} ;
    for (size_t i = 0 ; i < len ; i++)
        event_aggregate(&s2.ag, (char const *)buf + i, 1, sink_cb, &s2) ;
    verify_stream(&s2, exp, STORM_N) ;
}

/* the payload_len bound, exercised as its own storm: a valid max frame (plen=23)
 * decodes; plen=24 and 0xFFFFFFFF resync away, and a valid frame after them is
 * still recovered. */
static void test_storm_plen_boundary(void)
{
    struct timespec ts = fixed_stamp() ;
    unsigned char buf[256] ;
    size_t off = 0 ;

    /* a real transition frame carries plen exactly 23 (the bound): must decode */
    off += event_frame_pack_transition((char *)buf, STATUS_STATE_UP, 0, 0, 42, 7, &ts, 0) ;

    /* plen = 24 (one over): resync */
    buf[off++] = EVENT_VERSION ; buf[off++] = 0 ; buf[off++] = 0 ; buf[off++] = 0 ;
    buf[off++] = 0 ; buf[off++] = 0 ; buf[off++] = 0 ; buf[off++] = 24 ;

    /* plen = 0xFFFFFFFF: resync */
    buf[off++] = EVENT_VERSION ; buf[off++] = 0 ; buf[off++] = 0 ; buf[off++] = 0 ;
    buf[off++] = 0xFF ; buf[off++] = 0xFF ; buf[off++] = 0xFF ; buf[off++] = 0xFF ;

    /* a trailing valid frame that must survive the two resyncs */
    off += event_frame_pack_lifecycle((char *)buf + off, EVENT_LIFECYCLE_DOWN, &ts) ;

    sink_t s = {0} ;
    event_aggregate(&s.ag, (char const *)buf, off, sink_cb, &s) ;
    T_ASSERT_EQ(2, (long long)s.n, "exactly the two valid frames decoded; over-long headers dropped") ;
    T_ASSERT_EQ(EVENT_KIND_TRANSITION, s.f[0].kind, "first is the plen=23 transition") ;
    T_ASSERT_EQ(7, (long long)s.f[0].pid, "its pid survived") ;
    T_ASSERT_EQ(EVENT_KIND_LIFECYCLE, s.f[1].kind, "second is the trailing lifecycle") ;
    T_ASSERT_EQ(EVENT_LIFECYCLE_DOWN, s.f[1].phase, "trailing frame recovered after both resyncs") ;
}

/* prove BOUNDED work on pure hostile noise: thousands of pseudo-random bytes fed
 * in pseudo-random chunk sizes never crash, never over-read (ASan), and produce
 * at most len/EVENT_HDR_LEN frames (each frame is >= 8 bytes). The alarm proves
 * termination. */
static void test_storm_random_noise_bounded(void)
{
    enum { TOTAL = 200000 } ;
    static unsigned char noise[TOTAL] ;
    uint32_t x = 0x9e3779b9u ;   // deterministic xorshift, reproducible
    for (size_t i = 0 ; i < TOTAL ; i++) {
        x ^= x << 13 ; x ^= x >> 17 ; x ^= x << 5 ;
        noise[i] = (unsigned char)x ;
    }

    size_t cb_count = 0 ;
    sink_t s = {0} ;   // reused; we only care about s.n as the invocation count
    size_t i = 0 ;
    while (i < TOTAL) {
        x ^= x << 13 ; x ^= x >> 17 ; x ^= x << 5 ;
        size_t step = (x % 4096) + 1 ;
        if (step > TOTAL - i) step = TOTAL - i ;
        size_t before = s.n ;
        event_aggregate(&s.ag, (char const *)noise + i, step, sink_cb, &s) ;
        cb_count += s.n - before ;
        T_ASSERT(s.ag.len <= EVENT_FRAME_MAX, "buffer bounded during noise") ;
        s.n = 0 ;   // avoid the sink array; we track cb_count separately
        i += step ;
    }
    /* each emitted frame consumes >= EVENT_HDR_LEN input bytes: work is bounded. */
    T_ASSERT(cb_count <= TOTAL / EVENT_HDR_LEN, "callback count bounded by input size") ;
}

/* ================================================================== */
/* SCENARIO 2: matcher under contradictory bursts (100+ frames)         */
/* ================================================================== */

static event_frame_t mk_transition(uint8_t state, uint8_t flags)
{
    event_frame_t f = {0} ;
    f.version = EVENT_VERSION ; f.kind = EVENT_KIND_TRANSITION ;
    f.flags = flags ; f.state = state ;
    return f ;
}
static event_frame_t mk_lifecycle(uint8_t phase)
{
    event_frame_t f = {0} ;
    f.version = EVENT_VERSION ; f.kind = EVENT_KIND_LIFECYCLE ; f.phase = phase ;
    return f ;
}
static event_frame_t mk_signal(uint8_t signo)
{
    event_frame_t f = {0} ;
    f.version = EVENT_VERSION ; f.kind = EVENT_KIND_SIGNAL ; f.signo = signo ;
    return f ;
}

/* 100+ non-satisfying, non-terminal transitions: the matcher must track the LAST
 * state ABSOLUTELY (overwrite), never accumulate. A cumulative matcher would (a)
 * report OK mid-burst once up and ready had both been seen, and (b) leave up=1
 * after ending on a down state — both are asserted against. */
static void test_match_burst_overwrite_not_cumulative(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_UP_READY, 0, 0) ;

    /* STARTING(up=1,ready=0), DOWN(up=0,ready=1), FINISHING(0,0), STOPPING(1,0):
     * none is up&&ready, so each must stay PENDING under overwrite semantics. */
    uint8_t seq[] = { STATUS_STATE_STARTING, STATUS_STATE_DOWN, STATUS_STATE_FINISHING, STATUS_STATE_STOPPING } ;
    for (int i = 0 ; i < 160 ; i++) {
        event_frame_t f = mk_transition(seq[i % 4], 0) ;
        T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &f),
                    "no single non-ready state ever satisfies (overwrite, not cumulative)") ;
    }

    /* end deterministically on FINISHING: absolute state must be (up=0, ready=0) */
    event_frame_t fin = mk_transition(STATUS_STATE_FINISHING, 0) ;
    T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &fin), "still pending after the burst") ;
    T_ASSERT_EQ(0, m.up, "up reflects the LAST state absolutely, not an accumulation") ;
    T_ASSERT_EQ(0, m.ready, "ready reflects the LAST state absolutely") ;

    /* the real UP now drives it home */
    event_frame_t up = mk_transition(STATUS_STATE_UP, 0) ;
    T_ASSERT_EQ(EVENT_STATE_OK, event_state_update(&m, &up), "a final UP reaches ready") ;
}

/* a consumer latch (the event_wait / svc_launch idiom): once a verdict is OK, the
 * consumer sets `done` and IGNORES the rest of the stream — no re-trigger, no
 * double count, even under a contradictory terminal-failure burst afterwards. */
static void test_match_latch_no_retrigger(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_UP, 0, 0) ;

    int done = 0, verdict = EVENT_STATE_PENDING, triggered = 0 ;

    event_frame_t start = mk_transition(STATUS_STATE_STARTING, 0) ; // up=1 => reaches UP wait
    if (!done) {
        verdict = event_state_update(&m, &start) ;
        if (verdict == EVENT_STATE_OK) { done = 1 ; triggered++ ; }
    }
    T_ASSERT_EQ(1, done, "latched OK on the first up") ;
    T_ASSERT_EQ(1, triggered, "triggered exactly once") ;

    /* a hostile burst of terminal failures + supervisor-down would flip a
     * non-latched consumer to FAIL; the latch must swallow all of it. */
    for (int i = 0 ; i < 120 ; i++) {
        event_frame_t bad = (i & 1) ? mk_transition(STATUS_STATE_FAILED, EVENT_FLAG_TERMINAL)
                                    : mk_lifecycle(EVENT_LIFECYCLE_DOWN) ;
        if (!done) {   // the latch: the consumer must never feed a finished matcher
            verdict = event_state_update(&m, &bad) ;
            if (verdict != EVENT_STATE_PENDING) triggered++ ;
        }
    }
    T_ASSERT_EQ(1, triggered, "no re-trigger after the latch: contradictory burst ignored") ;
    T_ASSERT_EQ(EVENT_STATE_OK, verdict, "verdict stays OK, never overwritten to FAIL") ;
}

/* two-phase RESTART drowned in noise: an UP seen BEFORE the down phase must NOT
 * satisfy; only down-then-up does. Signals throughout are inert. */
static void test_match_restart_in_noise(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_RESTART, 1, 0) ;   // seeded up

    /* pre-down noise: signals, and UP frames that must not complete the restart */
    for (int i = 0 ; i < 60 ; i++) {
        event_frame_t f = (i % 2) ? mk_signal((uint8_t)(i % 30 + 1)) : mk_transition(STATUS_STATE_UP, 0) ;
        T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &f), "up-before-down never satisfies restart") ;
    }
    T_ASSERT_EQ(0, m.restart_done, "down phase not yet seen") ;

    /* the down phase latches restart_done, still pending until up returns */
    event_frame_t fin = mk_transition(STATUS_STATE_FINISHING, 0) ;
    T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &fin), "down phase: pending, awaiting up") ;
    T_ASSERT_EQ(1, m.restart_done, "restart_done latched by the down phase") ;

    /* more inert signals, then the up that finally completes the restart */
    for (int i = 0 ; i < 20 ; i++) {
        event_frame_t sig = mk_signal((uint8_t)(i % 30 + 1)) ;
        T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &sig), "signal inert after down phase") ;
    }
    event_frame_t up = mk_transition(STATUS_STATE_UP, 0) ;
    T_ASSERT_EQ(EVENT_STATE_OK, event_state_update(&m, &up), "down-then-up: restart reached") ;
}

/* TERMINAL while waiting up => FAIL; TERMINAL while waiting down => OK (down is
 * satisfied by the FAILED state before the terminal check bites). */
static void test_match_terminal_directions(void)
{
    event_state_t mu ;
    event_state_init(&mu, EVENT_UP, 0, 0) ;
    event_frame_t f = mk_transition(STATUS_STATE_FAILED, EVENT_FLAG_TERMINAL) ;
    T_ASSERT_EQ(EVENT_STATE_FAIL, event_state_update(&mu, &f), "terminal down while waiting up: FAIL") ;

    event_state_t md ;
    event_state_init(&md, EVENT_DOWN, 1, 0) ;   // seeded up so not pre-satisfied
    event_frame_t g = mk_transition(STATUS_STATE_FAILED, EVENT_FLAG_TERMINAL) ;
    T_ASSERT_EQ(EVENT_STATE_OK, event_state_update(&md, &g), "terminal frame still satisfies a down wait") ;
}

/* ================================================================== */
/* SCENARIO 3: anti-starvation of the producer fanout (the core ask)    */
/* ================================================================== */

enum { N_HEALTHY = 30, N_DEAD = 25, N_FULL = 8, N_EMIT = 130 } ;

/* build a valid evtsub-named fifo path from a 6-char suffix; mkfifo it. */
static void make_named_fifo(char const *dir, char const *suffix, char *out, size_t outn)
{
    /* "evtsub:" (7) + "@"+24 hex (25) + ":" (1) + 6 = 39 = EVENT_FIFO_NAMELEN */
    snprintf(out, outn, "%s/evtsub:@400000000000000000000000:%s", dir, suffix) ;
    char const *base = strrchr(out, '/') + 1 ;
    T_ASSERT_EQ(EVENT_FIFO_NAMELEN, (long long)strlen(base), "crafted name length is 39") ;
    T_ASSERT_EQ(0, mkfifo(out, 0622), "mkfifo named subscriber") ;
}

typedef struct {
    event_aggregator_t ag ;
    event_frame_t f[SINK_CAP] ;
    size_t n ;
} hsink_t ;

static void h_cb(event_frame_t const *f, void *data)
{
    hsink_t *s = data ;
    if (s->n < SINK_CAP) s->f[s->n] = *f ;
    s->n++ ;
}
static void h_handler(event_reader_t *r, char const *buf, size_t len, void *data)
{
    (void)r ;
    hsink_t *s = data ;
    event_aggregate(&s->ag, buf, len, h_cb, s) ;
}

static void test_fanout_no_starvation(void)
{
    char tmpl[] = "/tmp/ev_fan_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifo_make(ev, (gid_t)-1), "make eventdir") ;

    /* dead subscribers: valid names, NO reader -> producer hits ENXIO, unlinks. */
    char dead[N_DEAD][1024] ;
    for (int i = 0 ; i < N_DEAD ; i++) {
        char suf[7] ; snprintf(suf, sizeof(suf), "d%05d", i) ;
        make_named_fifo(ev, suf, dead[i], sizeof(dead[i])) ;
    }

    /* full/slow subscribers: a live read end held but NEVER drained, and the pipe
     * pre-filled to capacity, so every producer write EAGAINs and is dropped. */
    char full[N_FULL][1024] ;
    int full_rfd[N_FULL], full_wfd[N_FULL] ;
    for (int i = 0 ; i < N_FULL ; i++) {
        char suf[7] ; snprintf(suf, sizeof(suf), "f%05d", i) ;
        make_named_fifo(ev, suf, full[i], sizeof(full[i])) ;
        full_rfd[i] = open(full[i], O_RDONLY | O_NONBLOCK | O_CLOEXEC) ;
        T_ASSERT(full_rfd[i] >= 0, "open full reader read end") ;
        full_wfd[i] = open(full[i], O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
        T_ASSERT(full_wfd[i] >= 0, "open full reader write end for prefill") ;
        /* prefill with non-version zeros until the pipe is full */
        char zeros[4096] ; memset(zeros, 0, sizeof(zeros)) ;
        for (;;) {
            ssize_t w = write(full_wfd[i], zeros, sizeof(zeros)) ;
            if (w < 0) { T_ASSERT_ERRNO(EAGAIN, "prefill stops on a full pipe (EAGAIN)") ; break ; }
        }
    }

    /* healthy subscribers via the real subscribe path, on one loop. */
    int fd_base = count_fds() ;
    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, N_HEALTHY + 1), "sse_new") ;

    event_fifo_t *fifos = malloc(N_HEALTHY * sizeof(*fifos)) ;
    hsink_t *sinks = calloc(N_HEALTHY, sizeof(*sinks)) ;
    T_ASSERT(fifos && sinks, "alloc healthy arrays") ;
    for (int i = 0 ; i < N_HEALTHY ; i++)
        T_ASSERT_EQ(1, event_subscribe(&fifos[i], &ep, ev, h_handler, &sinks[i], 0), "subscribe healthy") ;

    int subscribers = fanout_count(ev) ;
    T_ASSERT_EQ(N_HEALTHY + N_DEAD + N_FULL, subscribers, "all subscribers visible before fanout") ;

    /* the producer fans out 130 frames. It must NEVER block: bounded by the alarm
     * AND by a wall-clock ceiling measured here. */
    struct timespec ts = fixed_stamp() ;
    static fdesc_t exp[N_EMIT] ;
    struct timespec a, b ;
    clock_gettime(CLOCK_MONOTONIC, &a) ;
    for (int i = 0 ; i < N_EMIT ; i++) {
        fdesc_t d ; frame_params((size_t)i, &d) ; exp[i] = d ;
        int rc ;
        switch (d.kind) {
            case EVENT_KIND_TRANSITION :
                rc = event_emit_transition(ev, d.state, d.result, d.who, d.code, d.pid, &ts, d.flags) ; break ;
            case EVENT_KIND_SIGNAL :
                rc = event_emit_signal(ev, d.signo, d.who, &ts) ; break ;
            default :
                rc = event_emit_lifecycle(ev, d.phase, &ts) ; break ;
        }
        T_ASSERT_EQ(1, rc, "producer emit returns 1 (EAGAIN/ENXIO are not errors)") ;
    }
    clock_gettime(CLOCK_MONOTONIC, &b) ;
    long ms = (b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000 ;
    T_ASSERT(ms < 5000, "producer drained O(N) subscribers and returned; never blocked") ;

    /* EFFECT: dead subscribers were unlinked (ENXIO path). */
    for (int i = 0 ; i < N_DEAD ; i++) {
        struct stat st ; errno = 0 ;
        T_ASSERT_EQ(-1, stat(dead[i], &st), "dead subscriber gone") ;
        T_ASSERT_ERRNO(ENOENT, "dead subscriber unlinked (ENXIO)") ;
    }

    /* EFFECT: full/slow subscribers survived (EAGAIN is dropped, never unlinked)
     * and received ZERO valid frames — their frames were dropped, no stall. */
    for (int i = 0 ; i < N_FULL ; i++) {
        struct stat st ;
        T_ASSERT_EQ(0, stat(full[i], &st), "full subscriber kept (EAGAIN not unlinked)") ;
        sink_t drain = {0} ;
        char rb[8192] ;
        for (;;) {
            ssize_t r = read(full_rfd[i], rb, sizeof(rb)) ;
            if (r <= 0) break ;
            event_aggregate(&drain.ag, rb, (size_t)r, sink_cb, &drain) ;
        }
        T_ASSERT_EQ(0, (long long)drain.n, "slow reader got its frames DROPPED (only prefill junk)") ;
    }

    /* EFFECT: every HEALTHY reader, wedged among dead+slow ones, got ALL frames in
     * order — proof there is no global starvation. */
    for (int it = 0 ; it < 2000 ; it++) {
        int all = 1 ;
        for (int i = 0 ; i < N_HEALTHY ; i++) if (sinks[i].n < N_EMIT) { all = 0 ; break ; }
        if (all) break ;
        if (sse_run(&ep, 100) != 1) break ;
    }
    for (int i = 0 ; i < N_HEALTHY ; i++) {
        T_ASSERT_EQ((long long)N_EMIT, (long long)sinks[i].n, "healthy reader received every frame") ;
        for (int k = 0 ; k < N_EMIT ; k++)
            verify_desc(&sinks[i].f[k], &exp[k], (size_t)k) ;
    }

    /* teardown + leak checks. */
    for (int i = 0 ; i < N_HEALTHY ; i++) event_unsubscribe(&fifos[i]) ;
    free(fifos) ; free(sinks) ;
    sse_free(&ep) ;

    T_ASSERT_EQ(fd_base, count_fds(), "no fd leak across subscribe/fanout/unsubscribe") ;

    for (int i = 0 ; i < N_FULL ; i++) { close(full_rfd[i]) ; close(full_wfd[i]) ; unlink(full[i]) ; }

    T_ASSERT_EQ(0, dir_entries(ev), "no inode leak: fifodir empty after cleanup") ;

    rm_rf(ev) ; rm_rf(base) ;
}

/* ================================================================== */
/* SCENARIO 4: event_wait at scale                                      */
/* ================================================================== */

enum { N_WAIT = 50 } ;

static void test_wait_scale_no_double_count(void)
{
    char tmpl[] = "/tmp/ev_ws_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;

    char *dirs[N_WAIT] ;
    for (int i = 0 ; i < N_WAIT ; i++) {
        dirs[i] = malloc(1024) ;
        T_ASSERT(dirs[i] != NULL, "malloc dir") ;
        snprintf(dirs[i], 1024, "%s/ev%03d", base, i) ;
        T_ASSERT_EQ(1, event_fifo_make(dirs[i], (gid_t)-1), "make eventdir") ;
    }

    int fd_base = count_fds() ;

    event_wait_t w ;
    T_ASSERT_EQ(1, event_wait_init(&w, (char const *const *)dirs, N_WAIT, EVENT_UP_READY), "init N sources") ;

    /* each source gets its wanted UP transition SEVERAL times plus inert noise:
     * triggered must reach exactly N, never inflate. */
    struct timespec ts = fixed_stamp() ;
    for (int i = 0 ; i < N_WAIT ; i++) {
        T_ASSERT_EQ(1, event_emit_signal(dirs[i], 15, STATUS_WHO_SELF, &ts), "inert signal") ;
        for (int k = 0 ; k < 4 ; k++)
            T_ASSERT_EQ(1, event_emit_transition(dirs[i], STATUS_STATE_UP, STATUS_RESULT_SUCCESS,
                                                 STATUS_WHO_SELF, 0, 0, &ts, 0), "repeated UP") ;
    }

    int r = event_wait_run(&w, 5000) ;
    T_ASSERT_EQ(1, r, "run returns 1: every source triggered") ;
    T_ASSERT_EQ((long long)N_WAIT, (long long)w.triggered, "triggered == N, no double count from duplicates") ;
    T_ASSERT_EQ(0, w.failed, "no permanent failure") ;

    event_wait_free(&w) ;
    T_ASSERT_EQ(0, (long long)w.n, "free resets n") ;
    T_ASSERT(w.fifos == NULL, "sources freed") ;
    T_ASSERT(w.slots == NULL, "slots freed") ;
    T_ASSERT_EQ(fd_base, count_fds(), "no fd leak across init/run/free at scale") ;
    for (int i = 0 ; i < N_WAIT ; i++)
        T_ASSERT_EQ(0, dir_entries(dirs[i]), "each source fifo unlinked") ;

    for (int i = 0 ; i < N_WAIT ; i++) { rm_rf(dirs[i]) ; free(dirs[i]) ; }
    rm_rf(base) ;
}

/* ------------------------------------------------------------------ */

T_SUITE("event stress")
{
    VERBOSITY = 0 ;
    PROG = "stress" ;
    signal(SIGALRM, on_alarm) ;
    alarm(90) ;

    /* 1: decoder storm */
    T_RUN(test_storm_onechunk) ;
    T_RUN(test_storm_varied_chunks) ;
    T_RUN(test_storm_straddle_every_offset) ;
    T_RUN(test_storm_resync_recovers_all) ;
    T_RUN(test_storm_plen_boundary) ;
    T_RUN(test_storm_random_noise_bounded) ;

    /* 2: matcher bursts */
    T_RUN(test_match_burst_overwrite_not_cumulative) ;
    T_RUN(test_match_latch_no_retrigger) ;
    T_RUN(test_match_restart_in_noise) ;
    T_RUN(test_match_terminal_directions) ;

    /* 3: anti-starvation fanout */
    T_RUN(test_fanout_no_starvation) ;

    /* 4: event_wait at scale */
    T_RUN(test_wait_scale_no_double_count) ;
}
