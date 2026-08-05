/* test_event.c — exhaustive, mutation-proven test suite for the 66 `event`
 * module (event_frame.c / event_state.c / event_fifo.c / event_subscribe.c /
 * event_emit.c / event_reader.c / event_wait.c), rewritten for the length-framed payload API
 * (tagged union) that replaced the single-byte channel.
 *
 * Asserts by EFFECT: on-wire bytes at exact offsets, decoded struct fields,
 * stat modes, real fds, on-disk fifo state, matcher verdicts, frames delivered.
 * Never by return code alone. Hardened: ASan + UBSan + LSan, with a per-process
 * alarm() anti-hang (a mis-handled fifo would block).
 */
#include "ctest.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <dirent.h>
#include <signal.h>
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

/* anti-hang: any test that blocks on a fifo dies here, the run is a FAILURE. */
static void on_alarm(int sig) { (void)sig ; _exit(124) ; }

static void child_dies_with_parent(pid_t parent)
{
    prctl(PR_SET_PDEATHSIG, SIGKILL) ;
    if (getppid() != parent)
        _exit(1) ;
}

/* deterministic scratch dir under the sandbox tmpfs */
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

/* a fixed, round-trip-exact timestamp: tv_sec fits u64, tv_nsec fits u32. */
static struct timespec fixed_stamp(void)
{
    struct timespec ts ;
    ts.tv_sec = (time_t)0x1122334455LL ;
    ts.tv_nsec = 123456789L ;
    return ts ;
}

static uint32_t rd_u32be(unsigned char const *p)
{
    uint32_t v ;
    u32_unpack_big((char const *)p, &v) ;
    return v ;
}

/* ================================================================== */
/* event_frame.c — packers: on-wire layout + pack->decode round trip   */
/* ================================================================== */

/* a frame-decoding sink: records every complete frame the decoder reassembles,
 * plus the raw byte stream (the pump is byte-agnostic; the decoder gives meaning). */
typedef struct {
    event_aggregator_t ag ;
    event_frame_t frames[128] ;
    size_t n ;
    unsigned char raw[8192] ;
    size_t rawn ;
} framesink_t ;

static void framesink_on_frame(event_frame_t const *f, void *data)
{
    framesink_t *s = data ;
    if (s->n < sizeof(s->frames) / sizeof(s->frames[0]))
        s->frames[s->n++] = *f ;
}

/* pump handler: accumulate raw bytes and feed the reassembler. */
static void framesink_handler(event_reader_t *r, char const *buf, size_t len, void *data)
{
    (void)r ;
    framesink_t *s = data ;
    for (size_t i = 0 ; i < len && s->rawn < sizeof(s->raw) ; i++)
        s->raw[s->rawn++] = (unsigned char)buf[i] ;
    event_aggregate(&s->ag, buf, len, framesink_on_frame, s) ;
}

/* decode a self-contained buffer in one shot (no pump). */
static size_t decode_all(char const *buf, size_t len, event_frame_t *out, size_t max)
{
    framesink_t s = {0} ;
    event_aggregate(&s.ag, buf, len, framesink_on_frame, &s) ;
    size_t k = s.n < max ? s.n : max ;
    for (size_t i = 0 ; i < k ; i++) out[i] = s.frames[i] ;
    return s.n ;
}

static void assert_stamp_eq(struct timespec const *a, struct timespec const *b, char const *msg)
{
    T_ASSERT_EQ((long long)a->tv_sec, (long long)b->tv_sec, msg) ;
    T_ASSERT_EQ((long long)a->tv_nsec, (long long)b->tv_nsec, msg) ;
}

static void test_pack_transition_wire_and_roundtrip(void)
{
    struct timespec ts = fixed_stamp() ;
    char out[EVENT_FRAME_MAX] ;
    /* distinctive values to catch swapped/shifted fields */
    size_t len = event_frame_pack_transition(out, STATUS_STATE_UP, STATUS_RESULT_EXITED,
                                       STATUS_WHO_USER, 0xDEADBEEFu, 0x01020304u, &ts,
                                       EVENT_FLAG_TERMINAL) ;
    /* frame length = 8 header + 12 stamp + 11 payload = 31 = EVENT_FRAME_MAX */
    T_ASSERT_EQ(EVENT_FRAME_MAX, (long long)len, "transition frame length is 31") ;

    unsigned char const *b = (unsigned char const *)out ;
    /* header on the wire, byte by byte */
    T_ASSERT_EQ(EVENT_VERSION, b[0], "header[0] = version") ;
    T_ASSERT_EQ(EVENT_KIND_TRANSITION, b[1], "header[1] = kind TRANSITION") ;
    T_ASSERT_EQ(EVENT_FLAG_TERMINAL, b[2], "header[2] = flags (TERMINAL)") ;
    T_ASSERT_EQ(0, b[3], "header[3] = reserved 0") ;
    T_ASSERT_EQ(23, (long long)rd_u32be(b + 4), "header payload_len = 12+11 = 23") ;
    /* payload fields at their exact offsets (bites a pack-side offset mutation) */
    T_ASSERT_EQ(STATUS_STATE_UP, b[8 + 12 + 0], "wire state at offset 20") ;
    T_ASSERT_EQ(STATUS_RESULT_EXITED, b[8 + 12 + 1], "wire result at offset 21") ;
    T_ASSERT_EQ(STATUS_WHO_USER, b[8 + 12 + 2], "wire who at offset 22") ;
    T_ASSERT_EQ(0xDEADBEEFu, (long long)rd_u32be(b + 8 + 12 + 3), "wire code (u32 BE) at offset 23") ;
    T_ASSERT_EQ(0x01020304u, (long long)rd_u32be(b + 8 + 12 + 7), "wire pid (u32 BE) at offset 27") ;

    /* round trip: decode and confirm every field survives */
    event_frame_t f[2] ;
    T_ASSERT_EQ(1, (long long)decode_all(out, len, f, 2), "exactly one frame decoded") ;
    T_ASSERT_EQ(EVENT_VERSION, f[0].version, "decoded version") ;
    T_ASSERT_EQ(EVENT_KIND_TRANSITION, f[0].kind, "decoded kind") ;
    T_ASSERT_EQ(EVENT_FLAG_TERMINAL, f[0].flags, "decoded flags TERMINAL") ;
    assert_stamp_eq(&ts, &f[0].stamp, "decoded stamp round-trips") ;
    T_ASSERT_EQ(STATUS_STATE_UP, f[0].state, "decoded state") ;
    T_ASSERT_EQ(STATUS_RESULT_EXITED, f[0].result, "decoded result") ;
    T_ASSERT_EQ(STATUS_WHO_USER, f[0].who, "decoded who") ;
    T_ASSERT_EQ(0xDEADBEEFu, (long long)f[0].code, "decoded code u32") ;
    T_ASSERT_EQ(0x01020304u, (long long)f[0].pid, "decoded pid u32") ;
}

static void test_pack_transition_no_flag(void)
{
    struct timespec ts = fixed_stamp() ;
    char out[EVENT_FRAME_MAX] ;
    size_t len = event_frame_pack_transition(out, STATUS_STATE_STARTING, STATUS_RESULT_SUCCESS,
                                       STATUS_WHO_SELF, 0, 0, &ts, 0) ;
    T_ASSERT_EQ(0, (unsigned char)out[2], "flags byte 0 when no flag") ;
    event_frame_t f[1] ;
    T_ASSERT_EQ(1, (long long)decode_all(out, len, f, 1), "one frame") ;
    T_ASSERT_EQ(0, f[0].flags, "decoded flags 0") ;
    T_ASSERT_EQ(STATUS_STATE_STARTING, f[0].state, "state starting") ;
    T_ASSERT_EQ(0, (long long)f[0].code, "code 0") ;
    T_ASSERT_EQ(0, (long long)f[0].pid, "pid 0") ;
}

static void test_pack_signal_wire_and_roundtrip(void)
{
    struct timespec ts = fixed_stamp() ;
    char out[EVENT_FRAME_MAX] ;
    size_t len = event_frame_pack_signal(out, 15 /* SIGTERM */, STATUS_WHO_BOOT, &ts) ;
    /* 8 + 12 + 2 = 22 */
    T_ASSERT_EQ(22, (long long)len, "signal frame length is 22") ;

    unsigned char const *b = (unsigned char const *)out ;
    T_ASSERT_EQ(EVENT_VERSION, b[0], "version") ;
    T_ASSERT_EQ(EVENT_KIND_SIGNAL, b[1], "kind SIGNAL") ;
    T_ASSERT_EQ(0, b[2], "flags 0") ;
    T_ASSERT_EQ(14, (long long)rd_u32be(b + 4), "payload_len 12+2 = 14") ;
    T_ASSERT_EQ(15, b[8 + 12 + 0], "wire signo at offset 20") ;
    T_ASSERT_EQ(STATUS_WHO_BOOT, b[8 + 12 + 1], "wire who at offset 21") ;

    event_frame_t f[1] ;
    T_ASSERT_EQ(1, (long long)decode_all(out, len, f, 1), "one frame") ;
    T_ASSERT_EQ(EVENT_KIND_SIGNAL, f[0].kind, "decoded kind SIGNAL") ;
    assert_stamp_eq(&ts, &f[0].stamp, "decoded stamp") ;
    T_ASSERT_EQ(15, f[0].signo, "decoded signo") ;
    T_ASSERT_EQ(STATUS_WHO_BOOT, f[0].who, "decoded who") ;
}

static void test_pack_lifecycle_wire_and_roundtrip(void)
{
    struct timespec ts = fixed_stamp() ;
    char out[EVENT_FRAME_MAX] ;
    size_t len = event_frame_pack_lifecycle(out, EVENT_LIFECYCLE_DOWN, &ts) ;
    /* 8 + 12 + 1 = 21 */
    T_ASSERT_EQ(21, (long long)len, "lifecycle frame length is 21") ;

    unsigned char const *b = (unsigned char const *)out ;
    T_ASSERT_EQ(EVENT_VERSION, b[0], "version") ;
    T_ASSERT_EQ(EVENT_KIND_LIFECYCLE, b[1], "kind LIFECYCLE") ;
    T_ASSERT_EQ(13, (long long)rd_u32be(b + 4), "payload_len 12+1 = 13") ;
    T_ASSERT_EQ(EVENT_LIFECYCLE_DOWN, b[8 + 12 + 0], "wire phase at offset 20") ;

    event_frame_t f[1] ;
    T_ASSERT_EQ(1, (long long)decode_all(out, len, f, 1), "one frame") ;
    T_ASSERT_EQ(EVENT_KIND_LIFECYCLE, f[0].kind, "decoded kind LIFECYCLE") ;
    assert_stamp_eq(&ts, &f[0].stamp, "decoded stamp") ;
    T_ASSERT_EQ(EVENT_LIFECYCLE_DOWN, f[0].phase, "decoded phase DOWN") ;
}

