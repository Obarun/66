/*
 * 66-oneshot.c
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
 * */

#include <unistd.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/obgetopt.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>

#include <skalibs/sgetopt.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>

#include <66/constants.h>
#include <66/service.h>
#include <66/resolve.h>

#define USAGE "66-oneshot [ -h ] [ -v verbosity ] up|down /run/66/state/<uid>/<service>"

static inline void help_oneshot (void)
{
    DEFAULT_MSG = 0 ;

    static char const *help =
"\n"
"options :\n"
"   -h: print this help\n"
"   -v: increase/decrease verbosity\n"
;

    log_info(USAGE,"\n",help) ;
}

int main(int argc, char const *const *argv)
{
    char upath[UINT_FMT] ;
    upath[uint_fmt(upath, SS_MAX_PATH)] = 0 ;
    char const *path = 0 ;
    char *file = "up" ;

    PROG = "66-oneshot" ;
    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">hv:", &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
            switch (opt)
            {
                case 'h' :  help_oneshot(); return 0 ;
                case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) log_usage(USAGE) ; break ;
                default :   log_usage(USAGE) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(USAGE) ;

    if (argv[0][0] == 'd')
        file = "down" ;

    if (argv[0][0] != 'd' && argv[0][0] != 'u')
        log_die(LOG_EXIT_USER, "only up or down signals are allowed") ;

    if (strlen(argv[1]) >= SS_MAX_PATH_LEN)
        log_die(LOG_EXIT_USER, "path of script file is too long -- it cannot exceed: ", upath) ;

    else if (argv[1][0] != '/')
        log_die(LOG_EXIT_USER, "path of script file must be absolute") ;

    if (access(argv[1], F_OK) < 0) {

        if (file[0] == 'd')
            /* really nothing to do here */
            return 0 ;

        else
            log_dieusys(LOG_EXIT_SYS, "find: ", argv[1]) ;
    }

    path = argv[1] ;
    size_t len = strlen(path) ;

    char script[len + 1 + strlen(file) + 1] ;
    auto_strings(script, path, "/", file) ;

    if (file[0] == 'd')
        /** do not crash if the down file doesn't exist
         * considere it down instead*/
         if (access(script, F_OK) < 0)
            _exit(0) ;

    /**
     * be paranoid and avoid to crash just for a
     * not executable script
     * Cannot be possible to a read-only filesystem
    if (chmod(script, 0755) < 0)
        log_dieusys(LOG_EXIT_SYS,"chmod: ", script) ;
    */

    char const *newargv[3] ;
    unsigned int m = 0 ;
    newargv[m++] = script ;
    newargv[m++] = file ;
    newargv[m] = 0 ;

    if (chdir(path) < 0)
        log_dieusys(LOG_EXIT_SYS, "chdir to: ", path) ;

    execve(script, (char *const *)newargv, (char *const *)environ) ;

    log_dieusys(errno == ENOENT ? 127 : 126, "exec: ", script) ;
}
