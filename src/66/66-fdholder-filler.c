/*
 * 66-fdholder-filler.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 *
 * This file is a modified copy of s6-rc-fdholder-filler.c file to suit the needs of 66,
 * coming from skarnet software at https://skarnet.org/software/s6-rc with an ISC LICENSE.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 *
 * This program keep the same behavior of the original file. It's expected to run behind
 * an s6-ipcclient connection.
 * */

#include <unistd.h> //access
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/files.h>

#include <skalibs/sgetopt.h>
#include <skalibs/types.h>
#include <skalibs/tai.h>

#include <66/constants.h>

#include <s6/fdholder.h>

#define USAGE "66-fdholder-filler [ -h ] [ -v verbosity ] [ -1 ] [ -t timeout ]"

static inline void info_help (void)
{
    DEFAULT_MSG = 0 ;

    static char const *help =
"\n"
"options :\n"
"   -h: print this help\n"
"   -v: increase/decrease verbosity\n"
"\n"
;

    log_info(USAGE,"\n",help) ;
}
#define N 4096

static inline unsigned int class (char c)
{
    switch (c) {

        case 0 : return 0 ;
        case '\n' : return 1 ;
        case '#' : return 2 ;
        case ' ' :
        case '\r' :
        case '\t' : return 3 ;
        default : return 4 ;
    }
}

static inline unsigned int parse_servicenames (char *s, unsigned int *indices)
{
    static unsigned char const table[3][5] = {

        { 3, 0, 1, 0, 6 },
        { 3, 0, 1, 1, 1 },
        { 3, 8, 2, 2, 2 }
    } ;

    unsigned int pos = 0, n = 0, state = 0 ;

    for (; state < 3 ; pos++) {

        unsigned char c = table[state][class(s[pos])] ;
        state = c & 3 ;

        if (c & 4)
            indices[n++] = pos ;

        if (c & 8)
            s[pos] = 0 ;
    }
    return n ;
}

int main(int argc, char const *const *argv)
{
    int notif = 0 ;
    unsigned int n = 0, indices[N] ;
    char buf[N<<1] ;

    s6_fdholder_t a = S6_FDHOLDER_ZERO ;

    tain deadline ;

    PROG = "66-fdholder-filler" ;
    {
        unsigned int t = 0 ;
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc,argv, "hv:t:1", &l) ;
            if (opt == -1) break ;

            switch (opt)
            {
                case 'h' :  info_help(); return 0 ;
                case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) log_usage(USAGE) ; break ;
                case '1' :  notif = 1 ; break ;
                case 't' :  if (!uint0_scan(l.arg, &t)) log_usage(USAGE) ; break ;
                default  :  log_usage(USAGE) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
        if (t) tain_from_millisecs(&deadline, t) ;
        else deadline = tain_infinite_relative ;
    }

    {
        size_t r = allread(0, buf, N<<1) ;
        if (r >= N<<1)
            log_die(LOG_EXIT_USER, "file ", argv[0], " is too big") ;
        buf[r] = 0 ;
    }

    n = parse_servicenames(buf, indices) ;

    if (n) {

        tain offset = { .sec = TAI_ZERO } ;
        int p[2] ;
        unsigned int i = 0 ;
        s6_fdholder_fd_t dump[n<<1] ;

        close(0) ;

        s6_fdholder_init(&a, 6) ;

        tain_now_set_stopwatch_g() ;
        tain_add_g(&deadline, &deadline) ;

        for (; i < n ; i++) {

            size_t len = strlen(buf + indices[i]) ;

            if (len > S6_FDHOLDER_ID_SIZE) {
                errno = ENAMETOOLONG ;
                log_dieusys(LOG_EXIT_SYS, "create identifier for ", buf + indices[i]) ;
            }

            if (pipe(p) < 0)
                log_dieusys(LOG_EXIT_SYS, "create pipe") ;

            dump[i<<1].fd = p[0] ;
            tain_add_g(&dump[i<<1].limit, &tain_infinite_relative) ;
            offset.nano = i << 1 ;
            tain_add(&dump[i<<1].limit, &dump[i<<1].limit, &offset) ;
            memcpy(dump[i<<1].id, buf + indices[i], len + 1) ;
            dump[(i<<1)+1].fd = p[1] ;
            offset.nano = 1 ;
            tain_add(&dump[(i<<1)+1].limit, &dump[i<<1].limit, &offset) ;
            memcpy(dump[(i<<1)+1].id, buf + indices[i], len + 1) ;
            dump[(i<<1)+1].id[8] = 'w' ;
        }

        if (!s6_fdholder_setdump_g(&a, dump, n << 1, &deadline))
            log_dieusys(LOG_EXIT_SYS, "transfer pipes") ;
    }

    if (notif)
        if (write(1, "\n", 1) < 0)
            log_dieusys(LOG_EXIT_SYS, "notify failed") ;

    return 0 ;
}
