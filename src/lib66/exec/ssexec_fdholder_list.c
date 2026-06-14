/*
 * ssexec_fdholder_list.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/stream.h>

#include <66/ssexec.h>
#include <66/constants.h>
#include <66/fdholder.h>

static int fdh_list_timeout = -1 ;

int on_fdholder_list(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {
        case 't' : {
            uint32_t t ;
            if (!u32_scan_strict(arg, &t))
                log_dieu(LOG_EXIT_USER, "parse timeout: ", arg) ;
            fdh_list_timeout = (int)t ;
            break ;
        }
    }

    return 0 ;
}

int ssexec_fdholder_list(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    (void)argc ;
    (void)argv ;

    ssexec_t *info = data ;

    _alloc_strbuf_(sock, info->scandir.len + sizeof("/" SS_FDHOLDER "/s") + 1) ;
    auto_strings(sock.s, info->scandir.s, "/" SS_FDHOLDER "/s") ;

    fdholder_client_t c ;
    if (!fdholder_client_init(&c, sock.s))
        log_dieusys(LOG_EXIT_SYS, "connect to fdholder daemon: ", sock.s) ;

    int ok = fdholder_list(&c, fdh_list_timeout) ;
    uint8_t status = c.status ;

    if (ok) {
        size_t pos = 0 ;
        while (pos < c.resp_payload_len) {
            char const *name = c.paybuf + pos ;
            size_t l = strlen(name) ;
            if (!ostream_puts(ostream_1, name) || !ostream_put(ostream_1, "\n", 1))
                log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
            pos += l + 1 ;
        }
        if (!ostream_flush(ostream_1))
            log_dieusys(LOG_EXIT_SYS, "write to stdout") ;
    }

    fdholder_client_end(&c) ;

    if (!ok)
        log_die(LOG_EXIT_SYS, "list: ", fdholder_status_str(status)) ;

    return 0 ;
}
