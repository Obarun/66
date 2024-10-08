/*
 * ssexec_parse.c
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/sastr.h>

#include <skalibs/sgetopt.h>

#include <66/parse.h>
#include <66/ssexec.h>
#include <66/utils.h>
#include <66/sanitize.h>
#include <66/module.h>

int ssexec_parse(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow() ;

    int r = 0 ;
    uint8_t force = 0 , conf = 0 ;
    stralloc sa = STRALLOC_ZERO ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_PARSE, &l) ;
            if (opt == -1) break ;

            switch (opt)
            {
                case 'h' :

                    info_help(info->help, info->usage) ;
                    return 0 ;

                case 'f' :

                    /** only rewrite the service itself */
                    if (force)
                        log_usage(info->usage, "\n", info->help) ;
                    force = 1 ;
                    break ;

                case 'I' :

                    conf = 1 ;
                    break ;

                default :
                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(info->usage, "\n", info->help) ;

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
            _alloc_stk_(stk, len + 1) ;
            stack_add(&stk, sv, --r) ; // do not check output here, we are dying anyway
            stack_close(&stk) ;
            log_die(LOG_EXIT_USER, "service: ", sv," is part of a module and cannot be parsed alone -- please parse the entire module instead using \'66 parse ", stk.s, "\'") ;
        }

        if (!service_frontend_path(&sa, sv, info->owner, directory_forced, exclude, exlen))
            log_dieu(LOG_EXIT_USER, "find service frontend file of: ", sv) ;

        /** need to check all the contents of the stralloc.
         * service can be a directory name. In this case
         * we parse all services inside. */
        size_t pos = 0 ;
        struct resolve_hash_s *hres = NULL ;
        FOREACH_SASTR(&sa, pos)
            parse_service(&hres, sa.s + pos, info, force, conf) ;

        hash_free(&hres) ;
    }

    stralloc_free(&sa) ;
    sanitize_graph(info) ;

    return 0 ;
}
