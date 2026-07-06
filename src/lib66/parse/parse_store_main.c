/*
 * parse_store_main.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h> //free
#include <pwd.h>
#include <errno.h>

#include <oblibs/string.h>
#include <oblibs/sbl.h>
#include <oblibs/log.h>
#include <oblibs/account.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>
#include <oblibs/strbuf.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/service.h>
#include <66/enum_parser.h>
#include <66/utils.h>

int parse_store_main(resolve_service_t *res, resolve_service_addon_execute_t *ex, resolve_service_addon_dependencies_t *dep, strbuf *store, resolve_enum_table_t table)
{
    log_flow() ;

    int r = 0, e = 0 ;
    size_t pos = 0 ;
    _cleanup_wres_ resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    _cleanup_wres_ resolve_wrapper_t_ref depwres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, dep) ;
    uint32_t kid = table.u.parser.id ;

    switch(kid) {

        case E_PARSER_SECTION_MAIN_DESCRIPTION:

            res->description = resolve_add_string(wres, store->s) ;
            break ;

        case E_PARSER_SECTION_MAIN_VERSION:

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len > SS_SERVICE_VERSION_MAXLEN)
                parse_error_return(0, 0, table) ;

            res->version = resolve_add_string(wres, store->s) ;

            break ;

        case E_PARSER_SECTION_MAIN_TYPE:

            if (res->name)
                /** already passed through here */
                break ;

            r = key_to_enum(enum_list_parser_type, store->s) ;
            if (r == -1)
                parse_error_return(0, 0, table) ;

            res->type = (uint32_t)r ;

            break ;

        case E_PARSER_SECTION_MAIN_NOTIFY:

            parse_error_type(res->type, enum_list_parser_section_main, kid) ;

            if (!u32_scan_strict(store->s, &ex->notify))
                parse_error_return(0, 3, table) ;

            if (ex->notify < 3)
                parse_error_return(0, 0, table) ;

            break ;

        case E_PARSER_SECTION_MAIN_DEATH:

            if (!u32_scan_strict(store->s, &ex->maxdeath))
                parse_error_return(0, 3, table) ;

            if (ex->maxdeath > 16)
                parse_error_return(0, 0, table) ;

            break ;

        case E_PARSER_SECTION_MAIN_DEATHTIME:

            if (!u32_scan_strict(store->s, &ex->maxdeathtime))
                parse_error_return(0, 3, table) ;

            break ;

        case E_PARSER_SECTION_MAIN_FLAGS:

            parse_error_type(res->type, enum_list_parser_section_main, kid) ;

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            {
                pos = 0 ;
                FOREACH_SBL(store, pos) {

                    r = key_to_enum(enum_list_parser_flags, store->s + pos) ;

                    if (r == -1)
                        parse_error_return(0, 0, table) ;

                    if (r == E_PARSER_FLAGS_DOWN)
                        ex->down = 1 ;/**0 means not enabled*/

                    if (r == E_PARSER_FLAGS_EARLIER)
                        res->earlier = 1 ;/**0 means not enabled*/
                }
            }

            break ;

        case E_PARSER_SECTION_MAIN_SIGNAL:

            parse_error_type(res->type, enum_list_parser_section_main, kid) ;

            int t = 0 ;
            if (!sig_parse(store->s, &t))
                parse_error_return(0, 3, table) ;

            ex->downsignal = (uint32_t)t ;

            break ;

        case E_PARSER_SECTION_MAIN_TIMESTART:

            parse_error_type(res->type, enum_list_parser_section_main, kid) ;

            log_1_warn("key TimeoutStart at section [Main] is deprecated -- declare it at section [Start] instead") ;

            if (!u32_scan_strict(store->s, &ex->timeout.start))
                parse_error_return(0, 3, table) ;

            break ;

        case E_PARSER_SECTION_MAIN_TIMESTOP:

            parse_error_type(res->type, enum_list_parser_section_main, kid) ;

            log_1_warn("key TimeoutStop at section [Main] is deprecated -- declare it at section [Stop] instead") ;

            if (!u32_scan_strict(store->s, &ex->timeout.stop))
                parse_error_return(0, 3, table) ;

            break ;

        case E_PARSER_SECTION_MAIN_COPYFROM:

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len) {

                size_t len = store->len ;
                char t[len + 1] ;

                memset(t, 0, sizeof(char) * len) ;
                memcpy(t, store->s, store->len) ;
                t[store->len] = 0 ;

                store->len = 0 ;

                for (pos = 0 ; pos < len ; pos += strlen(t + pos) + 1) {
                    if (!strbuf_catb(store, t + pos, strlen(t + pos)) ||
                        !strbuf_catb(store, " ", 1))
                        goto err ;
                }

                store->len-- ;
                if (!strbuf_terminate(store))
                    goto err ;

                res->copyfrom = resolve_add_string(wres, store->s) ;
            }

            break ;

        case E_PARSER_SECTION_MAIN_OPTIONS:

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len) {

                pos = 0 ;
                FOREACH_SBL(store, pos) {

                    uint8_t reverse = store->s[pos] == '!' ? 1 : 0 ;

                    r = key_to_enum(enum_list_parser_opts, store->s + pos + reverse) ;

                    if (r == -1)
                        parse_error_return(0, 0, table) ;

                    /** do no set a logger by default */
                    if (reverse && r == E_PARSER_OPTS_LOGGER)
                        res->has_logger = 0 ;
                }
            }

            break ;

        case E_PARSER_SECTION_MAIN_USER:

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len) {

                _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;

                uid_t user[256] ;
                memset(user, 0, 256 * sizeof(uid_t)) ;

                uid_t owner = MYUID ;
                if (!owner) {

                    if (sbl_search(store, "root") == -1)
                        log_warnu_return(LOG_EXIT_ZERO, "use the service -- permission denied") ;
                }
                /** special case, we don't know which user want to use
                 * the service, we need a general name to allow the current owner
                 * of the process. The term "user" is took here to allow him */
                ssize_t p = sbl_search(store, "user") ;
                pos = 0 ;
                FOREACH_SBL(store, pos) {

                    if (pos == (size_t)p) {

                        if (!owner)
                            /** avoid field e.g root root where originaly
                             * we want e.g. user root. The term user will be
                             * root at getpwuid() call */
                            continue ;

                        struct passwd *pw = getpwuid(owner);
                        if (!pw) {

                            if (!errno) errno = ESRCH ;
                            log_warnu_return(LOG_EXIT_ZERO,"get user name") ;
                        }

                        if (!scan_uidlist(pw->pw_name, user))
                            parse_error_return(0, 0, table) ;

                        if (!auto_strbuf(&sa, pw->pw_name, " "))
                            log_warnu_return(LOG_EXIT_ZERO, "strbuf") ;

                        continue ;
                    }

                    if (!scan_uidlist(store->s + pos, user))
                        parse_error_return(0, 0, table) ;

                    if (!auto_strbuf(&sa, store->s + pos, " "))
                        log_warnu_return(LOG_EXIT_ZERO, "strbuf") ;

                }
                int nb = (int)user[0] ;
                if (p == -1 && owner) {

                    int e = 0 ;
                    for (int i = 1; i < nb+1; i++) {
                        if (user[i] == owner) {
                            e = 1 ;
                            break ;
                        }
                    }
                    if (!e)
                        log_warnu_return(LOG_EXIT_ZERO,"use the service -- permission denied") ;
                }

                res->user = resolve_add_string(wres, sa.s) ;
            }

            break ;

        case E_PARSER_SECTION_MAIN_DEPENDS:

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len)
                dep->depends = parse_compute_list(depwres, store, &dep->ndepends, 0) ;

            break ;

        case E_PARSER_SECTION_MAIN_REQUIREDBY:

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len)
                dep->requiredby = parse_compute_list(depwres, store, &dep->nrequiredby, 0) ;

            break ;

        case E_PARSER_SECTION_MAIN_OPTSDEPS:

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len)
                dep->optsdeps = parse_compute_list(depwres, store, &dep->noptsdeps, 1) ;

            break ;

        case E_PARSER_SECTION_MAIN_CONTENTS:

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len)
                dep->contents = parse_compute_list(depwres, store, &dep->ncontents, 0) ;

            break ;

        case E_PARSER_SECTION_MAIN_INTREE:

            if (res->intree)
                /** already passed through here */
                break ;

            res->intree = resolve_add_string(wres, store->s) ;

            break ;

        case E_PARSER_SECTION_MAIN_STDIN:
        case E_PARSER_SECTION_MAIN_STDOUT:
        case E_PARSER_SECTION_MAIN_STDERR:
            // handled by the io addon (parse_io reads them from the store)
            break ;

        case E_PARSER_SECTION_MAIN_PROVIDE:

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len)
                dep->provide = parse_compute_list(depwres, store, &dep->nprovide, 0) ;

            break ;

        case E_PARSER_SECTION_MAIN_CONFLICT:

            if (!parse_list(store))
                parse_error_return(0, 8, table) ;

            if (store->len)
                dep->conflict = parse_compute_list(depwres, store, &dep->nconflict, 0) ;

            break ;

        default:
            /** never happen*/
            log_warn_return(LOG_EXIT_ZERO, "unknown id key in section main -- please make a bug report") ;
    }

    e = 1 ;

    err :
        return e ;
}
