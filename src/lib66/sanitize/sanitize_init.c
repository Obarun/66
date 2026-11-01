/*
 * sanitize_init.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/bits.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>

#include <66/graph.h>
#include <66/selection.h>
#include <66/state.h>
#include <66/service.h>
#include <66/sanitize.h>
#include <66/symlink.h>
#include <66/utils.h>
#include <66/enum_parser.h>
#include <66/event.h>
#include <66/fdholder.h>
#include <66/svc.h>

static void cleanup(selection_service_t const *lot, service_graph_resolve_t *store, bitset32_t const *created)
{
    int e = errno ;
    bool connected = false ;
    fdholder_client_t a ;

    for (uint32_t index = 0 ; index < service_graph_nvertex(lot->g) ; index++) {

        struct resolve_hash_s *h ;
        ss_state_t sta = STATE_ZERO ;

        if (!bitset32_isvalid(created, index) || service_graph_resolve_get(store, index, 0, &h) <= 0)
            continue ;

        resolve_service_t *res = &h->res ;

        if (!connected) {
            connected = sanitize_fdholder_start(&a, res->sa.s + res->live.fdholderdir) ;
            if (!connected)
                log_warnusys("start fdholder: ", res->sa.s + res->live.fdholderdir) ;
        }

        if (connected && !sanitize_fdholder(res, &a, &sta, STATE_FLAGS_FALSE, 0))
            log_warnusys("sanitize fdholder directory: ", res->sa.s + res->live.fdholderdir) ;

        log_trace("remove directory: ", res->sa.s + res->live.servicedir) ;
        if (!dir_destroy(res->sa.s + res->live.servicedir))
            log_warnusys("remove live directory: ", res->sa.s + res->live.servicedir) ;

        log_trace("remove symlink: ", res->sa.s + res->live.scandir) ;
        file_tryunlink(res->sa.s + res->live.scandir) ;
    }

    if (connected)
        fdholder_client_end(&a) ;

    errno = e ;
}

static int prepare(struct resolve_hash_s *h, uint32_t index, bool earlier_stage, bitset32_t *created, bitset32_t *subscribe, bitset32_t *quiet)
{
    resolve_service_t *res = &h->res ;
    char const *name = res->sa.s + res->name ;
    char const *scandir = res->sa.s + res->live.scandir ;
    ss_state_t sta = STATE_ZERO ;
    int present ;

    if (state_read(&sta, res) <= 0) {
        log_warnusys("read state file of: ", name) ;
        return -1 ;
    }

    present = access(scandir, F_OK) == 0 ;
    if (!present && errno != ENOENT) {
        log_warnusys("check the scandir entry of: ", name) ;
        return -1 ;
    }

    if (!present)
        bitset32_set(created, index) ;

    if (!sanitize_livestate(res, &sta)) {
        log_warnu("sanitize state directory: ", name) ;
        return -1 ;
    }

    if (res->type != E_PARSER_TYPE_CLASSIC && sta.tounsupervise != STATE_FLAGS_TRUE) {

        if (!dir_create_parent(res->sa.s + res->live.supervisedir, 0755)) {
            log_warnusys("create supervise directory of: ", name) ;
            return -1 ;
        }

        if (symlink_atomic_live(res->owner, res->sa.s + res->live.livedir, res->sa.s + res->live.servicedir, scandir) < 0) {
            log_warnusys("create scandir symlink of: ", name) ;
            return -1 ;
        }
    }

    if (res->type == E_PARSER_TYPE_MODULE)
        return 1 ;

    if (!res->earlier && present) {
        log_warn(name, " already initialized -- ignore it") ;
        bitset32_set(quiet, index) ;
        return 1 ;
    }

    if (!present) {

        state_set_flag(&sta, STATE_FLAGS_TOINIT, STATE_FLAGS_TRUE) ;
        if (!sanitize_scandir(res, &sta)) {
            log_warnusys("sanitize scandir directory: ", scandir) ;
            return -1 ;
        }
        state_set_flag(&sta, STATE_FLAGS_TOINIT, STATE_FLAGS_FALSE) ;

        if (res->type == E_PARSER_TYPE_ONESHOT || res->type == E_PARSER_TYPE_EVENT) {

            if (res->type == E_PARSER_TYPE_ONESHOT) {

                log_trace("create fifo: ", res->sa.s + res->live.eventdir) ;
                if (!event_fifo_make(res->sa.s + res->live.eventdir, getgid())) {
                    log_warnusys("create fifo: ", res->sa.s + res->live.eventdir) ;
                    return -1 ;
                }
            }

            if (!state_write(&sta, res)) {
                log_warnusys("write state file of: ", name) ;
                return -1 ;
            }

            bitset32_set(quiet, index) ;
            return 1 ;
        }
    }

    if (!earlier_stage) {

        log_trace("create file: ", scandir, "/down") ;
        if (!file_create_empty(scandir, "down", 0644) && errno != EEXIST) {
            log_warnusys("create file: ", scandir, "/down") ;
            return -1 ;
        }

        if (!present) {

            log_trace("create fifo: ", res->sa.s + res->live.eventdir) ;
            if (!event_fifo_make(res->sa.s + res->live.eventdir, getgid())) {
                log_warnusys("create fifo: ", res->sa.s + res->live.eventdir) ;
                return -1 ;
            }
        }
    }

    if (!state_write(&sta, res)) {
        log_warnusys("write state file of: ", name) ;
        return -1 ;
    }

    bitset32_set(subscribe, index) ;

    return 1 ;
}

static int synchronize(selection_service_t const *lot, service_graph_resolve_t *store, bitset32_t const *subscribe)
{
    uint32_t n = service_graph_nvertex(lot->g), neventdirs = 0 ;
    char const *eventdirs[n + 1] ;
    resolve_service_t *one = 0 ;
    ss_state_t sta = STATE_ZERO ;
    event_wait_t fifo ;

    for (uint32_t index = 0 ; index < n ; index++) {

        struct resolve_hash_s *h ;

        if (!bitset32_isvalid(subscribe, index))
            continue ;

        if (service_graph_resolve_get(store, index, 0, &h) <= 0)
            return -1 ;

        if (h->res.type != E_PARSER_TYPE_CLASSIC || h->res.earlier)
            continue ;

        one = &h->res ;
        eventdirs[neventdirs++] = h->res.sa.s + h->res.live.eventdir ;
        log_trace("subscribe to fifo: ", eventdirs[neventdirs - 1]) ;
    }

    if (!neventdirs)
        return 1 ;

    if (!event_wait_init(&fifo, eventdirs, neventdirs, EVENT_SUPERVISE_UP)) {
        log_warnusys("subscribe to event fifos") ;
        return -1 ;
    }

    state_set_flag(&sta, STATE_FLAGS_TORELOAD, STATE_FLAGS_TRUE) ;

    if (!sanitize_scandir(one, &sta)) {
        event_wait_free(&fifo) ;
        log_warnusys("sanitize scandir directory: ", one->sa.s + one->live.scandir) ;
        return -1 ;
    }

    log_trace("waiting for events on fifo...") ;

    /** TODO
     * waiting for 3 seconds here,
     * it should be the -T option if exist.
    */
    int w = event_wait_run(&fifo, 3000) ;
    event_wait_free(&fifo) ;

    if (w < 0) {
        log_warnusys("wait for events") ;
        return -1 ;
    }

    if (!w) {
        log_warn("timed out waiting for services to be supervised") ;
        return (errno = ETIMEDOUT, -1) ;
    }

    return 1 ;
}

