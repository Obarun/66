/*
 * 66-nuke.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 *
 * This file is a modified copy of s6-linux-init-nuke.c file
 * coming from skarnet software at https://skarnet.org/software/s6-linux-init.
 * All credits goes to Laurent Bercot <ska-remove-this-if-you-are-not-a-bot@skarnet.org>
 * */

#include <signal.h>
#include <unistd.h>

#include <oblibs/log.h>

#include <skalibs/sgetopt.h>

#define USAGE "66-nuke"

static inline void info_help (void)
{
    DEFAULT_MSG = 0 ;

    static char const *help =
"\n"
"options :\n"
"   -h: print this help\n"
;

    log_info(USAGE,"\n",help) ;
}

int main (int argc, char const *const *argv)
{

    {
        subgetopt_t l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc,argv, "h", &l) ;

            if (opt == -1) break ;
            switch (opt)
            {
                case 'h' :

                    info_help();
                    return 0 ;

                default :

                    log_usage(USAGE) ;
            }
        }
    }

    if (getuid())
        log_die(LOG_EXIT_SYS,"You must be root to use this program") ;

    return kill(-1, SIGKILL) < 0 ;
}
