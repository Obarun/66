/*
 * tmpdir_destroy.c
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

#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/directory.h>

void tmpdir_destroy(char const *dir)
{
    log_flow() ;

    int e = errno ;

    log_trace("remove temporary directory: ", dir) ;
    if (!dir_destroy(dir))
        log_warnusys("remove temporary directory: ", dir) ;

    errno = e ;
}
