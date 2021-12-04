/*
 * tree_seed.c
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


#include <66/tree.h>

#include <sys/types.h>
#include <string.h>
#include <unistd.h>//getuid

#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/log.h>
#include <oblibs/environ.h>
#include <oblibs/sastr.h>
#include <oblibs/types.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include <66/enum.h>
#include <66/utils.h>
#include <66/config.h>

stralloc saseed = STRALLOC_ZERO ;

void tree_seed_free(void)
{

    stralloc_free(&saseed) ;

}

static ssize_t tree_seed_get_key(char *table,char const *str)
{
    ssize_t pos = -1 ;

    pos = get_len_until(str,'=') ;

    if (pos == -1)
        return -1 ;

    auto_strings(table,str) ;

    table[pos] = 0 ;

    pos++ ; // remove '='

    return pos ;
}

int tree_seed_parse_file(tree_seed_t *seed, char const *seedpath)
{
    log_flow() ;

    int r, e = 0 ;

    stralloc sa = STRALLOC_ZERO ;
    size_t pos = 0 ;

    size_t filesize=file_get_size(seedpath) ;
    if (!filesize) {

        log_warn(seedpath," is empty") ;
        goto err ;
    }

    r = openreadfileclose(seedpath, &sa, filesize) ;
    if(!r) {

        log_warnusys("open ", seedpath) ;
        goto err ;
    }

    /** ensure that we have an empty line at the end of the string*/
    if (!auto_stra(&sa,"\n"))
        log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

    if (!environ_get_clean_env(&sa)) {

        log_warnu("clean seed file: ", seedpath) ;
        goto err ;
    }

    if (!sastr_split_string_in_nline(&sa))
        goto err ;

    FOREACH_SASTR(&sa, pos) {


        char *line = sa.s + pos, *key = 0, *val = 0 ;
        size_t len = strlen(line) ;

        char tmp[len + 1] ;

        r = tree_seed_get_key(tmp, line) ;

        /** should never happens */
        if (r == -1) {
            log_warn("invalid format of key: ",line," -- please make a bug report") ;
            goto err ;
        }

        key = tmp ;
        val = line + r ;

        ssize_t e = get_enum_by_key(key) ;

        switch (e) {

            case SEED_DEPENDS :

                seed->depends = saseed.len ;
                if (!sastr_add_string(&saseed, val))
                    goto err ;

                seed->nopts++ ;

                break ;

            case SEED_REQUIREDBY :

                seed->requiredby = saseed.len ;
                if (!sastr_add_string(&saseed, val))
                    goto err ;

                seed->nopts++ ;

                break ;

            case SEED_ENABLE :

                if (!strcmp(val,"true") || !strcmp(val,"True"))
                    seed->enabled = 1 ;

                seed->nopts++ ;

                break ;

            case SEED_ALLOW :

                seed->allow = saseed.len ;
                if (!sastr_add_string(&saseed, val))
                    goto err ;

                seed->nopts++ ;

                break ;

            case SEED_DENY :

                seed->deny = saseed.len ;
                if (!sastr_add_string(&saseed, val))
                    goto err ;

                seed->nopts++ ;

                break ;

            case SEED_CURRENT :

                if (!strcmp(val,"true") || !strcmp(val,"True"))
                    seed->current = 1 ;

                seed->nopts++ ;

                break ;

            case SEED_GROUP :

                if (strcmp(val,"boot") && strcmp(val,"admin") && strcmp(val,"user")) {

                    log_warn("invalid group: ", val) ;
                    goto err ;
                }

                seed->group = saseed.len ;
                if (!sastr_add_string(&saseed, val))
                    goto err ;

                seed->nopts++ ;

                break ;
            /*
            case SEED_SERVICES :
                {
                    stralloc sv = STRALLOC_ZERO ;

                    if (!sastr_clean_string_wdelim(&sv, val, ',') ||
                        !sastr_rebuild_in_oneline(&sv)) {

                            log_warnu("clean service list") ;
                            goto err ;
                    }

                    seed->services = saseed.len ;
                    if (!sastr_add_string(&saseed, sv.s))
                        goto err ;

                    seed->nopts++ ;

                    break ;
                }
            */
            default :

                log_warn("unknown key: ", key, " -- ignoring") ;
                break ;
        }

    }

    e = 1 ;
    err:
        stralloc_free(&sa) ;
        return e ;
}

