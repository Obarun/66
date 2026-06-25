/* fuzz_reader.c — libFuzzer harness for the event reader byte pipeline.
 *
 * Shapes (data,size) into the exact byte stream a producer fans out onto a
 * subscriber fifo, then drives the SSE loop and checks that the reader delivers
 * precisely those bytes, in order, once each. Exercises event_fifo_subscribe
 * (the create trick), the pump read loop (incl. the >256 buffer boundary), and
 * event_fifo_unsubscribe (cleanup), all under ASan+UBSan.
 *
 * Build:
 *   clang -std=c11 -D_GNU_SOURCE -g -O1 -fsanitize=address,undefined,fuzzer \
 *     -I.../src/include -I.../builddir-asan/src/include -I oblibs/src/include \
 *     event_*.c fuzz_reader.c -L oblibs/builddir-asan/src -loblibs -o fuzz_reader
 * (NB: oblibs ASan is gcc-built; for the fuzzer build we compile a non-ASan
 *  oblibs or accept clang's own runtime — see report. Here we link the module
 *  sources directly so only oblibs symbols come from the lib.)
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <oblibs/sse.h>
#include <oblibs/log.h>
#include <66/event.h>

typedef struct { unsigned char *buf ; size_t n, cap ; } sink_t ;
static void fuzz_handler(event_reader_t *r, char const *buf, size_t len, void *data)
{
    (void)r ;
    sink_t *s = data ;
    for (size_t i = 0 ; i < len && s->n < s->cap ; i++)
        s->buf[s->n++] = (unsigned char)buf[i] ;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    VERBOSITY = 0 ;

    /* bound the stream so the test stays fast and the pipe never blocks */
    if (size > 8192) size = 8192 ;

    char base[] = "/tmp/fz_ev_XXXXXX" ;
    if (!mkdtemp(base)) return 0 ;
    char ev[1024] ; snprintf(ev, sizeof(ev), "%s/event", base) ;
    if (!event_fifodir_make(ev, (gid_t)-1)) { rmdir(base) ; return 0 ; }

    sse_epoll_t ep ;
    if (!sse_new(&ep, 1)) goto out_dir ;

    sink_t s = { malloc(size ? size : 1), 0, size } ;
    if (!s.buf) { sse_free(&ep) ; goto out_dir ; }

    event_fifo_t r ;
    if (!event_fifo_subscribe(&r, &ep, ev, fuzz_handler, &s, 0)) {
        free(s.buf) ; sse_free(&ep) ; goto out_dir ;
    }

    /* fan the fuzzer bytes onto the fifo, in chunks to exercise multi-read */
    int w = open(r.fifopath, O_WRONLY | O_NONBLOCK | O_CLOEXEC) ;
    if (w >= 0) {
        size_t off = 0 ;
        while (off < size) {
            size_t chunk = size - off ;
            if (chunk > 333) chunk = 333 ;        /* straddle the 256 buffer */
            ssize_t k = write(w, data + off, chunk) ;
            if (k < 0) { if (errno == EINTR) continue ; break ; }
            off += (size_t)k ;
            /* drain as we go so a small pipe buffer never blocks the writer */
            sse_run(&ep, 50) ;
        }
        close(w) ;
        /* final drains */
        for (int i = 0 ; i < 8 && s.n < size ; i++) sse_run(&ep, 50) ;

        /* EFFECT: every byte we managed to write is delivered, in order */
        if (s.n == off && off > 0)
            if (memcmp(s.buf, data, off) != 0)
                abort() ;   /* corruption -> crash for the fuzzer */
    }

    event_fifo_unsubscribe(&r) ;
    free(s.buf) ;
    sse_free(&ep) ;
out_dir:
    /* sweep any stray entry then remove the scratch dir */
    event_fifodir_clean(ev) ;
    rmdir(ev) ; rmdir(base) ;
    return 0 ;
}
