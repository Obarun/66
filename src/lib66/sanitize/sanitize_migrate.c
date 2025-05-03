/*
 * sanitize_migrate.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <oblibs/log.h>
#include <oblibs/stack.h>
#include <oblibs/string.h>

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/sanitize.h>

#include <66/migrate_0721.h>
#include <66/migrate.h>

#define MIGRATE_NVERSION 7
static const char *version_list[MIGRATE_NVERSION] = {
    "0.7.2.1",
    "0.8.0.0",
    "0.8.0.1",
    "0.8.0.2",
    "0.8.1.0",
    "0.8.1.1",
    "0.8.2.0",
} ;

enum migrate_version_e
{
    VERSION_0721 = 0,
    VERSION_0800,
    VERSION_0801,
    VERSION_0802,
    VERSION_0810,
    VERSION_0811,
    VERSION_0820,
    VERSION_ENDOFKEY
} ;

static const uint8_t migrate_state [MIGRATE_NVERSION][MIGRATE_NVERSION] = {
    //  VERSION_0721    VERSION_0800      VERSION_0801      VERSION_0802        VERSION_0810        VERSION_0811        VERSION_0820
    { VERSION_ENDOFKEY, VERSION_0800,     VERSION_0800,     VERSION_0800,       VERSION_0800,       VERSION_0800,       VERSION_0800 },     // VERSION_0721 old
    { VERSION_ENDOFKEY, VERSION_ENDOFKEY, VERSION_0801,     VERSION_0802,       VERSION_0810,       VERSION_0810,       VERSION_0810 },     // VERSION_0800 old
    { VERSION_ENDOFKEY, VERSION_ENDOFKEY, VERSION_ENDOFKEY, VERSION_0802,       VERSION_0810,       VERSION_0810,       VERSION_0810 },     // VERSION_0801 old
    { VERSION_ENDOFKEY, VERSION_ENDOFKEY, VERSION_ENDOFKEY, VERSION_ENDOFKEY,   VERSION_0810,       VERSION_0810,       VERSION_0810 },     // VERSION_0802 old
    { VERSION_ENDOFKEY, VERSION_ENDOFKEY, VERSION_ENDOFKEY, VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_0811,       VERSION_0811 },     // VERSION_0810 old
    { VERSION_ENDOFKEY, VERSION_ENDOFKEY, VERSION_ENDOFKEY, VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_0820 },     // VERSION_0811 old
    { VERSION_ENDOFKEY, VERSION_ENDOFKEY, VERSION_ENDOFKEY, VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY,   VERSION_ENDOFKEY }  // VERSION_0820 old
} ;

static uint8_t str_to_int(const char *version)
{
    uint8_t pos = 0 ;
    for (; pos < MIGRATE_NVERSION ; pos++) {

        if (!version_compare(version, version_list[pos]))
            return pos ;
    }

    log_dieu(LOG_EXIT_SYS, "unable to compare version -- please make a bug report") ;
}

void migrate_create_snap(ssexec_t *info, const char *version)
{
    _alloc_stk_(stk, 7 + strlen(version)) ;
    int argc = 4 ;
    int m = 0 ;
    char const *prog = PROG ;
    char const *newargv[argc] ;

    char const *help = info->help ;
    char const *usage = info->usage ;

    info->help = help_snapshot_create ;
    info->usage = usage_snapshot_create ;

    auto_strings(stk.s, "system@", version) ;

    newargv[m++] = "snapshot" ;
    newargv[m++] = "-s" ;
    newargv[m++] = stk.s ;
    newargv[m] = 0 ;

    PROG = "snapshot" ;
    if (ssexec_snapshot_create(m, newargv, info))
        log_dieu(LOG_EXIT_SYS, "create snapshot", stk.s) ;
    PROG = prog ;

    info->help = help ;
    info->usage = usage ;

}

/** Return 0 if no migration was made else 1 */
int sanitize_migrate(ssexec_t *info, const char *oversion, short exist)
{
    log_flow() ;

    uint8_t state = str_to_int(oversion), current = str_to_int(SS_VERSION), did = 0 ;

    while (state < VERSION_ENDOFKEY) {

        state = migrate_state[state][current] ;

        switch (state) {

            case VERSION_0721:
                // should not happen
                return 0 ;

            case VERSION_ENDOFKEY:
                return did ;

            case VERSION_0800:
                if (!exist) {
                    migrate_create_snap(info, oversion) ;
                    migrate_0721(info) ;
                    sanitize_graph(info) ;
                    did++ ;
                }
                state = VERSION_0800 ;
                break ;

            case VERSION_0801:
            case VERSION_0802:
                migrate_create_snap(info, oversion) ;
                if (!sanitize_resolve(info, DATA_SERVICE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize services resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize trees resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE_MASTER))
                    log_dieusys(LOG_EXIT_SYS, "sanitize Master resolve files") ;
                state = VERSION_0802 ;
                did++ ;
                break ;

            case VERSION_0810:
                migrate_create_snap(info, oversion) ;
                migrate_0802() ;
                if (!sanitize_resolve(info, DATA_TREE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize trees resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE_MASTER))
                    log_dieusys(LOG_EXIT_SYS, "sanitize Master resolve files") ;
                state = VERSION_0810 ;
                did++ ;
                break ;

            case VERSION_0811:
                migrate_create_snap(info, oversion) ;
                if (!sanitize_resolve(info, DATA_SERVICE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize services resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize trees resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE_MASTER))
                    log_dieusys(LOG_EXIT_SYS, "sanitize Master resolve files") ;
                state = VERSION_0811 ;
                did++ ;
                break ;

            case VERSION_0820:
                migrate_create_snap(info, oversion) ;
                migrate_0811() ;
                if (!sanitize_resolve(info, DATA_TREE))
                    log_dieusys(LOG_EXIT_SYS, "sanitize trees resolve files") ;
                if (!sanitize_resolve(info, DATA_TREE_MASTER))
                    log_dieusys(LOG_EXIT_SYS, "sanitize Master resolve files") ;
                did++ ;
                break ;

            default:
                log_dieu(LOG_EXIT_SYS, "invalid version state -- please make a bug report") ;
                break ;
        }
    }
    return did ;
}
