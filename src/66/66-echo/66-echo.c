/*
 * 66-echo.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 *
 * This file is a strict copy of s6-echo.c file
 * coming from skarnet software at https://skarnet.org/software/s6-portable-utils.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#include <oblibs/log.h>

#include <skalibs/sgetopt.h>
#include <skalibs/buffer.h>

#define USAGE "66-echo [ -h ] [ -n ] [ -s sep ] args..."

static inline void info_help (void)
{
  static char const *help =
"\n"
"options :\n"
"   -h: print this help\n"
"   -n: do not output a trailing newline\n"
"   -s: use as character separator\n"
;

    log_info(USAGE,"\n",help) ;
}

int main (int argc, char const *const *argv)
{
    char sep = ' ' ;
    char donl = 1 ;
    PROG = "66-echo" ;
    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, "hns:", &l) ;
            if (opt == -1) break ;
            switch (opt)
            {
                case 'h': info_help() ; return 0 ;
                case 'n': donl = 0 ; break ;
                case 's': sep = *l.arg ; break ;
                default : log_usage(USAGE) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }
    for ( ; *argv ; argv++)
        if ((buffer_puts(buffer_1small, *argv) < 0)
        || (argv[1] && (buffer_put(buffer_1small, &sep, 1) < 0)))
        goto err ;
    if (donl && (buffer_put(buffer_1small, "\n", 1) < 0)) goto err ;
    if (!buffer_flush(buffer_1small)) goto err ;
    return 0 ;
    err:
        log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
}
