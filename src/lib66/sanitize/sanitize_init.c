/*
 * sanitize_init.c
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
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/hash.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>

#include <skalibs/tai.h>

#include <s6/ftrigr.h>
#include <s6/ftrigw.h>

#include <66/graph.h>
#include <66/state.h>
#include <66/service.h>
#include <66/sanitize.h>
#include <66/enum_parser.h>

#include <s6/fdholder.h>

void cleanup(resolve_service_t *res, uint32_t nres)
{
    uint32_t pos = 0 ;
    int e = errno ;
    ss_state_t sta = STATE_ZERO ;
    resolve_service_t_ref pres = 0 ;
    s6_fdholder_t a = S6_FDHOLDER_ZERO ;

    if (!sanitize_fdholder_start(&a, pres->sa.s + pres->live.fdholderdir))
        log_warnusys("start fdholder: ", pres->sa.s + pres->live.fdholderdir) ;

    for (; pos < nres ; pos++) {

        pres = &res[pos] ;

        if (!sanitize_fdholder(pres, &a, &sta, STATE_FLAGS_FALSE, 0))
            log_warnusys("sanitize fdholder directory: ", pres->sa.s + pres->live.fdholderdir);

        log_trace("remove directory: ", pres->sa.s + pres->live.servicedir) ;
        if (!dir_destroy(pres->sa.s + pres->live.servicedir))
            log_warnusys("remove live directory: ", pres->sa.s + pres->live.servicedir) ;

        log_trace("remove symlink: ", pres->sa.s + pres->live.scandir) ;
        (void)unlink(pres->sa.s + pres->live.scandir) ;

    }

    s6_fdholder_end(&a) ;

    errno = e ;
}

void sanitize_init(service_graph_t *g, uint32_t flag)
{
    int r, issupervised = 0 ;
    uint32_t pos = 0, ntoclean = 0, nsubscribe = 0, msg[g->g.nvertexes] ;
    resolve_service_t toclean[g->g.nvertexes], tosubscribe[g->g.nvertexes] ;
    vertex_t *c, *tmp ;
    resolve_service_t *pres ;
    ftrigr_t fifo = FTRIGR_ZERO ;
    ss_state_t sta = STATE_ZERO ;
    bool earlier = false, isstarted = false ;
    s6_fdholder_t a = S6_FDHOLDER_ZERO ;

    memset(msg, 0, g->g.nvertexes * sizeof(uint32_t)) ;
    memset(toclean, 0, g->g.nvertexes * sizeof(resolve_service_t)) ;
    memset(tosubscribe, 0, g->g.nvertexes * sizeof(resolve_service_t)) ;

    HASH_ITER(hh, g->g.vertexes, c, tmp) {

        char *name = c->name ;
        struct resolve_hash_s *hash = hash_search(&g->hres, name) ;

        if (hash == NULL)
            log_die(LOG_EXIT_USER, "service: ", name, " not available -- please make a bug report") ;

        pres = &hash->res ;
        toclean[ntoclean++] = hash->res ;
        earlier = pres->earlier ? true : false ;

        char *scandir = pres->sa.s + pres->live.scandir ;
        size_t scandirlen = strlen(scandir) ;

        r = state_read(&sta, pres) ;

        if (!r)
            log_dieu(LOG_EXIT_SYS, "read state file of: ", name, " -- please make a bug reports") ;

        /**
         * Oneshot are not supervised by a scandir.
         * Check for state directory instead.
        */
        if (pres->type == E_PARSER_TYPE_ONESHOT)
            issupervised = access(pres->sa.s + pres->live.statedir, F_OK) ;
        else
            issupervised = access(scandir, F_OK) ;

        if (!sanitize_livestate(pres, &sta)) {
            cleanup(toclean, ntoclean) ;
            log_dieu(LOG_EXIT_SYS, "sanitize state directory: ", pres->sa.s + pres->name) ;
        }

        /**
         * Module type are not a daemons. We don't need to supervise it.
         * Special case for Oneshot, we only deal with the scandir symlink. */
        if (pres->type == E_PARSER_TYPE_MODULE)
            continue ;

        if (!earlier && !issupervised) {
            log_warn(name," already initialized -- ignore it") ;
            msg[c->index] = 1 ;
            continue ;
        }

        if (issupervised == -1) {

            state_set_flag(&sta, STATE_FLAGS_TOINIT, STATE_FLAGS_TRUE) ;
            if (!sanitize_scandir(pres, &sta)) {
                cleanup(toclean, pos) ;
                log_dieusys(LOG_EXIT_SYS, "sanitize_scandir directory: ", pres->sa.s + pres->live.scandir) ;
            }
            state_set_flag(&sta, STATE_FLAGS_TOINIT, STATE_FLAGS_FALSE) ;

            if (pres->type == E_PARSER_TYPE_ONESHOT) {

                if (!state_write(&sta, pres)) {
                    cleanup(toclean, pos) ;
                    log_dieusys(LOG_EXIT_SYS, "write status file of: ", pres->sa.s + pres->name) ;
                }
                msg[c->index] = 1 ;
                continue ;
            }
        }

        /* down file */
        if (!FLAGS_ISSET(flag, GRAPH_WANT_EARLIER)) {

            char downfile[scandirlen + 6] ;
            auto_strings(downfile, scandir, "/down") ;
            log_trace("create file: ", downfile) ;
            if (!file_write(downfile, "", 0)) {
                cleanup(toclean, pos) ;
                log_dieusys(LOG_EXIT_SYS, "create file: ", downfile) ;
            }

            if (issupervised) {

                if (!isstarted) {
                    if (!sanitize_fdholder_start(&a, pres->sa.s + pres->live.fdholderdir))
                        log_dieu(LOG_EXIT_SYS, "start fdholder: ", pres->sa.s + pres->live.fdholderdir) ;
                    isstarted = true ;
                }

                if (!sanitize_fdholder(pres, &a, &sta, STATE_FLAGS_TRUE, 1)) {
                    cleanup(toclean, pos) ;
                    log_dieusys(LOG_EXIT_SYS, "sanitize fdholder directory for: ", pres->sa.s + pres->name) ;
                }

                log_trace("create fifo: ", pres->sa.s + pres->live.eventdir) ;
                if (!ftrigw_fifodir_make(pres->sa.s + pres->live.eventdir, getgid(), 0)) {
                    cleanup(toclean, pos) ;
                    log_dieusys(LOG_EXIT_SYS, "create fifo: ", pres->sa.s + pres->live.eventdir) ;
                }
            }

        }

        if (!state_write(&sta, pres)) {
            cleanup(toclean, pos) ;
            log_dieu(LOG_EXIT_SYS, "write state file of: ", name) ;
        }

        tosubscribe[nsubscribe++] = hash->res ;

    }

    if (isstarted)
        s6_fdholder_end(&a) ;

    /**
     * scandir is already running, we need to synchronize with it
     * */
    if (!FLAGS_ISSET(flag, GRAPH_WANT_EARLIER) && nsubscribe) {

        uint16_t ids[nsubscribe] ;
        unsigned int nids = 0, fake = 0 ;
        tain deadline ;

        memset(ids, 0, nsubscribe * sizeof(uint16_t)) ;

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

        for (pos = 0 ; pos < nsubscribe ; pos++) {

            if (tosubscribe[pos].type == E_PARSER_TYPE_CLASSIC && !tosubscribe[pos].earlier) {

                fake = pos ;
                char *sa = tosubscribe[pos].sa.s ;
                char *eventdir = sa + tosubscribe[pos].live.eventdir ;

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

            if (!sanitize_scandir(&tosubscribe[fake], &sta)) {
                cleanup(toclean, ntoclean) ;
                log_dieusys(LOG_EXIT_SYS, "sanitize scandir directory: ", tosubscribe[fake].sa.s + tosubscribe[fake].live.scandir) ;
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
    HASH_ITER(hh, g->g.vertexes, c, tmp) {

        ss_state_t sta = STATE_ZERO ;
        char *name = c->name ;
        struct resolve_hash_s *hash = hash_search(&g->hres, c->name) ;

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

        if (pres->type == E_PARSER_TYPE_CLASSIC) {

            if (!FLAGS_ISSET(flag, GRAPH_WANT_EARLIER)) {

                log_trace("clean event directory: ", sa + pres->live.eventdir) ;
                if (!ftrigw_clean(sa + pres->live.eventdir))
                    log_warnu("clean event directory: ", sa + pres->live.eventdir) ;
            }
        }

        /** Consider Module as supervised */
        state_set_flag(&sta, STATE_FLAGS_TOINIT, STATE_FLAGS_FALSE) ;
        state_set_flag(&sta, STATE_FLAGS_ISSUPERVISED, STATE_FLAGS_TRUE) ;

        if (!state_write(&sta, pres)) {
            cleanup(toclean, ntoclean) ;
            log_dieusys(LOG_EXIT_SYS, "write status file of: ", sa + pres->name) ;
        }

        if (!msg[c->index])
            log_info("Initialized successfully: ", name) ;
    }
}
