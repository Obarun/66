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


/***
 *
 * Not sure of this implementation. The contents of the a tree can be found easily
 * now with the SS_SYSTEM_DIR/system/.resolve/service directory. This needs is only necessary
 * at worth for the init process to quickly found a service. On the other side, each manipulation
 * of the tree need to be specified at Master service file.
 *
 * Actually, init do not pass through the Master service file of the tree. Maybe a Master service for
 * all trees localized at SS_SYSTEM_DIR/system/.resolve/service/Master can be a better way to do it.
 *
 * At the end, i think that good API to know/acknowledge of a global system state changes should be provide. After all, handling events will appear in the future.
 *
 *
 *
void service_master_modify_contents(resolve_service_t *res, resolve_service_master_enum_t ENUM)
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

    switch (ENUM) {

        case E_RESOLVE_SERVICE_MASTER_CLASSIC:

            if (mres.nclassic)
                mres.classic = resolve_add_string(wres, sa.s) ;
            else
                mres.classic = resolve_add_string(wres, "") ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_BUNDLE:

            if (mres.nbundle)
                mres.bundle = resolve_add_string(wres, sa.s) ;
            else
                mres.bundle = resolve_add_string(wres, "") ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_ONESHOT:

            if (mres.noneshot)
                mres.oneshot = resolve_add_string(wres, sa.s) ;
            else
                mres.oneshot = resolve_add_string(wres, "") ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_MODULE:

            if (mres.nmodule)
                mres.module = resolve_add_string(wres, sa.s) ;
            else
                mres.module = resolve_add_string(wres, "") ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_ENABLED:

            if (mres.nenabled)
                mres.enabled = resolve_add_string(wres, sa.s) ;
            else
                mres.enabled = resolve_add_string(wres, "") ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_DISABLED:

            if (mres.ndisabled)
                mres.disabled = resolve_add_string(wres, sa.s) ;
            else
                mres.disabled = resolve_add_string(wres, "") ;
            break ;

        case E_RESOLVE_SERVICE_MASTER_CONTENTS:

            if (mres.ncontents)
                mres.contents = resolve_add_string(wres, sa.s) ;
            else
                mres.contents = resolve_add_string(wres, "") ;

            //mres.ncontents = (uint32_t)ncontents ;

            break ;

        default:
            break ;
    }

    if (!resolve_write(wres, solve, SS_MASTER + 1))
        log_dieusys(LOG_EXIT_SYS, "write resolve Master file of services") ;

    stralloc_free(&sa) ;
    resolve_free(wres) ;
}
*/

static void service_notify_add_string(stralloc *sa, char const *name, char const *str)
{
    if (!sastr_clean_string(sa, str))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    if (!sastr_add_string(sa, name))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    if (!sastr_sortndrop_element(sa))
        log_dieu(LOG_EXIT_SYS, "sort string") ;
}

static void service_notify_tree(char const *name, char const *base, char const *treename, uint8_t field)
{
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    stralloc sa = STRALLOC_ZERO ;

    log_trace("modify field contents of resolve tree file: ", treename) ;

    if (!resolve_read_g(wres, base, treename))
        log_dieusys(LOG_EXIT_SYS, "read resolve file of tree: ", treename) ;

    if (field == E_RESOLVE_TREE_CONTENTS) {

        if (tres.ncontents)
            service_notify_add_string(&sa, name, tres.sa.s + tres.contents) ;
        else if (!sastr_add_string(&sa, name))
            log_dieu(LOG_EXIT_SYS, "add string") ;

        tres.ncontents += 1 ;
    } else goto freed ;

    if (!sastr_rebuild_in_oneline(&sa))
        log_dieu(LOG_EXIT_SYS, "rebuild stralloc list") ;

    if (!resolve_modify_field(wres, field, sa.s))
        log_dieusys(LOG_EXIT_SYS, "modify resolve file of: ", treename) ;

    if (!resolve_write_g(wres, base, treename))
        log_dieusys(LOG_EXIT_SYS, "write resolve file of tree: ", treename) ;

    freed :
    stralloc_free(&sa) ;
    resolve_free(wres) ;
}

void parse_service(char const *sv, ssexec_t *info, uint8_t force, uint8_t conf)
{
    unsigned int areslen = 0, residx = 0, pos = 0 ;
    resolve_service_t ares[SS_MAX_SERVICE] ;

    char main[strlen(sv) + 1] ;
    if (!ob_basename(main, sv))
        log_dieu(LOG_EXIT_SYS, "get basename of: ", sv) ;

    int r = parse_frontend(sv, ares, &areslen, info, force, conf, &residx, 0, main) ;
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

        //service_master_modify_contents(&ares[pos], E_RESOLVE_SERVICE_MASTER_CONTENTS) ;
        service_notify_tree(ares[pos].sa.s + ares[pos].name, info->base.s, ares[pos].sa.s + ares[pos].treename, E_RESOLVE_TREE_CONTENTS) ;

        log_info("Parsed successfully: ", ares[pos].sa.s + ares[pos].name, " at tree: ", ares[pos].sa.s + ares[pos].treename) ;
    }

    service_resolve_array_free(ares, areslen) ;
}
