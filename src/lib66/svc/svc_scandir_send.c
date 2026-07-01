/*
 * svc_scandir_send.c
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

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/io.h>
#include <oblibs/fd.h>

#include <66/svc.h>
#include <66/constants.h>

int svc_scandir_send(char const *scandir, char const *signal)
{
    log_flow() ;

    size_t slen = strlen(scandir) ;
    char fn[slen + SS_SVSCAN_LEN + 1 + SS_CONTROL_LEN + 1] ;
    auto_strings(fn, scandir, SS_SVSCAN, "/", SS_CONTROL) ;

    log_trace("send signal: ", signal, " to scandir: ", scandir) ;

    /* O_NONBLOCK so a missing reader fails with ENXIO instead of blocking the
     * open; the running 66-scandir holds the read end open, so it succeeds. */
    int fd = io_open(fn, O_WRONLY | O_NONBLOCK) ;
    if (fd < 0) {
        if (errno == ENXIO)
            log_warnu_return(LOG_EXIT_ZERO, "control: ", scandir, ": scandir not listening") ;

        log_warnusys_return(LOG_EXIT_ZERO, "open scandir control: ", fn) ;
    }

    if (!io_unsetfl(fd, O_NONBLOCK)) {
        close_fd(fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "set blocking on scandir control: ", fn) ;
    }

    size_t len = strlen(signal) ;
    if (len && io_write(fd, (char *)signal, len) < 0) {
        close_fd(fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "write to scandir control: ", fn) ;
    }

    close_fd(fd) ;
    return 1 ;
}
