/*
 * parse_service.c
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
#include <stdlib.h>
#include <unistd.h> //access

#include <oblibs/log.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>

#include <skalibs/djbunix.h>

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
#include <66/symlink.h>
#include <66/hash.h>

lexer_config LEXER_CONFIG_SECTION = { \
    .str = 0,\
    .slen = 0,\
    .open = "[",\
    .olen = 1,\
    .close = "]",\
    .clen = 1,\
    .skip = " \t\r",\
    .skiplen = 3,\
    .kopen = 0,\
    .kclose = 0,\
    .style = 0,\
    .forceopen = 0,\
    .forceclose = 0,\
    .pos = 0,\
    .opos = 0,\
    .cpos = 0,\
    .invalue = 0,\
    .found = 0,\
    .exitcode = 0,\
} ;

lexer_config LEXER_CONFIG_QUOTE = { \
    .str = 0,\
    .slen = 0,\
    .open = "\"",\
    .olen = 1,\
    .close = "\n\"",\
    .clen = 2,\
    .skip = 0,\
    .skiplen = 0,\
    .kopen = 0,\
    .kclose = 0,\
    .style = 0,\
    .forceopen = 0,\
    .forceclose = 0,\
    .pos = 0,\
    .opos = 0,\
    .cpos = 0,\
    .invalue = 0,\
    .found = 0,\
    .exitcode = 0,\
} ;

lexer_config LEXER_CONFIG_INLINE = { \
    .str = 0,\
    .slen = 0,\
    .open = "=",\
    .olen = 1,\
    .close = "\n",\
    .clen = 1,\
    .skip = " \t\r",\
    .skiplen = 3,\
    .kopen = 0,\
    .kclose = 0,\
    .style = 0,\
    .forceopen = 0,\
    .forceclose = 0,\
    .pos = 0,\
    .opos = 0,\
    .cpos = 0,\
    .invalue = 0,\
    .found = 0,\
    .exitcode = 0,\
} ;

lexer_config LEXER_CONFIG_LIST = { \
    .str = 0,\
    .slen = 0,\
    .open = 0,\
    .olen = 0,\
    .close = " \n",\
    .clen = 2,\
    .skip = " \t\r\n",\
    .skiplen = 4,\
    .kopen = 1,\
    .kclose = 0,\
    .style = 0,\
    .forceopen = 1,\
    .forceclose = 1,\
    .pos = 0,\
    .opos = 0,\
    .cpos = 0,\
    .invalue = 0,\
    .found = 0,\
    .exitcode = 0,\
} ;

lexer_config LEXER_CONFIG_KEY = { \
    .str = 0,\
    .slen = 0,\
    .open = "@",\
    .olen = 1,\
    .close = "=\n",\
    .clen = 2,\
    .skip = " \t\r",\
    .skiplen = 3,\
    .kopen = 1,\
    .kclose = 0,\
    .style = 0,\
    .forceopen = 0,\
    .forceclose = 0,\
    .pos = 0,\
    .opos = 0,\
    .cpos = 0,\
    .invalue = 0,\
    .found = 0,\
    .exitcode = 0,\
} ;

void parse_cleanup(resolve_service_t *res, char const *tmpdir, uint8_t force)
{
    log_flow() ;

    size_t namelen = strlen(res->sa.s + res->name), homelen = strlen(res->sa.s + res->path.home) ;
    char dir[homelen + SS_SYSTEM_LEN + SS_SERVICE_LEN + SS_SVC_LEN + 1 + namelen + 1] ;

    if (!dir_rm_rf(tmpdir))
        log_warnu("remove temporary directory: ", tmpdir) ;

    if (!force) {

        auto_strings(dir, res->sa.s + res->path.home, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", res->sa.s + res->name) ;

        if (!dir_rm_rf(dir))
            log_warnu("remove service directory: ", dir) ;
    }
}

static void parse_copy_to_source(char const *dst, char const *src, resolve_service_t *res, uint8_t force)
{
    log_flow() ;

    size_t pos = 0, srclen = strlen(src) ;

    if (!access(dst, F_OK)) {

        stralloc sa = STRALLOC_ZERO ;

        char const *exclude[6] = { SS_EVENTDIR + 1, SS_SUPERVISEDIR + 1, SS_SCANDIR, SS_STATE + 1, SS_RESOLVE + 1, 0 } ;
        if (!sastr_dir_get_recursive(&sa, dst, exclude, S_IFDIR|S_IFREG, 1)) {
            parse_cleanup(res, src, force) ;
            stralloc_free(&sa) ;
            log_dieusys(LOG_EXIT_SYS, "get file list of: ", dst) ;
        }

        FOREACH_SASTR(&sa, pos) {

            char base[strlen(sa.s + pos) + 1] ;
            if (!ob_basename(base, sa.s + pos)) {
                parse_cleanup(res, src, force) ;
                stralloc_free(&sa) ;
                log_dieusys(LOG_EXIT_SYS, "basename of: ", sa.s + pos) ;
            }

            char element[srclen + 1 + strlen(base) + 1] ;

            auto_strings(element, src, "/", base) ;

            if (access(element, F_OK) < 0) {
                log_trace("remove element: ", sa.s + pos) ;
                if (!dir_rm_rf(sa.s + pos)) {
                    parse_cleanup(res, src, force) ;
                    stralloc_free(&sa) ;
                    log_dieusys(LOG_EXIT_SYS, "remove element: ", sa.s + pos) ;
                }
            }
        }

        stralloc_free(&sa) ;
    }

    if (access(dst, F_OK) < 0) {

        log_trace("create directory: ", dst) ;
        if (!dir_create_parent(dst, 0755)) {
            parse_cleanup(res, src, force) ;
            log_dieusys(LOG_EXIT_SYS, "create directory: ", dst) ;
        }
    }

    log_trace("copy:", src, " to: ", dst) ;
    if (!hiercopy(src, dst)) {
        parse_cleanup(res, src, force) ;
        log_dieusys(LOG_EXIT_SYS, "copy: ", src, " to: ", dst) ;
    }

    /** be paranoid after the use of hiercopy and be sure
     * to have dst in 0755 mode. If not the log cannot be
     * executed with other permissions than root */
    if (chmod(dst, 0755)< 0) {
        parse_cleanup(res, src, force) ;
        log_dieusys(LOG_EXIT_SYS,"chmod: ", dst) ;
    }
}