/* ================================================================== */
/* event_frame.c — event_aggregate reassembly / resync               */
/* ================================================================== */

static void test_decode_single(void)
{
    struct timespec ts = fixed_stamp() ;
    char buf[EVENT_FRAME_MAX] ;
    size_t len = event_frame_pack_transition(buf, STATUS_STATE_UP, 0, 0, 7, 99, &ts, 0) ;
    event_frame_t f[2] ;
    T_ASSERT_EQ(1, (long long)decode_all(buf, len, f, 2), "one whole frame delivered") ;
    T_ASSERT_EQ(STATUS_STATE_UP, f[0].state, "state UP") ;
    T_ASSERT_EQ(7, (long long)f[0].code, "code 7") ;
    T_ASSERT_EQ(99, (long long)f[0].pid, "pid 99") ;
}

static void test_decode_two_concatenated(void)
{
    struct timespec ts = fixed_stamp() ;
    char buf[2 * EVENT_FRAME_MAX] ;
    size_t l0 = event_frame_pack_transition(buf, STATUS_STATE_STARTING, 0, 0, 0, 11, &ts, 0) ;
    size_t l1 = event_frame_pack_lifecycle(buf + l0, EVENT_LIFECYCLE_UP, &ts) ;

    event_frame_t f[4] ;
    T_ASSERT_EQ(2, (long long)decode_all(buf, l0 + l1, f, 4), "both frames in one chunk delivered") ;
    T_ASSERT_EQ(EVENT_KIND_TRANSITION, f[0].kind, "first is TRANSITION") ;
    T_ASSERT_EQ(11, (long long)f[0].pid, "first pid 11") ;
    T_ASSERT_EQ(EVENT_KIND_LIFECYCLE, f[1].kind, "second is LIFECYCLE") ;
    T_ASSERT_EQ(EVENT_LIFECYCLE_UP, f[1].phase, "second phase UP, order preserved") ;
}

/* a frame split across two feeds at EVERY interior cut point (mid-header and
 * mid-payload) must be delivered exactly once, complete. */
static void test_decode_split_all_cutpoints(void)
{
    struct timespec ts = fixed_stamp() ;
    char buf[EVENT_FRAME_MAX] ;
    size_t len = event_frame_pack_transition(buf, STATUS_STATE_UP, STATUS_RESULT_SUCCESS,
                                       STATUS_WHO_EVENT, 0xCAFEBABEu, 4242, &ts, 0) ;
    for (size_t cut = 1 ; cut < len ; cut++) {
        framesink_t s = {0} ;
        event_aggregate(&s.ag, buf, cut, framesink_on_frame, &s) ;
        T_ASSERT_EQ(0, (long long)s.n, "nothing delivered before the frame completes") ;
        event_aggregate(&s.ag, buf + cut, len - cut, framesink_on_frame, &s) ;
        T_ASSERT_EQ(1, (long long)s.n, "exactly one frame after the second half") ;
        T_ASSERT_EQ(0xCAFEBABEu, (long long)s.frames[0].code, "code survives the split") ;
        T_ASSERT_EQ(4242, (long long)s.frames[0].pid, "pid survives the split") ;
        T_ASSERT_EQ(STATUS_WHO_EVENT, s.frames[0].who, "who survives the split") ;
    }
}

/* one chunk far larger than the pump's 256-byte read buffer, holding many whole
 * frames plus a trailing partial one: all whole frames delivered in order, the
 * partial buffered until its tail arrives. (Tests the reassembler directly, so
 * it is independent of the pump chunking.) */
static void test_decode_bigchunk_many_plus_partial(void)
{
    enum { K = 30 } ;   /* 30 * 31 = 930 bytes > 256 and > 512 */
    struct timespec ts = fixed_stamp() ;
    char buf[K * EVENT_FRAME_MAX + EVENT_FRAME_MAX] ;
    size_t off = 0 ;
    for (int i = 0 ; i < K ; i++)
        off += event_frame_pack_transition(buf + off, STATUS_STATE_UP, 0, 0, 0, (uint32_t)i, &ts, 0) ;
    /* append one more frame but only feed part of it (a straddling partial) */
    size_t last = event_frame_pack_transition(buf + off, STATUS_STATE_DOWN, 0, 0, 0, 777, &ts, 0) ;
    size_t partial = 5 ;   /* only 5 bytes of the last frame */

    framesink_t s = {0} ;
    event_aggregate(&s.ag, buf, off + partial, framesink_on_frame, &s) ;
    T_ASSERT_EQ(K, (long long)s.n, "all K whole frames delivered from the big chunk") ;
    for (int i = 0 ; i < K ; i++)
        T_ASSERT_EQ(i, (long long)s.frames[i].pid, "frames in order with correct pid") ;

    /* feed the tail of the partial: the K+1-th frame now completes */
    event_aggregate(&s.ag, buf + off + partial, last - partial, framesink_on_frame, &s) ;
    T_ASSERT_EQ(K + 1, (long long)s.n, "the straddling frame completes on its tail") ;
    T_ASSERT_EQ(777, (long long)s.frames[K].pid, "tail frame pid 777") ;
    T_ASSERT_EQ(STATUS_STATE_DOWN, s.frames[K].state, "tail frame state DOWN") ;
}

static void test_decode_len_zero_noop(void)
{
    framesink_t s = {0} ;
    event_aggregate(&s.ag, NULL, 0, framesink_on_frame, &s) ;
    T_ASSERT_EQ(0, (long long)s.n, "len==0 is a no-op, no frame") ;
    T_ASSERT_EQ(0, (long long)s.ag.len, "decoder buffer untouched") ;
}

/* a corrupt header (version != 1) is dropped byte by byte until the next valid
 * frame is found: the good frame after it is still delivered. */
static void test_decode_corrupt_version_resync(void)
{
    struct timespec ts = fixed_stamp() ;
    char buf[8 + EVENT_FRAME_MAX] ;
    /* several stray non-version bytes, then a real frame */
    buf[0] = (char)0xFF ; buf[1] = (char)0x02 ; buf[2] = (char)0x7E ; buf[3] = (char)0x00 ;
    size_t len = 4 + event_frame_pack_lifecycle(buf + 4, EVENT_LIFECYCLE_UP, &ts) ;

    event_frame_t f[2] ;
    T_ASSERT_EQ(1, (long long)decode_all(buf, len, f, 2), "resync past bad version, one frame") ;
    T_ASSERT_EQ(EVENT_KIND_LIFECYCLE, f[0].kind, "the recovered frame is the valid one") ;
    T_ASSERT_EQ(EVENT_LIFECYCLE_UP, f[0].phase, "recovered phase UP") ;
}

/* a header whose payload_len exceeds the largest known payload triggers a resync
 * (drop one byte, re-scan): the valid frame after it survives. */
static void test_decode_bad_len_resync(void)
{
    struct timespec ts = fixed_stamp() ;
    char buf[8 + EVENT_FRAME_MAX] ;
    /* a plausible header start (version 1) but payload_len = 255 (> 23) */
    buf[0] = (char)EVENT_VERSION ;
    buf[1] = (char)EVENT_KIND_TRANSITION ;
    buf[2] = 0 ; buf[3] = 0 ;
    u32_pack_big(buf + 4, 255) ;
    size_t len = 8 + event_frame_pack_lifecycle(buf + 8, EVENT_LIFECYCLE_DOWN, &ts) ;

    event_frame_t f[2] ;
    T_ASSERT_EQ(1, (long long)decode_all(buf, len, f, 2), "resync past over-long payload_len") ;
    T_ASSERT_EQ(EVENT_KIND_LIFECYCLE, f[0].kind, "recovered valid frame") ;
    T_ASSERT_EQ(EVENT_LIFECYCLE_DOWN, f[0].phase, "recovered phase DOWN") ;
}

/* a header that is valid AND in-bounds but names an UNKNOWN kind: the frame is
 * consumed (its bytes skipped) but the callback is NOT invoked. A valid frame
 * placed right after it is delivered — proving the unknown frame was consumed as
 * a whole, not resynced byte-by-byte. */
static void test_decode_unknown_kind_consumed_no_cb(void)
{
    struct timespec ts = fixed_stamp() ;
    char buf[64] ;
    /* unknown kind 0x7F, payload_len 13 (<= 23, in bounds): 8 + 13 = 21 bytes */
    buf[0] = (char)EVENT_VERSION ;
    buf[1] = (char)0x7F ;
    buf[2] = 0 ; buf[3] = 0 ;
    u32_pack_big(buf + 4, 13) ;
    memset(buf + 8, 0xAB, 13) ;
    size_t off = 21 ;
    off += event_frame_pack_lifecycle(buf + off, EVENT_LIFECYCLE_UP, &ts) ;

    event_frame_t f[3] ;
    size_t n = decode_all(buf, off, f, 3) ;
    T_ASSERT_EQ(1, (long long)n, "unknown kind not delivered; only the valid frame is") ;
    T_ASSERT_EQ(EVENT_KIND_LIFECYCLE, f[0].kind, "the delivered frame is the valid one after it") ;
    T_ASSERT_EQ(EVENT_LIFECYCLE_UP, f[0].phase, "phase UP: frame boundary honored") ;
}

/* a valid header naming TRANSITION but with an in-bounds payload_len too short to
 * hold the transition payload: event_frame_unpack returns 0, the frame is consumed, no
 * callback; a following valid frame is delivered. */
