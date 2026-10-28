/*
 * resolve_contract.c
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

/* The contracts of the resolve primitives, by effect: resolve_get_sa() says a
 * key "sa" missing, empty or a cdb malformed by 0 EINVAL; resolve_get_field()
 * tells a resolve file missing (0) from a system error (-1); resolve_write_at()
 * derives the suffix of an addon from its type (resolve_addon_suffix());
 * resolve_add_list() joins an sbl into one field. Under a private mkdtemp
 * directory, inside a write transaction for the writes. */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include <oblibs/string.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/strbuf.h>
#include <oblibs/sbl.h>
#include <oblibs/cdb.h>
#include <oblibs/fd.h>
#include <oblibs/io.h>

#include <66/resolve.h>
#include <66/graph.h>
#include <66/service.h>
#include <66/constants.h>
#include <66/enum_service.h>

static char base[64] = "/tmp/66-resolve-contract.XXXXXX" ;

// a cdb at @file holding the key "sa" with @len bytes of @sa, or no key at all under @len < 0
static void make_cdb(char const *file, char const *sa, int len)
{
    ocdbmaker c = OCDBMAKER_ZERO ;
    int fd = io_open_mode(file, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644) ;
    assert(fd >= 0) ;
    assert(ocdb_make_start(&c, fd)) ;
    if (len >= 0)
        assert(ocdb_make_add(&c, "sa", 2, sa, (uint32_t)len)) ;
    else
        assert(ocdb_make_add(&c, "other", 5, "x", 1)) ;
    assert(ocdb_make_finish(&c)) ;
    close_fd(fd) ;
}

// the entries of @dir, . and .. aside
static int nentries(char const *dir)
{
    int n = 0 ;
    struct dirent *e ;
    DIR *d = opendir(dir) ;

    assert(d) ;
    while ((e = readdir(d)))
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
            n++ ;
    closedir(d) ;

    return n ;
}

static void test_get_sa(void)
{
    printf("resolve_get_sa: missing, empty or malformed is 0 EINVAL\n") ;

    char file[SS_MAX_PATH_LEN] ;
    strbuf sa = STRBUF_ZERO ;
    ocdb c = OCDB_ZERO ;

    auto_strings(file, base, "full") ;
    make_cdb(file, "a\0b", 3) ;
    assert(ocdb_init(&c, file)) ;
    assert(resolve_get_sa(&sa, &c) == 1 && sa.len == 3) ;
    ocdb_free(&c) ;

    auto_strings(file, base, "empty") ;
    make_cdb(file, "", 0) ;
    assert(ocdb_init(&c, file)) ;
    errno = 0 ;
    assert(resolve_get_sa(&sa, &c) == 0 && errno == EINVAL && !sa.len) ;
    ocdb_free(&c) ;

    auto_strings(file, base, "nokey") ;
    make_cdb(file, 0, -1) ;
    assert(ocdb_init(&c, file)) ;
    errno = 0 ;
    assert(resolve_get_sa(&sa, &c) == 0 && errno == EINVAL) ;
    ocdb_free(&c) ;

    strbuf_free(&sa) ;
}

static void test_write_at(void)
{
    printf("resolve_write_at: the suffix of the file comes from the type\n") ;

    char dir[SS_MAX_PATH_LEN], file[SS_MAX_PATH_LEN] ;
    resolve_service_addon_execute_t ex = RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE_EXECUTE, &ex) ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t wl = RESOLVE_WRAPPER(DATA_SERVICE_LOGGER, &res) ;

    char tmpdir[SS_MAX_PATH_LEN] ;

    auto_strings(dir, base, "at") ;
    auto_strings(file, dir, SS_RESOLVE) ;
    assert(dir_create_parent(file, 0755)) ;
    auto_strings(tmpdir, dir, "/tmp") ;
    assert(dir_create_parent(tmpdir, 0755)) ;

    resolve_init(&w) ;
    ex.notify = 7 ;
    ex.chdir = resolve_add_string(&w, "/x") ;
    assert(resolve_write_at(&w, dir, "svc", tmpdir) == 1) ;

    // the temporary was made in the directory given, and renamed: none left there
    assert(nentries(tmpdir) == 0) ;

    // made nowhere else: a directory given that does not exist fails the write
    {
        char gone[SS_MAX_PATH_LEN] ;
        auto_strings(gone, dir, "/gone") ;
        errno = 0 ;
        assert(resolve_write_at(&w, dir, "svc", gone) == 0 && errno == ENOENT) ;
        assert(nentries(file) == 1) ;
    }

    auto_strings(file, dir, SS_RESOLVE, "/svc", SS_ADDON_EXECUTE_SUFFIX) ;
    assert(access(file, F_OK) == 0) ;
    auto_strings(file, dir, SS_RESOLVE, "/svc") ;
    assert(access(file, F_OK) < 0 && errno == ENOENT) ;

    resolve_free_obj(&w) ;
    ex = (resolve_service_addon_execute_t)RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    char rdir[SS_MAX_PATH_LEN] ;
    auto_strings(rdir, dir, SS_RESOLVE, "/") ;
    assert(resolve_read_cdb(&w, rdir, "svc" SS_ADDON_EXECUTE_SUFFIX) == 1 && ex.notify == 7) ;
    resolve_free_obj(&w) ;

    // no file for a logger addon: refused, nothing written
    errno = 0 ;
    assert(resolve_write_at(&wl, dir, "svc", tmpdir) == 0 && errno == EINVAL) ;
}

