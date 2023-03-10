/*
 * regex_get_replace.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <sys/types.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>

#include <66/module.h>

void get_list(stralloc *list, char const *src, char const *name, mode_t mode)
{
    log_flow() ;

    list->len = 0 ;
    char const *exclude[2] = { SS_MODULE_CONFIG_DIR + 1, 0 } ;

    char t[strlen(src) + 1] ;

    auto_strings(t, src) ;

    if (!sastr_dir_get_recursive(list, t, exclude, mode, 1))
        log_dieusys(LOG_EXIT_SYS,"get file(s) of module: ", name) ;

}