static void test_decode_short_payload_consumed_no_cb(void)
{
    struct timespec ts = fixed_stamp() ;
    char buf[64] ;
    /* TRANSITION but payload_len 13 => rest = 1 < 11: too short */
    buf[0] = (char)EVENT_VERSION ;
    buf[1] = (char)EVENT_KIND_TRANSITION ;
    buf[2] = 0 ; buf[3] = 0 ;
    u32_pack_big(buf + 4, 13) ;
    memset(buf + 8, 0, 13) ;
    size_t off = 21 ;
    off += event_frame_pack_signal(buf + off, 9, STATUS_WHO_SELF, &ts) ;

    event_frame_t f[3] ;
    size_t n = decode_all(buf, off, f, 3) ;
    T_ASSERT_EQ(1, (long long)n, "short-payload transition dropped; only the signal delivered") ;
    T_ASSERT_EQ(EVENT_KIND_SIGNAL, f[0].kind, "the valid signal after it is delivered") ;
    T_ASSERT_EQ(9, f[0].signo, "signo 9") ;
}

/* a valid, in-bounds header whose payload_len is shorter than the timestamp
 * itself (< CLOCK_PACK): event_frame_unpack bails at the clock check, the frame is
 * consumed, no callback; a following valid frame is delivered. */
static void test_decode_plen_below_clockpack(void)
{
    struct timespec ts = fixed_stamp() ;
    char buf[64] ;
    buf[0] = (char)EVENT_VERSION ;
    buf[1] = (char)EVENT_KIND_LIFECYCLE ;
    buf[2] = 0 ; buf[3] = 0 ;
    u32_pack_big(buf + 4, 5) ;   /* payload_len 5 < CLOCK_PACK (12), still <= 23 */
    memset(buf + 8, 0, 5) ;
    size_t off = 8 + 5 ;
    off += event_frame_pack_lifecycle(buf + off, EVENT_LIFECYCLE_DOWN, &ts) ;

    event_frame_t f[3] ;
    size_t n = decode_all(buf, off, f, 3) ;
    T_ASSERT_EQ(1, (long long)n, "sub-timestamp payload dropped; only the valid frame delivered") ;
    T_ASSERT_EQ(EVENT_LIFECYCLE_DOWN, f[0].phase, "valid frame after it delivered") ;
}

/* stray bytes BETWEEN two valid frames must not swallow either frame: both are
 * delivered, the garbage in the middle is resynced away. */
static void test_decode_stray_bytes_between(void)
{
    struct timespec ts = fixed_stamp() ;
    char buf[2 * EVENT_FRAME_MAX + 8] ;
    size_t off = event_frame_pack_lifecycle(buf, EVENT_LIFECYCLE_UP, &ts) ;
    buf[off++] = 0x00 ; buf[off++] = (char)0xAA ; buf[off++] = 0x00 ; /* garbage */
    off += event_frame_pack_lifecycle(buf + off, EVENT_LIFECYCLE_DOWN, &ts) ;

    event_frame_t f[4] ;
    T_ASSERT_EQ(2, (long long)decode_all(buf, off, f, 4), "both frames survive stray bytes between") ;
    T_ASSERT_EQ(EVENT_LIFECYCLE_UP, f[0].phase, "first frame UP") ;
    T_ASSERT_EQ(EVENT_LIFECYCLE_DOWN, f[1].phase, "second frame DOWN") ;
}

/* ================================================================== */
/* event_state.c — the shared transition interpreter                   */
/* ================================================================== */

/* build a TRANSITION frame in a static struct (no wire), for direct matching. */
static event_frame_t mk_transition(uint8_t state, uint8_t flags)
{
    event_frame_t f = {0} ;
    f.version = EVENT_VERSION ; f.kind = EVENT_KIND_TRANSITION ;
    f.flags = flags ; f.state = state ; f.result = 0 ; f.who = 0 ;
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

static void test_match_up(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_UP, 0, 0) ;
    event_frame_t fin = mk_transition(STATUS_STATE_FINISHING, 0) ;
    T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &fin), "finishing (0,0): pending for up") ;
    event_frame_t st = mk_transition(STATUS_STATE_STARTING, 0) ;
    T_ASSERT_EQ(EVENT_STATE_OK, event_state_update(&m, &st), "starting sets up=1: up wait reached") ;
}

static void test_match_ready(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_UP_READY, 0, 0) ;
    event_frame_t st = mk_transition(STATUS_STATE_STARTING, 0) ;
    T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &st), "starting (1,0): pending for ready") ;
    event_frame_t up = mk_transition(STATUS_STATE_UP, 0) ;
    T_ASSERT_EQ(EVENT_STATE_OK, event_state_update(&m, &up), "up (1,1): ready reached") ;
}

static void test_match_down_from_up(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_DOWN, 1, 0) ;   /* seeded up */
    event_frame_t up = mk_transition(STATUS_STATE_UP, 0) ;
    T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &up), "still up: pending for down") ;
    event_frame_t fin = mk_transition(STATUS_STATE_FINISHING, 0) ;
    T_ASSERT_EQ(EVENT_STATE_OK, event_state_update(&m, &fin), "finishing (0,*): down reached") ;
}

static void test_match_down_ready(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_DOWN_READY, 1, 0) ;
    event_frame_t fin = mk_transition(STATUS_STATE_FINISHING, 0) ;
    T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &fin), "finishing (0,0): down but not ready") ;
    event_frame_t dn = mk_transition(STATUS_STATE_DOWN, 0) ;
    T_ASSERT_EQ(EVENT_STATE_OK, event_state_update(&m, &dn), "down (0,1): fully down reached") ;
}

static void test_match_restart_two_phase(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_RESTART, 1, 0) ;   /* seeded up */
    event_frame_t up = mk_transition(STATUS_STATE_UP, 0) ;
    T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &up), "still up, no down seen: pending") ;
    event_frame_t fin = mk_transition(STATUS_STATE_FINISHING, 0) ;
    T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &fin), "down phase seen, not up again: pending") ;
    T_ASSERT_EQ(1, m.restart_done, "restart_done latched after the down phase") ;
    event_frame_t up2 = mk_transition(STATUS_STATE_UP, 0) ;
    T_ASSERT_EQ(EVENT_STATE_OK, event_state_update(&m, &up2), "up after down: restart reached") ;
}

static void test_match_supervise_up(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_SUPERVISE_UP, 0, 0) ;
    event_frame_t tr = mk_transition(STATUS_STATE_UP, 0) ;
    T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &tr), "transition inert for supervise-up") ;
    event_frame_t sig = mk_signal(15) ;
    T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &sig), "signal inert for supervise-up") ;
    event_frame_t dn = mk_lifecycle(EVENT_LIFECYCLE_DOWN) ;
    T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &dn), "lifecycle-down is not the wanted up") ;
    event_frame_t up = mk_lifecycle(EVENT_LIFECYCLE_UP) ;
    T_ASSERT_EQ(EVENT_STATE_OK, event_state_update(&m, &up), "lifecycle-up: supervise-up reached") ;
}

static void test_match_supervise_down(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_SUPERVISE_DOWN, 0, 0) ;
    event_frame_t up = mk_lifecycle(EVENT_LIFECYCLE_UP) ;
    T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &up), "lifecycle-up is not the wanted down") ;
    event_frame_t dn = mk_lifecycle(EVENT_LIFECYCLE_DOWN) ;
    T_ASSERT_EQ(EVENT_STATE_OK, event_state_update(&m, &dn), "lifecycle-down: supervise-down reached") ;
}

static void test_match_terminal_fastfail_up(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_UP, 0, 0) ;
    event_frame_t f = mk_transition(STATUS_STATE_FAILED, EVENT_FLAG_TERMINAL) ;
    T_ASSERT_EQ(EVENT_STATE_FAIL, event_state_update(&m, &f), "terminal down while waiting up: FAIL") ;
}

static void test_match_terminal_ok_when_down(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_DOWN, 1, 0) ;   /* seeded up so not pre-satisfied */
    event_frame_t f = mk_transition(STATUS_STATE_FAILED, EVENT_FLAG_TERMINAL) ;
    /* down is satisfied by the FAILED (0,1) state BEFORE the terminal check bites */
    T_ASSERT_EQ(EVENT_STATE_OK, event_state_update(&m, &f), "down wait: terminal frame still satisfies down") ;
}

static void test_match_lifecycle_down_fails_service_wait(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_UP_READY, 0, 0) ;
    event_frame_t dn = mk_lifecycle(EVENT_LIFECYCLE_DOWN) ;
    T_ASSERT_EQ(EVENT_STATE_FAIL, event_state_update(&m, &dn), "supervisor exiting fails a service wait") ;
}

static void test_match_signal_inert(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_UP_READY, 0, 0) ;
    event_frame_t sig = mk_signal(9) ;
    T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &sig), "signal does not move (up,ready)") ;
    T_ASSERT_EQ(0, m.up, "signal left up untouched") ;
    T_ASSERT_EQ(0, m.ready, "signal left ready untouched") ;
    /* the real transitions still drive it to OK afterwards */
    event_frame_t st = mk_transition(STATUS_STATE_STARTING, 0) ;
    T_ASSERT_EQ(EVENT_STATE_PENDING, event_state_update(&m, &st), "starting still pending") ;
    event_frame_t up = mk_transition(STATUS_STATE_UP, 0) ;
    T_ASSERT_EQ(EVENT_STATE_OK, event_state_update(&m, &up), "up reaches ready after the inert signal") ;
}

static void test_match_seed_already_up(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_UP, 1, 0) ;   /* already up */
    /* any first frame reports OK because the seed already satisfies the wait */
    event_frame_t sig = mk_signal(1) ;
    T_ASSERT_EQ(EVENT_STATE_OK, event_state_update(&m, &sig), "seed up satisfies up wait on first frame") ;
}

static void test_match_satisfied_seed(void)
{
    event_state_t m ;
    /* EVENT_DOWN with a down seed (up=0) is already satisfied without any frame */
    event_state_init(&m, EVENT_DOWN, 0, 0) ;
    T_ASSERT_EQ(1, event_state_satisfied(&m), "down wait, down seed: satisfied") ;
    /* EVENT_UP with up=0 is NOT satisfied */
    event_state_init(&m, EVENT_UP, 0, 0) ;
    T_ASSERT_EQ(0, event_state_satisfied(&m), "up wait, down seed: not satisfied") ;
    /* EVENT_UP with up=1 IS satisfied */
    event_state_init(&m, EVENT_UP, 1, 0) ;
    T_ASSERT_EQ(1, event_state_satisfied(&m), "up wait, up seed: satisfied") ;
    /* lifecycle and restart waits are never satisfied by the seed */
    event_state_init(&m, EVENT_SUPERVISE_UP, 1, 1) ;
    T_ASSERT_EQ(0, event_state_satisfied(&m), "supervise-up never satisfied by seed") ;
    event_state_init(&m, EVENT_RESTART, 0, 0) ;
    T_ASSERT_EQ(0, event_state_satisfied(&m), "restart never satisfied by seed") ;
}

