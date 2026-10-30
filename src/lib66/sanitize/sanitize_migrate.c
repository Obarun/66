/*
 * sanitize_migrate.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <sys/types.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <oblibs/log.h>
#include <oblibs/strbuf.h>
#include <oblibs/string.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/config.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/enum_parser.h>
#include <66/sanitize.h>
#include <66/migrate.h>

#define MIGRATE_NVERSION 13
static const char *version_list[MIGRATE_NVERSION] = {
    "0.8.0.0",
    "0.8.0.1",
    "0.8.0.2",
    "0.8.1.0",
    "0.8.1.1",
    "0.8.2.0",
    "0.8.2.1",
    "0.8.2.2",
    "0.9.0.0",
    "0.9.1.0",
    "0.9.1.1",
    "0.9.1.2",
    "0.9.2.0"
} ;

enum migrate_version_e
{
    VERSION_0800 = 0,
    VERSION_0801,
    VERSION_0802,
    VERSION_0810,
    VERSION_0811,
    VERSION_0820,
    VERSION_0821,
    VERSION_0822,
    VERSION_0900,
    VERSION_0910,
    VERSION_0911,
    VERSION_0912,
    VERSION_0920,
    VERSION_ENDOFKEY
} ;

static const uint8_t migrate_state [MIGRATE_NVERSION][MIGRATE_NVERSION] = {
    //  VERSION_0800            VERSION_0801            VERSION_0802            VERSION_0810            VERSION_0811            VERSION_0820            VERSION_0821            VERSION_0822            VERSION_0900            VERSION_0910              VERSION_0911        VERSION_0912            VERSION_0920
    { VERSION_ENDOFKEY,   VERSION_0801,       VERSION_0802,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,   VERSION_0810 },    // VERSION_0800 old
    { VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_0802,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,   VERSION_0810 },    // VERSION_0801 old
    { VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,       VERSION_0810,   VERSION_0810 },    // VERSION_0802 old
    { VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_0811,       VERSION_0811,       VERSION_0811,       VERSION_0811,       VERSION_0820,       VERSION_0820,       VERSION_0820,       VERSION_0820,   VERSION_0820 },    // VERSION_0810 old
    { VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_0820,       VERSION_0820,       VERSION_0820,       VERSION_0820,       VERSION_0820,       VERSION_0820,       VERSION_0820,   VERSION_0820 },    // VERSION_0811 old
    { VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_0821,       VERSION_0821,       VERSION_0822,       VERSION_0822,       VERSION_0822,       VERSION_0822,   VERSION_0822 },    // VERSION_0820 old
    { VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_0822,       VERSION_0822,       VERSION_0822,       VERSION_0822,   VERSION_0822 },    // VERSION_0821 old
    { VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_0900,       VERSION_0900,       VERSION_0900,       VERSION_0900,   VERSION_0900 },    // VERSION_0822 old
    { VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_0910,       VERSION_0910,       VERSION_0910,   VERSION_0910 },    // VERSION_0900 old
    { VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_0911,       VERSION_0911,   VERSION_0911 },    // VERSION_0910 old
    { VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_0912,   VERSION_0912 },    // VERSION_0911 old
    { VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_0920 }, // VERSION_0912 old
    { VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY } // VERSION_0920 old
} ;

static uint8_t str_to_int(const char *version)
{
    uint8_t pos = 0 ;
    for (; pos < MIGRATE_NVERSION ; pos++) {

        if (!version_compare(version, version_list[pos]))
            return pos ;
    }

    log_dieu(LOG_EXIT_SYS, "compare version -- please make a bug report") ;
}

void migrate_create_snap(ssexec_t *info, const char *version)
{
    char stk[7 + strlen(version) + 1] ;
    int argc = 4 ;
    int m = 0 ;
    char const *prog = PROG ;
    char const *newargv[argc] ;

    extern opt_on_option_fn on_snapshot_create ;

    auto_strings(stk, "system@", version) ;

    newargv[m++] = stk ;
    newargv[m] = 0 ;

    PROG = "snapshot" ;
    /* internal system snapshot: allow the reserved system@ prefix (-s). */
    on_snapshot_create('s', 0, info) ;
    if (ssexec_snapshot_create(m, newargv, info))
        log_dieu(LOG_EXIT_SYS, "create snapshot", stk) ;
    PROG = prog ;
}

