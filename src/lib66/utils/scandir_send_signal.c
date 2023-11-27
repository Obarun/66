/*
 * scandir_send_signal.c
 *
 * Copyright (c) 2018-2023 Eric Vidal <eric@obarun.org>
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

#include <66/utils.h>

#include <s6/supervise.h>

int scandir_send_signal(char const *scandir,char const *signal)
{
    log_flow() ;

    size_t idlen = strlen(signal) ;
    char data[idlen + 1] ;
    size_t datalen = 0 ;

    for (; datalen < idlen ; datalen++)
        data[datalen] = signal[datalen] ;

    log_trace("send signal: ", signal, " to scandir: ", scandir) ;
    switch (s6_svc_writectl(scandir, S6_SVSCAN_CTLDIR, data, datalen))
    {
        case -1: log_warnusys("control: ", scandir) ;
                return 0 ;
        case -2: log_warnsys("something is wrong with the ", scandir, "/" S6_SVSCAN_CTLDIR " directory. errno reported") ;
                return 0 ;
        case 0: log_warnu("control: ", scandir, ": supervisor not listening") ;
                return 0 ;
    }

    return 1 ;
}