/* event_state_satisfied must NOT mutate the caller's matcher. */
static void test_match_satisfied_is_const(void)
{
    event_state_t m ;
    event_state_init(&m, EVENT_RESTART, 1, 1) ;
    m.restart_done = 0 ;
    (void)event_state_satisfied(&m) ;
    T_ASSERT_EQ(0, m.restart_done, "satisfied leaves restart_done untouched") ;
    T_ASSERT_EQ(1, m.up, "satisfied leaves up untouched") ;
}

/* ================================================================== */
/* event_fifo_make                                                  */
/* ================================================================== */

static void test_make_nogid_mode_01733(void)
{
    char tmpl[] = "/tmp/ev_mk_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char path[1024] ; snprintf(path, sizeof(path), "%s/fd", base) ;

    mode_t old = umask(077) ;

    int r = event_fifo_make(path, (gid_t)-1) ;
    T_ASSERT_EQ(1, r, "make no-gid returns 1") ;

    struct stat st ;
    T_ASSERT_EQ(0, stat(path, &st), "stat created dir") ;
    T_ASSERT(S_ISDIR(st.st_mode), "is a directory") ;
    T_ASSERT_EQ(01733, st.st_mode & 07777, "no-gid mode is exactly 01733") ;

    mode_t now = umask(old) ;
    T_ASSERT_EQ(077, now, "umask restored after make") ;

    rm_rf(base) ;
}

static void test_make_reapplies_mode(void)
{
    char tmpl[] = "/tmp/ev_idem_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char path[1024] ; snprintf(path, sizeof(path), "%s/fd", base) ;

    T_ASSERT_EQ(1, event_fifo_make(path, (gid_t)-1), "first make") ;
    T_ASSERT_EQ(0, chmod(path, 0700), "force mode 0700") ;

    T_ASSERT_EQ(1, event_fifo_make(path, (gid_t)-1), "second make re-applies") ;

    struct stat st ;
    T_ASSERT_EQ(0, stat(path, &st), "stat") ;
    T_ASSERT_EQ(01733, st.st_mode & 07777, "second make restores the canonical mode") ;

    rm_rf(base) ;
}

static void test_make_existing_not_dir_ENOTDIR(void)
{
    char tmpl[] = "/tmp/ev_nd_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char path[1024] ; snprintf(path, sizeof(path), "%s/fd", base) ;

    int fd = open(path, O_CREAT | O_WRONLY, 0600) ;
    T_ASSERT(fd >= 0, "create regular file") ;
    close(fd) ;

    errno = 0 ;
    int r = event_fifo_make(path, (gid_t)-1) ;
    T_ASSERT_EQ(0, r, "make on a regular file returns 0") ;
    T_ASSERT_ERRNO(ENOTDIR, "errno ENOTDIR on non-dir") ;

    rm_rf(base) ;
}

static void test_make_rejects_symlink(void)
{
    char tmpl[] = "/tmp/ev_sym_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char target[1024] ; snprintf(target, sizeof(target), "%s/real", base) ;
    char path[1024] ;   snprintf(path, sizeof(path), "%s/fd", base) ;

    T_ASSERT_EQ(0, mkdir(target, 0700), "create owned target dir") ;
    T_ASSERT_EQ(0, symlink(target, path), "plant symlink at fifodir path") ;

    struct stat before ;
    T_ASSERT_EQ(0, stat(target, &before), "stat target before") ;

    errno = 0 ;
    int r = event_fifo_make(path, (gid_t)-1) ;
    T_ASSERT_EQ(0, r, "make on a symlink returns 0") ;
    T_ASSERT_ERRNO(ENOTDIR, "errno ENOTDIR: symlink rejected, not followed") ;

    struct stat after ;
    T_ASSERT_EQ(0, stat(target, &after), "stat target after") ;
    T_ASSERT_EQ((int)(before.st_mode & 07777), (int)(after.st_mode & 07777),
                "symlink target perms left untouched") ;

    rm_rf(base) ;
}

static void test_make_gid_path_chmod_branch(void)
{
    /* The chown leg is load-bearing: a fresh dir inherits the process egid as its
     * group, so to prove chown actually ran we target a SUPPLEMENTARY group that
     * differs from the egid. But in a rootless user namespace only the mapped gid
     * is chownable; a supplementary group maps to the unmapped overflow gid and
     * chown(-1, gid) fails with EPERM. So we PROBE chownability on the owned base
     * dir first: if the differing group is chownable, the chown leg is separable
     * and we assert the dir group changed; otherwise we fall back to the egid and
     * declare the chown leg not separable (it still exercises the 03730 branch). */
    gid_t egid = getegid() ;
    gid_t groups[64] ;
    int ng = getgroups(64, groups) ;
    T_ASSERT(ng >= 0, "getgroups") ;

    char tmpl[] = "/tmp/ev_gid_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;

    gid_t g = egid ;
    int separable = 0 ;
    for (int i = 0 ; i < ng ; i++) {
        if (groups[i] == egid) continue ;
        if (chown(base, (uid_t)-1, groups[i]) == 0) {   // probe: is it chownable here?
            g = groups[i] ;
            separable = 1 ;
            T_ASSERT_EQ(0, chown(base, (uid_t)-1, egid), "restore base group after probe") ;
            break ;
        }
    }

    char path[1024] ; snprintf(path, sizeof(path), "%s/fd", base) ;

    int r = event_fifo_make(path, g) ;
    T_ASSERT_EQ(1, r, "make with own gid returns 1") ;

    struct stat st ;
    T_ASSERT_EQ(0, stat(path, &st), "stat") ;
    T_ASSERT_EQ(03730, st.st_mode & 07777, "gid mode is exactly 03730 (sgid+grp-write)") ;
    T_ASSERT_EQ((long long)g, (long long)st.st_gid, "dir group is the requested gid (chown ran)") ;
    if (!separable)
        fprintf(stderr, "[no chownable group != egid here; chown leg not separable] ") ;

    rm_rf(base) ;
}

static void test_make_parent_missing_returns_0(void)
{
    char path[] = "/tmp/ev_nope_does_not_exist_XXX/sub/fd" ;
    errno = 0 ;
    int r = event_fifo_make(path, (gid_t)-1) ;
    T_ASSERT_EQ(0, r, "make with missing parent returns 0") ;
    T_ASSERT_ERRNO(ENOENT, "errno ENOENT propagated from mkdir") ;
}

/* ================================================================== */
/* event_fifo_clean                                                 */
/* ================================================================== */

static void make_fifo_named(char const *dir, char const *name, char *out, size_t outn)
{
    snprintf(out, outn, "%s/%s", dir, name) ;
    T_ASSERT_EQ(0, mkfifo(out, 0622), "mkfifo named") ;
}

#define VALID_NAME_A "evtsub:@400000000000000000000000:aaaaaa"
#define VALID_NAME_B "evtsub:@400000000000000000000000:bbbbbb"

static void test_clean_orphan_unlinked(void)
{
    char tmpl[] = "/tmp/ev_cl1_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;

    char fifo[1024] ;
    make_fifo_named(base, VALID_NAME_A, fifo, sizeof(fifo)) ;
    T_ASSERT_EQ(EVENT_FIFO_NAMELEN, (long long)strlen(VALID_NAME_A), "name is exactly 39") ;

    int r = event_fifo_clean(base) ;
    T_ASSERT_EQ(1, r, "clean returns 1") ;

    struct stat st ;
    errno = 0 ;
    T_ASSERT_EQ(-1, stat(fifo, &st), "orphan fifo is gone") ;
    T_ASSERT_ERRNO(ENOENT, "orphan fifo unlinked (ENOENT)") ;

    rm_rf(base) ;
}

static void test_clean_live_kept(void)
{
    char tmpl[] = "/tmp/ev_cl2_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;

    char fifo[1024] ;
    make_fifo_named(base, VALID_NAME_A, fifo, sizeof(fifo)) ;

    int rfd = open(fifo, O_RDONLY | O_NONBLOCK | O_CLOEXEC) ;
    T_ASSERT(rfd >= 0, "open read end") ;

    int r = event_fifo_clean(base) ;
    T_ASSERT_EQ(1, r, "clean returns 1") ;

    struct stat st ;
    T_ASSERT_EQ(0, stat(fifo, &st), "live fifo is kept") ;
    T_ASSERT(S_ISFIFO(st.st_mode), "still a fifo") ;

    close(rfd) ;
    rm_rf(base) ;
}

static void test_clean_ignores_nonmatching(void)
{
    char tmpl[] = "/tmp/ev_cl3_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;

    char f1[1024] ; snprintf(f1, sizeof(f1), "%s/%s", base, "zzzzzz:@4000000000000000000000000:aaaaaa") ;
    T_ASSERT_EQ(0, mkfifo(f1, 0622), "mkfifo wrong-prefix orphan") ;
    char f2[1024] ; snprintf(f2, sizeof(f2), "%s/%s", base, "evtsub:short") ;
    T_ASSERT_EQ(0, mkfifo(f2, 0622), "mkfifo short orphan") ;

    int r = event_fifo_clean(base) ;
    T_ASSERT_EQ(1, r, "clean returns 1") ;

    struct stat st ;
    T_ASSERT_EQ(0, stat(f1, &st), "wrong-prefix entry kept") ;
    T_ASSERT_EQ(0, stat(f2, &st), "wrong-length entry kept") ;

    rm_rf(base) ;
}

static void test_clean_mixed_orphan_and_live(void)
{
    char tmpl[] = "/tmp/ev_cl4_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;

    char orphan[1024], live[1024] ;
    make_fifo_named(base, VALID_NAME_A, orphan, sizeof(orphan)) ;
    make_fifo_named(base, VALID_NAME_B, live, sizeof(live)) ;

    int rfd = open(live, O_RDONLY | O_NONBLOCK | O_CLOEXEC) ;
    T_ASSERT(rfd >= 0, "open live read end") ;

    T_ASSERT_EQ(1, event_fifo_clean(base), "clean returns 1") ;

    struct stat st ;
    errno = 0 ;
    T_ASSERT_EQ(-1, stat(orphan, &st), "orphan swept") ;
    T_ASSERT_ERRNO(ENOENT, "orphan gone") ;
    T_ASSERT_EQ(0, stat(live, &st), "live kept") ;

    close(rfd) ;
    rm_rf(base) ;
}

