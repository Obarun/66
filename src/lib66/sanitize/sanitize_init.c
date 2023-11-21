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
#include <errno.h>
#include <unistd.h>// unlink

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sastr.h>
#include <oblibs/environ.h>
#include <oblibs/directory.h>

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
#include <66/symlink.h>

static unsigned int toclean[SS_MAX_SERVICE + 1] ;

void cleanup(resolve_service_t *ares, unsigned int areslen)
{
    unsigned int pos = 0 ;
    int e = errno ;
    ss_state_t sta = STATE_ZERO ;

    for (; pos < areslen ; pos++) {

        if (toclean[pos]) {

            if (!sanitize_fdholder(&ares[pos], &sta, STATE_FLAGS_FALSE))
                log_warnusys("sanitize fdholder directory: ", ares[pos].sa.s + ares[pos].live.fdholderdir);

            log_trace("remove directory: ", ares[pos].sa.s + ares[pos].live.servicedir) ;
            if (!dir_rm_rf(ares[pos].sa.s + ares[pos].live.servicedir))
                log_warnusys("remove live directory: ", ares[pos].sa.s + ares[pos].live.servicedir) ;

            log_trace("remove symlink: ", ares[pos].sa.s + ares[pos].live.scandir) ;
            unlink(ares[pos].sa.s + ares[pos].live.scandir) ;
        }
    }
    errno = e ;
}

