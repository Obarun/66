/*
 * 66-echo.c
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
 * */

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/stream.h>

static opt_t const opts[] = {
    { .id = OPT_ID_HELP,    .shortname = 'h', .longname = "help",       .arg = OPT_NONE,                             .help = "print this help" },
    { .id = 'n',            .shortname = 'n', .longname = "no-newline", .arg = OPT_NONE,                             .help = "do not output a trailing newline" },
    { .id = 's',            .shortname = 's', .longname = "separator",  .arg = OPT_REQUIRED, .argname = "separator", .help = "use as character separator" },
} ;

static opt_cmd_t const cmd = {
    .name = "66-echo",
    .operands = "args...",
    .opts = opts,
    .nopts = OPT_COUNT(opts),
} ;

int main (int argc, char const *const *argv)
{
    char sep = ' ' ;
    char donl = 1 ;
    PROG = "66-echo" ;
    {
        opt_scan_t st = OPT_SCAN_ZERO ;

        for (;;)
        {
            int o = opt_scan(argc, argv, opts, OPT_COUNT(opts), &st) ;
            if (o == OPT_END) break ;
            switch (o) {
                case OPT_ID_HELP: return opt_emit_help(&cmd) ;
                case 'n': donl = 0 ; break ;
                case 's': sep = *st.arg ; break ;
                default : return opt_emit_error(&cmd, o, &st) ;
            }
        }
        argc -= st.ind ; argv += st.ind ;
    }
    for ( ; *argv ; argv++)
        if ((!ostream_puts(ostream_1, *argv))
        || (argv[1] && (!ostream_put(ostream_1, &sep, 1))))
        goto err ;
    if (donl && (!ostream_put(ostream_1, "\n", 1))) goto err ;
    if (!ostream_flush(ostream_1)) goto err ;
    return 0 ;
    err:
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}