static int settle(struct resolve_hash_s *h, bool earlier_stage, bool quiet, uint8_t who)
{
    resolve_service_t *res = &h->res ;
    char const *sa = res->sa.s ;
    ss_state_t sta = STATE_ZERO ;

    if (state_read(&sta, res) <= 0) {
        log_warnusys("read state file of: ", sa + res->name) ;
        return -1 ;
    }

    if (res->type == E_PARSER_TYPE_CLASSIC && !earlier_stage) {

        log_trace("clean event directory: ", sa + res->live.eventdir) ;
        if (!event_fifo_clean(sa + res->live.eventdir))
            log_warnu("clean event directory: ", sa + res->live.eventdir) ;
    }

    state_set_flag(&sta, STATE_FLAGS_TOINIT, STATE_FLAGS_FALSE) ;
    state_set_flag(&sta, STATE_FLAGS_ISSUPERVISED, STATE_FLAGS_TRUE) ;

    if (res->type != E_PARSER_TYPE_EVENT && !earlier_stage) {

        if (!svcd_notify(sa + res->live.eventddir, 'a', who, sa + res->name)) {

            if (res->has_event)
                log_warnusys("arm event reactor: ", sa + res->name) ;
            else
                log_trace("unable to announce the supervision to 66-eventd of: ", sa + res->name) ;
        }
    }

    if (!state_write(&sta, res)) {
        log_warnusys("write state file of: ", sa + res->name) ;
        return -1 ;
    }

    if (!quiet)
        log_info("Initialized successfully: ", sa + res->name) ;

    return 1 ;
}

int sanitize_init(selection_service_t const *lot, service_graph_resolve_t *store, uint8_t stage, uint8_t who)
{
    log_flow() ;

    bool earlier_stage = stage == SVC_LOT_EARLIER ;
    bitset32_t created = bitset32_init(SS_MAX_SERVICE) ;
    bitset32_t subscribe = bitset32_init(SS_MAX_SERVICE) ;
    bitset32_t quiet = bitset32_init(SS_MAX_SERVICE) ;

    if (stage > SVC_LOT_EARLIER)
        return (errno = EINVAL, -1) ;

    uint32_t index ;

    FOREACH_SELECTION_MEMBER(lot, index) {

        struct resolve_hash_s *h ;

        if (service_graph_resolve_get(store, index, 0, &h) <= 0 || prepare(h, index, earlier_stage, &created, &subscribe, &quiet) < 0) {
            cleanup(lot, store, &created) ;
            return -1 ;
        }
    }

    if (!earlier_stage && synchronize(lot, store, &subscribe) < 0) {
        cleanup(lot, store, &created) ;
        return -1 ;
    }

    FOREACH_SELECTION_MEMBER(lot, index) {

        struct resolve_hash_s *h ;

        if (service_graph_resolve_get(store, index, 0, &h) <= 0 || settle(h, earlier_stage, bitset32_isvalid(&quiet, index), who) < 0) {
            cleanup(lot, store, &created) ;
            return -1 ;
        }
    }

    return 1 ;
}
