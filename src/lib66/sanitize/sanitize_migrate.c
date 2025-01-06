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

#include <66/ssexec.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/sanitize.h>

#include <66/migrate_0721.h>

#define MIGRATE_NVERSION 3
static const char *version_list[MIGRATE_NVERSION] = {
    "0.7.2.1",
    "0.8.0.0",
    "0.8.0.1"
} ;

enum migrate_version_e
{
    VERSION_0721 = 0,
    VERSION_0800,
    VERSION_0801,
    VERSION_ENDOFKEY
} ;

static const uint8_t migrate_state [3][3] = {
    { VERSION_ENDOFKEY, VERSION_0800, VERSION_0800 }, // VERSION_0721
    { VERSION_ENDOFKEY, VERSION_ENDOFKEY, VERSION_0801 }, // VERSION_0800
    { VERSION_ENDOFKEY, VERSION_ENDOFKEY, VERSION_ENDOFKEY }, // VERSION_0801
} ;

static uint8_t str_to_int(const char *version)
{
    uint8_t pos = 0 ;
    for (; pos < MIGRATE_NVERSION ; pos++) {

        if (!version_compare(version, version_list[pos], SS_SYSTEM_VERSION_NDOT))
            return pos ;
    }

    log_dieu(LOG_EXIT_SYS, "unable to compare version -- please make a bug report") ;
}

/** Return 0 if no migration was made else 1 */
int sanitize_migrate(ssexec_t *info, const char *sversion, short exist)
{
    log_flow() ;

    uint8_t state = str_to_int(sversion), current = str_to_int(SS_VERSION), did = 0 ;

    while (state < VERSION_ENDOFKEY) {

        state = migrate_state[state][current] ;

        switch (state) {

            case VERSION_0721:
                // should not be happen
                return 0 ;

            case VERSION_ENDOFKEY:
                return did ;

            case VERSION_0800:
                if (!exist) {
                    migrate_0721(info) ;
                    sanitize_graph(info) ;
                    did++ ;
                }
                state = VERSION_0800 ;
                break ;

            case VERSION_0801:
                state = VERSION_ENDOFKEY ;
                did++ ;
                break ;

            default:
                log_dieu(LOG_EXIT_SYS, "invalid version state -- please make a bug report") ;
                break ;
        }
    }
    return did ;
}