static void test_clean_missing_dir_returns_0(void)
{
    errno = 0 ;
    int r = event_fifo_clean("/tmp/ev_absent_dir_zzz_XXX") ;
    T_ASSERT_EQ(0, r, "clean on absent dir returns 0") ;
    T_ASSERT_ERRNO(ENOENT, "opendir ENOENT") ;
}

/* ================================================================== */
/* subscribe scaffolding + a raw byte sink for the pump-level tests    */
/* ================================================================== */

typedef struct { char buf[4096] ; size_t n ; } sink_t ;
static void sink_handler(event_reader_t *r, char const *buf, size_t len, void *data)
{
    (void)r ;
    sink_t *s = data ;
    for (size_t i = 0 ; i < len && s->n < sizeof(s->buf) ; i++)
        s->buf[s->n++] = buf[i] ;
}

/* drive the loop until the framesink has at least `want` frames, or polls run out. */
static void pump_frames(sse_epoll_t *ep, framesink_t *s, size_t want)
{
    for (int i = 0 ; i < 1000 && s->n < want ; i++)
        if (sse_run(ep, 200) != 1) break ;
}

/* ================================================================== */
/* event_fifo_notify / event_emit_* — producer fanout, end to end   */
/* ================================================================== */

static void test_notify_delivers_frame_to_subscriber(void)
{
    char tmpl[] = "/tmp/ev_nf_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifo_make(ev, (gid_t)-1), "make eventdir") ;

    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    framesink_t s = {0} ;
    event_fifo_t r ;
    T_ASSERT_EQ(1, event_subscribe(&r, &ep, ev, framesink_handler, &s, 0), "subscribe") ;

    struct timespec ts = fixed_stamp() ;
    char frame[EVENT_FRAME_MAX] ;
    size_t len = event_frame_pack_transition(frame, STATUS_STATE_UP, STATUS_RESULT_SUCCESS,
                                       STATUS_WHO_USER, 55, 4321, &ts, 0) ;
    T_ASSERT_EQ(1, event_fifo_notify(ev, frame, len), "notify returns 1") ;

    pump_frames(&ep, &s, 1) ;
    T_ASSERT_EQ(1, (long long)s.n, "exactly one frame delivered") ;
    T_ASSERT_EQ((long long)len, (long long)s.rawn, "raw bytes delivered == frame length") ;
    T_ASSERT_EQ(EVENT_KIND_TRANSITION, s.frames[0].kind, "kind TRANSITION") ;
    T_ASSERT_EQ(STATUS_STATE_UP, s.frames[0].state, "state UP") ;
    T_ASSERT_EQ(55, (long long)s.frames[0].code, "code 55") ;
    T_ASSERT_EQ(4321, (long long)s.frames[0].pid, "pid 4321") ;
    T_ASSERT_EQ(STATUS_WHO_USER, s.frames[0].who, "who USER") ;

    event_unsubscribe(&r) ;
    sse_free(&ep) ;
    rm_rf(ev) ; rm_rf(base) ;
}

/* two frames concatenated in ONE notify (one write) arrive and decode as two,
 * in order — the reassembler splits them at the producer boundary. */
static void test_notify_two_frames_one_call(void)
{
    char tmpl[] = "/tmp/ev_nf2_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifo_make(ev, (gid_t)-1), "make eventdir") ;

    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    framesink_t s = {0} ;
    event_fifo_t r ;
    T_ASSERT_EQ(1, event_subscribe(&r, &ep, ev, framesink_handler, &s, 0), "subscribe") ;

    struct timespec ts = fixed_stamp() ;
    char buf[2 * EVENT_FRAME_MAX] ;
    size_t l0 = event_frame_pack_transition(buf, STATUS_STATE_STARTING, 0, 0, 0, 1, &ts, 0) ;
    size_t l1 = event_frame_pack_transition(buf + l0, STATUS_STATE_UP, 0, 0, 0, 2, &ts, 0) ;
    T_ASSERT_EQ(1, event_fifo_notify(ev, buf, l0 + l1), "notify two-frame buffer") ;

    pump_frames(&ep, &s, 2) ;
    T_ASSERT_EQ(2, (long long)s.n, "two frames delivered from one write") ;
    T_ASSERT_EQ(1, (long long)s.frames[0].pid, "first frame pid 1") ;
    T_ASSERT_EQ(STATUS_STATE_STARTING, s.frames[0].state, "first state STARTING") ;
    T_ASSERT_EQ(2, (long long)s.frames[1].pid, "second frame pid 2") ;
    T_ASSERT_EQ(STATUS_STATE_UP, s.frames[1].state, "second state UP") ;

    event_unsubscribe(&r) ;
    sse_free(&ep) ;
    rm_rf(ev) ; rm_rf(base) ;
}

static void test_emit_transition_e2e(void)
{
    char tmpl[] = "/tmp/ev_et_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifo_make(ev, (gid_t)-1), "make eventdir") ;

    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    framesink_t s = {0} ;
    event_fifo_t r ;
    T_ASSERT_EQ(1, event_subscribe(&r, &ep, ev, framesink_handler, &s, 0), "subscribe") ;

    struct timespec ts = fixed_stamp() ;
    T_ASSERT_EQ(1, event_emit_transition(ev, STATUS_STATE_FAILED, STATUS_RESULT_CRASH_LIMIT,
                                         STATUS_WHO_SELF, 125, 0, &ts, EVENT_FLAG_TERMINAL),
                "emit_transition returns 1") ;
    pump_frames(&ep, &s, 1) ;
    T_ASSERT_EQ(1, (long long)s.n, "one frame") ;
    T_ASSERT_EQ(STATUS_STATE_FAILED, s.frames[0].state, "state FAILED") ;
    T_ASSERT_EQ(STATUS_RESULT_CRASH_LIMIT, s.frames[0].result, "result CRASH_LIMIT") ;
    T_ASSERT_EQ(EVENT_FLAG_TERMINAL, s.frames[0].flags, "flags TERMINAL carried e2e") ;
    T_ASSERT_EQ(125, (long long)s.frames[0].code, "code 125") ;
    assert_stamp_eq(&ts, &s.frames[0].stamp, "stamp carried e2e") ;

    event_unsubscribe(&r) ;
    sse_free(&ep) ;
    rm_rf(ev) ; rm_rf(base) ;
}

static void test_emit_signal_e2e(void)
{
    char tmpl[] = "/tmp/ev_es_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifo_make(ev, (gid_t)-1), "make eventdir") ;

    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    framesink_t s = {0} ;
    event_fifo_t r ;
    T_ASSERT_EQ(1, event_subscribe(&r, &ep, ev, framesink_handler, &s, 0), "subscribe") ;

    struct timespec ts = fixed_stamp() ;
    T_ASSERT_EQ(1, event_emit_signal(ev, 9 /* SIGKILL */, STATUS_WHO_USER, &ts), "emit_signal returns 1") ;
    pump_frames(&ep, &s, 1) ;
    T_ASSERT_EQ(1, (long long)s.n, "one frame") ;
    T_ASSERT_EQ(EVENT_KIND_SIGNAL, s.frames[0].kind, "kind SIGNAL") ;
    T_ASSERT_EQ(9, s.frames[0].signo, "signo 9") ;
    T_ASSERT_EQ(STATUS_WHO_USER, s.frames[0].who, "who USER") ;

    event_unsubscribe(&r) ;
    sse_free(&ep) ;
    rm_rf(ev) ; rm_rf(base) ;
}

static void test_emit_lifecycle_e2e(void)
{
    char tmpl[] = "/tmp/ev_el_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifo_make(ev, (gid_t)-1), "make eventdir") ;

    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    framesink_t s = {0} ;
    event_fifo_t r ;
    T_ASSERT_EQ(1, event_subscribe(&r, &ep, ev, framesink_handler, &s, 0), "subscribe") ;

    struct timespec ts = fixed_stamp() ;
    T_ASSERT_EQ(1, event_emit_lifecycle(ev, EVENT_LIFECYCLE_UP, &ts), "emit_lifecycle returns 1") ;
    pump_frames(&ep, &s, 1) ;
    T_ASSERT_EQ(1, (long long)s.n, "one frame") ;
    T_ASSERT_EQ(EVENT_KIND_LIFECYCLE, s.frames[0].kind, "kind LIFECYCLE") ;
    T_ASSERT_EQ(EVENT_LIFECYCLE_UP, s.frames[0].phase, "phase UP") ;

    event_unsubscribe(&r) ;
    sse_free(&ep) ;
    rm_rf(ev) ; rm_rf(base) ;
}

static void test_notify_sweeps_orphan_and_missing_dir(void)
{
    char tmpl[] = "/tmp/ev_no_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifo_make(ev, (gid_t)-1), "make eventdir") ;

    char name[EVENT_FIFO_NAMELEN + 1] ;
    memset(name, 'a', EVENT_FIFO_NAMELEN) ;
    memcpy(name, EVENT_FIFO_PREFIX, EVENT_FIFO_PREFIXLEN) ;
    name[EVENT_FIFO_NAMELEN] = 0 ;
    char orphan[sizeof(ev) + EVENT_FIFO_NAMELEN + 1] ;
    snprintf(orphan, sizeof(orphan), "%s/%s", ev, name) ;
    T_ASSERT_EQ(0, mkfifo(orphan, 0622), "create orphan fifo") ;
    T_ASSERT_EQ(1, fanout_count(ev), "orphan is producer-eligible") ;

    struct timespec ts = fixed_stamp() ;
    char frame[EVENT_FRAME_MAX] ;
    size_t len = event_frame_pack_lifecycle(frame, EVENT_LIFECYCLE_UP, &ts) ;
    T_ASSERT_EQ(1, event_fifo_notify(ev, frame, len), "notify sweeps orphan, returns 1") ;
    T_ASSERT_EQ(0, fanout_count(ev), "orphan unlinked") ;

    errno = 0 ;
    T_ASSERT_EQ(0, event_fifo_notify("/tmp/ev_absent_zzz_QQQ", frame, len), "missing dir returns 0") ;
    T_ASSERT_ERRNO(ENOENT, "opendir ENOENT on missing dir") ;

    rm_rf(ev) ; rm_rf(base) ;
}

