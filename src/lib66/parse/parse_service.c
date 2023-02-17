/*
 * parse_service.c
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
#include <stdint.h>

#include <stddef.h>

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>
#include <oblibs/types.h>

#include <skalibs/genalloc.h>

#include <66/enum.h>
#include <66/constants.h>
#include <66/parser.h>
#include <66/ssexec.h>
#include <66/config.h>
#include <66/write.h>
#include <66/state.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>

parse_mill_t MILL_GET_SECTION_NAME = \
{ \
    .open = '[', .close = ']', \
    .forceclose = 1, .forceskip = 1, \
    .skip = " \t\r", .skiplen = 3, \
    .inner.debug = "get_section_name" } ;

parse_mill_t MILL_GET_KEY = \
{ \
    .close = '=', .forceclose = 1, \
    .forceopen = 1, .keepopen = 1, \
    .skip = " \n\t\r", .skiplen = 4, .forceskip = 1, \
    .inner.debug = "get_key_nclean" } ;

parse_mill_t MILL_GET_VALUE = \
{ \
    .open = '=', .close = '\n', .forceclose = 1, \
    .skip = " \t\r", .skiplen = 3, .forceskip = 1, \
    .inner.debug = "get_value" } ;

void parse_service(char const *sv, ssexec_t *info, uint8_t force, uint8_t conf)
{
    log_flow();

    int r ;
    unsigned int areslen = 0, residx = 0, pos = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE] ;

    char main[strlen(sv) + 1] ;
    if (!ob_basename(main, sv))
        log_dieu(LOG_EXIT_SYS, "get basename of: ", sv) ;

    r = parse_frontend(sv, ares, &areslen, info, force, conf, &residx, 0, main) ;
    if (r == 2)
        /** already parsed */
        return ;

    for (; pos < areslen ; pos++) {

        if (force) {

            resolve_service_t res = RESOLVE_SERVICE_ZERO ;
            resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
            /** warn old tree of the change and modify the desired one.
             * Force was used so we are supposed to have an existing resolve file of
             * the service. Before overwritting its resolve file, check if the tree is the same.*/
            if (resolve_read_g(wres, info->base.s, ares[pos].sa.s + ares[pos].name) &&
                strcmp(ares[pos].sa.s + ares[pos].treename, res.sa.s + res.treename))
                    tree_service_remove(info->base.s, res.sa.s + res.treename, res.sa.s + res.name) ;
        }

        parse_compute_resolve(&ares[pos], info) ;
        tree_service_add(info->base.s, ares[pos].sa.s + ares[pos].treename, ares[pos].sa.s + ares[pos].name) ;

        char dst[strlen(ares[pos].sa.s + ares[pos].path.home) + SS_SYSTEM_LEN + SS_SERVICE_LEN + 1] ;
        auto_strings(dst, ares[pos].sa.s + ares[pos].path.home, SS_SYSTEM, SS_SERVICE) ;

        write_services(&ares[pos], dst, force) ;

        ss_state_t sta = STATE_ZERO ;
        FLAGS_SET(sta.toinit, STATE_FLAGS_TRUE) ;
        FLAGS_SET(sta.isparsed, STATE_FLAGS_TRUE) ;
        FLAGS_SET(sta.isearlier, ares[pos].earlier ? STATE_FLAGS_TRUE : STATE_FLAGS_FALSE) ;
        FLAGS_SET(sta.isdownfile, ares[pos].execute.down ? STATE_FLAGS_TRUE : STATE_FLAGS_FALSE) ;

        if (!state_write(&sta, ares[pos].sa.s + ares[pos].path.home, ares[pos].sa.s + ares[pos].name))
            log_dieu(LOG_EXIT_SYS, "write state file of: ", ares[pos].sa.s + ares[pos].name) ;

        if (ares[pos].logger.name && ares[pos].type == TYPE_CLASSIC)
            if (!state_write(&sta, ares[pos].sa.s + ares[pos].path.home, ares[pos].sa.s + ares[pos].logger.name))
                log_dieu(LOG_EXIT_SYS, "write state file of: ", ares[pos].sa.s + ares[pos].logger.name) ;

        log_info("Parsed successfully: ", ares[pos].sa.s + ares[pos].name, " at tree: ", ares[pos].sa.s + ares[pos].treename) ;
    }

    service_resolve_array_free(ares, areslen) ;
}
