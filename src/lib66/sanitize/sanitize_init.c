/*
 * sanitize_init.c
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

void cleanup(struct resolve_hash_s *hash, unsigned int alen)
{
    unsigned int pos = 0 ;
    int e = errno ;
    ss_state_t sta = STATE_ZERO ;
    resolve_service_t_ref pres = 0 ;

    for (; pos < alen ; pos++) {

        pres = &hash[pos].res ;

        if (!sanitize_fdholder(pres, &sta, STATE_FLAGS_FALSE, 0))
            log_warnusys("sanitize fdholder directory: ", pres->sa.s + pres->live.fdholderdir);

        log_trace("remove directory: ", pres->sa.s + pres->live.servicedir) ;
        if (!dir_rm_rf(pres->sa.s + pres->live.servicedir))
            log_warnusys("remove live directory: ", pres->sa.s + pres->live.servicedir) ;

        log_trace("remove symlink: ", pres->sa.s + pres->live.scandir) ;
        unlink(pres->sa.s + pres->live.scandir) ;

    }
    errno = e ;
}

void sanitize_init(unsigned int *alist, unsigned int alen, graph_t *g, struct resolve_hash_s **hres)
{
    log_flow() ;

    /* nothing to do */
    if (!alen)
        return ;

    ftrigr_t fifo = FTRIGR_ZERO ;
    uint32_t earlier ;
    gid_t gid = getgid() ;
    int is_supervised = 0 ;
    unsigned int pos = 0, nsv = 0, msg[alen] ;
    ss_state_t sta = STATE_ZERO ;
    resolve_service_t_ref pres = 0 ;
    struct resolve_hash_s toclean[alen] ;
    struct resolve_hash_s real[alen] ;
    unsigned int ntoclean = 0 ;

    memset(msg, 0, alen * sizeof(unsigned int)) ;
    memset(toclean, 0, alen * sizeof(struct resolve_hash_s)) ;
    memset(real, 0, alen * sizeof(struct resolve_hash_s)) ;

    for (; pos < alen ; pos++) {

        char *name = g->data.s + genalloc_s(graph_hash_t,&g->hash)[alist[pos]].vertex ;

        struct resolve_hash_s *hash = hash_search(hres,name) ;
        if (hash == NULL)
            log_dieu(LOG_EXIT_SYS,"find ares id -- please make a bug reports") ;

        pres = &hash->res ;

        toclean[ntoclean++] = *hash ;
        earlier = pres->earlier ;
        char *scandir = pres->sa.s + pres->live.scandir ;
        size_t scandirlen = strlen(scandir) ;

        int r = state_read(&sta, pres) ;

        if (!r)
            log_dieu(LOG_EXIT_SYS, "read state file of: ", name, " -- please make a bug reports") ;

        if (!sanitize_livestate(pres, &sta)) {
            cleanup(toclean, ntoclean) ;
            log_dieu(LOG_EXIT_SYS, "sanitize state directory: ", pres->sa.s + pres->name) ;
        }

        /**
         * Module type are not a daemons. We don't need to supervise it.
         * Special case for Oneshot, we only deal with the scandir symlink. */
        if (pres->type == TYPE_MODULE)
            continue ;

        is_supervised = access(scandir, F_OK) ;

        if (!earlier && !is_supervised) {
            log_trace(name," already initialized -- ignore it") ;
            msg[pos] = 1 ;
            continue ;
        }

        if (is_supervised == -1) {

            if (!sanitize_scandir(pres, &sta)) {
                cleanup(toclean, pos) ;
                log_dieusys(LOG_EXIT_SYS, "sanitize_scandir directory: ", pres->sa.s + pres->live.scandir) ;
            }

            if (pres->type == TYPE_ONESHOT) {

                if (!state_write(&sta, pres)) {
                    cleanup(toclean, pos) ;
                    log_dieusys(LOG_EXIT_SYS, "write status file of: ", pres->sa.s + pres->name) ;
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
                cleanup(toclean, pos) ;
                log_dieusys(LOG_EXIT_SYS, "create file: ", downfile) ;
            }
            fd_close(fd) ;
        }

        if (!earlier && is_supervised) {

            if (!sanitize_fdholder(pres, &sta, STATE_FLAGS_TRUE, 1)) {
                cleanup(toclean, pos) ;
                log_dieusys(LOG_EXIT_SYS, "sanitize fdholder directory: ", pres->sa.s + pres->live.fdholderdir) ;
            }

            log_trace("create fifo: ", pres->sa.s + pres->live.eventdir) ;
            if (!ftrigw_fifodir_make(pres->sa.s + pres->live.eventdir, gid, 0)) {
                cleanup(toclean, pos) ;
                log_dieusys(LOG_EXIT_SYS, "create fifo: ", pres->sa.s + pres->live.eventdir) ;
            }
        }

        if (!state_write(&sta, pres)) {
            cleanup(toclean, pos) ;
            log_dieu(LOG_EXIT_SYS, "write state file of: ", name) ;
        }

        real[nsv++] = *hash ;
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
            cleanup(toclean, ntoclean) ;
            log_dieusys(LOG_EXIT_SYS, "ftrigr") ;
        }

        for (pos = 0 ; pos < nsv ; pos++) {

            if (real[pos].res.type == TYPE_CLASSIC && !real[pos].res.earlier) {

                fake = pos ;
                char *sa = real[pos].res.sa.s ;
                char *eventdir = sa + real[pos].res.live.eventdir ;

                log_trace("subcribe to fifo: ", eventdir) ;
                /** unsubscribe automatically, options is 0 */
                ids[nids] = ftrigr_subscribe_g(&fifo, eventdir, "s", 0, &deadline) ;

                if (!ids[nids++]) {
                    cleanup(toclean, ntoclean) ;
                    log_dieusys(LOG_EXIT_SYS, "subcribe to fifo: ", eventdir) ;
                }
            }
        }

        if (nids) {

            state_set_flag(&sta, STATE_FLAGS_TORELOAD, STATE_FLAGS_TRUE) ;

            if (!sanitize_scandir(&real[fake].res, &sta)) {
                cleanup(toclean, ntoclean) ;
                log_dieusys(LOG_EXIT_SYS, "sanitize scandir directory: ", real[fake].res.sa.s + real[fake].res.live.scandir) ;
            }

            log_trace("waiting for events on fifo...") ;
            if (ftrigr_wait_and_g(&fifo, ids, nids, &deadline) < 0) {
                cleanup(toclean, ntoclean) ;
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

        struct resolve_hash_s *hash = hash_search(hres, name) ;
        if (hash == NULL) {
            cleanup(toclean, ntoclean) ;
            log_dieu(LOG_EXIT_SYS, "find hash id of: ", name, " -- please make a bug reports") ;
        }

        pres = &hash->res ;
        char *sa = pres->sa.s ;

        if (!state_read(&sta, pres)) {
            cleanup(toclean, ntoclean) ;
            log_dieusys(LOG_EXIT_SYS, "read status file of: ", sa + pres->name) ;
        }

        if (pres->type == TYPE_CLASSIC) {

            if (!earlier) {

                log_trace("clean event directory: ", sa + pres->live.eventdir) ;
                if (!ftrigw_clean(sa + pres->live.eventdir))
                    log_warnu("clean event directory: ", sa + pres->live.eventdir) ;
            }
        }

        if ((pres->type == TYPE_CLASSIC || pres->type == TYPE_ONESHOT) && pres->logger.want) {

            /** Creation of the logger destination. This is made here to avoid
             * issues on tmpfs logger directory destination */
            uid_t log_uid ;
            gid_t log_gid ;
            char *logrunner = pres->sa.s + pres->logger.execute.run.runas ;
            char *dst = pres->sa.s + pres->logger.destination ;

            if (!youruid(&log_uid, logrunner) || !yourgid(&log_gid, log_uid)) {
                cleanup(toclean, ntoclean) ;
                log_dieusys(LOG_EXIT_SYS, "get uid and gid of: ", logrunner) ;
            }

            log_trace("create directory: ", dst) ;
            if (!dir_create_parent(dst, 0755)) {
                cleanup(toclean, ntoclean) ;
                log_dieusys(LOG_EXIT_SYS, "create directory: ", dst) ;
            }

            if (!pres->owner && (strcmp(pres->sa.s + pres->logger.execute.run.build, "custom"))) {

                if (chown(dst, log_uid, log_gid) == -1) {
                    cleanup(toclean, ntoclean) ;
                    log_dieusys(LOG_EXIT_SYS, "chown: ", dst) ;
                }
            }
        }

        /** Consider Module as supervised */
        state_set_flag(&sta, STATE_FLAGS_TOINIT, STATE_FLAGS_FALSE) ;
        state_set_flag(&sta, STATE_FLAGS_ISSUPERVISED, STATE_FLAGS_TRUE) ;

        if (!state_write(&sta, pres)) {
            cleanup(toclean, ntoclean) ;
            log_dieusys(LOG_EXIT_SYS, "write status file of: ", sa + pres->name) ;
        }

        if (!msg[pos])
            log_info("Initialized successfully: ", name) ;
    }
}