/* ================================================================== */
/* event_subscribe — the fifo create trick (format-agnostic)      */
/* ================================================================== */

static void test_subscribe_effects_and_mode(void)
{
    char tmpl[] = "/tmp/ev_sub_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifo_make(ev, (gid_t)-1), "make eventdir") ;

    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;

    sink_t s = {0} ;
    event_fifo_t r ;
    int rc = event_subscribe(&r, &ep, ev, sink_handler, &s, 0) ;
    T_ASSERT_EQ(1, rc, "subscribe returns 1") ;

    T_ASSERT(r.fifopath[0] != 0, "fifopath populated") ;
    T_ASSERT(strstr(r.fifopath, "/evtsub:") != NULL, "visible name has ftrig1 prefix") ;
    T_ASSERT(strstr(r.fifopath, "/.evtsub:") == NULL, "visible name is NOT the hidden name") ;

    struct stat st ;
    T_ASSERT_EQ(0, stat(r.fifopath, &st), "fifo exists on disk") ;
    T_ASSERT(S_ISFIFO(st.st_mode), "it is a fifo") ;
    T_ASSERT_EQ(0622, st.st_mode & 07777, "fifo mode forced to 0622") ;

    T_ASSERT(r.wfd >= 0, "write end held open") ;

    int enx = -1 ;
    T_ASSERT_EQ(1, fanout_count(ev), "exactly one visible fifo") ;
    T_ASSERT_EQ(1, fanout_open_ok(ev, &enx), "producer opens it without ENXIO") ;
    T_ASSERT_EQ(0, enx, "no ENXIO on the published fifo") ;

    T_ASSERT_EQ(1, dir_entries(ev), "no stray (hidden) leftover entry") ;

    event_unsubscribe(&r) ;
    T_ASSERT_EQ(-1, r.wfd, "wfd reset to -1") ;
    T_ASSERT_EQ(0, r.fifopath[0], "fifopath cleared") ;
    T_ASSERT_EQ(0, dir_entries(ev), "fifo unlinked on unsubscribe") ;

    sse_free(&ep) ;
    rm_rf(ev) ; rm_rf(base) ;
}

static void test_subscribe_trick_never_visible_without_reader(void)
{
    char tmpl[] = "/tmp/ev_trick_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifo_make(ev, (gid_t)-1), "make eventdir") ;

    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    sink_t s = {0} ;

    for (int i = 0 ; i < 200 ; i++) {
        event_fifo_t r ;
        T_ASSERT_EQ(1, event_subscribe(&r, &ep, ev, sink_handler, &s, 0), "subscribe cycle") ;

        int enx = -1 ;
        int ok = fanout_open_ok(ev, &enx) ;
        T_ASSERT_EQ(1, ok, "visible fifo always openable by producer") ;
        T_ASSERT_EQ(0, enx, "never a visible fifo without a reader") ;

        event_unsubscribe(&r) ;
        T_ASSERT_EQ(0, dir_entries(ev), "no leftover after unsubscribe") ;
    }

    sse_free(&ep) ;
    rm_rf(ev) ; rm_rf(base) ;
}

static void test_subscribe_trick_hidden_name(void)
{
    char tmpl[] = "/tmp/ev_hid_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifo_make(ev, (gid_t)-1), "make eventdir") ;

    sse_epoll_t epp ;
    T_ASSERT_EQ(1, sse_new(&epp, 1), "sse_new") ;
    sink_t s = {0} ;
    event_fifo_t r ;
    T_ASSERT_EQ(1, event_subscribe(&r, &epp, ev, sink_handler, &s, 0), "subscribe") ;

    char const *slash = strrchr(r.fifopath, '/') ;
    T_ASSERT(slash != NULL, "fifopath has a slash") ;
    T_ASSERT_EQ(0, strncmp(slash + 1, EVENT_FIFO_PREFIX, EVENT_FIFO_PREFIXLEN),
                "published basename starts with visible evtsub: (not '.')") ;
    T_ASSERT(slash[1] != '.', "published basename is not the hidden '.evtsub:' name") ;

    DIR *d = opendir(ev) ; T_ASSERT(d != NULL, "opendir") ;
    struct dirent *e ; int hidden = 0, total = 0 ;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue ;
        total++ ;
        if (!strncmp(e->d_name, ".evtsub:", 8)) hidden++ ;
    }
    closedir(d) ;
    T_ASSERT_EQ(0, hidden, "no hidden '.evtsub:' temp left behind") ;
    T_ASSERT_EQ(1, total, "exactly one entry on disk") ;
    T_ASSERT_EQ(1, fanout_count(ev), "one producer-visible fifo for one live reader") ;

    event_unsubscribe(&r) ;
    sse_free(&epp) ;
    rm_rf(ev) ; rm_rf(base) ;
}

static void test_subscribe_trick_concurrent_smoke(void)
{
    char tmpl[] = "/tmp/ev_race_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifo_make(ev, (gid_t)-1), "make eventdir") ;

    volatile long *sh = mmap(NULL, 2 * sizeof(long), PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS, -1, 0) ;
    T_ASSERT(sh != MAP_FAILED, "mmap shared") ;
    sh[0] = 0 ;
    sh[1] = 0 ;

    pid_t parent = getpid() ;
    pid_t pid = fork() ;
    T_ASSERT(pid >= 0, "fork") ;
    if (pid == 0) {
        child_dies_with_parent(parent) ;
        while (!sh[0]) { int enx = 0 ; fanout_open_ok(ev, &enx) ; sh[1] += enx ; }
        _exit(0) ;
    }

    sse_epoll_t epp ;
    T_ASSERT_EQ(1, sse_new(&epp, 1), "sse_new") ;
    sink_t s = {0} ;
    for (int i = 0 ; i < 2000 ; i++) {
        event_fifo_t r ;
        T_ASSERT_EQ(1, event_subscribe(&r, &epp, ev, sink_handler, &s, 0), "subscribe churn") ;
        event_unsubscribe(&r) ;
        T_ASSERT_EQ(-1, r.wfd, "wfd cleared after unsubscribe") ;
    }
    sse_free(&epp) ;

    sh[0] = 1 ;
    int wst = 0 ;
    T_ASSERT(waitpid(pid, &wst, 0) == pid, "reap child") ;

    T_ASSERT_EQ(0, (int)sh[1], "no ENXIO on a fifo still on disk (unlink-before-close)") ;

    munmap((void *)sh, 2 * sizeof(long)) ;
    rm_rf(ev) ; rm_rf(base) ;
}

/* DOUBLE-FD: the reader holds the write end, so an external producer opening and
 * closing its own write end repeatedly never puts the read end at EOF. Proven by
 * still delivering frames cleanly after the writer churn. */
