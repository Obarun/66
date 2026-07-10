/*
 * event_frame.c
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

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <oblibs/types.h>
#include <oblibs/clock.h>

#include <66/event.h>

#define EVENT_PAYLOAD_TRANSITION 11  // state + result + who + code(4) + pid(4)
#define EVENT_PAYLOAD_SIGNAL      2  // signo + who
#define EVENT_PAYLOAD_LIFECYCLE   1  // phase

static size_t pack_header(char *out, uint8_t kind, uint8_t flags, uint32_t payload_len)
{
    out[0] = (char)EVENT_VERSION ;
    out[1] = (char)kind ;
    out[2] = (char)flags ;
    out[3] = 0 ;
    u32_pack_big(out + 4, payload_len) ;
    return EVENT_HDR_LEN ;
}

size_t event_frame_pack_transition(char *out, uint8_t state, uint8_t result, uint8_t who, uint32_t code, uint32_t pid, struct timespec const *stamp, uint8_t flags)
{
    uint32_t plen = CLOCK_PACK + EVENT_PAYLOAD_TRANSITION ;
    pack_header(out, EVENT_KIND_TRANSITION, flags, plen) ;
    clock_pack(out + EVENT_HDR_LEN, stamp) ;
    char *p = out + EVENT_HDR_LEN + CLOCK_PACK ;
    p[0] = (char)state ;
    p[1] = (char)result ;
    p[2] = (char)who ;
    u32_pack_big(p + 3, code) ;
    u32_pack_big(p + 7, pid) ;
    return EVENT_HDR_LEN + plen ;
}

size_t event_frame_pack_signal(char *out, uint8_t signo, uint8_t who, struct timespec const *stamp)
{
    uint32_t plen = CLOCK_PACK + EVENT_PAYLOAD_SIGNAL ;
    pack_header(out, EVENT_KIND_SIGNAL, 0, plen) ;
    clock_pack(out + EVENT_HDR_LEN, stamp) ;
    char *p = out + EVENT_HDR_LEN + CLOCK_PACK ;
    p[0] = (char)signo ;
    p[1] = (char)who ;
    return EVENT_HDR_LEN + plen ;
}

size_t event_frame_pack_lifecycle(char *out, uint8_t phase, struct timespec const *stamp)
{
    uint32_t plen = CLOCK_PACK + EVENT_PAYLOAD_LIFECYCLE ;
    pack_header(out, EVENT_KIND_LIFECYCLE, 0, plen) ;
    clock_pack(out + EVENT_HDR_LEN, stamp) ;
    (out + EVENT_HDR_LEN + CLOCK_PACK)[0] = (char)phase ;
    return EVENT_HDR_LEN + plen ;
}

static int event_frame_unpack(unsigned char const *b, uint32_t plen, event_frame_t *f)
{
    f->version = b[0] ;
    f->kind = b[1] ;
    f->flags = b[2] ;

    if (plen < CLOCK_PACK)
        return 0 ;

    clock_unpack((char const *)b + EVENT_HDR_LEN, &f->stamp) ;

    unsigned char const *p = b + EVENT_HDR_LEN + CLOCK_PACK ;
    uint32_t rest = plen - CLOCK_PACK ;

    switch (f->kind) {

        case EVENT_KIND_TRANSITION :

            if (rest < EVENT_PAYLOAD_TRANSITION)
                return 0 ;

            f->state = p[0] ;
            f->result = p[1] ;
            f->who = p[2] ;
            u32_unpack_big((char const *)p + 3, &f->code) ;
            u32_unpack_big((char const *)p + 7, &f->pid) ;
            return 1 ;

        case EVENT_KIND_SIGNAL :

            if (rest < EVENT_PAYLOAD_SIGNAL)
                return 0 ;
            f->signo = p[0] ;
            f->who = p[1] ;
            return 1 ;

        case EVENT_KIND_LIFECYCLE :

            if (rest < EVENT_PAYLOAD_LIFECYCLE)
                return 0 ;
            f->phase = p[0] ;
            return 1 ;

        default :
            return 0 ;
    }
}

void event_aggregate(event_aggregator_t *d, char const *buf, size_t len, event_frame_cb_t *cb, void *data)
{
    for (size_t i = 0 ; i < len ; i++) {

        d->buf[d->len++] = (unsigned char)buf[i] ;

        /* Normalize the accumulated buffer: drop leading bytes until it starts on
         * a plausible header, and emit every complete frame. Byte-by-byte feeding
         * keeps `d->buf` bounded (a frame is consumed the moment it completes, so
         * len never exceeds EVENT_FRAME_MAX). */
        for (;;) {

            if (!d->len)
                break ;

            if (d->buf[0] != EVENT_VERSION) { // stray byte / false header start
                memmove(d->buf, d->buf + 1, --d->len) ;
                continue ;
            }

            if (d->len < EVENT_HDR_LEN)
                break ; // header incomplete, wait for more

            uint32_t plen ;
            u32_unpack_big((char const *)d->buf + 4, &plen) ;

            if (plen > EVENT_FRAME_MAX - EVENT_HDR_LEN) { // corrupt length: resync
                memmove(d->buf, d->buf + 1, --d->len) ;
                continue ;
            }

            if (d->len < EVENT_HDR_LEN + plen)
                break ; // payload incomplete, wait for more

            event_frame_t f = { 0 } ;
            if (event_frame_unpack(d->buf, plen, &f))
                cb(&f, data) ;

            size_t flen = EVENT_HDR_LEN + plen ;
            memmove(d->buf, d->buf + flen, d->len - flen) ;
            d->len -= flen ;
        }
    }
}
