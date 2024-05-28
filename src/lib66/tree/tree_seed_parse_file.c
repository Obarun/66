/*
 * tree_seed_parse_file.c
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

#include <sys/types.h>
#include <string.h>

#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/log.h>
#include <oblibs/environ.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/enum.h>
#include <66/tree.h>

int tree_seed_parse_file(tree_seed_t *seed, char const *seedpath)
{
    log_flow() ;

    int r ;
    size_t pos = 0 ;
    _alloc_sa_(sa) ;

    if (!environ_merge_file(&sa, seedpath))
        log_warn_return(LOG_EXIT_ZERO, "merge seed file: ", seedpath) ;

    FOREACH_SASTR(&sa, pos) {

        char *line = sa.s + pos, *key = 0, *val = 0 ;
        size_t len = strlen(line) ;

        char tmp[len + 1] ;

        r = tree_seed_get_key(tmp, line) ;

        /** should never happens */
        if (r == -1) {
            log_warn("invalid format of key: ",line," -- please make a bug report") ;
            return 0 ;
        }

        key = tmp ;
        val = line + r ;

        ssize_t e = get_enum_by_key(list_seed, key) ;

        switch (e) {

            case SEED_DEPENDS :

                seed->depends = seed->sa.len ;
                if (!sastr_add_string(&seed->sa, val))
                    return 0 ;

                seed->nopts++ ;

                break ;

            case SEED_REQUIREDBY :

                seed->requiredby = seed->sa.len ;
                if (!sastr_add_string(&seed->sa, val))
                    return 0 ;

                seed->nopts++ ;

                break ;

            case SEED_ENABLE :

                if (!strcmp(val,"true") || !strcmp(val,"True"))
                    seed->disen = 1 ;

                seed->nopts++ ;

                break ;

            case SEED_ALLOW :

                seed->allow = seed->sa.len ;
                if (!sastr_add_string(&seed->sa, val))
                    return 0 ;

                seed->nopts++ ;

                break ;

            case SEED_DENY :

                seed->deny = seed->sa.len ;
                if (!sastr_add_string(&seed->sa, val))
                    return 0 ;

                seed->nopts++ ;

                break ;

            case SEED_CURRENT :

                if (!strcmp(val,"true") || !strcmp(val,"True"))
                    seed->current = 1 ;

                seed->nopts++ ;

                break ;

            case SEED_GROUPS :

                if (strcmp(val,TREE_GROUPS_BOOT) && strcmp(val,TREE_GROUPS_ADM) && strcmp(val,TREE_GROUPS_USER)) {

                    log_warn("invalid group: ", val) ;
                    return 0 ;
                }

                seed->groups = seed->sa.len ;
                if (!sastr_add_string(&seed->sa, val))
                    return 0 ;

                seed->nopts++ ;

                break ;

            default :

                log_warn("unknown key: ", key, " -- ignoring") ;
                return 0 ;
        }

    }

    return 1 ;
}
