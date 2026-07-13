/*
 * svcd_notify.c
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
#include <fcntl.h>
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/socket.h>
#include <oblibs/io.h>
#include <oblibs/fd.h>

#include <66/svc.h>

int svcd_notify(char const *eventddir, char verb, char const *name)
{
    log_flow() ;

    size_t namelen = strlen(name) ;

    char sock[strlen(eventddir) + 2 + 1] ;
    auto_strings(sock, eventddir, "/s") ;

    char frame[1 + namelen] ; // <verb><name>, no terminator ; the daemon reads to EOF
    frame[0] = verb ;
    memcpy(frame + 1, name, namelen) ;

    int fd = socketunix_create(O_CLOEXEC) ;
    if (fd < 0)
        return 0 ;

    if (socketunix_connect(fd, sock) < 0) {
        close_fd(fd) ;
        return 0 ;
    }

    if (!io_writenclose(fd, frame, 1 + namelen))
        return 0 ; // io_writenclose closed fd already ; errno set

    return 1 ;
}
