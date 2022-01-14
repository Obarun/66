/*
 * tree_resolve.c
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

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>//free
#include <pwd.h>
#include <sys/stat.h>

#include <oblibs/log.h>
#include <oblibs/string.h>

#include <skalibs/stralloc.h>
#include <skalibs/cdbmake.h>
#include <skalibs/cdb.h>

#include <66/tree.h>
#include <66/resolve.h>
#include <66/constants.h>
#include <66/graph.h>


resolve_field_table_t resolve_tree_field_table[] = {

    [TREE_ENUM_NAME] = { .field = "name" },
    [TREE_ENUM_DEPENDS] = { .field = "depends" },
    [TREE_ENUM_REQUIREDBY] = { .field = "requiredby" },
    [TREE_ENUM_ALLOW] = { .field = "allow" },
    [TREE_ENUM_GROUPS] = { .field = "groups" },
    [TREE_ENUM_CONTENTS] = { .field = "contents" },
    [TREE_ENUM_NDEPENDS] = { .field = "ndepends" },
    [TREE_ENUM_NREQUIREDBY] = { .field = "nrequiredby" },
    [TREE_ENUM_NALLOW] = { .field = "nallow" },
    [TREE_ENUM_NGROUPS] = { .field = "ngroups" },
    [TREE_ENUM_NCONTENTS] = { .field = "ncontents" },
    [TREE_ENUM_INIT] = { .field = "init" },
    [TREE_ENUM_DISEN] = { .field = "disen" },
    [TREE_ENUM_ENDOFKEY] = { .field = 0 }
} ;

resolve_field_table_t resolve_tree_master_field_table[] = {

    [TREE_ENUM_MASTER_NAME] = { .field = "name" },
    [TREE_ENUM_MASTER_ALLOW] = { .field = "allow" },
    [TREE_ENUM_MASTER_ENABLED] = { .field = "enabled" },
    [TREE_ENUM_MASTER_CURRENT] = { .field = "current" },
    [TREE_ENUM_MASTER_NENABLED] = { .field = "nenabled" },
    [TREE_ENUM_MASTER_ENDOFKEY] = { .field = 0 }
} ;

int tree_read_cdb(cdb *c, resolve_tree_t *tres)
{
    log_flow() ;

    stralloc tmp = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres ;
    uint32_t x ;

    wres = resolve_set_struct(DATA_TREE, tres) ;

    resolve_init(wres) ;

    /* name */
    resolve_find_cdb(&tmp,c,"name") ;
    tres->name = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* depends */
    resolve_find_cdb(&tmp,c,"depends") ;
    tres->depends = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* requiredby */
    resolve_find_cdb(&tmp,c,"requiredby") ;
    tres->requiredby = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* allow */
    resolve_find_cdb(&tmp,c,"allow") ;
    tres->allow = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* groups */
    resolve_find_cdb(&tmp,c,"groups") ;
    tres->groups = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* contents */
    resolve_find_cdb(&tmp,c,"contents") ;
    tres->contents = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* ndepends */
    x = resolve_find_cdb(&tmp,c,"ndepends") ;
    tres->ndepends = x ;

    /* nrequiredby */
    x = resolve_find_cdb(&tmp,c,"nrequiredby") ;
    tres->nrequiredby = x ;

    /* nallow */
    x = resolve_find_cdb(&tmp,c,"nallow") ;
    tres->nallow = x ;

    /* ngroups */
    x = resolve_find_cdb(&tmp,c,"ngroups") ;
    tres->ngroups = x ;

    /* ncontents */
    x = resolve_find_cdb(&tmp,c,"ncontents") ;
    tres->ncontents = x ;

    /* init */
    x = resolve_find_cdb(&tmp,c,"init") ;
    tres->init = x ;

    /* disen */
    x = resolve_find_cdb(&tmp,c,"disen") ;
    tres->disen = x ;

    free(wres) ;
    stralloc_free(&tmp) ;

    return 1 ;
}

int tree_write_cdb(cdbmaker *c, resolve_tree_t *tres)
{
    log_flow() ;

    char *str = tres->sa.s ;

    /* name */
    if (!resolve_add_cdb(c,"name",str + tres->name) ||

    /* depends */
    !resolve_add_cdb(c,"depends",str + tres->depends) ||

    /* requiredby */
    !resolve_add_cdb(c,"requiredby",str + tres->requiredby) ||

    /* allow */
    !resolve_add_cdb(c,"allow",str + tres->allow) ||

    /* groups */
    !resolve_add_cdb(c,"groups",str + tres->groups) ||

    /* contents */
    !resolve_add_cdb(c,"contents",str + tres->contents) ||

    /* ndepends */
    !resolve_add_cdb_uint(c,"ndepends",tres->ndepends) ||

    /* nrequiredby */
    !resolve_add_cdb_uint(c,"nrequiredby",tres->nrequiredby) ||

    /* nallow */
    !resolve_add_cdb_uint(c,"nallow",tres->nallow) ||

    /* ngroups */
    !resolve_add_cdb_uint(c,"ngroups",tres->ngroups) ||

    /* ncontents */
    !resolve_add_cdb_uint(c,"ncontents",tres->ncontents) ||

    /* init */
    !resolve_add_cdb_uint(c,"init",tres->init) ||

    /* disen */
    !resolve_add_cdb_uint(c,"disen",tres->disen)) return 0 ;

    return 1 ;
}