static void parse_write_state(resolve_service_t *res, char const *dst, uint8_t force)
{
    log_flow() ;
    ss_state_t sta = STATE_ZERO ;
    char dir[strlen(dst) + SS_STATE_LEN + 1] ;

    auto_strings(dir, dst, SS_STATE) ;

    state_set_flag(&sta, STATE_FLAGS_TOINIT, STATE_FLAGS_TRUE) ;
    state_set_flag(&sta, STATE_FLAGS_TOPARSE, STATE_FLAGS_FALSE) ;
    state_set_flag(&sta, STATE_FLAGS_ISPARSED, STATE_FLAGS_TRUE) ;

    if (!state_write_remote(&sta, dir)) {
        parse_cleanup(res, dst, force) ;
        log_dieu(LOG_EXIT_SYS, "write state file of: ", res->sa.s + res->name) ;
    }
}

void parse_service(struct resolve_hash_s **hres, char const *sv, ssexec_t *info, uint8_t force, uint8_t conf)
{
    log_flow();

    int r ;
    uint8_t rforce = 0 ;
    _alloc_sa_(sa) ;
    struct resolve_hash_s *c, *tmp ;

    char main[strlen(sv) + 1] ;
    if (!ob_basename(main, sv))
        log_dieu(LOG_EXIT_SYS, "get basename of: ", sv) ;

    r = parse_frontend(sv, hres, info, force, conf, 0, main, 0, 0) ;
    if (r == 2)
        /** already parsed */
        return ;

    HASH_ITER(hh, *hres, c, tmp) {

        sa.len = 0 ;
        /** be paranoid */
        rforce = 0 ;
        size_t namelen = strlen(c->res.sa.s + c->res.name), homelen = strlen(c->res.sa.s + c->res.path.home) ;

        char servicedir[homelen + SS_SYSTEM_LEN + SS_SERVICE_LEN + SS_SVC_LEN + 1 + namelen + 1] ;

        auto_strings(servicedir, c->res.sa.s + c->res.path.home, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", c->res.sa.s + c->res.name) ;

        if (sanitize_write(&c->res, force))
            rforce = 1 ;

        if (!auto_stra(&sa, "/tmp/", c->res.sa.s + c->res.name, ":XXXXXX"))
            log_die_nomem("stralloc") ;

        if (!mkdtemp(sa.s))
            log_dieusys(LOG_EXIT_SYS, "create temporary directory") ;

        write_services(&c->res, sa.s, rforce) ;

        parse_write_state(&c->res, sa.s, rforce) ;

        service_resolve_write_remote(&c->res, sa.s, rforce) ;

        parse_copy_to_source(servicedir, sa.s, &c->res, rforce) ;

        /** do not die here, just warn the user */
        log_trace("remove temporary directory: ", sa.s) ;
        if (!dir_rm_rf(sa.s))
            log_warnu("remove temporary directory: ", sa.s) ;

        tree_service_add(c->res.sa.s + c->res.treename, c->res.sa.s + c->res.name, info) ;

        if (!symlink_make(&c->res))
            log_dieusys(LOG_EXIT_SYS, "make service symlink") ;

        /** symlink may exist already, be sure to point to the correct location */

        if (!symlink_switch(&c->res, SYMLINK_SOURCE))
            log_dieusys(LOG_EXIT_SYS, "sanitize_symlink") ;

        log_info("Parsed successfully: ", c->res.sa.s + c->res.name, " at tree: ", c->res.sa.s + c->res.treename) ;
    }
}
