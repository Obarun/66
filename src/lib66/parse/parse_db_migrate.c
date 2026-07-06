/*
 * parse_db_migrate.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
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
#include <string.h>

#include <oblibs/log.h>
#include <oblibs/sbl.h>
#include <oblibs/string.h>
#include <oblibs/lexer.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/constants.h>

static void service_db_tree(resolve_service_t *old, resolve_service_addon_dependencies_t *olddep, resolve_service_t *new, resolve_service_addon_dependencies_t *newdep, ssexec_t *info)
{
    log_flow() ;
    (void)old ;
    (void)new ;

    char *ocontents = olddep->sa.s + olddep->contents ;
    char *ncontents = newdep->sa.s + newdep->contents ;

    size_t pos = 0, olen = strlen(ocontents) ;
    _alloc_sbl_(sremove, olen + 1) ;

    {
        _alloc_sbl_(sold, olen + 1) ;

        if (!sbl_clean_string(&sold, ocontents))
            log_dieusys(LOG_EXIT_SYS, "convert string") ;

        if (newdep->ncontents) {

            size_t nlen = strlen(ncontents) ;
            _alloc_sbl_(snew, nlen + 1) ;

            if (!sbl_clean_string(&snew, ncontents))
                log_dieusys(LOG_EXIT_SYS, "convert string") ;

            FOREACH_SBL(&sold, pos) {
                if (sbl_search(&snew, sold.s + pos) < 0) {
                    if (!sbl_add(&sremove, sold.s + pos))
                        log_dieu(LOG_EXIT_SYS, "add string") ;
                }
            }
        }
    }

    if (sremove.len) {

        unsigned int m = 0 ;
        int nargc = 3 + sbl_count(&sremove)  ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;

        newargv[m++] = "remove" ;
        newargv[m++] = "-Pf" ;

        pos = 0 ;
        FOREACH_SBL(&sremove, pos) {

            char *name = sremove.s + pos ;
              if (get_rstrlen_until(name, SS_LOG_SUFFIX) < 0)
                newargv[m++] = name ;
        }

        newargv[m] = 0 ;

        PROG= "remove" ;
        int e = opt_dispatch(m, newargv, &cmd_remove, info) ;
        PROG = prog ;

        if (e)
            log_dieu(LOG_EXIT_SYS, "unable to remove selection from module: ", new->sa.s + new->name) ;
    }
}

void parse_db_migrate(resolve_service_t *res, resolve_service_addon_dependencies_t *dep, ssexec_t *info)
{
    log_flow() ;

    int r ;
    resolve_service_t ores = RESOLVE_SERVICE_ZERO ;
    resolve_service_addon_dependencies_t oresdep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    resolve_wrapper_t_ref owres = resolve_set_struct(DATA_SERVICE, &ores) ;

    /** Try to open the old resolve file.
     * Do not crash if it does not find it. User
     * can use -f options even if it's the first parse
     * process of the module.*/
    r = resolve_read(owres, info->base.s, res->sa.s + res->name) ;
    if (r < 0) {

        log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", res->sa.s + res->name) ;

    } else if (r) {

        /** the old module's dependencies live in its own addon on disk */
        if (ores.has_dependencies) {
            resolve_wrapper_t_ref odw = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &oresdep) ;
            if (resolve_read(odw, info->base.s, ores.sa.s + ores.name) <= 0)
                log_dieusys(LOG_EXIT_SYS, "read dependencies addon of: ", ores.sa.s + ores.name) ;
            free(odw) ;
        }

        /* depends */
        service_db_migrate(&ores, &oresdep, res, dep, info->base.s, 0) ;

        /* requiredby */
        service_db_migrate(&ores, &oresdep, res, dep, info->base.s, 1) ;

        /* contents of the previous tree */
        service_db_tree(&ores, &oresdep, res, dep, info) ;
    }
    strbuf_free(&oresdep.sa) ;
    resolve_free(owres) ;
}
