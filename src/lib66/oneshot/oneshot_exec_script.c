/*
 * oneshot_exec_script.c
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

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/environ.h>

#include <66/oneshot.h>

void oneshot_exec_script(char const *servicedir, uint8_t down)
{
    char const *file = down ? "finish" : "run" ;
    size_t len = strlen(servicedir) ;

    char script[len + 1 + 6 + 1] ;
    auto_strings(script, servicedir, "/", file) ;

    if (chdir(servicedir) < 0)
        log_dieusys(LOG_EXIT_SYS, "chdir to: ", servicedir) ;

    char const *newargv[3] ;
    unsigned int m = 0 ;
    newargv[m++] = script ;
    newargv[m++] = file ;
    newargv[m] = 0 ;

    execve(script, (char *const *)newargv, (char *const *)environ) ;

    log_dieusys(errno == ENOENT ? 127 : 126, "exec: ", script) ;
}
