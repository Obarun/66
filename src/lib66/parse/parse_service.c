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
#include <66/parse.h>
#include <66/ssexec.h>
#include <66/config.h>
#include <66/write.h>
#include <66/state.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/tree.h>
#include <66/graph.h>
#include <66/sanitize.h>

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
    unsigned int areslen = 0, count = 0, pos = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE] ;
    ss_state_t sta = STATE_ZERO ;

    char main[strlen(sv) + 1] ;
    if (!ob_basename(main, sv))
        log_dieu(LOG_EXIT_SYS, "get basename of: ", sv) ;

    r = parse_frontend(sv, ares, &areslen, info, force, conf, 0, main, 0) ;
    if (r == 2)
        /** already parsed */
        return ;

    /** parse_compute_resolve add the logger
     * to the ares array */
    count = areslen ;
    for (; pos < count ; pos++)
        parse_compute_resolve(pos, ares, &areslen, info) ;

    for (pos = 0 ; pos < areslen ; pos++) {

        sanitize_write(&ares[pos], force) ;

        write_services(&ares[pos], ares[pos].sa.s + ares[pos].path.servicedir) ;

        if (state_check(&ares[pos])) {

            if (!state_read(&sta, &ares[pos]))
                log_dieu(LOG_EXIT_SYS, "read state file of: ", ares[pos].sa.s + ares[pos].name) ;
        }

        state_set_flag(&sta, STATE_FLAGS_TOINIT, STATE_FLAGS_TRUE) ;
        state_set_flag(&sta, STATE_FLAGS_TOPARSE, STATE_FLAGS_FALSE) ;
        state_set_flag(&sta, STATE_FLAGS_ISPARSED, STATE_FLAGS_TRUE) ;
        state_set_flag(&sta, STATE_FLAGS_ISEARLIER, ares[pos].earlier ? STATE_FLAGS_TRUE : STATE_FLAGS_FALSE) ;
        state_set_flag(&sta, STATE_FLAGS_ISDOWNFILE, ares[pos].execute.down ? STATE_FLAGS_TRUE : STATE_FLAGS_FALSE) ;

        if (!state_write(&sta, &ares[pos]))
            log_dieu(LOG_EXIT_SYS, "write state file of: ", ares[pos].sa.s + ares[pos].name) ;

        tree_service_add(info->base.s, ares[pos].sa.s + ares[pos].treename, ares[pos].sa.s + ares[pos].name) ;

        log_info("Parsed successfully: ", ares[pos].sa.s + ares[pos].name, " at tree: ", ares[pos].sa.s + ares[pos].treename) ;
    }

    service_resolve_array_free(ares, areslen) ;
}
