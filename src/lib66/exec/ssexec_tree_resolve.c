/*
 * ssexec_tree_resolve.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include <oblibs/log.h>
#include <oblibs/opt.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/resolve.h>
#include <66/ssexec.h>
#include <66/tree.h>
#include <66/info.h>
#include <66/constants.h>
#include <66/config.h>

/* One row per cdb key, in the order and with the names written by
 * tree_resolve_write_cdb.c (init and supervised are stored but not displayed). */

static info_field_t const fields_tree[] = {
    { "name",        INFO_FIELD_STR, offsetof(resolve_tree_t, name) },
    { "enabled",     INFO_FIELD_U32, offsetof(resolve_tree_t, enabled) },
    { "depends",     INFO_FIELD_STR, offsetof(resolve_tree_t, depends) },
    { "requiredby",  INFO_FIELD_STR, offsetof(resolve_tree_t, requiredby) },
    { "allow",       INFO_FIELD_STR, offsetof(resolve_tree_t, allow) },
    { "groups",      INFO_FIELD_STR, offsetof(resolve_tree_t, groups) },
    { "contents",    INFO_FIELD_STR, offsetof(resolve_tree_t, contents) },
    { "ndepends",    INFO_FIELD_U32, offsetof(resolve_tree_t, ndepends) },
    { "nrequiredby", INFO_FIELD_U32, offsetof(resolve_tree_t, nrequiredby) },
    { "nallow",      INFO_FIELD_U32, offsetof(resolve_tree_t, nallow) },
    { "ngroups",     INFO_FIELD_U32, offsetof(resolve_tree_t, ngroups) },
    { "ncontents",   INFO_FIELD_U32, offsetof(resolve_tree_t, ncontents) },
    { "rversion",    INFO_FIELD_STR, offsetof(resolve_tree_t, rversion) },
} ;

/* One row per cdb key, as written by tree_resolve_master_write_cdb.c. */

static info_field_t const fields_master[] = {
    { "name",        INFO_FIELD_STR, offsetof(resolve_tree_master_t, name) },
    { "allow",       INFO_FIELD_STR, offsetof(resolve_tree_master_t, allow) },
    { "current",     INFO_FIELD_STR, offsetof(resolve_tree_master_t, current) },
    { "contents",    INFO_FIELD_STR, offsetof(resolve_tree_master_t, contents) },
    { "nallow",      INFO_FIELD_U32, offsetof(resolve_tree_master_t, nallow) },
    { "ncontents",   INFO_FIELD_U32, offsetof(resolve_tree_master_t, ncontents) },
    { "rversion",    INFO_FIELD_STR, offsetof(resolve_tree_master_t, rversion) },
} ;

/* option state, set by on_tree_resolve, drained at the top of ssexec_tree_resolve */
static char const *opt_field = 0 ;
static uint8_t opt_noname = 0 ;

int on_tree_resolve(int id, char const *arg, void *data)
{
    (void)data ;

    switch (id) {

        case 'f' :

            opt_field = arg ;
            break ;

        case 'n' :

            opt_noname = 1 ;
            break ;
    }

    return 0 ;
}

int ssexec_tree_resolve(int argc, char const *const *argv, void *data)
{
    ssexec_t *info = data ;

    /* drain option state into locals, then reset the statics for re-entrancy */
    char const *field = opt_field ;
    uint8_t noname = opt_noname ;
    opt_field = 0 ;
    opt_noname = 0 ;

    int r = 0 ;
    uint8_t master = 0 ;
    char const *treename = 0 ;

    resolve_wrapper_t_ref wres = 0 ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;

    if (argc < 1)
        log_die(LOG_EXIT_USER, "missing tree argument") ;

    treename = argv[0] ;

    if (treename[0] == '/') {

        _alloc_strbuf_(basename, strlen(treename) + 1) ;
        _alloc_strbuf_(dirname, strlen(treename) + 1) ;

        if (!ob_basename(basename.s, treename))
            log_dieusys(LOG_EXIT_SYS, "get basename of: ", treename) ;

        if (!strcmp(basename.s, SS_MASTER + 1)) {

            master = 1 ;
            wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;

        } else {

            wres = resolve_set_struct(DATA_TREE, &tres) ;
        }

        if (!ob_dirname(dirname.s, treename))
            log_dieu(LOG_EXIT_SYS, "get dirname of: ", treename) ;

        if (resolve_read_cdb(wres, dirname.s, basename.s) <= 0)
            log_dieusys(LOG_EXIT_SYS, "read resolve file") ;

    } else {

        if (!strcmp(treename, SS_MASTER + 1)) {

            master = 1 ;
            wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;

        } else {

            wres = resolve_set_struct(DATA_TREE, &tres) ;

            r = tree_isvalid(info->base.s, treename) ;

            if (r < 0)
                log_dieu(LOG_EXIT_SYS, "check validity of tree: ", treename) ;

            if (!r)
                log_dieusys(LOG_EXIT_SYS, "find tree: ", treename) ;
        }

        if (resolve_read_g(wres, info->base.s, treename) <= 0)
            log_dieusys(LOG_EXIT_SYS, "read resolve file") ;
    }

    if (master)
        info_resolve_display(&mres, mres.sa.s, fields_master, OPT_COUNT(fields_master), field, noname) ;
    else
        info_resolve_display(&tres, tres.sa.s, fields_tree, OPT_COUNT(fields_tree), field, noname) ;

    resolve_free(wres) ;

    return 0 ;
}
