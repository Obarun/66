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

#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/io.h>
#include <oblibs/fd.h>

#include <66/svc.h>
#include <66/constants.h>

/** Send @nops control ops to the supervisor as a fixed <op><who> pair each, in a
 * single write so a sequence stays atomic (2*nops <= PIPE_BUF). @who is the
 * provenance category (status_who_e) latched by the op that flips wantup. */
int svc_control_send(char const *scandir, char const *ops, size_t nops, uint8_t who)
{
    log_flow() ;

    size_t slen = strlen(scandir) ;
    char fn[slen + SS_SUPERVISEDIR_LEN + 1 + SS_CONTROL_LEN + 1] ;
    auto_strings(fn, scandir, SS_SUPERVISEDIR, "/", SS_CONTROL) ;

    char pairs[nops * 2] ;
    for (size_t i = 0 ; i < nops ; i++) {
        pairs[i * 2] = ops[i] ;
        pairs[i * 2 + 1] = (char)who ;
    }

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

    if (nops && io_write(fd, pairs, nops * 2) < 0) {
        close_fd(fd) ;
        log_warnusys_return(LOG_EXIT_ZERO, "write to control fifo: ", fn) ;
    }

    close_fd(fd) ;
    return 1 ;
}
