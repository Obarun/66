/*
 * write_execute_scripts.c
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
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/files.h>

#ifndef FAKELEN
#define FAKELEN strlen(run)
#endif

void write_execute_scripts(char const *file, char const *contents, char const *dst)
{

    log_flow() ;

    char write[strlen(dst) + 1 + strlen(file) + 1] ;

    auto_strings(write, dst, "/", file) ;

    char run[strlen(contents) + 1] ;
    auto_strings(run, contents) ;

    if (!file_write_unsafe(dst, file, run, FAKELEN))
        log_dieusys(LOG_EXIT_SYS, "write: ", dst, "/", file) ;

    if (chmod(write, 0755) < 0)
        log_dieusys(LOG_EXIT_SYS, "chmod", write) ;
}
