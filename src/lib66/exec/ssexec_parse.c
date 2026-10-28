/*
 * ssexec_parse.c
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
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>
#include <oblibs/hash.h>
#include <oblibs/bits.h>

#include <66/config.h>
#include <66/parse.h>
#include <66/ssexec.h>
#include <66/graph.h>
#include <66/selection.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/utils.h>
#include <66/command.h>

static uint8_t opt_force = 0 ;
static uint8_t opt_conf = 0 ;

static opt_t const opts_parse[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",      .arg = OPT_NONE, .help = "print this help" },
    { .id = 'f',         .shortname = 'f', .longname = "force",     .arg = OPT_NONE, .help = "force to overwrite existing destination" },
    { .id = 'I',         .shortname = 'I', .longname = "no-import", .arg = OPT_NONE, .help = "do not import modified configuration files from previous version" },
} ;

static int on_parse(int id, char const *arg, void *data)
{
    (void)arg ;
    (void)data ;

    switch (id) {

        case 'f' :

            /** only rewrite the service itself */
            opt_force = 1 ;
            break ;

        case 'I' :

            opt_conf = 1 ;
            break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_parse = {
    .name = "66 parse",
    .help = "parse a frontend service file and install its result to resolve files",
    .operands = "service...",
    .opts = opts_parse,
    .nopts = OPT_COUNT(opts_parse),
    .on_option = &on_parse,
    .fn = &ssexec_parse,
} ;

static void remove_dropped(bitset32_t const *dropped, command_graph_t *d, ssexec_t *info)
{
    int r, e = 0 ;
    uint32_t index ;
    service_graph_t *g = &d->g ;
    service_graph_resolve_t *store = &d->store ;
    selection_service_t sel = SELECTION_SERVICE_ZERO ;
    selection_service_t sup = SELECTION_SERVICE_ZERO ;
    selection_service_policy_t policy = command_policy(SELECTION_SERVICE_PROPAGATE_NONE, false, false, false) ;

    selection_service_propagate(&sel, g, dropped, &policy) ;

    if (!sel.member.count) {
        service_graph_resolve_free(store) ;
        command_write_end(g) ;
        return ;
    }

    command_write_end(0) ;

    r = svc_scandir_ok(info->scandir.s) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "check the scandir: ", info->scandir.s) ;

    if (r) {

        command_resolve_load(d, &sel) ;

        e = command_free_lot(&sel, store, info, 0, 0) ;

        if (e) {

            if (selection_svc_supervised(&sup, &sel, store) < 0 ||
                selection_svc_filter(&sup, &sup, store, 0, command_outside_boot) < 0)
                    log_dieusys(LOG_EXIT_SYS, "select the services dropped by the parse") ;

            FOREACH_SELECTION_MEMBER(&sup, index)
                selection_service_remove(&sel, index) ;
        }
    }

    if (service_graph_write_begin(info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "lock the system") ;

    FOREACH_SELECTION_MEMBER(&sel, index) {

        if (service_remove_one(info->base.s, info->owner, service_graph_name(g, index)) < 0)
            log_dieusys(LOG_EXIT_SYS, "remove service: ", service_graph_name(g, index)) ;

        log_info("Removed successfully: ", service_graph_name(g, index)) ;
    }

    service_graph_resolve_free(store) ;
    command_write_end(g) ;

    if (e)
        log_dieu(LOG_EXIT_SYS, "stop the services dropped by the parse") ;
}

static void refuse_type_change(hash_t *hres, service_graph_t const *g, service_graph_resolve_t *store)
{
    struct resolve_hash_s *c, *tmp, *old ;

    HASH_FOREACH(hres, c, tmp) {

        uint32_t index = service_graph_find(g, c->name) ;

        if (index == SERVICE_GRAPH_NOINDEX)
            continue ;

        uint8_t state = service_graph_state(g, index) ;
        if (state == SERVICE_GRAPH_STATE_ABSENT || state == SERVICE_GRAPH_STATE_UNREADABLE)
            continue ;

        int r = service_graph_resolve_read(store, index, 0, &old) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "read service: ", c->name) ;

        if (!r || old->res.type == c->res.type)
            continue ;

        r = svc_issupervised(&old->res) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "read the state of service: ", c->name) ;

        if (r) {
            command_write_end(0) ;
            log_die(LOG_EXIT_USER, "service: ", c->name, " is supervised and its frontend changes its type -- free it first with 66 free ", c->name, ", or use 66 reconfigure ", c->name) ;
        }
    }
}

