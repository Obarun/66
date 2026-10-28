/*
 * command_free_unreadable.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
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
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/bits.h>
#include <oblibs/sbl.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/ssexec.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/parse.h>
#include <66/event.h>
#include <66/svc.h>
#include <66/tree.h>
#include <66/fdholder.h>
#include <66/enum_parser.h>
#include <66/command.h>

// whether the graph holds the logger of @name, the vertex <name>-log
static bool has_logger(service_graph_t const *g, char const *name)
{
    char logger[strlen(name) + SS_LOG_SUFFIX_LEN + 1] ;

    auto_strings(logger, name, SS_LOG_SUFFIX) ;

    return service_graph_find_parsed(g, logger) != SERVICE_GRAPH_NOINDEX ;
}

/* @name as its scandir entry knows it, made of its name and of the places the
 * parse gives it under the live directory of @info: a classic, with its logger
 * under @logger. Nothing is read: its resolve file does not read. Its event
 * addon is known as none, so it runs by its status; the disarm by its name is
 * sent all the same, and 66-eventd does nothing for a name it does not hold */
static int made_of(struct resolve_hash_s *h, char const *name, bool logger, ssexec_t *info)
{
    ssize_t off ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE, &h->res) ;

    *h = (struct resolve_hash_s)RESOLVE_HASH_ZERO ;
    resolve_init(&w) ;

    auto_strings(h->name, name) ;
    h->read = SERVICE_READ_EVENT ;
    h->res.type = E_PARSER_TYPE_CLASSIC ;
    h->res.owner = info->owner ;
    h->res.has_event = 1 ;
    h->res.logger = logger ? 1 : 0 ;

    off = resolve_add_string(&w, name) ;
    if (off < 0)
        return -1 ;

    h->res.name = (uint32_t)off ;

    off = resolve_add_string(&w, info->ownerstr) ;
    if (off < 0)
        return -1 ;

    h->res.ownerstr = (uint32_t)off ;

    off = resolve_add_string(&w, info->base.s) ;
    if (off < 0)
        return -1 ;

    h->res.path.home = (uint32_t)off ;

    return parse_compute_live(&w, info) ;
}

/* a tree of the boot group keeps the supervision of its services, the change
 * applies at the next boot: the tree is the one whose contents name @name */
static int in_boot(char const *base, char const *name)
{
    int r ;
    _cleanup_strbuf_ strbuf tree = STRBUF_ZERO ;

    r = tree_find_service(&tree, base, name) ;
    if (r <= 0)
        return r ;

    return tree_ongroups(base, tree.s, TREE_GROUPS_BOOT) ;
}

/* @name brought down through its scandir entry when a supervisor holds it
 * there, its state said in @up or @down: 1 when it is down or no supervisor
 * holds it, *@held telling the latter apart; 0 when it did not stop; -1 on a
 * system error */
static int stop_one(char const *name, bool logger, ssexec_t *info, strbuf *up, strbuf *down, bool *held)
{
    int r, e = -1 ;
    struct resolve_hash_s h ;
    int timeout = info->timeout > INT_MAX ? INT_MAX : (int)info->timeout ;

    *held = false ;

    if (made_of(&h, name, logger, info) < 0)
        goto freed ;

    r = svc_issupervised(&h.res) ;
    if (r < 0)
        goto freed ;

    e = 1 ;

    if (!r)
        goto freed ;

    /** a oneshot or a module has no supervisor of its own: its entry is hidden
     * from the scandir, nothing is sent to it */
    r = scan_mode(h.res.sa.s + h.res.live.scandir, FLAGS_DIR) ;
    if (!r && errno != ENOENT) {
        e = -1 ;
        goto freed ;
    }

    if (r <= 0) {
        log_warn("skipping service without a supervisor in the scandir: ", name) ;
        goto freed ;
    }

    if (up) {

        // a supervisor holds it, nothing changed since its state was read above
        r = svc_isrunning_supervised(&h) ;
        if (r < 0 || !sbl_add(r ? up : down, name)) {
            e = -1 ;
            goto freed ;
        }
    }

    r = svc_send_daemon(h.res.sa.s + h.res.live.scandir, "d", info->who, EVENT_DOWN_READY, timeout) ;
    if (r < 0) {
        e = -1 ;
        goto freed ;
    }

    if (!r) {
        log_warnu("stop service: ", name) ;
        e = 0 ;
        goto freed ;
    }

    log_info("Successfully stopped service: ", name) ;
    *held = true ;

    freed:
        service_read_free(&h) ;
        return e ;
}

// @name taken out of the supervision as free does, disarmed included
static int take_out(char const *name, bool logger, ssexec_t *info, fdholder_client_t *a, bool *started)
{
    int e = -1 ;
    struct resolve_hash_s h ;

    if (made_of(&h, name, logger, info) >= 0)
        e = svc_unsupervise_one(&h.res, a, started, info->who) ;

    service_read_free(&h) ;

    return e ;
}

int command_free_unreadable(selection_service_t const *sel, ssexec_t *info, strbuf *up, strbuf *down)
{
    log_flow() ;

    int r, e = 0 ;
    size_t pos = 0 ;
    uint32_t index ;
    bool held, started = false ;
    fdholder_client_t a ;
    service_graph_t const *g = sel->g ;
    // what was stopped, in the order it is taken out: a service before its logger
    _cleanup_strbuf_ strbuf taken = STRBUF_ZERO ;

    FOREACH_SELECTION_MEMBER(sel, index) {

        char const *name = service_graph_name(g, index) ;

        if (!bitset32_isvalid(&sel->named, index) || service_graph_state(g, index) != SERVICE_GRAPH_STATE_UNREADABLE)
            continue ;

        r = in_boot(info->base.s, name) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "find the tree of service: ", name) ;

        if (r)
            continue ;

        bool logger = has_logger(g, name) ;
        char logname[strlen(name) + SS_LOG_SUFFIX_LEN + 1] ;

        auto_strings(logname, name, SS_LOG_SUFFIX) ;

        r = stop_one(name, logger, info, up, down, &held) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "stop service: ", name) ;

        if (!r)
            e = LOG_EXIT_SYS ;

        if (held && !sbl_add(&taken, name))
            log_die_nomem("strbuf") ;

        if (!logger)
            continue ;

        // the logger stops with its service, its own state said: it comes back as it was
        r = stop_one(logname, false, info, up, down, &held) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "stop service: ", logname) ;

        if (!r)
            e = LOG_EXIT_SYS ;

        if (held && !sbl_add(&taken, logname))
            log_die_nomem("strbuf") ;
    }

    // nothing is taken out when one did not stop
    if (e)
        return e ;

    FOREACH_SBL(&taken, pos) {

        char const *name = taken.s + pos ;

        r = take_out(name, has_logger(g, name), info, &a, &started) ;
        if (r < 0) {
            int err = errno ;
            if (started)
                fdholder_client_end(&a) ;
            errno = err ;
            log_dieusys(LOG_EXIT_SYS, "unsupervise service: ", name) ;
        }
    }

    if (started)
        fdholder_client_end(&a) ;

    return 0 ;
}
