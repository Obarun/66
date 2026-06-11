/*
 * 66-fdholder-filler.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <oblibs/opt.h>
#include <oblibs/strbuf.h>
#include <oblibs/files.h>
#include <oblibs/types.h>
#include <oblibs/stream.h>

#include <skalibs/tai.h>
#include <skalibs/genalloc.h>

#include <66/constants.h>

#include <s6/fdholder.h>

static opt_t const opts[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",    .arg = OPT_NONE,                                .help = "print this help" },
    { .id = 'v',         .shortname = 'v', .longname = "verbose", .arg = OPT_REQUIRED, .argname = "verbosity",    .help = "increase/decrease verbosity" },
    { .id = '1',         .shortname = '1', .longname = "notify",  .arg = OPT_NONE,                                .help = "notify readiness on file descriptor 1" },
    { .id = 't',         .shortname = 't', .longname = "timeout", .arg = OPT_REQUIRED, .argname = "milliseconds", .help = "timeout to apply" },
} ;

static opt_cmd_t const cmd = {
    .name = "66-fdholder-filler",
    .opts = opts,
    .nopts = OPT_COUNT(opts),
} ;

static inline uint8_t class (char c)
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

static inline char cnext (void)
{
  char c ;
  ssize_t r = istream_get(istream_0, &c, 1) ;
  if (r == -1) log_dieusys(LOG_EXIT_SYS, "read from stdin") ;
  return r ? c : 0 ;
}


static inline void parse_servicenames (strbuf *sa, genalloc *g)
{
    static uint8_t const table[3][5] = {

        { 3, 0, 1, 0, 6 },
        { 3, 0, 1, 1, 1 },
        { 3, 8, 2, 2, 2 }
    } ;

    uint8_t state = 0 ;

    while (state < 3) {

        char cur = cnext() ;
        uint8_t c = table[state][class(cur)] ;
        state = c & 3 ;

        if (c & 4)
            if (!genalloc_append(size_t, g, &sa->len))
                log_die_nomem("strbuf") ;

        if (c & 8) {
            if (!strbuf_terminate(sa))
                log_die_nomem("strbuf") ;

        } else {
            if (!strbuf_catb(sa, &cur, 1))
                log_die_nomem("strbuf") ;
        }
    }
}

int main(int argc, char const *const *argv)
{
    s6_fdholder_t a = S6_FDHOLDER_ZERO ;
    strbuf sa = STRBUF_ZERO ;
    genalloc ga = GENALLOC_ZERO ; // size_t
    size_t n ;
    size_t const *indices ;
    tain deadline ;
    int notif = 0 ;

    PROG = "66-fdholder-filler" ;
    {
        unsigned int t = 0 ;
        opt_scan_t st = OPT_SCAN_ZERO ;

        for (;;)
        {
            int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
            if (o == OPT_END) break ;

            switch (o) {
                case OPT_ID_HELP : return opt_emit_help(&cmd) ;
                case 'v' :
                    if (!u32_scan_strict(st.arg, &VERBOSITY))
                        return opt_emit_usage(&cmd) ;
                    break ;
                case '1' : notif = 1 ; break ;
                case 't' :
                    if (!u32_scan_strict(st.arg, &t))
                        return opt_emit_usage(&cmd) ;
                    break ;
                default : return opt_emit_error(&cmd, o, &st) ;
            }
        }
        argc -= st.ind ; argv += st.ind ;
        if (t) tain_from_millisecs(&deadline, t) ;
        else deadline = tain_infinite_relative ;
    }

    parse_servicenames(&sa, &ga) ;
    n = genalloc_len(size_t, &ga) ;
    indices = genalloc_s(size_t, &ga) ;

    if (n) {

        tain offset = { .sec = TAI_ZERO } ;
        int p[2] ;
        size_t i = 0 ;
        s6_fdholder_fd_t dump[n<<1] ;

        close(0) ;

        s6_fdholder_init(&a, 6) ;

        tain_now_set_stopwatch_g() ;
        tain_add_g(&deadline, &deadline) ;

        for (; i < n ; i++) {

            size_t len = strlen(sa.s + indices[i]) ;

            if (len > S6_FDHOLDER_ID_SIZE) {
                errno = ENAMETOOLONG ;
                log_dieusys(LOG_EXIT_SYS, "create identifier for ", sa.s + indices[i]) ;
            }

            if (pipe(p) < 0)
                log_dieusys(LOG_EXIT_SYS, "create pipe") ;

            dump[i<<1].fd = p[0] ;
            tain_add_g(&dump[i<<1].limit, &tain_infinite_relative) ;
            offset.nano = i << 1 ;
            tain_add(&dump[i<<1].limit, &dump[i<<1].limit, &offset) ;
            memcpy(dump[i<<1].id, sa.s + indices[i], len + 1) ;
            dump[(i<<1)+1].fd = p[1] ;
            offset.nano = 1 ;
            tain_add(&dump[(i<<1)+1].limit, &dump[i<<1].limit, &offset) ;
            memcpy(dump[(i<<1)+1].id, sa.s + indices[i], len + 1) ;
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