int tree_resolve_copy(resolve_tree_t *dst, resolve_tree_t *tres)
{
    log_flow() ;

    stralloc_free(&dst->sa) ;

    size_t len = tres->sa.len - 1 ;
    dst->salen = tres->salen ;

    if (!stralloc_catb(&dst->sa,tres->sa.s,len) ||
        !stralloc_0(&dst->sa))
            return 0 ;

    dst->name = tres->name ;
    dst->depends = tres->depends ;
    dst->requiredby = tres->requiredby ;
    dst->allow = tres->allow ;
    dst->groups = tres->groups ;
    dst->contents = tres->contents ;
    dst->ndepends = tres->ndepends ;
    dst->nrequiredby = tres->nrequiredby ;
    dst->nallow = tres->nallow ;
    dst->ngroups = tres->ngroups ;
    dst->ncontents = tres->ncontents ;
    dst->init = tres->init ;
    dst->disen = tres->disen ;

    return 1 ;

}

int tree_resolve_modify_field(resolve_tree_t *tres, uint8_t field, char const *data)
{
    log_flow() ;

    uint32_t ifield ;
    int e = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, tres) ;

    switch(field) {

        case TREE_ENUM_NAME:
            tres->name = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_DEPENDS:
            tres->depends = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_REQUIREDBY:
            tres->requiredby = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_ALLOW:
            tres->allow = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_GROUPS:
            tres->groups = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_CONTENTS:
            tres->contents = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_NDEPENDS:
            if (!uint0_scan(data, &ifield)) goto err ;
            tres->ndepends = ifield ;
            break ;

        case TREE_ENUM_NREQUIREDBY:
            if (!uint0_scan(data, &ifield)) goto err ;
            tres->nrequiredby = ifield ;
            break ;

        case TREE_ENUM_NALLOW:
            if (!uint0_scan(data, &ifield)) goto err ;
            tres->nallow = ifield ;
            break ;

        case TREE_ENUM_NGROUPS:
            if (!uint0_scan(data, &ifield)) goto err ;
            tres->ngroups = ifield ;
            break ;

        case TREE_ENUM_NCONTENTS:
            if (!uint0_scan(data, &ifield)) goto err ;
            tres->ncontents = ifield ;
            break ;

        case TREE_ENUM_INIT:
            if (!uint0_scan(data, &ifield)) goto err ;
            tres->init = ifield ;
            break ;

        case TREE_ENUM_DISEN:
            if (!uint0_scan(data, &ifield)) goto err ;
            tres->disen = ifield ;
            break ;

        default:
            break ;
    }

    e = 1 ;

    err:
        free(wres) ;
        return e ;

}

int tree_resolve_field_tosa(stralloc *sa, resolve_tree_t *tres, resolve_tree_enum_t field)
{
    log_flow() ;

    uint32_t ifield ;

    switch(field) {

        case TREE_ENUM_NAME:
            ifield = tres->name ;
            break ;

        case TREE_ENUM_DEPENDS:
            ifield = tres->depends ;
            break ;

        case TREE_ENUM_REQUIREDBY:
            ifield = tres->requiredby ;
            break ;

        case TREE_ENUM_ALLOW:
            ifield = tres->allow ;
            break ;

        case TREE_ENUM_GROUPS:
            ifield = tres->groups ;
            break ;

        case TREE_ENUM_CONTENTS:
            ifield = tres->contents ;
            break ;

        case TREE_ENUM_NDEPENDS:
            ifield = tres->ndepends ;
            break ;

        case TREE_ENUM_NREQUIREDBY:
            ifield = tres->nrequiredby ;
            break ;

        case TREE_ENUM_NALLOW:
            ifield = tres->nallow ;
            break ;

        case TREE_ENUM_NGROUPS:
            ifield = tres->ngroups ;
            break ;

        case TREE_ENUM_NCONTENTS:
            ifield = tres->ncontents ;
            break ;

        case TREE_ENUM_INIT:
            ifield = tres->init ;
            break ;

        case TREE_ENUM_DISEN:
            ifield = tres->disen ;
            break ;

        default:
            return 0 ;
    }

    if (!auto_stra(sa,tres->sa.s + ifield))
        return 0 ;

    return 1 ;
}

