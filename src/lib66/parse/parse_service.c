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

static void parse_notify_add_string(stralloc *sa, char const *name, char const *str)
{
    if (!sastr_clean_string(sa, str))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    if (!sastr_add_string(sa, name))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    if (!sastr_sortndrop_element(sa))
        log_dieu(LOG_EXIT_SYS, "sort string") ;
}

static void parse_notify_tree(resolve_service_t *res, char const *base, uint8_t field)
{
    log_flow() ;

    char *treename = res->sa.s + res->treename ;
    char *name = res->sa.s + res->name ;

    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    stralloc sa = STRALLOC_ZERO ;

    log_trace("modify field contents of resolve tree file: ", treename) ;

    if (!resolve_read_g(wres, base, treename))
        log_dieusys(LOG_EXIT_SYS, "read resolve file of tree: ", treename) ;

    if (field == E_RESOLVE_TREE_CONTENTS) {

        char atree[SS_MAX_TREENAME + 1] ;

        if (service_is_g(atree, name, STATE_FLAGS_ISPARSED)) {

            if (strcmp(atree, treename)) {

                /** remove it from the previous used tree */
                resolve_tree_t res = RESOLVE_TREE_ZERO ;
                resolve_wrapper_t_ref wtres = resolve_set_struct(DATA_TREE, &res) ;

                if (!resolve_read_g(wtres, base, atree))
                    log_dieu(LOG_EXIT_SYS, "read resolve file of tree: ", atree) ;

                if (!sastr_clean_string(&sa, res.sa.s + res.contents))
                    log_dieu(LOG_EXIT_SYS, "clean string") ;

                if (!sastr_remove_element(&sa, name))
                    log_dieu(LOG_EXIT_SYS, "remove service: ", name, " list") ;

                if (sa.len) {
                    if (!sastr_rebuild_in_oneline(&sa))
                        log_dieu(LOG_EXIT_SYS, "rebuild stralloc list") ;
                } else
                    stralloc_0(&sa) ;


                if (!tree_resolve_modify_field(&res, E_RESOLVE_TREE_CONTENTS, sa.s))
                    log_dieu(LOG_EXIT_SYS, "modify resolve field of tree: ", atree) ;

                res.ncontents-- ;

                if (!resolve_write_g(wtres, base, atree))
                    log_dieu(LOG_EXIT_SYS, "write resolve file of tree: ", atree) ;

                resolve_free(wtres) ;
            }
            sa.len = 0 ;
        }

        if (tres.ncontents)
            parse_notify_add_string(&sa, name, tres.sa.s + tres.contents) ;
        else if (!sastr_add_string(&sa, name))
            log_dieu(LOG_EXIT_SYS, "add string") ;

        tres.ncontents++ ;

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
    log_flow();

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

        /** notify first the resolve Master file of the tree
         * about the location of the service. If the service is
         * already parsed and the user ask to force it on different tree,
         * we can know the old place of the service by the old resolve service file*/
        parse_notify_tree(&ares[pos], info->base.s, E_RESOLVE_TREE_CONTENTS) ;

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

        log_info("Parsed successfully: ", ares[pos].sa.s + ares[pos].name, " at tree: ", ares[pos].sa.s + ares[pos].treename) ;
    }

    service_resolve_array_free(ares, areslen) ;
}
