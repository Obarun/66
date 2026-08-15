/*
 * dependencies_counter_normalise.c
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

/* Offset 0 is the unset sentinel, so the offset is what says whether a list is
 * there. A version writing into a block it had never initialised landed its
 * first string on 0 and kept the count, leaving an addon that contradicts
 * itself: nrequiredby = 1 with requiredby unset. Readers trust the count and
 * then walk an empty string, which sbl_clean_string rejects with EINVAL -- a
 * whole command dies on a service it only meant to look at.
 *
 * The reader settles it: a list whose offset is unset has no members, whatever
 * the count claims. This covers the six pairs of the addon. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/types.h>

#include <66/resolve.h>
#include <66/service.h>
#include <66/constants.h>

static void make_layout(char const *base, char const *name)
{
    char entry[SS_MAX_PATH_LEN], svcdir[SS_MAX_PATH_LEN], svcres[SS_MAX_PATH_LEN], link[SS_MAX_PATH_LEN] ;
    auto_strings(entry, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;
    auto_strings(svcdir, base, SS_SYSTEM, SS_SERVICE, SS_SVC, "/", name) ;
    auto_strings(svcres, svcdir, SS_RESOLVE) ;
    auto_strings(link, entry, "/", name) ;

    assert(dir_create_parent(entry, 0755)) ;
    assert(dir_create_parent(svcres, 0755)) ;
    assert(symlink(svcdir, link) == 0) ;
}

static void write_core(char const *base, char const *name)
{
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE, &res) ;
    resolve_init(w) ;
    res.name = resolve_add_string(w, name) ;
    res.path.home = resolve_add_string(w, base) ;
    res.has_dependencies = 1 ;
    assert(resolve_write(w, base, name) > 0) ;
    resolve_free(w) ;
}

static void read_addon(resolve_service_addon_dependencies_t *dep, char const *base, char const *name)
{
    resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, dep) ;
    assert(resolve_read(w, base, name) > 0) ;
    free(w) ;
}

int main(void)
{
    printf("Starting dependencies counter normalisation test...\n") ;

    char base[64] = "/tmp/66-depnorm.XXXXXX" ;
    assert(mkdtemp(base)) ;
    strcat(base, "/") ;

    /* the state a pre-sentinel version left on disk: every count set, every
     * offset still unset */
    make_layout(base, "liar") ;
    write_core(base, "liar") ;
    {
        resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
        resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &dep) ;
        resolve_init(w) ;
        dep.ndepends = 1 ;
        dep.nrequiredby = 2 ;
        dep.noptsdeps = 3 ;
        dep.ncontents = 4 ;
        dep.nprovide = 5 ;
        dep.nconflict = 6 ;
        assert(resolve_write(w, base, "liar") > 0) ;
        resolve_free(w) ;
    }
    {
        resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
        read_addon(&dep, base, "liar") ;

        /* the offsets are still unset, so no list has members */
        assert(dep.depends == 0 && dep.ndepends == 0) ;
        assert(dep.requiredby == 0 && dep.nrequiredby == 0) ;
        assert(dep.optsdeps == 0 && dep.noptsdeps == 0) ;
        assert(dep.contents == 0 && dep.ncontents == 0) ;
        assert(dep.provide == 0 && dep.nprovide == 0) ;
        assert(dep.conflict == 0 && dep.nconflict == 0) ;
        strbuf_free(&dep.sa) ;
    }

    /* a sound addon keeps every count it was written with */
    make_layout(base, "sound") ;
    write_core(base, "sound") ;
    {
        resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
        resolve_wrapper_t *w = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &dep) ;
        resolve_init(w) ;
        dep.depends = resolve_add_string(w, "alpha beta") ;
        dep.ndepends = 2 ;
        dep.requiredby = resolve_add_string(w, "gamma") ;
        dep.nrequiredby = 1 ;
        dep.contents = resolve_add_string(w, "delta") ;
        dep.ncontents = 1 ;
        assert(resolve_write(w, base, "sound") > 0) ;
        resolve_free(w) ;
    }
    {
        resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
        read_addon(&dep, base, "sound") ;

        assert(dep.ndepends == 2 && !strcmp(dep.sa.s + dep.depends, "alpha beta")) ;
        assert(dep.nrequiredby == 1 && !strcmp(dep.sa.s + dep.requiredby, "gamma")) ;
        assert(dep.ncontents == 1 && !strcmp(dep.sa.s + dep.contents, "delta")) ;
        /* the lists it never had stay empty on both counts */
        assert(dep.noptsdeps == 0 && dep.nprovide == 0 && dep.nconflict == 0) ;
        strbuf_free(&dep.sa) ;
    }

    printf("All tests passed!\n") ;
    return 0 ;
}