/** Master */
int tree_read_master_cdb(cdb *c, resolve_tree_master_t *mres)
{
    log_flow() ;

    stralloc tmp = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres ;
    uint32_t x ;

    wres = resolve_set_struct(DATA_TREE_MASTER, mres) ;

    resolve_init(wres) ;

    /* name */
    resolve_find_cdb(&tmp,c,"name") ;
    mres->name = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* allow */
    resolve_find_cdb(&tmp,c,"allow") ;
    mres->allow = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* enabled */
    resolve_find_cdb(&tmp,c,"enabled") ;
    mres->enabled = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* current */
    resolve_find_cdb(&tmp,c,"current") ;
    mres->current = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* nenabled */
    x = resolve_find_cdb(&tmp,c,"nenabled") ;
    mres->nenabled = x ;

    free(wres) ;
    stralloc_free(&tmp) ;

    return 1 ;
}

int tree_write_master_cdb(cdbmaker *c, resolve_tree_master_t *mres)
{
    log_flow() ;

    char *str = mres->sa.s ;

    /* name */
    if (!resolve_add_cdb(c,"name",str + mres->name) ||

    /* allow */
    !resolve_add_cdb(c,"allow",str + mres->allow) ||

    /* enabled */
    !resolve_add_cdb(c,"enabled",str + mres->enabled) ||

    /* current */
    !resolve_add_cdb(c,"current",str + mres->current) ||

    /* nenabled */
    !resolve_add_cdb_uint(c,"nenabled",mres->nenabled)) return 0 ;

    return 1 ;
}

int tree_resolve_master_create(char const *base, uid_t owner)
{
    int e = 0 ;
    size_t baselen = strlen(base) ;
    struct passwd *pw = getpwuid(owner) ;
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;
    char dst[baselen + SS_SYSTEM_LEN + 1] ;

    if (!pw) {

        if (!errno)
            errno = ESRCH ;
        goto err ;
    }

    resolve_init(wres) ;

    auto_strings(dst, base, SS_SYSTEM) ;

    mres.name = resolve_add_string(wres, SS_MASTER + 1) ;
    mres.allow = resolve_add_string(wres, pw->pw_name) ;

    log_trace("write inner resolve file of trees") ;
    if (!resolve_write(wres, dst, SS_MASTER + 1))
        goto err ;

    e  = 1 ;

    err:
        resolve_free(wres) ;
        return e ;
}

int tree_resolve_master_copy(resolve_tree_master_t *dst, resolve_tree_master_t *mres)
{
    log_flow() ;

    stralloc_free(&dst->sa) ;

    size_t len = mres->sa.len - 1 ;
    dst->salen = mres->salen ;

    if (!stralloc_catb(&dst->sa,mres->sa.s,len) ||
        !stralloc_0(&dst->sa))
            return 0 ;

    dst->name = mres->name ;
    dst->allow = mres->allow ;
    dst->enabled = mres->enabled ;
    dst->current = mres->current ;
    dst->nenabled = mres->nenabled ;

    return 1 ;

}

int tree_resolve_master_modify_field(resolve_tree_master_t *mres, uint8_t field, char const *data)
{
    log_flow() ;

    uint32_t ifield ;
    int e = 0 ;

    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, mres) ;

    switch(field) {

        case TREE_ENUM_MASTER_NAME:
            mres->name = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_MASTER_ALLOW:
            mres->allow = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_MASTER_ENABLED:
            mres->enabled = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_MASTER_CURRENT:
            mres->current = resolve_add_string(wres,data) ;
            break ;

        case TREE_ENUM_MASTER_NENABLED:
            if (!uint0_scan(data, &ifield)) goto err ;
            mres->nenabled = ifield ;
            break ;

        default:
            break ;
    }

    e = 1 ;

    err:
        free(wres) ;
        return e ;

}

int tree_resolve_master_field_tosa(stralloc *sa, resolve_tree_master_t *mres, resolve_tree_master_enum_t field)
{
    log_flow() ;

    uint32_t ifield ;

    switch(field) {

        case TREE_ENUM_MASTER_NAME:
            ifield = mres->name ;
            break ;

        case TREE_ENUM_MASTER_ALLOW:
            ifield = mres->name ;
            break ;

        case TREE_ENUM_MASTER_ENABLED:
            ifield = mres->enabled ;
            break ;

        case TREE_ENUM_MASTER_CURRENT:
            ifield = mres->current ;
            break ;

        case TREE_ENUM_MASTER_NENABLED:
            ifield = mres->nenabled ;
            break ;

        default:
            return 0 ;
    }

    if (!auto_stra(sa,mres->sa.s + ifield))
        return 0 ;

    return 1 ;
}
