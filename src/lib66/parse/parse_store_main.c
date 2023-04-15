/*
 * parse_store_main.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
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

#include <oblibs/string.h>
#include <oblibs/sastr.h>
#include <oblibs/log.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>

#include <skalibs/stralloc.h>
#include <skalibs/types.h>
#include <skalibs/sig.h>

#include <66/parse.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/service.h>
#include <66/enum.h>
#include <66/utils.h>

int parse_store_main(resolve_service_t *res, char *store, int idsec, int idkey)
{
    int r = 0, e = 0 ;
    size_t pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch(idkey) {

        case KEY_MAIN_DESCRIPTION:

            if (!parse_clean_quotes(store))
                parse_error_return(0, 8, idsec, idkey) ;

            res->description = resolve_add_string(wres, store) ;

            break ;

        case KEY_MAIN_VERSION:

            if (!parse_clean_line(store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (!auto_stra(&sa, store))
                goto err ;

            r = version_scan(&sa, store, SS_CONFIG_VERSION_NDOT) ;
            if (r == -1)
                goto err ;

            if (!r)
                parse_error_return(0, 0, idsec, idkey) ;

            res->version = resolve_add_string(wres, sa.s) ;

            break ;

        case KEY_MAIN_TYPE:

            if (res->name)
                /** already passed through here */
                break ;

            if (!parse_clean_line(store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (!strcmp(store, "longrun")) {
                log_1_warn("deprecated type longrun -- convert it automatically to classic type") ;
                res->type = 0 ;
                break ;
            }

            r = get_enum_by_key(store) ;
            if (r == -1)
                parse_error_return(0, 0, idsec, idkey) ;

            res->type = (uint32_t)r ;

            break ;

        case KEY_MAIN_NOTIFY:

            parse_error_type(res->type,ENUM_KEY_SECTION_MAIN, idkey) ;

            if (!uint320_scan(store, &res->notify))
                parse_error_return(0, 3, idsec, idkey) ;

            if (res->notify < 3)
                parse_error_return(0, 0, idsec, idkey) ;

            break ;

        case KEY_MAIN_DEATH:


            if (!uint320_scan(store, &res->maxdeath))
                parse_error_return(0, 3, idsec, idkey) ;

            if (res->maxdeath > 4096)
                parse_error_return(0, 0, idsec, idkey) ;

            break ;

        case KEY_MAIN_FLAGS:

            parse_error_type(res->type,ENUM_KEY_SECTION_MAIN, idkey) ;

            if (!parse_clean_list(&sa, store))
                parse_error_return(0, 8, idsec, idkey) ;

            {
                pos = 0 ;
                FOREACH_SASTR(&sa, pos) {

                    r = get_enum_by_key_one(sa.s + pos, ENUM_FLAGS) ;

                    if (r == -1)
                        parse_error_return(0, 0, idsec, idkey) ;

                    if (r == FLAGS_DOWN)
                        res->execute.down = 1 ;/**0 means not enabled*/

                    if (r == FLAGS_EARLIER)
                        res->earlier = 1 ;/**0 means not enabled*/
                }
            }

            break ;

        case KEY_MAIN_SIGNAL:

            parse_error_type(res->type,ENUM_KEY_SECTION_MAIN, idkey) ;

            if (!parse_clean_line(store))
                parse_error_return(0, 8, idsec, idkey) ;

            int t = 0 ;
            if (!sig0_scan(store, &t))
                parse_error_return(0, 3, idsec, idkey) ;

            res->execute.downsignal = (uint32_t)t ;

            break ;

        case KEY_MAIN_T_KILL:

            parse_error_type(res->type,ENUM_KEY_SECTION_MAIN, idkey) ;

            if (!uint320_scan(store, &res->execute.timeout.kill))
                parse_error_return(0, 3, idsec, idkey) ;

            break ;

        case KEY_MAIN_T_FINISH:

            parse_error_type(res->type,ENUM_KEY_SECTION_MAIN, idkey) ;

            if (!uint320_scan(store, &res->execute.timeout.finish))
                parse_error_return(0, 3, idsec, idkey) ;

            break ;

        case KEY_MAIN_T_UP:

            parse_error_type(res->type,ENUM_KEY_SECTION_MAIN, idkey) ;

            if (!uint320_scan(store, &res->execute.timeout.up))
                parse_error_return(0, 3, idsec, idkey) ;

            break ;

        case KEY_MAIN_T_DOWN:

            parse_error_type(res->type,ENUM_KEY_SECTION_MAIN, idkey) ;

            if (!uint320_scan(store, &res->execute.timeout.down))
                parse_error_return(0, 3, idsec, idkey) ;

            break ;

        case KEY_MAIN_HIERCOPY:

            if (res->type == TYPE_BUNDLE)
                log_warn_return(LOG_EXIT_ONE,"key: ", get_key_by_enum(ENUM_KEY_SECTION_MAIN, idkey), ": is not valid for type ", get_key_by_enum(ENUM_TYPE, res->type), " -- ignoring it") ;

            if (!parse_clean_list(&sa, store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (sa.len) {

                size_t len = sa.len ;

                char t[len + 1] ;

                sastr_to_char(t, &sa) ;

                sa.len = 0 ;
                pos = 0 ;

                for (; pos < len ; pos += strlen(t + pos) + 1) {
                    if (!auto_stra(&sa, t + pos, " "))
                            goto err ;
                }

                sa.len-- ;
                if (!stralloc_0(&sa))
                    goto err ;

                res->hiercopy = resolve_add_string(wres, sa.s) ;
            }

            break ;

        case KEY_MAIN_OPTIONS:

            if (res->type == TYPE_BUNDLE)
                log_warn_return(LOG_EXIT_ONE,"key: ", get_key_by_enum(ENUM_KEY_SECTION_MAIN, idkey), ": is not valid for type ", get_key_by_enum(ENUM_TYPE, res->type), " -- ignoring it") ;

            if (!parse_clean_list(&sa, store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (sa.len) {

                pos = 0 ;
                FOREACH_SASTR(&sa, pos) {

                    uint8_t reverse = sa.s[pos] == '!' ? 1 : 0 ;

                    r = get_enum_by_key(sa.s + pos + reverse) ;

                    if (r == -1)
                        parse_error_return(0, 0, idsec, idkey) ;

                    /** do no set a logger by default */
                    if (reverse && r == OPTS_LOGGER)
                        res->logger.want = 0 ;

                    if (r == OPTS_ENVIR)
                        log_warn("options: env is deprecated -- simply set an [environment] section") ;
                }
            }

            break ;

        case KEY_MAIN_USER:

            if (!parse_clean_list(&sa, store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (sa.len) {

                uid_t user[256] ;
                memset(user, 0, 256*sizeof(uid_t)) ;

                uid_t owner = MYUID ;
                if (!owner) {

                    if (sastr_find(&sa, "root") == -1)
                        log_warnu_return(LOG_EXIT_ZERO, "use the service -- permission denied") ;
                }
                /** special case, we don't know which user want to use
                 * the service, we need a general name to allow the current owner
                 * of the process. The term "user" is took here to allow him */
                ssize_t p = sastr_cmp(&sa, "user") ;
                size_t len = sa.len ;
                pos = 0 ;

                char t[len + 1] ;

                sastr_to_char(t, &sa) ;

                sa.len = 0 ;

                for (; pos < len ; pos += strlen(t + pos) + 1) {

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
                            parse_error_return(0, 0, idsec, idkey) ;

                        if (!auto_stra(&sa, pw->pw_name, " "))
                            log_warnu_return(LOG_EXIT_ZERO, "stralloc") ;

                        continue ;
                    }

                    if (!scan_uidlist(t + pos, user))
                        parse_error_return(0, 0, idsec, idkey) ;

                    if (!auto_stra(&sa, t + pos, " "))
                        log_warnu_return(LOG_EXIT_ZERO, "stralloc") ;

                }
                uid_t nb = user[0] ;
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

        case KEY_MAIN_DEPENDS:

            if (!parse_clean_list(&sa, store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (sa.len)
                res->dependencies.depends = parse_compute_list(wres, &sa, &res->dependencies.ndepends, 0) ;

            break ;

        case KEY_MAIN_REQUIREDBY:

            if (!parse_clean_list(&sa, store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (sa.len)
                res->dependencies.requiredby = parse_compute_list(wres, &sa, &res->dependencies.nrequiredby, 0) ;

            break ;

        case KEY_MAIN_OPTSDEPS:

            if (!parse_clean_list(&sa, store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (sa.len)
                res->dependencies.optsdeps = parse_compute_list(wres, &sa, &res->dependencies.noptsdeps, 1) ;

            break ;

        case KEY_MAIN_CONTENTS:

            if (!parse_clean_list(&sa, store))
                parse_error_return(0, 8, idsec, idkey) ;

            if (sa.len)
                res->dependencies.contents = parse_compute_list(wres, &sa, &res->dependencies.ncontents, 0) ;

            break ;

        case KEY_MAIN_INTREE:

            if (!parse_clean_line(store))
                parse_error_return(0, 8, idsec, idkey) ;

            res->intree = resolve_add_string(wres, store) ;

            break ;

        default:
            log_warn_return(LOG_EXIT_ZERO, "unknown key: ", get_key_by_key_all(idsec, idkey)) ;
    }

    e = 1 ;

    err :
        stralloc_free(&sa) ;
        free(wres) ;
        return e ;

}