static void refuse_written(service_graph_t const *g, char const *const *argv, int argc)
{
    _cleanup_strbuf_ strbuf msg = STRBUF_ZERO ;
    _cleanup_strbuf_ strbuf last = STRBUF_ZERO ;
    strbuf frontends = STRBUF_ZERO ;

    for (int n = 0 ; n < argc ; n++) {

        char bname[strlen(argv[n]) + 1] ;
        char const *name = argv[n] ;

        if (name[0] == '/') {

            if (!ob_basename(bname, name))
                log_dieusys(LOG_EXIT_SYS, "get the name of: ", name) ;

            name = bname ;
        }

        uint32_t index = service_graph_find(g, name) ;

        if (index == SERVICE_GRAPH_NOINDEX)
            continue ;

        msg.len = 0 ;

        int r = service_graph_invalid_message(&msg, g, index, false) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "compose the message of service: ", name) ;

        if (!r)
            continue ;

        if (last.len)
            log_warn(last.s) ;

        last.len = 0 ;
        if (!auto_strbuf(&last, msg.s))
            log_die_nomem("strbuf") ;
    }

    if (last.len)
        log_die(LOG_EXIT_USER, last.s) ;

    if (service_graph_warn_invalid(g, &frontends, 0) < 0)
        log_dieusys(LOG_EXIT_SYS, "warn about the invalid services") ;

    strbuf_free(&frontends) ;
}

int ssexec_parse(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    int r = 0 ;
    uint32_t flags = PARSE_FRONTEND | (opt_force ? PARSE_FORCE : 0) | (opt_conf ? PARSE_CONF : 0) ;
    hash_t hres = HASH_ZERO ;
    command_graph_t d = { .g = SERVICE_GRAPH_ZERO, .store = SERVICE_GRAPH_RESOLVE_ZERO, .reloaded = true } ;
    bitset32_t dropped = bitset32_init(SS_MAX_SERVICE) ;
    _cleanup_strbuf_ strbuf msg = STRBUF_ZERO ;

    opt_force = 0 ;
    opt_conf = 0 ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    if (!hash_init(&hres, 0, offsetof(struct resolve_hash_s, node)))
        log_dieusys(LOG_EXIT_SYS, "initialize hash table") ;

    if (service_graph_load(&d.g, info->owner, info->base.s, info->live.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "load the graph of the system") ;

    for (int n = 0 ; n < argc ; n++) {

        char bname[strlen(argv[n]) + 1] ;
        char const *name = argv[n] ;

        if (name[0] == '/') {

            if (!ob_basename(bname, name))
                log_dieusys(LOG_EXIT_SYS, "get the name of: ", name) ;

            name = bname ;
        }

        r = name_isvalid(name, &msg) ;
        if (r < 0)
            log_dieusys(LOG_EXIT_SYS, "check the name: ", name) ;

        if (!r)
            log_die(LOG_EXIT_USER, msg.s) ;
    }

    r = parse_names(&hres, &d.g, argv, argc, info, flags, &msg) ;
    if (r < 0)
        log_dieusys(LOG_EXIT_SYS, "parse the services") ;

    if (!r)
        log_die(LOG_EXIT_USER, msg.s) ;

    if (!hash_count(&hres)) {
        resolve_hash_free(&hres) ;
        service_graph_free(&d.g) ;
        return 0 ;
    }

    if (!command_write_begin_checked(&hres, info, &msg))
        log_die(LOG_EXIT_USER, msg.s) ;

    if (service_graph_resolve_init(&d.store, &d.g, info->base.s) < 0)
        log_dieusys(LOG_EXIT_SYS, "initialize the store of the services") ;

    refuse_type_change(&hres, &d.g, &d.store) ;

    if (parse_dropped(&dropped, &hres, &d.g) < 0)
        log_dieusys(LOG_EXIT_SYS, "find the services the parse drops") ;

    if (parse_write(&hres, info) <= 0)
        log_dieusys(LOG_EXIT_SYS, "write the services") ;

    remove_dropped(&dropped, &d, info) ;

    refuse_written(&d.g, argv, argc) ;

    service_graph_free(&d.g) ;
    resolve_hash_free(&hres) ;

    return 0 ;
}
