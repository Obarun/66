/*
 * svc_scandir_ok.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <errno.h>
#include <fcntl.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/fd.h>
#include <oblibs/io.h>

#include <66/svc.h>
#include <66/constants.h>

/** Probe whether a scandir is running by opening its control fifo write-only:
 * an absent reader (ENXIO) or a missing fifo (ENOENT) means it is not. */
int svc_scandir_ok (char const *dir)
{
    log_flow() ;

    size_t dirlen = strlen(dir) ;
    int fd ;
    char fn[dirlen + SS_SVSCAN_LEN + sizeof("/control")] ;

    auto_strings(fn, dir, SS_SVSCAN, "/control") ;

    fd = io_open(fn, O_WRONLY|O_NONBLOCK) ;
    if (fd < 0)
    {
        if ((errno == ENXIO) || (errno == ENOENT)) return 0 ;
        else return -1 ;
    }
    close_fd(fd) ;

    return 1 ;
}
