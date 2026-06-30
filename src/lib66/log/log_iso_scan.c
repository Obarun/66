/*
 * log_iso_scan.c
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

#include <time.h>
#include <stddef.h>
#include <stdint.h>

#include <oblibs/clock.h>

#include <66/log.h>

#define C_DIGIT 0
#define C_DASH  1
#define C_COLON 2
#define C_DOT   3
#define C_SEP   4
#define C_OTHER 5
#define NCLASS  6

enum {
    S_Y1 = 0,
    S_Y2,
    S_Y3,
    S_Y4,
    S_DASH1,
    S_MO1,
    S_MO2,
    S_DASH2,
    S_D1,
    S_D2,
    S_DATE_OK,
    S_H1,
    S_H2,
    S_COL1,
    S_MI1,
    S_MI2,
    S_COL2,
    S_S1,
    S_S2,
    S_DT_OK,
    S_FR1,
    S_FR,
    S_ERR,
    NSTATES
} ;

#define F_Y 0
#define F_MO 1
#define F_D 2
#define F_H 3
#define F_MI 4
#define F_S 5
#define F_FR 6

static uint8_t classify(char c)
{
    if (c >= '0' && c <= '9') return C_DIGIT ;
    switch (c) {
        case '-' : return C_DASH ;
        case ':' : return C_COLON ;
        case '.' : return C_DOT ;
        case ' ' : case 'T' : case 't' : return C_SEP ;
        default : return C_OTHER ;
    }
}

static uint8_t const trans[NSTATES][NCLASS] = {
    //              DIGIT         DASH        COLON         DOT           SEP           OTHER
    [S_Y1]      = { S_Y2,     S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_Y2]      = { S_Y3,     S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_Y3]      = { S_Y4,     S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_Y4]      = { S_DASH1,  S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_DASH1]   = { S_ERR,    S_MO1,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_MO1]     = { S_MO2,    S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_MO2]     = { S_DASH2,  S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_DASH2]   = { S_ERR,    S_D1,   S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_D1]      = { S_D2,     S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_D2]      = { S_DATE_OK,S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_DATE_OK] = { S_ERR,    S_ERR,  S_ERR,    S_ERR,    S_H1,     S_ERR },
    [S_H1]      = { S_H2,     S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_H2]      = { S_COL1,   S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_COL1]    = { S_ERR,    S_ERR,  S_MI1,    S_ERR,    S_ERR,    S_ERR },
    [S_MI1]     = { S_MI2,    S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_MI2]     = { S_COL2,   S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_COL2]    = { S_ERR,    S_ERR,  S_S1,     S_ERR,    S_ERR,    S_ERR },
    [S_S1]      = { S_S2,     S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_S2]      = { S_DT_OK,  S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_DT_OK]   = { S_ERR,    S_ERR,  S_ERR,    S_FR1,    S_ERR,    S_ERR },
    [S_FR1]     = { S_FR,     S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_FR]      = { S_FR,     S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
    [S_ERR]     = { S_ERR,    S_ERR,  S_ERR,    S_ERR,    S_ERR,    S_ERR },
} ;

static uint8_t const field_of[NSTATES] = {
    [S_Y1] = F_Y,
    [S_Y2] = F_Y,
    [S_Y3] = F_Y,
    [S_Y4] = F_Y,
    [S_MO1] = F_MO,
    [S_MO2] = F_MO,
    [S_D1] = F_D,
    [S_D2] = F_D,
    [S_H1] = F_H,
    [S_H2] = F_H,
    [S_MI1] = F_MI,
    [S_MI2] = F_MI,
    [S_S1] = F_S,
    [S_S2] = F_S,
    [S_FR1] = F_FR,
    [S_FR] = F_FR,
} ;

static uint8_t const accept[NSTATES] = {
    [S_DATE_OK] = 1, [S_DT_OK] = 2, [S_FR] = 2,
} ;

int log_iso_scan(char const *s, size_t len, struct timespec *ts, size_t *end)
{
    int acc[6] = { 0 } ;
    long frac = 0 ;
    unsigned int fracdigits = 0 ;
    uint8_t state = S_Y1, got = 0 ;
    size_t i = 0, last_end = 0 ;

    while (i < len) {

        uint8_t cls = classify(s[i]) ;
        uint8_t next = trans[state][cls] ;
        if (next == S_ERR)
            break ;

        if (cls == C_DIGIT) {
            uint8_t f = field_of[state] ;
            if (f == F_FR) {
                if (fracdigits < 9) {
                    frac = frac * 10 + (s[i] - '0') ;
                    fracdigits++ ;
                }
            } else {
                acc[f] = acc[f] * 10 + (s[i] - '0') ;
            }
        }

        state = next ;
        i++ ;

        if (accept[state]) {
            last_end = i ;
            got = accept[state] ;
        }
    }

    if (!got)
        return 0 ;

    /* a date-only match (got==1) may have accumulated H/MI/S from a longer
     * datetime attempt the DFA ultimately rejected; those belong to no accepted
     * match, so a bare date is local midnight -- never validate/use them here */
    int h = got == 2 ? acc[F_H] : 0 ;
    int mi = got == 2 ? acc[F_MI] : 0 ;
    int se = got == 2 ? acc[F_S] : 0 ;

    /* explicit range check: clock_from_localtm (mktime) would otherwise
     * normalise out-of-range fields (month 13 -> next January) */
    if (acc[F_MO] < 1 || acc[F_MO] > 12 || acc[F_D] < 1 || acc[F_D] > 31 ||
        h > 23 || mi > 59 || se > 60)
        return 0 ;

    struct tm tm = { 0 } ;
    tm.tm_year = acc[F_Y] - 1900 ;
    tm.tm_mon = acc[F_MO] - 1 ;
    tm.tm_mday = acc[F_D] ;
    tm.tm_hour = h ;
    tm.tm_min = mi ;
    tm.tm_sec = se ;
    tm.tm_isdst = -1 ;

    if (!clock_from_localtm(ts, &tm))
        return 0 ;

    while (fracdigits < 9) {
        frac *= 10 ;
        fracdigits++ ;
    }

    ts->tv_nsec = frac ;
    *end = last_end ;

    return 1 ;
}