int sanitize_migrate(ssexec_t *info, const char *oversion)
{
    log_flow() ;

    uint8_t state = str_to_int(oversion), current = str_to_int(SS_VERSION), did = 0 ;

    if (state < current)
        migrate_create_snap(info, oversion) ;

    if (state < VERSION_0920)
        migrate_0912_provide_symlink() ;

    while (state < VERSION_ENDOFKEY) {

        state = migrate_state[state][current] ;

        switch (state) {

            case VERSION_ENDOFKEY:
                return did ;

            case VERSION_0810:
                migrate_0802() ;
                if (!sanitize_resolve(info, DATA_TREE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize trees resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE_MASTER))
                    log_dieusys(LOG_EXIT_SYS, "sanitize Master resolve files") ;
                state = VERSION_0810 ;
                did++ ;
                break ;

            case VERSION_0820:
                migrate_0811() ;
                if (!sanitize_resolve(info, DATA_TREE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize trees resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE_MASTER))
                    log_dieusys(LOG_EXIT_SYS, "sanitize Master resolve files") ;
                did++ ;
                state = VERSION_0820 ;
                break ;

            case VERSION_0822:
                migrate_0821() ;
                if (!sanitize_resolve(info, DATA_TREE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize trees resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE_MASTER))
                    log_dieusys(LOG_EXIT_SYS, "sanitize Master resolve files") ;
                did++ ;
                state = VERSION_0822 ;
                break ;

            case VERSION_0900:
                migrate_0822() ;
                if (!sanitize_resolve(info, DATA_TREE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize trees resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE_MASTER))
                    log_dieusys(LOG_EXIT_SYS, "sanitize Master resolve files") ;
                did++ ;
                state = VERSION_0900 ;
                break ;

            case VERSION_0910:
                migrate_0900() ;
                if (!sanitize_resolve(info, DATA_SERVICE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize services resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize trees resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE_MASTER))
                    log_dieusys(LOG_EXIT_SYS, "sanitize Master resolve files") ;
                did++ ;
                state = VERSION_0910 ;
                break ;

            case VERSION_0911:
                if (!sanitize_resolve(info, DATA_SERVICE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize services resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize trees resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE_MASTER))
                    log_dieusys(LOG_EXIT_SYS, "sanitize Master resolve files") ;
                did++ ;
                state = VERSION_0911 ;
                break ;

            case VERSION_0912:
                if (!sanitize_resolve(info, DATA_SERVICE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize services resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize trees resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE_MASTER))
                    log_dieusys(LOG_EXIT_SYS, "sanitize Master resolve files") ;
                did++ ;
                state = VERSION_0912 ;
                break ;

            case VERSION_0920:
                migrate_0920() ;
                if (migrate_0920_state() < 0)
                    log_dieusys(LOG_EXIT_SYS, "drop the unused fields of the state files") ;
                if (!sanitize_resolve(info, DATA_SERVICE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize services resolve files") ;
                if (migrate_0920_logsuffix() < 0)
                    log_dieusys(LOG_EXIT_SYS, "remove the services named with the logger suffix") ;
                if (migrate_0920_dependencies() < 0)
                    log_dieusys(LOG_EXIT_SYS, "write the dependencies and the stop delay of the services") ;
                if (migrate_0920_runas() < 0)
                    log_dieusys(LOG_EXIT_SYS, "write the runas of the loggers") ;
                if (!sanitize_resolve(info, DATA_TREE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize trees resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE_MASTER))
                    log_dieusys(LOG_EXIT_SYS, "sanitize Master resolve files") ;
                did++ ;
                state = VERSION_0920 ;
                break ;

            default:
                log_dieu(LOG_EXIT_SYS, "invalid version state -- please make a bug report") ;
                break ;
        }
    }
    return did ;
}
