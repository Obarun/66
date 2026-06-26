/* fuzz_match.c — libFuzzer harness for the event_match transition interpreter.
 *
 * Drives event_match_init/feed with an arbitrary wanted state, seed, and byte
 * stream fed in arbitrary-sized chunks, and checks the matcher's invariants on
 * every step: the verdict is always one of FAIL/PENDING/OK, and the tracked
 * (up, ready, restart_done) bits never leave {0, 1}. Runs under ASan+UBSan.
 *
 * Build (standalone, clang owns the sanitizer runtime; oblibs linked
 * un-instrumented as only its symbols come from the lib):
 *   clang -std=c11 -D_GNU_SOURCE -g -O1 -fsanitize=address,undefined,fuzzer \
 *     -I <66>/src/include -I <oblibs>/src/include \
 *     event_match.c fuzz_match.c -L <oblibs>/builddir/src -loblibs -o fuzz_match
 */
#include <stdint.h>
#include <stddef.h>

#include <66/event.h>

static void check(event_match_t *m, int verdict)
{
    if (verdict != EVENT_MATCH_FAIL && verdict != EVENT_MATCH_PENDING && verdict != EVENT_MATCH_OK)
        __builtin_trap() ;
    if (m->up > 1 || m->ready > 1 || m->restart_done > 1)
        __builtin_trap() ;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2)
        return 0 ;

    event_t wanted = (event_t)(data[0] % 9) ;   // UP..RESTART_READY (the 9 wait values, not NONE)
    unsigned char up = data[1] & 1u ;
    unsigned char ready = (data[1] >> 1) & 1u ;

    event_match_t m ;
    event_match_init(&m, wanted, up, ready) ;
    check(&m, event_match_feed(&m, 0, 0)) ;   // seed evaluation (len 0)

    size_t off = 2 ;
    while (off < size) {
        size_t chunk = (size_t)(data[off] & 3u) + 1u ;      // 1..4 bytes
        if (chunk > size - off)
            chunk = size - off ;
        int verdict = event_match_feed(&m, (char const *)(data + off), chunk) ;
        check(&m, verdict) ;
        off += chunk ;
        if (verdict != EVENT_MATCH_PENDING)
            break ;   // contract: do not feed a matcher that has resolved
    }

    return 0 ;
}