void sanitize_init(unsigned int *alist, unsigned int alen, graph_t *g, resolve_service_t *ares, unsigned int areslen)
{
    log_flow() ;

    ftrigr_t fifo = FTRIGR_ZERO ;
    uint32_t earlier ;
    gid_t gid = getgid() ;
    int is_supervised = 0 ;
    unsigned int pos = 0, nsv = 0 ;
    unsigned int real[alen], msg[areslen] ;
    ss_state_t sta = STATE_ZERO ;

    memset(msg, 0, areslen * sizeof(unsigned int)) ;
    memset(toclean, 0, (SS_MAX_SERVICE + 1) * sizeof(unsigned int)) ;

    /* nothing to do */
    if (!alen)
        return ;

    for (; pos < alen ; pos++) {

        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[alist[pos]].vertex ;

        int aresid = service_resolve_array_search(ares, areslen, name) ;
        if (aresid < 0)
            log_dieu(LOG_EXIT_SYS,"find ares id -- please make a bug reports") ;

        toclean[pos] = 1 ;
        earlier = ares[aresid].earlier ;
        char *scandir = ares[aresid].sa.s + ares[aresid].live.scandir ;
        size_t scandirlen = strlen(scandir) ;

        int r = state_read(&sta, &ares[aresid]) ;

        if (!r)
            log_dieu(LOG_EXIT_SYS, "read state file of: ", name, " -- please make a bug reports") ;

        if (!sanitize_livestate(&ares[aresid], &sta)) {
            cleanup(ares, areslen) ;
            log_dieu(LOG_EXIT_SYS, "sanitize state directory: ", ares[aresid].sa.s + ares[aresid].name) ;
        }

        /**
         * Module type are not a daemons. We don't need to supervise it.
         * Special case for Oneshot, we only deal with the scandir symlink. */
        if (ares[aresid].type == TYPE_MODULE)
            continue ;

        is_supervised = access(scandir, F_OK) ;

        if (!earlier && !is_supervised) {
            log_trace(name," already initialized -- ignore it") ;
            msg[aresid] = 1 ;
            continue ;
        }

        if (is_supervised == -1) {

            if (!sanitize_scandir(&ares[aresid], &sta)) {
                cleanup(ares, areslen) ;
                log_dieusys(LOG_EXIT_SYS, "sanitize_scandir directory: ", ares[aresid].sa.s + ares[aresid].live.scandir) ;
            }

            if (ares[aresid].type == TYPE_ONESHOT) {

                if (!state_write(&sta, &ares[aresid])) {
                    cleanup(ares, areslen) ;
                    log_dieusys(LOG_EXIT_SYS, "write status file of: ", ares[aresid].sa.s + ares[aresid].name) ;
                }

                continue ;
            }

        }

        /* down file */
        if (!earlier) {

            char downfile[scandirlen + 6] ;
            auto_strings(downfile, scandir, "/down") ;
            log_trace("create file: ", downfile) ;
            int fd = open_trunc(downfile) ;
            if (fd < 0) {
                cleanup(ares, areslen) ;
                log_dieusys(LOG_EXIT_SYS, "create file: ", downfile) ;
            }
            fd_close(fd) ;
        }

        if (!earlier && is_supervised) {

            if (!sanitize_fdholder(&ares[aresid], &sta, STATE_FLAGS_TRUE)) {
                cleanup(ares, areslen) ;
                log_dieusys(LOG_EXIT_SYS, "sanitize fdholder directory: ", ares[aresid].sa.s + ares[aresid].live.fdholderdir) ;
            }

            log_trace("create fifo: ", ares[aresid].sa.s + ares[aresid].live.eventdir) ;
            if (!ftrigw_fifodir_make(ares[aresid].sa.s + ares[aresid].live.eventdir, gid, 0)) {
                cleanup(ares, areslen) ;
                log_dieusys(LOG_EXIT_SYS, "create fifo: ", ares[aresid].sa.s + ares[aresid].live.eventdir) ;
            }
        }

        if (!state_write(&sta, &ares[aresid])) {
            cleanup(ares, areslen) ;
            log_dieu(LOG_EXIT_SYS, "write state file of: ", name) ;
        }

        real[nsv++] = (unsigned int)aresid ;
    }

    /**
     * scandir is already running, we need to synchronize with it
     * */
    if (!earlier && nsv) {

        uint16_t ids[nsv] ;
        unsigned int nids = 0, fake = 0 ;
        tain deadline ;

        memset(ids, 0, nsv * sizeof(uint16_t)) ;

        tain_now_set_stopwatch_g() ;
        /** TODO
         * waiting for 3 seconds here,
         * it should be the -T option if exist.
        */
        tain_addsec(&deadline, &STAMP, 3) ;

        if (!ftrigr_startf_g(&fifo, &deadline)) {
            cleanup(ares, areslen) ;
            log_dieusys(LOG_EXIT_SYS, "ftrigr") ;
        }

        for (pos = 0 ; pos < nsv ; pos++) {

            if (ares[real[pos]].type == TYPE_CLASSIC && !ares[real[pos]].earlier) {

                fake = pos ;
                char *sa = ares[real[pos]].sa.s ;
                char *eventdir = sa + ares[real[pos]].live.eventdir ;

                log_trace("subcribe to fifo: ", eventdir) ;
                /** unsubscribe automatically, options is 0 */
                ids[nids] = ftrigr_subscribe_g(&fifo, eventdir, "s", 0, &deadline) ;

                if (!ids[nids++]) {
                    cleanup(ares, areslen) ;
                    log_dieusys(LOG_EXIT_SYS, "subcribe to fifo: ", eventdir) ;
                }
            }
        }

        if (nids) {

            state_set_flag(&sta, STATE_FLAGS_TORELOAD, STATE_FLAGS_TRUE) ;

            if (!sanitize_scandir(&ares[real[fake]], &sta)) {
                cleanup(ares, areslen) ;
                log_dieusys(LOG_EXIT_SYS, "sanitize scandir directory: ", ares[real[fake]].sa.s + ares[real[fake]].live.scandir) ;
            }

            log_trace("waiting for events on fifo...") ;
            if (ftrigr_wait_and_g(&fifo, ids, nids, &deadline) < 0) {
                cleanup(ares, areslen) ;
                log_dieusys(LOG_EXIT_SYS, "wait for events") ;
            }
        }
    }

    ftrigr_end(&fifo) ;

    /**
     * We pass through here even for Module and Oneshot.
     * We need to write the state file anyway. Thus can always
     * be consider as initialized.
     * */
    for (pos = 0 ; pos < alen ; pos++) {

        ss_state_t sta = STATE_ZERO ;
        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[alist[pos]].vertex ;
        int aresid = service_resolve_array_search(ares, areslen, name) ;
        if (aresid < 0) {
            cleanup(ares, areslen) ;
            log_dieu(LOG_EXIT_SYS, "find ares id of: ", name, " -- please make a bug reports") ;
        }

        char *sa = ares[aresid].sa.s ;

        if (!state_read(&sta, &ares[aresid])) {
            cleanup(ares, areslen) ;
            log_dieusys(LOG_EXIT_SYS, "read status file of: ", sa + ares[aresid].name) ;
        }

        if (ares[aresid].type == TYPE_CLASSIC) {

            if (!earlier) {

                log_trace("clean event directory: ", sa + ares[aresid].live.eventdir) ;
                if (!ftrigw_clean(sa + ares[aresid].live.eventdir))
                    log_warnu("clean event directory: ", sa + ares[aresid].live.eventdir) ;
            }
        }
        if (ares[aresid].type == TYPE_CLASSIC || ares[aresid].type == TYPE_ONESHOT) {

            if (ares[aresid].logger.want) {
                /** Creation of the logger destination. This is made here to avoid
                 * issues on tmpfs logger directory destination */
                uid_t log_uid ;
                gid_t log_gid ;
                char *logrunner = ares[aresid].logger.execute.run.runas ? ares[aresid].sa.s + ares[aresid].logger.execute.run.runas : SS_LOGGER_RUNNER ;
                char *dst = ares[aresid].sa.s + ares[aresid].logger.destination ;

                if (!youruid(&log_uid, logrunner) || !yourgid(&log_gid, log_uid)) {
                    cleanup(ares, areslen) ;
                    log_dieusys(LOG_EXIT_SYS, "get uid and gid of: ", logrunner) ;
                }

                log_trace("create directory: ", dst) ;
                if (!dir_create_parent(dst, 0755)) {
                    cleanup(ares, areslen) ;
                    log_dieusys(LOG_EXIT_SYS, "create directory: ", ares[aresid].sa.s + ares[aresid].logger.destination) ;
                }

                if (!ares[aresid].owner && (strcmp(ares[aresid].sa.s + ares[aresid].logger.execute.run.build, "custom"))) {

                    if (chown(dst, log_uid, log_gid) == -1) {
                        cleanup(ares, areslen) ;
                        log_dieusys(LOG_EXIT_SYS, "chown: ", dst) ;
                    }
                }
            }
        }

        /** Consider Module as supervised */
        state_set_flag(&sta, STATE_FLAGS_TOINIT, STATE_FLAGS_FALSE) ;
        state_set_flag(&sta, STATE_FLAGS_ISSUPERVISED, STATE_FLAGS_TRUE) ;

        if (!state_write(&sta, &ares[aresid])) {
            cleanup(ares, areslen) ;
            log_dieusys(LOG_EXIT_SYS, "write status file of: ", sa + ares[aresid].name) ;
        }

        if (!msg[aresid])
            log_info("Initialized successfully: ", name) ;
    }
}
