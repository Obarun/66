/*
 * ssexec_fdholder_store.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/ssexec.h>
#include <66/constants.h>
#include <66/fdholder.h>

static uint32_t fdh_store_fd = 0 ;
static uint32_t fdh_store_expire = 0 ;
static int fdh_store_timeout = -1 ;

int on_fdholder_store(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {
        case 'T' : {
            uint32_t t ;
            if (!u32_scan_strict(arg, &t))
                log_dieu(LOG_EXIT_USER, "parse timeout: ", arg) ;
            fdh_store_timeout = (int)t ;
            break ;
        }
        case 'd' :
            if (!u32_scan_strict(arg, &fdh_store_fd))
                log_dieu(LOG_EXIT_USER, "parse fd: ", arg) ;
            break ;
        case 'e' :
            if (!u32_scan_strict(arg, &fdh_store_expire))
                log_dieu(LOG_EXIT_USER, "parse expire: ", arg) ;
            break ;
    }

    return 0 ;
}

int ssexec_fdholder_store(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing identifier") ;

    char const *id = argv[0] ;

    char sock[info->scandir.len + sizeof("/" SS_FDHOLDER "/s") + 1] ;
    auto_strings(sock, info->scandir.s, "/" SS_FDHOLDER "/s") ;

    fdholder_client_t c ;
    if (!fdholder_client_init(&c, sock))
        log_dieusys(LOG_EXIT_SYS, "connect to fdholder daemon: ", sock) ;

    int ok = fdholder_store(&c, id, (int)fdh_store_fd, fdh_store_expire, fdh_store_timeout) ;
    uint8_t status = c.status ;
    fdholder_client_end(&c) ;

    if (!ok)
        log_die(LOG_EXIT_SYS, "store '", id, "': ", fdholder_status_str(status)) ;

    return 0 ;
}
