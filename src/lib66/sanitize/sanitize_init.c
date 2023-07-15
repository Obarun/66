/*
 * sanitize_init.c
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

#include <string.h>
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>
#include <oblibs/environ.h>

#include <skalibs/types.h>
#include <skalibs/genalloc.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <skalibs/unix-transactional.h>//atomic_symlink

#include <s6/supervise.h>
#include <s6/ftrigr.h>
#include <s6/ftrigw.h>

#include <66/utils.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/ssexec.h>
#include <66/state.h>
#include <66/enum.h>
#include <66/sanitize.h>

void sanitize_init(unsigned int *alist, unsigned int alen, graph_t *g, resolve_service_t *ares, unsigned int areslen)
{
    log_flow() ;

    ftrigr_t fifo = FTRIGR_ZERO ;
    uint32_t earlier ;
    gid_t gid = getgid() ;
    int is_supervised = 0, is_init = 0, gearlier = 0 ;
    unsigned int pos = 0, nsv = 0 ;
    unsigned int real[alen] ;
    unsigned int msg[areslen] ;
    ss_state_t sta = STATE_ZERO ;

    memset(msg, 0, areslen) ;

    /* nothing to do */
    if (!alen)
        return ;

    for (; pos < alen ; pos++) {

        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[alist[pos]].vertex ;

        int aresid = service_resolve_array_search(ares, areslen, name) ;
        if (aresid < 0)
            log_dieu(LOG_EXIT_SYS,"find ares id -- please make a bug reports") ;

        if (!state_read(&sta, &ares[aresid]))
            log_dieu(LOG_EXIT_SYS, "read state file of: ", name) ;

        earlier = service_is(&sta, STATE_FLAGS_ISEARLIER) == STATE_FLAGS_TRUE ? 1 : 0 ;
        char *sa = ares[aresid].sa.s ;
        char *scandir = sa + ares[aresid].live.scandir ;
        size_t scandirlen = strlen(scandir) ;

        is_init = access(sa + ares[aresid].live.statedir, F_OK) ;
        if (is_init < 0 || service_is(&sta, STATE_FLAGS_TOINIT) == STATE_FLAGS_TRUE)
            sanitize_livestate(&ares[aresid]) ;

        if (earlier)
            gearlier = aresid ;
        /**
         * Bundle and module type are not a daemons. We don't need to supervise it.
         * Special case for Oneshot, we only deal with the scandir symlink. */
        if (ares[aresid].type == TYPE_BUNDLE || ares[aresid].type == TYPE_MODULE)
            continue ;

        is_supervised = access(scandir, F_OK) ;

        if (!earlier && !is_supervised) {
            log_warn(name," already initialized -- ignore it") ;
            msg[aresid] = 1 ;
            continue ;
        }

        if (is_supervised == -1) {

            sanitize_scandir(&ares[aresid]) ;

            if (ares[aresid].type == TYPE_ONESHOT)
                continue ;
        }

        /* down file */
        if (!earlier) {

            char downfile[scandirlen + 6] ;
            auto_strings(downfile, scandir, "/down") ;
            int fd = open_trunc(downfile) ;
            if (fd < 0)
                log_dieusys(LOG_EXIT_SYS, "create file: ", downfile) ;
            fd_close(fd) ;
        }

        if (!earlier && is_supervised) {

            sanitize_fdholder(&ares[aresid], STATE_FLAGS_TRUE) ;

            log_trace("create fifo: ", sa + ares[aresid].live.eventdir) ;
            if (!ftrigw_fifodir_make(sa + ares[aresid].live.eventdir, gid, 0))
                log_dieusys(LOG_EXIT_SYS, "create fifo: ", sa + ares[aresid].live.eventdir) ;
        }

        real[nsv++] = (unsigned int)aresid ;
    }

    /**
     * scandir is already running, we need to synchronize with it
     * */
    if (!earlier && nsv) {

        uint16_t ids[nsv] ;
        unsigned int nids = 0 ;

        tain deadline ;
        tain_now_set_stopwatch_g() ;
        tain_addsec(&deadline, &STAMP, 2) ;

        if (!ftrigr_startf_g(&fifo, &deadline))
            log_dieusys(LOG_EXIT_SYS, "ftrigr") ;

        for (pos = 0 ; pos < nsv ; pos++) {

            if (ares[real[pos]].type == TYPE_CLASSIC) {

                char *sa = ares[real[pos]].sa.s ;
                char *eventdir = sa + ares[real[pos]].live.eventdir ;

                log_trace("subcribe to fifo: ", eventdir) ;
                /** unsubscribe automatically, options is 0 */
                ids[nids] = ftrigr_subscribe_g(&fifo, eventdir, "s", 0, &deadline) ;

                if (!ids[nids++])
                    log_dieusys(LOG_EXIT_SYS, "subcribe to fifo: ", eventdir) ;

                if (!state_messenger(&ares[real[pos]], STATE_FLAGS_TORELOAD, STATE_FLAGS_TRUE))
                    log_dieusys(LOG_EXIT_SYS, "send message to state of: ", sa + ares[real[pos]].name) ;

            }
        }

        if (nids) {

            sanitize_scandir(&ares[real[0]]) ;

            log_trace("waiting for events on fifo...") ;
            if (ftrigr_wait_and_g(&fifo, ids, nids, &deadline) < 0)
                log_dieusys(LOG_EXIT_SYS, "wait for events") ;

        }
    }

    /**
     * We pass through here even for Bundle, Module and Oneshot.
     * We need to write the state file anyway. Thus can always
     * be consider as initialized.
     * */
    for (pos = 0 ; pos < alen ; pos++) {

        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[alist[pos]].vertex ;
        int aresid = service_resolve_array_search(ares, areslen, name) ;
        if (aresid < 0)
            log_dieu(LOG_EXIT_SYS,"find ares id -- please make a bug reports") ;

        char *sa = ares[aresid].sa.s ;

        if (ares[aresid].type == TYPE_CLASSIC || ares[aresid].type == TYPE_ONESHOT) {

            log_trace("clean event directory: ", sa + ares[aresid].live.eventdir) ;
            if (!ftrigw_clean(sa + ares[aresid].live.eventdir))
                log_warnu("clean event directory: ", sa + ares[aresid].live.eventdir) ;

        } else if (ares[aresid].type == TYPE_BUNDLE || ares[aresid].type == TYPE_MODULE) {

            /** Consider Module and Bundle as supervised */
            if (!state_messenger(&ares[aresid], STATE_FLAGS_ISSUPERVISED, STATE_FLAGS_TRUE))
                log_dieusys(LOG_EXIT_SYS, "send message to state of: ", name) ;
        }

        if (!state_messenger(&ares[aresid], STATE_FLAGS_TOINIT, STATE_FLAGS_FALSE))
            log_dieusys(LOG_EXIT_SYS, "send message to state of: ", name) ;

        if (!msg[aresid])
            log_info("Initialized successfully: ", name) ;
    }

    ftrigr_end(&fifo) ;

}
