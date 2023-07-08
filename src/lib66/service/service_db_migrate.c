/*
 * service_db_migrate.c
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

#include <string.h>
#include <stdint.h>

#include <oblibs/stack.h>
#include <oblibs/log.h>

#include <66/service.h>
#include <66/resolve.h>

void service_db_migrate(resolve_service_t *old, resolve_service_t *new, char const *base, uint8_t requiredby)
{
    log_flow() ;

    int r ;
    uint32_t *ofield = !requiredby ? &old->dependencies.depends : &old->dependencies.requiredby ;
    uint32_t *onfield = !requiredby ? &old->dependencies.ndepends : &old->dependencies.nrequiredby ;
    uint32_t *nfield = !requiredby ? &new->dependencies.depends : &new->dependencies.requiredby ;

    if (*onfield) {

        size_t pos = 0, olen = strlen(old->sa.s + *ofield) ;
        _init_stack_(sold, olen + 1) ;

        if (!stack_convert_string(&sold, old->sa.s + *ofield, olen))
            log_dieusys(LOG_EXIT_SYS, "convert string") ;

        {
            resolve_service_t dres = RESOLVE_SERVICE_ZERO ;
            resolve_wrapper_t_ref dwres = resolve_set_struct(DATA_SERVICE, &dres) ;

            size_t clen = strlen(new->sa.s + *nfield) ;
            _init_stack_(snew, clen + 1) ;

            /** new module configuration depends field may be empty.*/
            if (clen)
                if (!stack_convert_string(&snew, new->sa.s + *nfield, clen))
                    log_dieusys(LOG_EXIT_SYS, "convert string") ;

            /** check if the service was deactivated.*/
            FOREACH_STK(&sold, pos) {

                if (stack_retrieve_element(&snew, sold.s + pos) < 0 || !clen) {

                    uint32_t *dfield = requiredby ? &dres.dependencies.depends : &dres.dependencies.requiredby ;
                    uint32_t *dnfield = requiredby ? &dres.dependencies.ndepends : &dres.dependencies.nrequiredby ;

                    char *dname = sold.s + pos ;

                    r = resolve_read_g(dwres, base, dname) ;
                    if (r < 0)
                        log_die(LOG_EXIT_USER, "read resolve file of: ") ;

                    if (!r)
                        continue ;

                    if (*dnfield) {

                        size_t len = strlen(dres.sa.s + *dfield) ;
                        _init_stack_(stk, len + 1) ;

                        if (!stack_convert_string(&stk, dres.sa.s + *dfield, len))
                            log_dieusys(LOG_EXIT_SYS, "convert string to stack") ;

                        /** remove the module name to the depends field of the old service dependency*/
                        if (!stack_remove_element_g(&stk, new->sa.s + new->name))
                            log_dieusys(LOG_EXIT_SYS, "remove element") ;

                        (*dnfield) = (uint32_t)stack_count_element(&stk) ;

                        if (*dnfield) {

                            if (!stack_convert_tostring(&stk))
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
                             *

                            {
                                unsigned int m = 0 ;
                                int nargc = 4 ;
                                char const *prog = PROG ;
                                char const *newargv[nargc] ;

                                char const *help = info->help ;
                                char const *usage = info->usage  ;

                                info->help = help_disable ;
                                info->usage = usage_disable ;

                                newargv[m++] = "disable" ;
                                newargv[m++] = "-P" ;
                                newargv[m++] = dname ;
                                newargv[m] = 0 ;

                                PROG = "disable" ;
                                if (ssexec_disable(m, newargv, info))
                                    log_dieu(LOG_EXIT_SYS,"disable: ", dname) ;
                                PROG = prog ;

                                if (service_is_g(dname, STATE_FLAGS_ISSUPERVISED) == STATE_FLAGS_TRUE) {

                                    info->help = help_stop ;
                                    info->usage = usage_stop ;

                                    newargv[0] = "stop" ;
                                    newargv[1] = "-Pu" ;

                                    PROG = "stop" ;
                                    if (ssexec_stop(m, newargv, info))
                                        log_dieu(LOG_EXIT_SYS,"stop: ", dname) ;
                                    PROG = prog ;

                                    info->help = help ;
                                    info->usage = usage ;
                                }
                            }
                             */
                        }

                        if (!resolve_write_g(dwres, dres.sa.s + dres.path.home, dname))
                            log_dieusys(LOG_EXIT_SYS, "write resolve file of: ", dname) ;
                    }
                }
            }
            resolve_free(dwres) ;
        }
    }
}
