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


/***
 *
 *
 * This function is not fully operationnal.
 *
 * Need to classify the all the field of the Master resolve file
 *
 *
 * */
void service_master_modify_contents(resolve_service_t *res)
{
    stralloc sa = STRALLOC_ZERO ;
    resolve_service_master_t mres = RESOLVE_SERVICE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE_MASTER, &mres) ;
    char *treename = res->sa.s + res->treename ;
    char *tree = res->sa.s + res->path.tree ;
    size_t treelen = strlen(tree) ;
    char solve[treelen + SS_SVDIRS_LEN + SS_RESOLVE_LEN + 1] ;

    char const *exclude[2] = { SS_MASTER + 1, 0 } ;

    log_trace("modify field contents of resolve Master file of services") ;

    auto_strings(solve, tree, SS_SVDIRS, SS_RESOLVE) ;

    if (!sastr_dir_get(&sa, solve, exclude, S_IFREG))
        log_dieu(LOG_EXIT_SYS, "get resolve files of tree: ", treename) ;

    size_t ncontents = sastr_nelement(&sa) ;

    if (ncontents)
        if (!sastr_rebuild_in_oneline(&sa))
            log_dieu(LOG_EXIT_SYS, "rebuild stralloc") ;

    auto_strings(solve, tree, SS_SVDIRS) ;

    if (!resolve_read(wres, solve, SS_MASTER + 1))
        log_dieusys(LOG_EXIT_SYS, "read resolve Master file of services") ;

    mres.ncontents = (uint32_t)ncontents ;

    if (ncontents)
        mres.contents = resolve_add_string(wres, sa.s) ;
    else
        mres.contents = resolve_add_string(wres, "") ;

    if (!resolve_write(wres, solve, SS_MASTER + 1))
        log_dieusys(LOG_EXIT_SYS, "write resolve Master file of services") ;

    stralloc_free(&sa) ;
    resolve_free(wres) ;
}

void parse_service(char const *sv, ssexec_t *info, uint8_t force, uint8_t conf)
{
    unsigned int areslen = 0, residx = 0, pos = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE] ;

    int r = parse_frontend(sv, ares, &areslen, info, force, conf, &residx, 0) ;
    if (r == 2)
        /** already parsed */
        return ;

    for (; pos < areslen ; pos++) {

        char dst[strlen(ares[pos].sa.s + ares[pos].path.tree) + SS_SVDIRS_LEN + 1] ;
        auto_strings(dst, ares[pos].sa.s + ares[pos].path.tree, SS_SVDIRS) ;

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

        service_master_modify_contents(&ares[pos]) ;

        log_info("Parsed successfully: ", ares[pos].sa.s + ares[pos].name, " at tree: ", ares[pos].sa.s + ares[pos].treename) ;
    }

    service_resolve_array_free(ares, areslen) ;
}
