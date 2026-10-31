/*
 * execute_parse.c
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
 * The parser below is adapted from execline's el_parse:
 * https://git.skarnet.org/cgi-bin/cgit.cgi/execline/tree/src/libexecline/el_parse.c
 * All credits go to Laurent Bercot, author of execline. That code is
 * distributed under the following license:
 *
 * Copyright (c) 2011-2026 Laurent Bercot <ska-skaware@skarnet.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/strbuf.h>
#include <oblibs/types.h>

#include "execute.h"

int execute_parse(strbuf *sa, char const *s, size_t len)
{
    log_flow() ;

    static unsigned char const class[] = "`aaaaaaaaadaaaaaaaaaaaaaaaaaaaaaafcbffffffffffffjhhhhhhhiifffffffmmmmmmfffffffffffffffffffffeffffggmmmgfffffffkfffkfkfkflffnfoffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff" ;
    static uint16_t const table[17][16] = {
        { 0x0011, 0x0000, 0x0001, 0x2008, 0x0000, 0x0005, 0x2404, 0x2404, 0x2404, 0x2404, 0x2404, 0x2404, 0x2404, 0x2404, 0x2402, 0x2403 },
        { 0x0011, 0x0001, 0x0001, 0x0001, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001 },
        { 0x0012, 0x1000, 0x0404, 0x0008, 0x1000, 0x0005, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404 },
        { 0x0911, 0x0900, 0x0404, 0x0008, 0x0900, 0x0005, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404 },
        { 0x0111, 0x0100, 0x0404, 0x0008, 0x0100, 0x0006, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404 },
        { 0x0011, 0x2404, 0x2404, 0x2404, 0x0000, 0x2404, 0x2404, 0x2404, 0x2404, 0x2404, 0x2404, 0x2404, 0x2404, 0x2404, 0x2404, 0x2404 },
        { 0x0012, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404 },
        { 0x0012, 0x0408, 0x0408, 0x0408, 0x0008, 0x0408, 0x0408, 0x0208, 0x448c, 0x448c, 0x0089, 0x0208, 0x0408, 0x0408, 0x0408, 0x0408 },
        { 0x0012, 0x0408, 0x0408, 0x0004, 0x0408, 0x0007, 0x0408, 0x0408, 0x0408, 0x0408, 0x0408, 0x0408, 0x0408, 0x0408, 0x0408, 0x0408 },
        { 0x0012, 0x0012, 0x0012, 0x0012, 0x0012, 0x0012, 0x0012, 0x0012, 0x440a, 0x0012, 0x440a, 0x0012, 0x008e, 0x0012, 0x0012, 0x0012 },
        { 0x8111, 0x8408, 0x8408, 0x8004, 0x8408, 0x8007, 0x8408, 0x8408, 0x040b, 0x8408, 0x040b, 0x8408, 0x8408, 0x8408, 0x8408, 0x8408 },
        { 0x8111, 0x8408, 0x8408, 0x8004, 0x8408, 0x8007, 0x8408, 0x8408, 0x0410, 0x8408, 0x0410, 0x8408, 0x8408, 0x8408, 0x8408, 0x8408 },
        { 0x8111, 0x8408, 0x8408, 0x8004, 0x8408, 0x8007, 0x8408, 0x8408, 0x040d, 0x040d, 0x040d, 0x8408, 0x8408, 0x8408, 0x8408, 0x8408 },
        { 0x8111, 0x8408, 0x8408, 0x8004, 0x8408, 0x8007, 0x8408, 0x8408, 0x0410, 0x0410, 0x0410, 0x8408, 0x8408, 0x8408, 0x8408, 0x8408 },
        { 0x0012, 0x0012, 0x0012, 0x0012, 0x0012, 0x0012, 0x0012, 0x440f, 0x440f, 0x440f, 0x440f, 0x0012, 0x0012, 0x440f, 0x0012, 0x0012 },
        { 0x8111, 0x8408, 0x8408, 0x8004, 0x8408, 0x8007, 0x8408, 0x0410, 0x0410, 0x0410, 0x0410, 0x8408, 0x8408, 0x0410, 0x8408, 0x8408 },
        { 0x8111, 0x8408, 0x8408, 0x8004, 0x8408, 0x8007, 0x8408, 0x8408, 0x8408, 0x8408, 0x8408, 0x8408, 0x8408, 0x8408, 0x8408, 0x8408 }
    } ;
    static char const esc[] = "abtnvfr" ;

    size_t const base = sa->len ;
    size_t mark = 0, pos = 0 ;
    unsigned int n = 0, blevel = 0 ;
    uint8_t state = 0, numbase = 10 ;
    int r = -1 ;

    while (state < 0x11) {

        unsigned char cur = pos < len ? (unsigned char)s[pos++] : 0 ;
        uint16_t c = table[state][class[cur] - '`'] ;
        state = c & 0x1F ;

        if (c & 0x8000U) {

            uint64_t u ;

            if (!strbuf_terminate(sa))
                goto err ;

            sa->len = mark ;
            u64_scan_base(sa->s + sa->len, &u, numbase) ;
            if (!u || u > 0xff) {
                r = -2 ;
                goto err ;
            }

            sa->s[sa->len++] = (char)(unsigned char)u ;
        }

        if (c & 0x4000U)
            mark = sa->len ;

        if (c & 0x2000U) {

            unsigned int i = blevel ;

            if (!strbuf_reserve(sa, sa->len + i))
                goto err ;

            while (i--)
                sa->s[sa->len++] = ' ' ;
        }

        if (c & 0x1000U)
            sa->len -= ++blevel ;

        if (c & 0x0800U) {

            if (!blevel--) {
                r = -4 ;
                goto err ;
            }

            sa->len -= 2 ;
        }

        if (c & 0x0400U) {
            if (!strbuf_catb(sa, (char *)&cur, 1))
                goto err ;
        }

        if (c & 0x0200U) {

            char const *p = memchr(esc, cur, 7) ;
            char x = 7 + (p ? (char)(p - esc) : 7) ;

            if (!strbuf_catb(sa, &x, 1))
                goto err ;
        }

        if (c & 0x0100U) {

            if (n++ >= INT_MAX) {
                errno = E2BIG ;
                goto err ;
            }

            if (!strbuf_terminate(sa))
                goto err ;
        }

        if (c & 0x0080U) {

            switch (cur) {

                case 'x' :
                    numbase = 16 ;
                    break ;
                case '0' :
                    numbase = 8 ;
                    break ;
                default :
                    numbase = 10 ;
            }
        }
    }

    if (state > 0x11) {
        r = -2 ;
        goto err ;
    }

    if (blevel) {
        r = -3 ;
        goto err ;
    }

    return n ;

    err:
        sa->len = base ;
        return r ;
}
