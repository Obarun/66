/*
 * svc_control_send.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
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

int svc_control_send(char const *scandir, char const *bytes, size_t len)
{
    log_flow() ;

    size_t slen = strlen(scandir) ;
    char fn[slen + sizeof("/supervise/control")] ;
    auto_strings(fn, scandir, "/supervise/control") ;

    int fd = io_open(fn, O_WRONLY | O_NONBLOCK) ;
    if (fd < 0) {
        if (errno == ENXIO)
            log_warnu_return(LOG_EXIT_ZERO, "control: ", scandir, ": supervisor not listening") ;
        log_warnusys_return(LOG_EXIT_ZERO, "open control fifo: ", fn) ;
    }

    if (!io_unsetfl(fd, O_NONBLOCK)) {
        close_fd(fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "set blocking on control fifo: ", fn) ;
    }

    if (len && io_write(fd, (char *)bytes, len) < 0) {
        close_fd(fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "write to control fifo: ", fn) ;
    }

    close_fd(fd) ;
    return 1 ;
}
