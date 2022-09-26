/*
 * ssexec_parse.c
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

#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/obgetopt.h>

#include <66/parser.h>
#include <66/ssexec.h>

static void check_dir(char const *dir, uint8_t force, int main)
{
    log_flow() ;

    int r ;

    r = scan_mode(dir, S_IFDIR) ;
    if (r < 0) {
        errno = ENOTDIR ;
        log_diesys(LOG_EXIT_SYS,"conflicting format of: ",dir) ;
    }

    if (r && force && main) {

        if ((dir_rm_rf(dir) < 0) || !r )
            log_dieusys(LOG_EXIT_SYS,"sanitize directory: ",dir) ;
        r = 0 ;

    } else if (r && !force && main)
        log_die(LOG_EXIT_SYS,"destination: ",dir," already exist") ;

    if (!r)
        if (!dir_create_parent(dir, 0755))
            log_dieusys(LOG_EXIT_SYS,"create: ",dir) ;
}

int ssexec_parse(int argc, char const *const *argv, ssexec_t *info)
{
    char const *dir ;
    char const *sv  ;

    uint8_t force = 0 , conf = 0 ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">" OPTS_PARSE, &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER, "options must be set first") ;
            switch (opt)
            {
                case 'f' :

                    /** only rewrite the service itself */
                    force = 1 ;
                    break ;

                case 'F' :

                    /** force to rewrite it dependencies */
                    force = 2 ;
                    break ;

                case 'I' :

                    conf = 1 ;
                    break ;

                case 'c' :  log_1_warn("deprecated option -- ignoring") ; break ;
                case 'm' :  log_1_warn("deprecated option -- ignoring") ; break ;
                case 'C' :  log_1_warn("deprecated option -- ignoring") ; break ;

                default :
                    log_usage(usage_parse) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 2) log_usage(usage_parse) ;

    sv = argv[0] ;
    dir = argv[1] ;

    if (dir[0] != '/')
        log_die(LOG_EXIT_USER, "directory: ",dir," must be an absolute path") ;

    if (sv[0] != '/')
        log_die(LOG_EXIT_USER, "service: ",sv," must be an absolute path") ;

    char name[strlen(sv)] ;

    if (!ob_basename(name, sv))
        log_dieusys(LOG_EXIT_SYS, "get basename of: ", sv) ;

    parser(sv, dir, info, force, 0) ;

    log_info("Written successfully: ",name, " at: ",dir) ;

    return 0 ;
}
