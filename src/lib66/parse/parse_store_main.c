/*
 * parse_store_main.c
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

static void io_compute_stdin(resolve_service_t *res, char const *line, uint32_t type, int sid, int kid)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    res->io.fdin.type = type ;

    switch(type) {

        case IO_TYPE_TTY:

            if (line[0] != '/')
               log_dieusys(LOG_EXIT_SYS, "path must be absolute: ", line) ;

            res->io.fdin.destination = resolve_add_string(wres, line) ;
            break ;

        case IO_TYPE_S6LOG:
            res->io.fdin.destination = compute_log_dir(wres, res) ;
            break;

        case IO_TYPE_NULL:
            res->io.fdin.destination = resolve_add_string(wres, "/dev/null") ;
            break ;

        case IO_TYPE_PARENT:
        case IO_TYPE_CLOSE:
            break ;

        case IO_TYPE_CONSOLE:
        case IO_TYPE_FILE:
        case IO_TYPE_SYSLOG:
        case IO_TYPE_INHERIT:
        default:
            res->io.fdin.type = IO_TYPE_NOTSET ;
            break ;
    }
    free(wres) ;
}

static void io_compute_stdout(resolve_service_t *res, char const *line, uint32_t type, int sid, int kid)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    res->io.fdout.type = type ;

    switch(type) {

        case IO_TYPE_TTY:
        case IO_TYPE_FILE:
            if (line[0] != '/')
                log_dieusys(LOG_EXIT_SYS, "path must be absolute: ", line) ;

            res->io.fdout.destination = resolve_add_string(wres, line) ;
            break ;

        case IO_TYPE_CONSOLE:
            res->io.fdout.destination = resolve_add_string(wres, "/sys/class/tty/tty0/active") ;
            break ;

        case IO_TYPE_S6LOG:
            res->io.fdout.destination = compute_log_dir(wres, res) ;
            break ;

        case IO_TYPE_NULL:
            res->io.fdout.destination = resolve_add_string(wres, "/dev/null") ;
            break ;

        case IO_TYPE_SYSLOG:
            res->io.fdout.destination = resolve_add_string(wres, "/dev/log") ;
            break ;

        case IO_TYPE_PARENT:
        case IO_TYPE_CLOSE:
            break ;

        case IO_TYPE_INHERIT:
        default:
            res->io.fdout.type = IO_TYPE_NOTSET ;
            break ;
    }
    free(wres) ;
}

static void io_compute_stderr(resolve_service_t *res, char const *line, uint32_t type, int sid, int kid)
{
    log_flow() ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;
    res->io.fderr.type = type ;

    switch(type) {

        case IO_TYPE_TTY:
        case IO_TYPE_FILE:
            if (line[0] != '/')
                log_dieusys(LOG_EXIT_SYS, "path must be absolute: ", line) ;

            res->io.fderr.destination = resolve_add_string(wres, line) ;
            break ;

        case IO_TYPE_CONSOLE:
            res->io.fderr.destination = resolve_add_string(wres, "/sys/class/tty/tty0/active") ;
            break ;

        case IO_TYPE_NULL:
            res->io.fderr.destination = resolve_add_string(wres, "/dev/null") ;
            break ;

        case IO_TYPE_SYSLOG:
            res->io.fderr.destination = resolve_add_string(wres, "/dev/log") ;
            break ;

        case IO_TYPE_PARENT:
        case IO_TYPE_CLOSE:
        case IO_TYPE_INHERIT:
            break ;

        case IO_TYPE_S6LOG:
        default:
            res->io.fderr.type = IO_TYPE_NOTSET ;
            break ;
    }
    free(wres) ;
}

static int parse_io_type(resolve_service_t *res, char const *line, int sid, int kid)
{
    log_flow() ;

    size_t len = strlen(line) ;
    _alloc_stk_(stk, len) ;
    ssize_t delim = get_len_until(line,':'), type = -1 ;

    if (delim + 2 >= len)
        parse_error_return(0, 10, sid, list_io_type, kid) ;

    char *stype = (char *)line ;

    if (delim > 0) {

        memcpy(stk.s, stype, delim) ;
        stk.s[delim] = 0 ;

        stype = stk.s ;
    }

    type = get_enum_by_key(list_io_type, stype) ;

    if (type == -1) {
        /** default is applied */
        log_warn("invalid type for ",get_key_by_enum(list_section_main, kid), " key in section main -- applying default") ;
        return 1 ;
    }

    stack_reset(&stk) ;

    if (delim > 0) {
        if (!stack_add(&stk, line + delim + 1, len - delim + 1))
            log_die_nomem("stack") ;
    } else {
        if (!stack_add(&stk, line, len))
            log_die_nomem("stack") ;
    }

    switch(kid) {

        case KEY_MAIN_STDIN:
            io_compute_stdin(res, stk.s, (uint32_t)type, sid, kid) ;
            break ;

        case KEY_MAIN_STDOUT:
            io_compute_stdout(res, stk.s, (uint32_t)type, sid, kid) ;
            break ;

        case KEY_MAIN_STDERR:
            io_compute_stderr(res, stk.s, (uint32_t)type, sid, kid) ;
            break ;

        default:
            break ;
    }
    return 1 ;
}