static void test_get_field(void)
{
    printf("resolve_get_field: 0 for a resolve file missing, -1 for a system error\n") ;

    strbuf sa = STRBUF_ZERO ;
    char fbase[SS_MAX_PATH_LEN] ;
    resolve_enum_table_t t = E_TABLE_SERVICE_DEPS_ZERO ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE, &res) ;
    resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    resolve_wrapper_t wd = RESOLVE_WRAPPER(DATA_SERVICE_DEPENDENCIES, &dep) ;

    t.u.service.id = E_RESOLVE_SERVICE_DEPS_PROVIDE ;

    errno = 0 ;
    assert(resolve_get_field(&sa, &w, base, "nobody", t) == 0 && errno == ENOENT) ;
    assert(resolve_get_field(&sa, &w, base, 0, t) == 0) ;

    // an addon absent reads its defaults: an empty string field
    assert(resolve_get_field(&sa, &wd, base, "nobody", t) == 1 && !sa.len) ;
    resolve_free_obj(&wd) ;

    auto_strings(fbase, base, "full/") ;
    errno = 0 ;
    assert(resolve_get_field(&sa, &w, fbase, "x", t) == -1 && errno == ENOTDIR) ;

    strbuf_free(&sa) ;
}

static void test_damaged_servicedir(void)
{
    printf("resolve_read: a service link to something that is no directory does not read, 0 EINVAL\n") ;

    char entry[SS_MAX_PATH_LEN], svc[SS_MAX_PATH_LEN] ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE, &res) ;

    auto_strings(entry, base, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;
    auto_strings(svc, base, SS_SYSTEM, SS_SERVICE, SS_SVC) ;
    assert(dir_create_parent(entry, 0755) && dir_create_parent(svc, 0755)) ;
    auto_strings(entry + strlen(entry), "/damaged") ;
    auto_strings(svc + strlen(svc), "/damaged") ;
    // the service directory replaced by a file, its link left standing
    assert(file_write(svc, "x", 1)) ;
    assert(symlink(svc, entry) == 0) ;

    errno = 0 ;
    assert(resolve_read(&w, base, "damaged") == 0 && errno == EINVAL) ;
    resolve_free_obj(&w) ;
}

static void test_write_addon_errno(void)
{
    printf("resolve_write: an addon of a service without resolve file, 0 with the errno of the read\n") ;

    resolve_service_addon_limit_t l = RESOLVE_SERVICE_ADDON_LIMIT_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE_LIMIT, &l) ;

    resolve_init(&w) ;
    errno = 0 ;
    assert(resolve_write(&w, base, "nobody") == 0 && errno == ENOENT) ;
    resolve_free_obj(&w) ;
}

static void test_add_list(void)
{
    printf("resolve_add_list: an sbl joined by spaces, one field\n") ;

    uint32_t field = 99 ;
    strbuf list = STRBUF_ZERO ;
    resolve_service_addon_dependencies_t dep = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    resolve_wrapper_t w = RESOLVE_WRAPPER(DATA_SERVICE_DEPENDENCIES, &dep) ;

    resolve_init(&w) ;

    assert(resolve_add_list(&w, &list, &field) == 1 && !strcmp(dep.sa.s + field, "")) ;

    assert(sbl_add(&list, "a") && sbl_add(&list, "bb") && sbl_add(&list, "c")) ;
    assert(resolve_add_list(&w, &list, &field) == 1 && !strcmp(dep.sa.s + field, "a bb c")) ;
    // the list is left as it was
    assert(sbl_count(&list) == 3) ;

    strbuf_free(&list) ;
    resolve_free_obj(&w) ;
}

int main(void)
{
    printf("Starting resolve contract test...\n") ;

    assert(mkdtemp(base)) ;
    strcat(base, "/") ;

    assert(service_graph_write_begin(getuid(), base, base) == 1) ;

    test_get_sa() ;
    test_write_at() ;
    test_get_field() ;
    test_damaged_servicedir() ;
    test_write_addon_errno() ;
    test_add_list() ;

    assert(service_graph_write_end(0) == 1) ;
    assert(dir_destroy(base)) ;

    printf("All tests passed!\n") ;
    return 0 ;
}
