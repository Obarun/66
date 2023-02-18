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

#include <66/svc.h>

#include <string.h>
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>
#include <oblibs/environ.h>

#include <skalibs/types.h>
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

void sanitize_init(unsigned int *alist, unsigned int alen, graph_t *g, resolve_service_t *ares, unsigned int areslen, uint32_t flags)
{
    log_flow() ;

    ftrigr_t fifo = FTRIGR_ZERO ;
    uint32_t earlier = FLAGS_ISSET(flags, STATE_FLAGS_ISEARLIER) ;
    gid_t gid = getgid() ;
    int is_supervised = 0, is_init ;
    unsigned int pos = 0, nsv = 0 ;
    unsigned int real[alen] ;

    /* nothing to do */
    if (!alen)
        return ;

    for (; pos < alen ; pos++) {

        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[alist[pos]].vertex ;
        int aresid = service_resolve_array_search(ares, areslen, name) ;
        if (aresid < 0)
            log_dieu(LOG_EXIT_SYS,"find ares id -- please make a bug reports") ;

        char *sa = ares[aresid].sa.s ;
        char *scandir = sa + ares[aresid].live.scandir ;
        size_t scandirlen = strlen(scandir) ;

        is_init = access(sa + ares[aresid].live.statedir, F_OK) ;
        if (is_init < 0 || FLAGS_ISSET(flags, STATE_FLAGS_TOINIT))
            sanitize_livestate(&ares[aresid], STATE_FLAGS_UNKNOWN) ;

        /**
         * Bundle, module type are not a daemons. We don't need
         * to supervise it.
         * Special case for Oneshot, we only deal with
         * the scandir symlink.
         * */
        if (ares[aresid].type == TYPE_BUNDLE || ares[aresid].type == TYPE_MODULE )
            continue ;

        is_supervised = access(scandir, F_OK) ;

        if (!earlier && !is_supervised) {
            log_warn(name," already initialized -- ignore it") ;
            continue ;
        }

        if (is_supervised == -1) {

            sanitize_scandir(&ares[aresid], STATE_FLAGS_TOINIT) ;

            if (ares[aresid].type == TYPE_ONESHOT)
                continue ;
        }

        /* down file */
        char downfile[scandirlen + 6] ;
        auto_strings(downfile, scandir, "/down") ;
        int fd = open_trunc(downfile) ;
        if (fd < 0)
            log_dieusys(LOG_EXIT_SYS, "create file: ", downfile) ;
        fd_close(fd) ;

        if (!earlier && is_supervised) {

            if (!FLAGS_ISSET(flags, STATE_FLAGS_TORELOAD) && !FLAGS_ISSET(flags, STATE_FLAGS_TORESTART))
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
            }
        }

        if (nids) {

            sanitize_scandir(&ares[real[0]], STATE_FLAGS_TORELOAD) ;

            log_trace("waiting for events on fifo...") ;
            if (ftrigr_wait_and_g(&fifo, ids, nids, &deadline) < 0)
                log_dieusys(LOG_EXIT_SYS, "wait for events") ;

             {
                /** for now, i don't know exactly why
                 * but if the fdholder daemon isn't reloaded
                 * the up process hangs at a service logger start.
                 * Sending a -ru to fdholder solve the issue.*/
                char tfmt[UINT32_FMT] ;
                tfmt[uint_fmt(tfmt, 3000)] = 0 ;
                pid_t pid ;
                int wstat ;
                char *socket = ares[real[0]].sa.s + ares[real[0]].live.fdholderdir ;

                char const *newargv[8] ;
                unsigned int m = 0 ;

                newargv[m++] = "s6-svc" ;
                newargv[m++] = "-ru" ;
                newargv[m++] = "-T" ;
                newargv[m++] = tfmt ;
                newargv[m++] = "--" ;
                newargv[m++] = socket ;
                newargv[m++] = 0 ;

                log_trace("sending -ru signal to: ", socket) ;

                pid = child_spawn0(newargv[0], newargv, (char const *const *) environ) ;

                if (waitpid_nointr(pid, &wstat, 0) < 0)
                    log_dieusys(LOG_EXIT_SYS, "wait for reload of the fdholder daemon") ;

                if (wstat)
                    log_dieu(LOG_EXIT_SYS, "reload fdholder service; ", socket) ;
            }
        }
    }

    if (earlier)
        sanitize_scandir(&ares[0], STATE_FLAGS_ISEARLIER) ;

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

        if (!ftrigw_clean(sa + ares[aresid].live.eventdir))
            log_warnu("clean event directory: ", sa + ares[aresid].live.eventdir) ;

        if (!state_messenger(sa  + ares[aresid].path.home, name, STATE_FLAGS_TOINIT, STATE_FLAGS_FALSE))
            log_dieusys(LOG_EXIT_SYS, "send message to state of: ", name) ;

        log_info("Initialized successfully: ", name) ;
    }

    ftrigr_end(&fifo) ;

}
