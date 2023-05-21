/*
 * sanitize_fdholder.c
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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
#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>

#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <skalibs/types.h>

#include <66/service.h>
#include <66/constants.h>
#include <66/state.h>
#include <66/enum.h>
#include <66/svc.h>

#include <s6/fdholder.h>

void fdholder_store(s6_fdholder_t *a, char const *name, tain *deadline, tain *limit)
{
    size_t namelen = strlen(name) ;
    char fdname[SS_FDHOLDER_PIPENAME_LEN + 2 + namelen + 1] ;

    int fd[2] ;
    if (pipe(fd) < 0)
        log_dieu(LOG_EXIT_SYS, "pipe") ;

    auto_strings(fdname, SS_FDHOLDER_PIPENAME, "r-", name) ;

    log_trace("store identifier: ", fdname) ;
    if (!s6_fdholder_store_g(a, fd[0], fdname, limit, deadline)) {
        close(fd[0]) ;
        close(fd[1]) ;
        log_dieusys(LOG_EXIT_SYS, "store fd: ", fdname) ;
    }

    close(fd[0]) ;

    fdname[strlen(SS_FDHOLDER_PIPENAME)] = 'w' ;

    log_trace("store identifier: ", fdname) ;
    if (!s6_fdholder_store_g(a, fd[1], fdname, limit, deadline)) {
        close(fd[1]) ;
        log_dieusys(LOG_EXIT_SYS, "store fd: ", fdname) ;
    }

    close(fd[1]) ;
}

void fdholder_delete(s6_fdholder_t *a, char const *name, tain *deadline)
{
    size_t namelen = strlen(name) ;
    char fdname[SS_FDHOLDER_PIPENAME_LEN + 2 + namelen + 1] ;

    auto_strings(fdname, SS_FDHOLDER_PIPENAME, "r-", name) ;

    if (s6_fdholder_retrieve_g(a, fdname, deadline) >= 0) {
        log_trace("delete identifier: ", fdname) ;
        if (!s6_fdholder_delete_g(a, fdname, deadline))
            log_dieusys(LOG_EXIT_SYS, "delete fd: ", fdname) ;
    }

    fdname[strlen(SS_FDHOLDER_PIPENAME)] = 'w' ;

    if (s6_fdholder_retrieve_g(a, fdname, deadline) >= 0) {

        log_trace("delete identifier: ", fdname) ;
        if (!s6_fdholder_delete_g(a, fdname, deadline))
            log_dieusys(LOG_EXIT_SYS, "delete fd: ", fdname) ;
    }
}
/**
 * Accepted flag are
 *      - STATE_FLAGS_TRUE -> store the service A.K.A identifier
 *      - STATE_FLAGS_FALSE -> delete the service A.K.A identifier
 *
 * */

void sanitize_fdholder(resolve_service_t *res, uint32_t flag)
{
    log_flow() ;

    if (res->logger.name && res->type == TYPE_CLASSIC) {

        stralloc list = STRALLOC_ZERO ;
        char *sa = res->sa.s ;
        char *name = sa + res->logger.name ;
        char *socket = sa + res->live.fdholderdir ;
        size_t socketlen = strlen(socket) ;
        ss_state_t sta = STATE_ZERO ;
        s6_fdholder_t a = S6_FDHOLDER_ZERO ;
        tain deadline = tain_infinite_relative, limit = tain_infinite_relative ;
        char sock[socketlen + 3] ;

        auto_strings(sock, socket, "/s") ;

        if (!state_read(&sta, res))
            log_dieu(LOG_EXIT_SYS, "read state file of: ", name) ;

        tain_now_set_stopwatch_g() ;
        tain_add_g(&deadline, &deadline) ;
        tain_add_g(&limit, &limit) ;

        if (!s6_fdholder_start_g(&a, sock, &deadline))
            log_dieusys(LOG_EXIT_SYS, "connect to socket: ", sock) ;

        if (FLAGS_ISSET(flag, STATE_FLAGS_TRUE)) {

            if (service_is(&sta, STATE_FLAGS_ISSUPERVISED) == STATE_FLAGS_TRUE ||
                service_is(&sta, STATE_FLAGS_TORELOAD) == STATE_FLAGS_TRUE ||
                service_is(&sta, STATE_FLAGS_TORESTART) == STATE_FLAGS_TRUE) {

                fdholder_delete(&a, name, &deadline) ;
            }

            fdholder_store(&a, name, &deadline, &limit) ;

        } else if (FLAGS_ISSET(flag, STATE_FLAGS_FALSE)) {

            fdholder_delete(&a, name, &deadline) ;

        }

        if (s6_fdholder_list_g(&a, &list, &deadline) < 0)
            log_dieusys(LOG_EXIT_SYS, "list identifier") ;

        s6_fdholder_end(&a) ;

        size_t pos = 0, tlen = list.len ;
        char t[tlen + 1] ;

        sastr_to_char(t, &list) ;

        list.len = 0 ;

        for (; pos < tlen ; pos += strlen(t + pos) + 1) {

            if (str_start_with(t + pos, SS_FDHOLDER_PIPENAME "r-")) {
                /** only keep the reader, the writer is automatically created
                 * by the 66-fdholder-filler. see format of it */
                if (!auto_stra(&list, t + pos, "\n"))
                    log_die_nomem("stralloc") ;
            }
        }

        char file[socketlen + 17] ;

        auto_strings(file, socket, "/data/autofilled") ;

        if (!openwritenclose_unsafe(file, list.s, list.len))
            log_dieusys(LOG_EXIT_SYS, "write file: ", file) ;

        svc_send_fdholder(socket) ;

        stralloc_free(&list) ;
    }
}
