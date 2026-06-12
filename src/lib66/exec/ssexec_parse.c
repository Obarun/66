/*
 * ssexec_parse.c
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
#include <errno.h>
#include <sys/stat.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/sbl.h>
#include <oblibs/strbuf.h>

#include <66/parse.h>
#include <66/ssexec.h>
#include <66/utils.h>
#include <66/sanitize.h>
#include <66/module.h>

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

int ssexec_parse(int argc, char const *const *argv, void *data)
{
    log_flow() ;

    ssexec_t *info = data ;

    int r = 0 ;
    /* drain the option state into locals and reset the statics: parsing a
     * service re-dispatches "parse" for its dependencies (without -f), so the
     * statics must not leak across that re-entry. */
    uint8_t force = opt_force , conf = opt_conf ;
    opt_force = 0 ;
    opt_conf = 0 ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing service argument") ;

    for (int n = 0 ; n < argc && argv[n] ; n++) {

        sa.len = 0 ;
        size_t namelen = strlen(argv[n]) ;
        char const *sv = 0 ;
        char bname[namelen + 1] ;
        char dname[namelen + 1] ;
        char const *directory_forced = 0 ;
        uint8_t exlen = 2 ;
        char const *exclude[2] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1 } ;

        if (argv[0][0] == '/') {

            if (!ob_dirname(dname, argv[n]))
                log_dieu(LOG_EXIT_SYS, "get dirname of: ", argv[n]) ;

            if (!ob_basename(bname, argv[n]))
                log_dieu(LOG_EXIT_SYS, "get basename of: ", argv[n]) ;

            sv = bname ;
            directory_forced = dname ;

        } else
            sv = argv[n] ;

        name_isvalid(sv) ;

        r = str_contain(sv, ":") ;
        if (r >= 0) {
            size_t len = strlen(sv) ;
            _alloc_sbl_(stk, len + 1) ;
            sbl_addb(&stk, sv, --r) ; // do not check output here, we are dying anyway
            log_die(LOG_EXIT_USER, "service: ", sv," is part of a module and cannot be parsed alone -- please parse the entire module instead using \'66 parse ", stk.s, "\'") ;
        }

        if (!service_frontend_path(&sa, sv, info->owner, directory_forced, exclude, exlen))
            log_dieu(LOG_EXIT_USER, "find service frontend file of: ", sv) ;

        /** need to check all the contents of the strbuf.
         * service can be a directory name. In this case
         * we parse all services inside. */
        size_t pos = 0 ;
        struct resolve_hash_s *hres = NULL ;
        FOREACH_SBL(&sa, pos)
            parse_service(&hres, sa.s + pos, info, force, conf) ;

        hash_free(&hres) ;
    }

    sanitize_graph(info) ;

    return 0 ;
}