/** @Return -1 bad format e.g want REG get DIR
 * @Return  0 fail
 * @Return success */
static int tree_seed_file_isvalid(char const *seedpath, char const *treename)
{
    log_flow() ;
    int r ;
    size_t slen = strlen(seedpath), tlen = strlen(treename) ;
    char seed[slen + tlen + 1] ;
    auto_strings(seed, seedpath, treename) ;

    r = scan_mode(seed, S_IFREG) ;

    return r ;
}

int tree_seed_resolve_path(stralloc *sa, char const *seed)
{
    log_flow() ;

    int r ;

    char *src = 0 ;
    uid_t uid = getuid() ;
    if (!uid) {

        src = SS_SEED_ADMDIR ;

    } else {

        if (!set_ownerhome(sa, uid))
            log_warnusys_return(LOG_EXIT_ZERO, "set home directory") ;

        if (!auto_stra(sa, SS_SEED_USERDIR))
            log_warnsys_return(LOG_EXIT_ZERO, "stralloc") ;

        src = sa->s ;
    }

    r = tree_seed_file_isvalid(src, seed) ;
    if (r == -1)
        return 0 ;

    if (!r) {

        /** yeah double check because we can come from !uid */
        src = SS_SEED_ADMDIR ;
        r = tree_seed_file_isvalid(src, seed) ;
        if (r == -1)
            return 0 ;

        if (!r) {

            src = SS_SEED_SYSDIR ;
            r = tree_seed_file_isvalid(src, seed) ;
            if (r != 1)
                return 0 ;

        }
    }

    sa->len = 0 ;
    if (!auto_stra(sa,src, seed))
        log_warnsys_return(LOG_EXIT_ZERO, "stralloc") ;

    return 1 ;

}

int tree_seed_isvalid(char const *seed)
{
    int e = 1 ;
    stralloc src = STRALLOC_ZERO ;

    if (!tree_seed_resolve_path(&src, seed))
        e = 0 ;

    stralloc_free(&src) ;

    return e ;
}

int tree_seed_ismandatory(tree_seed_t *seed, uint8_t check_service)
{
    log_flow() ;

    int e = 0 ;
    uid_t uid = getuid() ;
    //size_t pos = 0 ;
    stralloc sv = STRALLOC_ZERO ;

    char *group = saseed.s + seed->group ;
    //char *service = saseed.s + seed->services ;

    if (!uid && (!strcmp(group, "user"))) {

        log_warn("Only regular user can use this seed") ;
        goto err ;

    } else if (uid && (!strcmp(group, "root"))) {

        log_warn("Only root user can use this seed") ;
        goto err ;
    }

    if (!strcmp(saseed.s + seed->name , "boot") && seed->enabled) {

        log_warn("enable was asked for a tree on group boot -- ignoring enable request") ;
        seed->enabled = 0 ;
    }
    /**
    if (check_service) {

        stralloc sasrc = STRALLOC_ZERO ;

        if (!stralloc_cats(&sv, service) ||
            !sastr_clean_element(&sv)) {

                log_warnu("clean service list") ;
                stralloc_free(&sasrc) ;
                goto err ;
        }

        FOREACH_SASTR(&sv, pos) {

            char *s = sv.s + pos ;

            // service_frontend_src already warn user
            if (!service_frontend_path(&sasrc,s, uid, 0)) {

                stralloc_free(&sasrc) ;
                goto err ;
            }
        }

        stralloc_free(&sasrc) ;
    }
    */
    e = 1 ;
    err:
        stralloc_free(&sv) ;
        return e ;
}

int tree_seed_setseed(tree_seed_t *seed, char const *treename, uint8_t check_service)
{
    log_flow() ;

    int e = 0 ;
    stralloc src = STRALLOC_ZERO ;

    if (!tree_seed_resolve_path(&src, treename))
        goto err ;

    seed->name = saseed.len ;
    if (!sastr_add_string(&saseed, treename))
        goto err ;

    if (!tree_seed_parse_file(seed, src.s) ||
        !tree_seed_ismandatory(seed, check_service))
            goto err ;

    e = 1 ;
    err:
        stralloc_free(&src) ;
        return e ;
}