int parse_store_main(resolve_service_t *res, stack *store, const int sid, const int kid)
{
    log_flow() ;

    int r = 0, e = 0 ;
    size_t pos = 0 ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, res) ;

    switch(kid) {

        case KEY_MAIN_DESCRIPTION:

            res->description = resolve_add_string(wres, store->s) ;
            break ;

        case KEY_MAIN_VERSION:

            {
                r = version_store(store, store->s, SS_SERVICE_VERSION_NDOT) ;
                if (r == -1)
                    goto err ;

                if (!r)
                    parse_error_return(0, 0, sid, list_section_main, kid) ;

                res->version = resolve_add_string(wres, store->s) ;
            }
            break ;

        case KEY_MAIN_TYPE:

            if (res->name)
                /** already passed through here */
                break ;

            if (!strcmp(store->s, "longrun")) {
                log_1_warn("deprecated type longrun -- convert it automatically to classic type") ;
                res->type = 0 ;
                break ;
            }

            r = get_enum_by_key(list_type, store->s) ;
            if (r == -1)
                parse_error_return(0, 0, sid, list_section_main, kid) ;

            res->type = (uint32_t)r ;

            break ;

        case KEY_MAIN_NOTIFY:

            parse_error_type(res->type, list_section_main, kid) ;

            if (!uint320_scan(store->s, &res->notify))
                parse_error_return(0, 3, sid, list_section_main, kid) ;

            if (res->notify < 3)
                parse_error_return(0, 0, sid, list_section_main, kid) ;

            break ;

        case KEY_MAIN_DEATH:

            if (!uint320_scan(store->s, &res->maxdeath))
                parse_error_return(0, 3, sid, list_section_main, kid) ;

            if (res->maxdeath > 4096)
                parse_error_return(0, 0, sid, list_section_main, kid) ;

            break ;

        case KEY_MAIN_FLAGS:

            parse_error_type(res->type, list_section_main, kid) ;

            if (!parse_list(store))
                parse_error_return(0, 8, sid, list_section_main, kid) ;

            {
                pos = 0 ;
                FOREACH_STK(store, pos) {

                    r = get_enum_by_key(list_flags, store->s + pos) ;

                    if (r == -1)
                        parse_error_return(0, 0, sid, list_section_main, kid) ;

                    if (r == FLAGS_DOWN)
                        res->execute.down = 1 ;/**0 means not enabled*/

                    if (r == FLAGS_EARLIER)
                        res->earlier = 1 ;/**0 means not enabled*/
                }
            }

            break ;

        case KEY_MAIN_SIGNAL:

            parse_error_type(res->type, list_section_main, kid) ;

            int t = 0 ;
            if (!sig0_scan(store->s, &t))
                parse_error_return(0, 3, sid, list_section_main, kid) ;

            res->execute.downsignal = (uint32_t)t ;

            break ;

        case KEY_MAIN_T_START:

            parse_error_type(res->type, list_section_main, kid) ;

            if (!uint320_scan(store->s, &res->execute.timeout.start))
                parse_error_return(0, 3, sid, list_section_main, kid) ;

            break ;

        case KEY_MAIN_T_STOP:

            parse_error_type(res->type, list_section_main, kid) ;

            if (!uint320_scan(store->s, &res->execute.timeout.stop))
                parse_error_return(0, 3, sid, list_section_main, kid) ;

            break ;

        case KEY_MAIN_HIERCOPY:

            if (!parse_list(store))
                parse_error_return(0, 8, sid, list_section_main, kid) ;

            if (store->len) {

                size_t len = store->len ;
                char t[len + 1] ;

                memset(t, 0, sizeof(char) * len) ;
                memcpy(t, store->s, store->len) ;
                t[store->len] = 0 ;

                stack_reset(store) ;

                for (pos = 0 ; pos < len ; pos += strlen(t + pos) + 1) {
                    if (!stack_add(store, t + pos, strlen(t + pos)) ||
                        !stack_add(store, " ", 1) ||
                        !stack_close(store))
                        goto err ;
                }

                store->len-- ;
                if (!stack_close(store))
                    goto err ;

                res->hiercopy = resolve_add_string(wres, store->s) ;
            }

            break ;

        case KEY_MAIN_OPTIONS:

            if (!parse_list(store))
                parse_error_return(0, 8, sid, list_section_main, kid) ;

            if (store->len) {

                pos = 0 ;
                FOREACH_STK(store, pos) {

                    uint8_t reverse = store->s[pos] == '!' ? 1 : 0 ;

                    r = get_enum_by_key(list_opts, store->s + pos + reverse) ;

                    if (r == -1)
                        parse_error_return(0, 0, sid, list_section_main, kid) ;

                    /** do no set a logger by default */
                    if (reverse && r == OPTS_LOGGER)
                        res->logger.want = 0 ;
                }
            }

            break ;

        case KEY_MAIN_USER:

            if (!parse_list(store))
                parse_error_return(0, 8, sid, list_section_main, kid) ;

            if (store->len) {

                stralloc sa = STRALLOC_ZERO ;

                uid_t user[256] ;
                memset(user, 0, 256 * sizeof(uid_t)) ;

                uid_t owner = MYUID ;
                if (!owner) {

                    if (stack_retrieve_element(store, "root") == -1)
                        log_warnu_return(LOG_EXIT_ZERO, "use the service -- permission denied") ;
                }
                /** special case, we don't know which user want to use
                 * the service, we need a general name to allow the current owner
                 * of the process. The term "user" is took here to allow him */
                ssize_t p = stack_retrieve_element(store, "user") ;
                pos = 0 ;
                FOREACH_STK(store, pos) {

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
                            parse_error_return(0, 0, sid, list_section_main, kid) ;

                        if (!auto_stra(&sa, pw->pw_name, " "))
                            log_warnu_return(LOG_EXIT_ZERO, "stralloc") ;

                        continue ;
                    }

                    if (!scan_uidlist(store->s + pos, user))
                        parse_error_return(0, 0, sid, list_section_main, kid) ;

                    if (!auto_stra(&sa, store->s + pos, " "))
                        log_warnu_return(LOG_EXIT_ZERO, "stralloc") ;

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
                stralloc_free(&sa) ;
            }

            break ;

        case KEY_MAIN_DEPENDS:

            if (!parse_list(store))
                parse_error_return(0, 8, sid, list_section_main, kid) ;

            if (store->len)
                res->dependencies.depends = parse_compute_list(wres, store, &res->dependencies.ndepends, 0) ;

            break ;

        case KEY_MAIN_REQUIREDBY:

            if (!parse_list(store))
                parse_error_return(0, 8, sid, list_section_main, kid) ;

            if (store->len)
                res->dependencies.requiredby = parse_compute_list(wres, store, &res->dependencies.nrequiredby, 0) ;

            break ;

        case KEY_MAIN_OPTSDEPS:

            if (!parse_list(store))
                parse_error_return(0, 8, sid, list_section_main, kid) ;

            if (store->len)
                res->dependencies.optsdeps = parse_compute_list(wres, store, &res->dependencies.noptsdeps, 1) ;

            break ;

        case KEY_MAIN_CONTENTS:

            if (!parse_list(store))
                parse_error_return(0, 8, sid, list_section_main, kid) ;

            if (store->len)
                res->dependencies.contents = parse_compute_list(wres, store, &res->dependencies.ncontents, 0) ;

            break ;

        case KEY_MAIN_INTREE:

            if (res->intree)
                /** already passed through here */
                break ;

            res->intree = resolve_add_string(wres, store->s) ;

            break ;

        case KEY_MAIN_STDIN:
        case KEY_MAIN_STDOUT:
        case KEY_MAIN_STDERR:

            if (!parse_io_type(res, store->s, sid, kid))
                return 0 ;
            break ;

        default:
            /** never happen*/
            log_warn_return(LOG_EXIT_ZERO, "unknown id key in section main -- please make a bug report") ;
    }

    e = 1 ;

    err :
        free(wres) ;
        return e ;
}
