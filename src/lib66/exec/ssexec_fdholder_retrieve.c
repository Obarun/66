/*
 * ssexec_fdholder_retrieve.c
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

#include <stdint.h>
#include <stdbool.h>

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>
#include <oblibs/fd.h>
#include <oblibs/exec.h>
#include <oblibs/environ.h>

#include <66/ssexec.h>
#include <66/constants.h>
#include <66/fdholder.h>

static int fdh_ret_timeout = -1 ;
static bool fdh_ret_delete = false ;

int on_fdholder_retrieve(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {
        case 't' : {
            uint32_t t ;
            if (!u32_scan_strict(arg, &t))
                log_dieu(LOG_EXIT_USER, "parse timeout: ", arg) ;
            fdh_ret_timeout = (int)t ;
            break ;
        }
        case 'D' :
            fdh_ret_delete = true ;
            break ;
    }

    return 0 ;
}

int ssexec_fdholder_retrieve(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    if (argc < 2)
        log_die(LOG_EXIT_USER, "needs an identifier and a program to execute") ;

    char const *id = argv[0] ;
    char const *const *prog = argv + 1 ;

    _alloc_strbuf_(sock, info->scandir.len + sizeof("/" SS_FDHOLDER "/s") + 1) ;
    auto_strings(sock.s, info->scandir.s, "/" SS_FDHOLDER "/s") ;

    fdholder_client_t c ;
    if (!fdholder_client_init(&c, sock.s))
        log_dieusys(LOG_EXIT_SYS, "connect to fdholder daemon: ", sock.s) ;

    int ok = fdholder_retrieve(&c, id, fdh_ret_delete, fdh_ret_timeout) ;
    uint8_t status = c.status ;
    int fd = c.received_fd ;

    if (!ok) {
        fdholder_client_end(&c) ;
        log_die(LOG_EXIT_SYS, "retrieve '", id, "': ", fdholder_status_str(status)) ;
    }

    if (fd < 0) {
        fdholder_client_end(&c) ;
        log_die(LOG_EXIT_SYS, "daemon returned no descriptor for '", id, "'") ;
    }

    /* place the retrieved descriptor on stdin (move_fd clears close-on-exec) */
    if (move_fd(0, fd) == -1)
        log_dieusys(LOG_EXIT_SYS, "move retrieved descriptor onto stdin") ;

    fdholder_client_end(&c) ;

    exec_path_die(prog[0], prog, (char const *const *)environ) ;
}
