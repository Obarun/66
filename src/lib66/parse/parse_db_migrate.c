/*
 * parse_db_migrate.c
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

#include <oblibs/log.h>
#include <oblibs/stack.h>
#include <oblibs/string.h>
#include <oblibs/lexer.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/tree.h>
#include <66/constants.h>

static void service_db_tree(resolve_service_t *old, resolve_service_t *new, ssexec_t *info)
{
    log_flow() ;

    char *ocontents = old->sa.s + old->dependencies.contents ;
    char *ncontents = new->sa.s + new->dependencies.contents ;

    size_t pos = 0, olen = strlen(ocontents) ;
    _alloc_stk_(sremove, olen + 1) ;

    {
        _alloc_stk_(sold, olen + 1) ;

        if (!stack_string_clean(&sold, ocontents))
            log_dieusys(LOG_EXIT_SYS, "convert string") ;

        if (new->dependencies.ncontents) {

            size_t nlen = strlen(ncontents) ;
            _alloc_stk_(snew, nlen + 1) ;

            if (!stack_string_clean(&snew, ncontents))
                log_dieusys(LOG_EXIT_SYS, "convert string") ;

            FOREACH_STK(&sold, pos) {
                if (stack_retrieve_element(&snew, sold.s + pos) < 0) {
                    if (!stack_add_g(&sremove, sold.s + pos))
                        log_dieu(LOG_EXIT_SYS, "add string") ;
                }
            }
        }
    }

    if (sremove.len) {

        unsigned int m = 0 ;
        int nargc = 3 + sremove.count  ;
        char const *prog = PROG ;
        char const *newargv[nargc] ;

        char const *help = info->help ;
        char const *usage = info->usage ;

        info->help = help_remove ;
        info->usage = usage_remove ;

        newargv[m++] = "remove" ;
        newargv[m++] = "-P" ;

        pos = 0 ;
        FOREACH_STK(&sremove, pos) {

            char *name = sremove.s + pos ;
              if (get_rstrlen_until(name, SS_LOG_SUFFIX) < 0)
                newargv[m++] = name ;
        }

        newargv[m] = 0 ;

        PROG= "remove" ;
        int e = ssexec_remove(m, newargv, info) ;
        PROG = prog ;

        info->help = help ;
        info->usage = usage ;

        if (e)
            log_dieu(LOG_EXIT_SYS, "unable to remove selection from module: ", new->sa.s + new->name) ;
    }
}

void parse_db_migrate(resolve_service_t *res, ssexec_t *info)
{
    log_flow() ;

    int r ;
    resolve_service_t ores = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref owres = resolve_set_struct(DATA_SERVICE, &ores) ;

    /** Try to open the old resolve file.
     * Do not crash if it does not find it. User
     * can use -f options even if it's the first parse
     * process of the module.*/
    r = resolve_read_g(owres, info->base.s, res->sa.s + res->name) ;
    if (r < 0) {

        log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", res->sa.s + res->name) ;

    } else if (r) {

        /* depends */
        service_db_migrate(&ores, res, info->base.s, 0) ;

        /* requiredby */
        service_db_migrate(&ores, res, info->base.s, 1) ;

        /* contents of the previous tree */
        service_db_tree(&ores, res, info) ;
    }
    resolve_free(owres) ;
}
