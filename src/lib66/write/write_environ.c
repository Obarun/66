/*
 * write_environ.c
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
 */

#include <string.h>

#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/types.h>

void write_environ(char const *name, char const *contents, char const *dst)
{
    log_flow() ;

    int r ;
    size_t len = strlen(contents) ;

    r = scan_mode(dst,S_IFDIR) ;
    if (r < 0)
        log_die(LOG_EXIT_SYS, "conflicting format of the environment directory: ", dst) ;
    else if (!r)
        log_dieusys(LOG_EXIT_SYS, "find environment directory: ", dst) ;

    if (!file_write_unsafe(dst, name, contents, len))
        log_dieusys(LOG_EXIT_SYS, "create file: ", dst, "/", name) ;
}
