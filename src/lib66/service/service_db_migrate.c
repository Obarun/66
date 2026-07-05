/*
 * service_db_migrate.c
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

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <oblibs/sbl.h>
#include <oblibs/log.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/service.h>
#include <66/resolve.h>
#include <66/utils.h>
#include <66/parse.h>

static void get_frontend_list(strbuf *sa, const char *name, const char *frontend, bool requiredby)
{
    resolve_service_t dres = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &dres) ;
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_MAIN_ZERO ;
    table.u.parser.id = E_PARSER_SECTION_MAIN_DEPENDS ;
    size_t len = strlen(frontend) ;
    char dirname[len], basename[len] ;

    if (!ob_basename(basename, frontend))
        log_dieu(LOG_EXIT_SYS, "get basename of: ", frontend) ;

    if (!ob_dirname(dirname, frontend))
        log_dieu(LOG_EXIT_SYS, "get dirname of: ", frontend) ;

    if (read_svfile(sa, basename, dirname) <= 0)
        log_dieu(LOG_EXIT_SYS, "read frontend service at: ", frontend) ;

    if (!identifier_replace(sa, name))
        log_dieu(LOG_EXIT_SYS, "replace regex for service: ", frontend) ;

     _alloc_sbl_(stk, sa->len + 1) ;

    /** field may not exist*/
    if (!parse_get_value_of_key(&stk, sa->s, table)) {
        log_warn("no field ", enum_to_key(table.u.parser.list, table.u.parser.id)," exist for service: ", basename) ;
        resolve_free(wres) ;
        return ;
    }


    table.u.parser.id = requiredby ? E_PARSER_SECTION_MAIN_REQUIREDBY :  E_PARSER_SECTION_MAIN_DEPENDS ;

    if (!parse_store_main(&dres, &stk, table))
        log_dieu(LOG_EXIT_SYS, "get field depends of service: ", basename) ;

    sa->len = 0 ;
    if ((requiredby ? dres.dependencies.nrequiredby : dres.dependencies.ndepends))
       if (!sbl_clean_string(sa, dres.sa.s + (requiredby ? dres.dependencies.requiredby : dres.dependencies.depends)))
           log_dieusys(LOG_EXIT_SYS,"clean the string") ;

    resolve_free(wres) ;
}

void service_db_migrate(resolve_service_t *old, resolve_service_t *new, char const *base, uint8_t requiredby)
{
    log_flow() ;


    uint32_t *ofield = !requiredby ? &old->dependencies.depends : &old->dependencies.requiredby ;
    uint32_t *onfield = !requiredby ? &old->dependencies.ndepends : &old->dependencies.nrequiredby ;
    uint32_t *nfield = !requiredby ? &new->dependencies.depends : &new->dependencies.requiredby ;

    if (*onfield) {

        _cleanup_strbuf_ strbuf frontend = STRBUF_ZERO ; _cleanup_strbuf_ strbuf dfront = STRBUF_ZERO ;
        size_t pos = 0, olen = strlen(old->sa.s + *ofield) ;
        _alloc_sbl_(sold, olen + 1) ;
        size_t clen = strlen(new->sa.s + *nfield) ;
        _alloc_sbl_(snew, clen + 1) ;
        resolve_service_t dres = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref dwres = resolve_set_struct(DATA_SERVICE, &dres) ;
        int r ;

        get_frontend_list(&frontend, old->sa.s + old->name, old->sa.s + old->path.frontend, (!requiredby ? false : true)) ;

        if (!sbl_clean_string(&sold, old->sa.s + *ofield))
            log_dieusys(LOG_EXIT_SYS, "convert string") ;

        /** new module configuration depends field may be empty.*/
        if (clen)
            if (!sbl_clean_string(&snew, new->sa.s + *nfield))
                log_dieusys(LOG_EXIT_SYS, "convert string") ;

        /** check if the service was deactivated.*/
        FOREACH_SBL(&sold, pos) {

            dfront.len = 0 ;
            char *dname = sold.s + pos ;
            dres = service_resolve_zero ;

            r = resolve_read(dwres, base, dname) ;
            if (r < 0)
                log_die(LOG_EXIT_USER, "read resolve file of: ") ;
            if (!r)
                continue ;

            get_frontend_list(&dfront, dres.sa.s + dres.name, dres.sa.s + dres.path.frontend, (!requiredby ? false : true)) ;

            if ((sbl_search(&snew, dname) < 0 || !clen) && sbl_search(&frontend, dname) < 0 && sbl_search(&dfront, old->sa.s + old->name) < 0) {

                uint32_t *dfield = requiredby ? &dres.dependencies.depends : &dres.dependencies.requiredby ;
                uint32_t *dnfield = requiredby ? &dres.dependencies.ndepends : &dres.dependencies.nrequiredby ;

                if (*dnfield) {

                    size_t len = strlen(dres.sa.s + *dfield) ;
                    _alloc_sbl_(stk, len + 1) ;

                    if (!sbl_clean_string(&stk, dres.sa.s + *dfield))
                        log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

                    /** remove the module name to the depends field of the old service dependency*/
                    if (!sbl_remove(&stk, new->sa.s + new->name))
                        log_dieusys(LOG_EXIT_SYS, "remove element") ;

                    (*dnfield) = (uint32_t)sbl_count(&stk) ;

                    if (*dnfield) {

                        if (!sbl_rebuild_with_delim(&stk, ' '))
                            log_dieusys(LOG_EXIT_SYS, "convert stack to string") ;

                        (*dfield) = resolve_add_string(dwres, stk.s) ;

                    } else {

                        (*dfield) = resolve_add_string(dwres, "") ;

                        /** If the module was enabled, the service dependency was as well.
                         * If the service dependency was only activated by the module
                         * (meaning the service only has the module as a "depends" dependency),
                         * the service should also be disabled.
                         *
                         * The point is: 66 WORK ON MECHANISM NOT POLICIES!
                         *
                         * */
                    }

                    if (!resolve_write(dwres, dres.sa.s + dres.path.home, dname))
                        log_dieusys(LOG_EXIT_SYS, "write resolve file of: ", dname) ;
                }
            }
        }
        resolve_free(dwres) ;
    }
}