static void test_subscribe_double_fd_no_eof(void)
{
    char tmpl[] = "/tmp/ev_dfd_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifo_make(ev, (gid_t)-1), "make eventdir") ;

    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    framesink_t s = {0} ;
    event_fifo_t r ;
    T_ASSERT_EQ(1, event_subscribe(&r, &ep, ev, framesink_handler, &s, 0), "subscribe") ;

    for (int i = 0 ; i < 50 ; i++) {
        int w = open(r.fifopath, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
        T_ASSERT(w >= 0, "external producer opens write end") ;
        close(w) ;
    }

    struct timespec ts = fixed_stamp() ;
    char frame[EVENT_FRAME_MAX] ;
    size_t len = event_frame_pack_transition(frame, STATUS_STATE_UP, 0, 0, 0, 1, &ts, 0) ;
    T_ASSERT_EQ(1, fanout_frame(ev, frame, len), "producer writes one frame") ;

    pump_frames(&ep, &s, 1) ;
    T_ASSERT_EQ(1, (long long)s.n, "exactly one frame delivered after writer churn") ;
    T_ASSERT_EQ(STATUS_STATE_UP, s.frames[0].state, "delivered state UP") ;

    len = event_frame_pack_lifecycle(frame, EVENT_LIFECYCLE_DOWN, &ts) ;
    T_ASSERT_EQ(1, fanout_frame(ev, frame, len), "producer writes again") ;
    pump_frames(&ep, &s, 2) ;
    T_ASSERT_EQ(2, (long long)s.n, "second frame delivered, read end still alive") ;
    T_ASSERT_EQ(EVENT_KIND_LIFECYCLE, s.frames[1].kind, "second frame LIFECYCLE") ;

    event_unsubscribe(&r) ;
    sse_free(&ep) ;
    rm_rf(ev) ; rm_rf(base) ;
}

static void test_subscribe_nametoolong(void)
{
    char ev[SS_MAX_PATH + 64] ;
    memset(ev, 'a', sizeof(ev) - 1) ;
    ev[0] = '/' ;
    ev[sizeof(ev) - 1] = 0 ;

    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;

    event_fifo_t r ;
    errno = 0 ;
    int rc = event_subscribe(&r, &ep, ev, sink_handler, NULL, 0) ;
    T_ASSERT_EQ(0, rc, "subscribe returns 0 on too-long path") ;
    T_ASSERT_ERRNO(ENAMETOOLONG, "errno ENAMETOOLONG") ;
    T_ASSERT_EQ(0, r.fifopath[0], "nothing published") ;
    T_ASSERT_EQ(-1, r.wfd, "no write fd opened") ;

    sse_free(&ep) ;
}

static void test_subscribe_eventdir_missing_cleanup(void)
{
    sse_epoll_t ep ;
    T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;

    event_fifo_t r ;
    errno = 0 ;
    int rc = event_subscribe(&r, &ep, "/tmp/ev_no_such_dir_zzz_XXX", sink_handler, NULL, 0) ;
    T_ASSERT_EQ(0, rc, "subscribe on missing dir returns 0") ;
    T_ASSERT_ERRNO(ENOENT, "errno ENOENT from mkfifo") ;
    T_ASSERT_EQ(0, r.fifopath[0], "fifopath cleared on failure") ;
    T_ASSERT_EQ(-1, r.wfd, "wfd stays -1 on failure") ;

    sse_free(&ep) ;
}

/* ================================================================== */
/* pump (event_reader) delivery + decoder reassembly across chunks     */
/* ================================================================== */

static void test_cb_single_frame(void)
{
    char tmpl[] = "/tmp/ev_cb1_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifo_make(ev, (gid_t)-1), "make") ;
    sse_epoll_t ep ; T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    framesink_t s = {0} ; event_fifo_t r ;
    T_ASSERT_EQ(1, event_subscribe(&r, &ep, ev, framesink_handler, &s, 0), "subscribe") ;

    struct timespec ts = fixed_stamp() ;
    char frame[EVENT_FRAME_MAX] ;
    size_t len = event_frame_pack_lifecycle(frame, EVENT_LIFECYCLE_UP, &ts) ;
    T_ASSERT_EQ(1, fanout_frame(ev, frame, len), "write one frame") ;
    pump_frames(&ep, &s, 1) ;
    T_ASSERT_EQ(1, (long long)s.n, "one frame delivered once") ;
    T_ASSERT_EQ((long long)len, (long long)s.rawn, "raw byte count matches frame length") ;
    T_ASSERT_EQ(EVENT_LIFECYCLE_UP, s.frames[0].phase, "phase UP") ;

    event_unsubscribe(&r) ; sse_free(&ep) ; rm_rf(ev) ; rm_rf(base) ;
}

/* many frames written in ONE write far larger than the pump's 256-byte buffer:
 * the pump chunks it into 256-byte reads and the decoder reassembles frames that
 * straddle those chunk boundaries. All K frames delivered, in order. */
static void test_cb_many_frames_over_256(void)
{
    char tmpl[] = "/tmp/ev_cb2_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifo_make(ev, (gid_t)-1), "make") ;
    sse_epoll_t ep ; T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    framesink_t s = {0} ; event_fifo_t r ;
    T_ASSERT_EQ(1, event_subscribe(&r, &ep, ev, framesink_handler, &s, 0), "subscribe") ;

    enum { K = 40 } ;   /* 40 * 31 = 1240 bytes, several 256-chunk boundaries */
    struct timespec ts = fixed_stamp() ;
    char big[K * EVENT_FRAME_MAX] ;
    size_t off = 0 ;
    for (int i = 0 ; i < K ; i++)
        off += event_frame_pack_transition(big + off, STATUS_STATE_UP, 0, 0, 0, (uint32_t)i, &ts, 0) ;

    int w = open(r.fifopath, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
    T_ASSERT(w >= 0, "open writer") ;
    ssize_t tot = 0 ;
    while ((size_t)tot < off) {
        ssize_t k = write(w, big + tot, off - tot) ;
        if (k < 0) { if (errno == EINTR) continue ; break ; }
        tot += k ;
    }
    close(w) ;
    T_ASSERT_EQ((long long)off, (long long)tot, "wrote the full batch") ;

    pump_frames(&ep, &s, K) ;
    T_ASSERT_EQ(K, (long long)s.n, "all K frames reassembled across 256-byte chunks") ;
    for (int i = 0 ; i < K ; i++)
        T_ASSERT_EQ(i, (long long)s.frames[i].pid, "frame order and pid preserved") ;

    event_unsubscribe(&r) ; sse_free(&ep) ; rm_rf(ev) ; rm_rf(base) ;
}

static void test_cb_multiple_writes(void)
{
    char tmpl[] = "/tmp/ev_cb4_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    T_ASSERT_EQ(1, event_fifo_make(ev, (gid_t)-1), "make") ;
    sse_epoll_t ep ; T_ASSERT_EQ(1, sse_new(&ep, 1), "sse_new") ;
    framesink_t s = {0} ; event_fifo_t r ;
    T_ASSERT_EQ(1, event_subscribe(&r, &ep, ev, framesink_handler, &s, 0), "subscribe") ;

    struct timespec ts = fixed_stamp() ;
    uint8_t states[3] = { STATUS_STATE_STARTING, STATUS_STATE_UP, STATUS_STATE_DOWN } ;
    for (int i = 0 ; i < 3 ; i++) {
        char frame[EVENT_FRAME_MAX] ;
        size_t len = event_frame_pack_transition(frame, states[i], 0, 0, 0, (uint32_t)(i + 100), &ts, 0) ;
        T_ASSERT_EQ(1, fanout_frame(ev, frame, len), "discrete frame write") ;
        pump_frames(&ep, &s, (size_t)(i + 1)) ;
    }
    T_ASSERT_EQ(3, (long long)s.n, "three discrete writes, three frames") ;
    for (int i = 0 ; i < 3 ; i++) {
        T_ASSERT_EQ(states[i], s.frames[i].state, "state order preserved across writes") ;
        T_ASSERT_EQ(i + 100, (long long)s.frames[i].pid, "pid order preserved across writes") ;
    }

    event_unsubscribe(&r) ; sse_free(&ep) ; rm_rf(ev) ; rm_rf(base) ;
}

/* ================================================================== */
/* event_wait — wait_and over N fifodirs (the prod scenario)           */
/* ================================================================== */

static void make_dirs(char *base, size_t n, char **dirs)
{
    for (size_t i = 0 ; i < n ; i++) {
        dirs[i] = malloc(1024) ;
        T_ASSERT(dirs[i] != NULL, "malloc dir") ;
        snprintf(dirs[i], 1024, "%s/ev%zu", base, i) ;
        T_ASSERT_EQ(1, event_fifo_make(dirs[i], (gid_t)-1), "make eventdir") ;
    }
}
static void free_dirs(size_t n, char **dirs) { for (size_t i = 0 ; i < n ; i++) { rm_rf(dirs[i]) ; free(dirs[i]) ; } }

/* emit a single UP transition (reaches READY) to dir. */
static int emit_ready(char const *dir)
{
    struct timespec ts = fixed_stamp() ;
    return event_emit_transition(dir, STATUS_STATE_UP, STATUS_RESULT_SUCCESS,
                                 STATUS_WHO_SELF, 0, 0, &ts, 0) ;
}

static void test_wait_all_triggered(void)
{
    char tmpl[] = "/tmp/ev_wa_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    enum { N = 4 } ;
    char *dirs[N] ; make_dirs(base, N, dirs) ;

    event_wait_t w ;
    T_ASSERT_EQ(1, event_wait_init(&w, (char const *const *)dirs, N, EVENT_UP_READY), "init") ;

    for (size_t i = 0 ; i < N ; i++)
        T_ASSERT_EQ(1, emit_ready(dirs[i]), "trigger each dir with an UP transition") ;

    int r = event_wait_run(&w, 2000) ;
    T_ASSERT_EQ(1, r, "run returns 1 when all triggered") ;
    T_ASSERT_EQ(N, (long long)w.triggered, "triggered == n") ;

    event_wait_free(&w) ;
    T_ASSERT_EQ(0, (long long)w.n, "free resets n") ;
    T_ASSERT(w.fifos == NULL, "fifo sources freed") ;
    T_ASSERT(w.slots == NULL, "slots freed") ;
    for (size_t i = 0 ; i < N ; i++) T_ASSERT_EQ(0, dir_entries(dirs[i]), "fifo unlinked") ;

    free_dirs(N, dirs) ; rm_rf(base) ;
}

static void test_wait_partial_timeout(void)
{
    char tmpl[] = "/tmp/ev_wp_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    enum { N = 3 } ;
    char *dirs[N] ; make_dirs(base, N, dirs) ;

    event_wait_t w ;
    T_ASSERT_EQ(1, event_wait_init(&w, (char const *const *)dirs, N, EVENT_UP_READY), "init") ;

    T_ASSERT_EQ(1, emit_ready(dirs[0]), "trigger 0") ;
    T_ASSERT_EQ(1, emit_ready(dirs[2]), "trigger 2") ;

    int r = event_wait_run(&w, 300) ;
    T_ASSERT_EQ(0, r, "run returns 0 on partial timeout") ;
    T_ASSERT_EQ(2, (long long)w.triggered, "triggered == k (2)") ;

    event_wait_free(&w) ;
    free_dirs(N, dirs) ; rm_rf(base) ;
}

static void test_wait_duplicate_frame_idempotent(void)
{
    char tmpl[] = "/tmp/ev_wd_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    enum { N = 2 } ;
    char *dirs[N] ; make_dirs(base, N, dirs) ;

    event_wait_t w ;
    T_ASSERT_EQ(1, event_wait_init(&w, (char const *const *)dirs, N, EVENT_UP_READY), "init") ;

    /* dir0 emits the wanted UP transition THREE times; dir1 once. The slot's
     * `done` flag must dedupe so triggered reaches exactly 2, never more. */
    for (int k = 0 ; k < 3 ; k++)
        T_ASSERT_EQ(1, emit_ready(dirs[0]), "dir0 duplicate UP transition") ;
    T_ASSERT_EQ(1, emit_ready(dirs[1]), "dir1 once") ;

    int r = event_wait_run(&w, 2000) ;
    T_ASSERT_EQ(1, r, "run returns 1") ;
    T_ASSERT_EQ(2, (long long)w.triggered, "duplicate frames do NOT inflate triggered") ;

    event_wait_free(&w) ;
    free_dirs(N, dirs) ; rm_rf(base) ;
}

static void test_wait_irrelevant_frames_ignored(void)
{
    char tmpl[] = "/tmp/ev_wn_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    enum { N = 2 } ;
    char *dirs[N] ; make_dirs(base, N, dirs) ;

    event_wait_t w ;
    T_ASSERT_EQ(1, event_wait_init(&w, (char const *const *)dirs, N, EVENT_UP_READY), "init") ;

    struct timespec ts = fixed_stamp() ;
    /* dir0: eventually reaches READY (UP) */
    T_ASSERT_EQ(1, emit_ready(dirs[0]), "dir0 reaches ready") ;
    /* dir1: only frames that never reach READY and never fail:
     * STARTING (up, not ready), DOWN (down, ready but not up), and a SIGNAL. */
    T_ASSERT_EQ(1, event_emit_transition(dirs[1], STATUS_STATE_STARTING, 0, 0, 0, 0, &ts, 0), "starting dir1") ;
    T_ASSERT_EQ(1, event_emit_transition(dirs[1], STATUS_STATE_DOWN, 0, 0, 0, 0, &ts, 0), "down dir1") ;
    T_ASSERT_EQ(1, event_emit_signal(dirs[1], 15, STATUS_WHO_SELF, &ts), "signal dir1 (inert)") ;

    int r = event_wait_run(&w, 300) ;
    T_ASSERT_EQ(0, r, "run times out: dir1 never reached READY") ;
    T_ASSERT_EQ(1, (long long)w.triggered, "only dir0 matched") ;
    T_ASSERT_EQ(0, w.failed, "no terminal frame: not flagged failed") ;

    event_wait_free(&w) ;
    free_dirs(N, dirs) ; rm_rf(base) ;
}

/* a terminal down while waiting up ends the wait_and immediately, like s6-svwait
 * exiting on such an event rather than waiting out the deadline. Proven by the
 * `failed` flag AND the elapsed time being far below the generous timeout. */
static void test_wait_permanent_failure_fast(void)
{
    char tmpl[] = "/tmp/ev_wf_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    enum { N = 2 } ;
    char *dirs[N] ; make_dirs(base, N, dirs) ;

    event_wait_t w ;
    T_ASSERT_EQ(1, event_wait_init(&w, (char const *const *)dirs, N, EVENT_UP_READY), "init") ;

    struct timespec ts = fixed_stamp() ;
    /* dir1 will never come up; dir0 reports a terminal FAILED -> the AND is doomed
     * and must fail immediately, not after the 5s deadline. */
    T_ASSERT_EQ(1, event_emit_transition(dirs[0], STATUS_STATE_FAILED, STATUS_RESULT_CRASH_LIMIT,
                                         STATUS_WHO_SELF, 0, 0, &ts, EVENT_FLAG_TERMINAL),
                "terminal FAILED dir0") ;

    struct timespec a, b ;
    clock_gettime(CLOCK_MONOTONIC, &a) ;
    int r = event_wait_run(&w, 5000) ;
    clock_gettime(CLOCK_MONOTONIC, &b) ;
    long ms = (b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000 ;

    T_ASSERT_EQ(0, r, "run returns 0 on permanent failure") ;
    T_ASSERT_EQ(1, w.failed, "permanent failure flagged") ;
    T_ASSERT(ms < 1000, "failed fast, well under the 5s deadline") ;

    event_wait_free(&w) ;
    free_dirs(N, dirs) ; rm_rf(base) ;
}

static void test_wait_zero_dirs(void)
{
    event_wait_t w ;
    T_ASSERT_EQ(1, event_wait_init(&w, NULL, 0, EVENT_UP_READY), "init n=0") ;
    T_ASSERT(w.fifos == NULL, "no fifo sources allocated for n=0") ;
    T_ASSERT(w.slots == NULL, "no slots allocated for n=0") ;
    int r = event_wait_run(&w, 1000) ;
    T_ASSERT_EQ(1, r, "run n=0 returns 1 immediately") ;
    event_wait_free(&w) ;
}

static void test_wait_timeout_zero_no_timer(void)
{
    char tmpl[] = "/tmp/ev_wz_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    enum { N = 1 } ;
    char *dirs[N] ; make_dirs(base, N, dirs) ;

    event_wait_t w ;
    T_ASSERT_EQ(1, event_wait_init(&w, (char const *const *)dirs, N, EVENT_UP_READY), "init") ;
    T_ASSERT_EQ(1, emit_ready(dirs[0]), "trigger") ;

    int r = event_wait_run(&w, 0) ;
    T_ASSERT_EQ(1, r, "run returns 1 (match) with timeout 0") ;
    T_ASSERT_EQ(0, w.timer_active, "no timer armed when timeout_ms==0") ;

    event_wait_free(&w) ;
    free_dirs(N, dirs) ; rm_rf(base) ;
}

static void test_wait_timeout_zero_blocks(void)
{
    char tmpl[] = "/tmp/ev_wzb_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    enum { N = 1 } ;
    char *dirs[N] ; make_dirs(base, N, dirs) ;

    pid_t parent = getpid() ;
    pid_t pid = fork() ;
    T_ASSERT(pid >= 0, "fork") ;
    if (pid == 0) {
        child_dies_with_parent(parent) ;
        alarm(0) ;
        event_wait_t cw ;
        if (!event_wait_init(&cw, (char const *const *)dirs, N, EVENT_UP_READY)) _exit(2) ;
        int rc = event_wait_run(&cw, 0) ;
        _exit(rc == 0 ? 50 : 51) ;
    }

    struct timespec ts = { 0, 350 * 1000000L } ; nanosleep(&ts, NULL) ;
    int st = 0 ;
    pid_t done = waitpid(pid, &st, WNOHANG) ;
    T_ASSERT_EQ(0, done, "child still blocked: timeout_ms==0 armed no deadline") ;

    kill(pid, SIGKILL) ;
    waitpid(pid, &st, 0) ;
    free_dirs(N, dirs) ; rm_rf(base) ;
}

static void test_wait_init_rollback_on_bad_dir(void)
{
    char tmpl[] = "/tmp/ev_wr_XXXXXX" ;
    char *base = mkdir_scratch(tmpl) ;
    char *dirs[2] ;
    dirs[0] = malloc(1024) ; snprintf(dirs[0], 1024, "%s/ev0", base) ;
    T_ASSERT_EQ(1, event_fifo_make(dirs[0], (gid_t)-1), "make ev0") ;
    dirs[1] = malloc(1024) ; snprintf(dirs[1], 1024, "%s/ev_absent", base) ;

    event_wait_t w ;
    errno = 0 ;
    int r = event_wait_init(&w, (char const *const *)dirs, 2, EVENT_UP_READY) ;
    T_ASSERT_EQ(0, r, "init returns 0 when a subscribe fails") ;
    T_ASSERT(w.fifos == NULL, "fifo sources freed on rollback") ;
    T_ASSERT(w.slots == NULL, "slots freed on rollback") ;
    T_ASSERT_EQ(0, dir_entries(dirs[0]), "rolled-back reader fifo unlinked") ;
    T_ASSERT_EQ(-1, w.epoll.fd, "epoll freed (fd -1)") ;

    rm_rf(dirs[0]) ; free(dirs[0]) ; free(dirs[1]) ; rm_rf(base) ;
}

/* ------------------------------------------------------------------ */

T_SUITE("event module")
{
    VERBOSITY = 0 ;
    PROG = "test_event" ;
    signal(SIGALRM, on_alarm) ;
    alarm(60) ;

    /* frame codec: packers + wire layout + round trip */
    T_RUN(test_pack_transition_wire_and_roundtrip) ;
    T_RUN(test_pack_transition_no_flag) ;
    T_RUN(test_pack_signal_wire_and_roundtrip) ;
    T_RUN(test_pack_lifecycle_wire_and_roundtrip) ;

    /* decoder: reassembly + resync */
    T_RUN(test_decode_single) ;
    T_RUN(test_decode_two_concatenated) ;
    T_RUN(test_decode_split_all_cutpoints) ;
    T_RUN(test_decode_bigchunk_many_plus_partial) ;
    T_RUN(test_decode_len_zero_noop) ;
    T_RUN(test_decode_corrupt_version_resync) ;
    T_RUN(test_decode_bad_len_resync) ;
    T_RUN(test_decode_unknown_kind_consumed_no_cb) ;
    T_RUN(test_decode_short_payload_consumed_no_cb) ;
    T_RUN(test_decode_plen_below_clockpack) ;
    T_RUN(test_decode_stray_bytes_between) ;

    /* matcher */
    T_RUN(test_match_up) ;
    T_RUN(test_match_ready) ;
    T_RUN(test_match_down_from_up) ;
    T_RUN(test_match_down_ready) ;
    T_RUN(test_match_restart_two_phase) ;
    T_RUN(test_match_supervise_up) ;
    T_RUN(test_match_supervise_down) ;
    T_RUN(test_match_terminal_fastfail_up) ;
    T_RUN(test_match_terminal_ok_when_down) ;
    T_RUN(test_match_lifecycle_down_fails_service_wait) ;
    T_RUN(test_match_signal_inert) ;
    T_RUN(test_match_seed_already_up) ;
    T_RUN(test_match_satisfied_seed) ;
    T_RUN(test_match_satisfied_is_const) ;

    /* fifodir_make */
    T_RUN(test_make_nogid_mode_01733) ;
    T_RUN(test_make_reapplies_mode) ;
    T_RUN(test_make_existing_not_dir_ENOTDIR) ;
    T_RUN(test_make_rejects_symlink) ;
    T_RUN(test_make_gid_path_chmod_branch) ;
    T_RUN(test_make_parent_missing_returns_0) ;

    /* fifodir_clean */
    T_RUN(test_clean_orphan_unlinked) ;
    T_RUN(test_clean_live_kept) ;
    T_RUN(test_clean_ignores_nonmatching) ;
    T_RUN(test_clean_mixed_orphan_and_live) ;
    T_RUN(test_clean_missing_dir_returns_0) ;

    /* fifodir_notify + emit_* (producer fanout, end to end) */
    T_RUN(test_notify_delivers_frame_to_subscriber) ;
    T_RUN(test_notify_two_frames_one_call) ;
    T_RUN(test_emit_transition_e2e) ;
    T_RUN(test_emit_signal_e2e) ;
    T_RUN(test_emit_lifecycle_e2e) ;
    T_RUN(test_notify_sweeps_orphan_and_missing_dir) ;

    /* subscribe / the trick */
    T_RUN(test_subscribe_effects_and_mode) ;
    T_RUN(test_subscribe_trick_never_visible_without_reader) ;
    T_RUN(test_subscribe_trick_hidden_name) ;
    T_RUN(test_subscribe_trick_concurrent_smoke) ;
    T_RUN(test_subscribe_double_fd_no_eof) ;
    T_RUN(test_subscribe_nametoolong) ;
    T_RUN(test_subscribe_eventdir_missing_cleanup) ;

    /* pump delivery + decoder reassembly across pump chunks */
    T_RUN(test_cb_single_frame) ;
    T_RUN(test_cb_many_frames_over_256) ;
    T_RUN(test_cb_multiple_writes) ;

    /* wait */
    T_RUN(test_wait_all_triggered) ;
    T_RUN(test_wait_partial_timeout) ;
    T_RUN(test_wait_duplicate_frame_idempotent) ;
    T_RUN(test_wait_irrelevant_frames_ignored) ;
    T_RUN(test_wait_permanent_failure_fast) ;
    T_RUN(test_wait_zero_dirs) ;
    T_RUN(test_wait_timeout_zero_no_timer) ;
    T_RUN(test_wait_timeout_zero_blocks) ;
    T_RUN(test_wait_init_rollback_on_bad_dir) ;
}
