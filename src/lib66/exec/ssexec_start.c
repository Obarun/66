/*
 * ssexec_start.c
 *
 * Copyright (c) 2019 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <stdbool.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/ssexec.h>
#include <66/service.h>
#include <66/svc.h>
#include <66/command.h>
#include <66/constants.h>
#include <66/enum_parser.h>
#include <66/state.h>
#include <66/status.h>

static uint8_t opt_nopropagate = 0 ;

static opt_t const opts_start[] = {
    { .id = OPT_ID_HELP, .shortname = 'h', .longname = "help",         .arg = OPT_NONE, .help = "print this help" },
    { .id = 'P',         .shortname = 'P', .longname = "no-propagate", .arg = OPT_NONE, .help = "do not propagate signal to its dependencies" },
} ;

static int on_start(int id, char const *arg, void *data)
{
    (void)arg ;
    (void)data ;

    switch (id) {

        case 'P' :

            opt_nopropagate = 1 ;
            break ;
    }

    return 0 ;
}

opt_cmd_t const cmd_start = {
    .name = "66 start",
    .help = "bring up services",
    .operands = "service...",
    .opts = opts_start,
    .nopts = OPT_COUNT(opts_start),
    .on_option = &on_start,
    .fn = &ssexec_start,
} ;

static bool stopped_reactor(char const *base, char const *name, char *eventddir)
{
    log_flow() ;

    bool stopped = false ;
    struct resolve_hash_s h = RESOLVE_HASH_ZERO ;
    ss_state_t sta = STATE_ZERO ;
    service_status_t st = STATUS_ZERO ;

    int r = service_read(&h, base, name, SERVICE_READ_EVENT) ;
    if (r < 0)
        log_warnusys("read resolve file of: ", name) ;

    if (r <= 0)
        goto freed ;

    if ((h.res.type != E_PARSER_TYPE_ONESHOT && h.res.type != E_PARSER_TYPE_MODULE) || svc_reactor_armed_idle(&h) <= 0)
        goto freed ;

    // the first start arms it, and the arm already reads its sources
    if (state_read(&sta, &h.res) <= 0 || sta.issupervised != STATE_FLAGS_TRUE)
        goto freed ;

    if (svc_status(&h.res, &st) < 0) {
        log_warnusys("read status of: ", name) ;
        goto freed ;
    }

    stopped = st.state == STATUS_STATE_DOWN ;
    if (stopped)
        auto_strings(eventddir, h.res.sa.s + h.res.live.eventddir) ;

    freed:
        service_read_free(&h) ;
        return stopped ;
}

int ssexec_start(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    bool nopropagate = opt_nopropagate ;
    opt_nopropagate = 0 ;

    int e, pos = 0 ;
    strbuf frontends = STRBUF_ZERO ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    bool catchup[argc] ;
    char eventddir[SS_MAX_PATH_LEN + 1] ;

    for (; pos < argc ; pos++)
        catchup[pos] = !info->opt_react && stopped_reactor(info->base.s, argv[pos], eventddir) ;

    e = command_up(argv, argc, info, &frontends, info->target, nopropagate, SVC_LOT_COMMAND, false, false) ;

    strbuf_free(&frontends) ;

    for (pos = 0 ; !e && pos < argc ; pos++) {
        if (catchup[pos] && !svcd_notify(eventddir, 'c', info->who, argv[pos]))
            log_warnusys("ask catch up of event reactor: ", argv[pos]) ;
    }

    return e ;
}
