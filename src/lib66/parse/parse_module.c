/*
 * parse_module.c
 *
 * Copyright (c) 2018-2022 Eric Vidal <eric@obarun.org>
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
#include <errno.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h> //chdir, unlink
#include <stdlib.h>
#include <stdio.h> //rename

#include <oblibs/string.h>
#include <oblibs/log.h>
#include <oblibs/mill.h>
#include <oblibs/sastr.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/environ.h>
#include <oblibs/files.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h> //hiercopy
#include <skalibs/env.h> //hiercopy
#include <skalibs/bytestr.h>

#include <66/write.h>
#include <66/utils.h>
#include <66/environ.h>
#include <66/resolve.h>
#include <66/service.h>
#include <66/constants.h>
#include <66/utils.h>

#define SS_MODULE_CONFIG_DIR "/configure"
#define SS_MODULE_CONFIG_DIR_LEN (sizeof SS_MODULE_CONFIG_DIR - 1)
#define SS_MODULE_CONFIG_SCRIPT "configure"
#define SS_MODULE_CONFIG_SCRIPT_LEN (sizeof SS_MODULE_CONFIG_SCRIPT - 1)
#define SS_MODULE_SERVICE "/service"
#define SS_MODULE_SERVICE_LEN (sizeof SS_MODULE_SERVICE - 1)
#define SS_MODULE_SERVICE_INSTANCE "/service@"
#define SS_MODULE_SERVICE_INSTANCE_LEN (sizeof SS_MODULE_SERVICE_INSTANCE - 1)

static void instance_splitname_to_char(char *store, char const *name, int len, int what)
{
    log_flow() ;

    char const *copy ;
    size_t tlen = len + 1, clen = 0 ;

    char template[tlen + 1] ;
    memcpy(template,name,tlen) ;
    template[tlen] = 0 ;

    copy = name + tlen ;

    if (!what) {

        auto_strings(store, template) ;

    } else {

        clen = strlen(copy) ;
        memcpy(store, copy, clen) ;
        store[clen] = 0 ;
    }

}

static void parse_module_check_dir(char const *src,char const *dir)
{
    log_flow() ;

    int r ;
    size_t srclen = strlen(src) ;
    size_t dirlen = strlen(dir) ;

    char t[srclen + dirlen + 1] ;
    auto_strings(t, src, dir) ;

    r = scan_mode(t,S_IFDIR) ;
    if (r < 0) {
        errno = EEXIST ;
        log_diesys(LOG_EXIT_ZERO, "conflicting format of: ", t) ;
    }

    if (!r)
        if (!dir_create_parent(t, 0755))
            log_dieusys(LOG_EXIT_ZERO, "create directory: ", t) ;
}

static void parse_module_check_name(char const *src, char const *name)
{
    log_flow() ;

    char basename[strlen(src)] ;
    int insta = -1 ;

    if (!ob_basename(basename, src))
        log_dieu(LOG_EXIT_SYS, "get basename of: ", src) ;

    insta = instance_check(name) ;

    if (insta <= 0)
        log_die(LOG_EXIT_SYS, "invalid module instance name: ", name) ;

    if (basename[0] != '@')
        log_die(LOG_EXIT_USER, "invalid directory name for module: ", name, " -- directory name must start with a '@' character") ;
}

static int parse_module_ownerhome(char *store)
{
    log_flow() ;

    char const *user_home = 0 ;
    int e = errno ;
    struct passwd *st = getpwuid(getuid()) ;
    errno = 0 ;
    if (!st) {
        if (!errno) errno = ESRCH ;
        return 0 ;
    }
    user_home = st->pw_dir ;
    errno = e ;
    if (!user_home)
        return 0 ;

    auto_strings(store, user_home, "/") ;

    return 1 ;
}

/* 0 filename undefine
 * -1 system error
 * should return at least 2 meaning :: no file define*/
static int regex_get_file_name(char *filename, char const *str)
{
    log_flow() ;

    int r ;
    size_t pos = 0 ;
    stralloc kp = STRALLOC_ZERO ;

    parse_mill_t MILL_GET_COLON = {
    .open = ':', .close = ':',
    .skip = " \t\r", .skiplen = 3,
    .forceclose = 1,
    .inner.debug = "get_colon" } ;

    r = mill_element(&kp, str, &MILL_GET_COLON, &pos) ;
    if (r == -1)
        log_dieu(LOG_EXIT_SYS, "get filename of line: ", str) ;

    auto_strings(filename, kp.s) ;

    stralloc_free(&kp) ;
    return pos ;
}

static void regex_get_replace(char *replace, char const *str)
{
    log_flow() ;

    int pos = get_len_until(str,'=') ;
    if (!pos || pos == -1)
       log_dieu(LOG_EXIT_SYS,"replace string of line: ", str) ;
    char tmp[pos + 1] ;
    memcpy(tmp,str,pos) ;
    tmp[pos] = 0 ;
    auto_strings(replace, tmp) ;
}

static void regex_get_regex(char *regex, char const *str)
{
    log_flow() ;

    size_t len = strlen(str) ;
    int pos = get_len_until(str,'=') ;
    if (!pos || pos == -1)
        log_dieu(LOG_EXIT_SYS, "get regex string of line: ", str) ;
    pos++ ; // remove '='
    char tmp[len + 1] ;
    memcpy(tmp,str + pos,len-pos) ;
    tmp[len-pos] = 0 ;
    auto_strings(regex,tmp) ;
}

static void get_list(stralloc *list, char const *src, char const *name, mode_t mode)
{
    log_flow() ;

    list->len = 0 ;
    char const *exclude[2] = { SS_MODULE_CONFIG_DIR + 1, 0 } ;

    char t[strlen(src) + 1] ;

    auto_strings(t, src) ;

    if (!sastr_dir_get_recursive(list, t, exclude, mode, 1))
        log_dieusys(LOG_EXIT_SYS,"get file(s) of module: ", name) ;

}

static void regex_replace(stralloc *list, resolve_service_t *res)
{
    log_flow() ;

    int r ;
    size_t pos = 0, idx = 0 ;

    stralloc frontend = STRALLOC_ZERO ;
    stralloc sa = STRALLOC_ZERO ;

    if (!res->regex.ninfiles)
        return ;

    if (!sastr_clean_string(&sa, res->sa.s + res->regex.infiles))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    FOREACH_SASTR(list, pos) {

        frontend.len = idx = 0 ;
        char *str = list->s + pos ;
        size_t len = strlen(str) ;
        char bname[len + 1] ;
        char dname[len + 1] ;

        if (!ob_basename(bname, str))
            log_dieu(LOG_EXIT_SYS, "get basename of: ", str) ;

        if (!ob_dirname(dname, str))
            log_dieu(LOG_EXIT_SYS, "get dirname of: ", str) ;

        //log_trace("read service file of: ", dname, bname) ;
        r = read_svfile(&frontend, bname, dname) ;
        if (!r)
            log_dieusys(LOG_EXIT_SYS, "read file: ", str) ;
        else if (r == -1)
            continue ;

        {
            FOREACH_SASTR(&sa, idx) {

                int all = 0, fpos = 0 ;
                char const *line = sa.s + idx ;
                size_t linelen = strlen(line) ;
                char filename[SS_MAX_SERVICE_NAME + 1] ;
                char replace[linelen + 1] ;
                char regex[linelen + 1] ;

                if (linelen >= SS_MAX_PATH_LEN)
                    log_die(LOG_EXIT_SYS, "limit exceeded in service: ", res->sa.s + res->name) ;

                if ((line[0] != ':') || (get_sep_before(line + 1, ':', '=') < 0))
                    log_die(LOG_EXIT_SYS, "bad format in line: ", line, " of key @infiles field") ;

                memset(filename, 0, SS_MAX_SERVICE_NAME + 1) ;
                memset(replace, 0, linelen + 1) ;
                memset(regex, 0, linelen + 1) ;

                fpos = regex_get_file_name(filename, line) ;
                if (fpos < 3) all = 1 ;

                regex_get_replace(replace, line + fpos) ;

                regex_get_regex(regex, line + fpos) ;

                if (!strcmp(bname, filename) || all) {

                    if (!sastr_replace_all(&frontend, replace, regex))
                        log_dieu(LOG_EXIT_SYS, "replace: ", replace, " by: ", regex, " in file: ", str) ;

                    if (!stralloc_0(&frontend))
                        log_dieusys(LOG_EXIT_SYS, "stralloc") ;

                    frontend.len-- ;

                    if (!file_write_unsafe(dname, bname, frontend.s, frontend.len))
                        log_dieusys(LOG_EXIT_SYS, "write: ", dname, "/", bname) ;
                }
            }
        }
    }

    stralloc_free(&sa) ;
}

static void regex_rename(stralloc *list, resolve_service_t *res, uint32_t element)
{
    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;

    if (!element)
        return ;

    if (!sastr_clean_string(&sa, res->sa.s + element))
        log_dieu(LOG_EXIT_SYS, "clean string") ;

    size_t pos = 0, idx = 0, salen = sa.len ;
    char t[sa.len] ;

    sastr_to_char(t, &sa) ;

    for (; pos < salen ; pos += strlen(t + pos) + 1) {

        idx = 0 ;
        char *line = t + pos ;
        char replace[SS_MAX_PATH] = { 0 } ;
        char regex[SS_MAX_PATH] = { 0 } ;

        regex_get_replace(replace,line) ;

        regex_get_regex(regex,line) ;

        FOREACH_SASTR(list, idx) {

            sa.len = 0 ;
            char *str = list->s + idx ;
            size_t len = strlen(str) ;
            char dname[len + 1] ;

            if (!ob_dirname(dname, str))
                log_dieu(LOG_EXIT_SYS, "get dirname of: ", str) ;

            if (!sabasename(&sa, str, len))
                log_dieu(LOG_EXIT_SYS, "get basename of: ", str) ;

            if (!stralloc_0(&sa))
                log_die_nomem("stralloc") ;

            if (!sastr_replace(&sa, replace, regex))
                log_dieu(LOG_EXIT_SYS, "replace: ", replace, " by: ", regex, " in file: ", str) ;

            if (!stralloc_0(&sa))
                log_die_nomem("stralloc") ;

            char new[len + sa.len + 1] ;
            auto_strings(new, dname, sa.s) ;

            /** do not try to rename the same directory */
            if (strcmp(str, new)) {

                log_trace("rename: ", str, " to: ", new) ;
                if (rename(str, new) == -1)
                    //log_warnusys( "rename: ", str, " to: ", new) ;
                    log_dieusys(LOG_EXIT_SYS, "rename: ", str, " to: ", new) ;

                break ;
            }
        }
    }
    stralloc_free(&sa) ;
}

static void regex_configure(resolve_service_t *res, ssexec_t *info, char const *path, char const *name)
{
    log_flow() ;

    int wstat, r ;
    pid_t pid ;
    size_t clen = res->regex.configure > 0 ? 1 : 0 ;
    size_t pathlen = strlen(path), n ;

    stralloc env = STRALLOC_ZERO ;

    char const *newargv[2 + clen] ;
    unsigned int m = 0 ;

    char pwd[pathlen + 1 + SS_MODULE_CONFIG_DIR_LEN + 1] ;
    auto_strings(pwd, path, "/", SS_MODULE_CONFIG_DIR + 1) ;

    char config_script[pathlen + 1 + SS_MODULE_CONFIG_DIR_LEN + 1 + SS_MODULE_CONFIG_SCRIPT_LEN + 1] ;
    auto_strings(config_script, path, "/", SS_MODULE_CONFIG_DIR + 1, "/", SS_MODULE_CONFIG_SCRIPT) ;

    r = scan_mode(config_script, S_IFREG) ;
    if (r > 0)
    {
        /** export ssexec_t info value on the environment */
        {
            char verbo[UINT_FMT];
            verbo[uid_fmt(verbo, VERBOSITY)] = 0 ;
            if (!auto_stra(&env, \
            "MOD_NAME=", name, "\n", \
            "MOD_BASE=", res->sa.s + res->path.home, "\n", \
            "MOD_LIVE=", res->sa.s + res->live.livedir, "\n", \
            "MOD_TREE=", res->sa.s + res->path.tree, "\n", \
            "MOD_SCANDIR=", res->sa.s + res->live.scandir, "\n", \
            "MOD_TREENAME=", res->sa.s + res->treename, "\n", \
            "MOD_OWNER=", res->sa.s + res->ownerstr, "\n", \
            "MOD_COLOR=", info->opt_color ? "1" : "0", "\n", \
            "MOD_VERBOSITY=", verbo, "\n", \
            "MOD_MODULE_DIR=", path, "\n", \
            "MOD_SKEL_DIR=", SS_SKEL_DIR, "\n", \
            "MOD_SERVICE_SYSDIR=", SS_SERVICE_SYSDIR, "\n", \
            "MOD_SERVICE_ADMDIR=", SS_SERVICE_ADMDIR, "\n", \
            "MOD_SERVICE_ADMCONFDIR=", SS_SERVICE_ADMCONFDIR, "\n", \
            "MOD_MODULE_SYSDIR=", SS_MODULE_SYSDIR, "\n", \
            "MOD_MODULE_ADMDIR=", SS_MODULE_ADMDIR, "\n", \
            "MOD_SCRIPT_SYSDIR=", SS_SCRIPT_SYSDIR, "\n", \
            "MOD_USER_DIR=", SS_USER_DIR, "\n", \
            "MOD_SERVICE_USERDIR=", SS_SERVICE_USERDIR, "\n", \
            "MOD_SERVICE_USERCONFDIR=", SS_SERVICE_USERCONFDIR, "\n", \
            "MOD_MODULE_USERDIR=", SS_MODULE_USERDIR, "\n", \
            "MOD_SCRIPT_USERDIR=", SS_SCRIPT_USERDIR, "\n"))
                log_dieu(LOG_EXIT_SYS, "append environment variables") ;
        }

        /** environment is not mandatory */
        if (res->environ.env > 0)
        {
            stralloc oenv = STRALLOC_ZERO ;
            stralloc dst = STRALLOC_ZERO ;
            char name[strlen(res->sa.s + res->name) + 2] ;
            auto_strings(name, ".", res->sa.s + res->name) ;

            if (!env_prepare_for_write(&dst, &oenv, res))
                log_dieu(LOG_EXIT_SYS, "prepare environment") ;

            write_environ(name, oenv.s, dst.s) ;

            /** Reads all files from the directory */
            if (!environ_clean_envfile(&env, dst.s))
                log_dieu(LOG_EXIT_SYS, "read environment") ;

            if (!environ_remove_unexport(&env, &env))
                log_dieu(LOG_EXIT_SYS, "remove exclamation mark from environment variables") ;

            stralloc_free(&oenv) ;
            stralloc_free(&dst) ;
        }

        if (!sastr_split_string_in_nline(&env))
            log_dieu(LOG_EXIT_SYS, "rebuild environment") ;

        n = env_len((const char *const *)environ) + 1 + byte_count(env.s,env.len,'\0') ;
        char const *newenv[n + 1] ;

        if (!env_merge (newenv, n ,(const char *const *)environ,env_len((const char *const *)environ), env.s, env.len))
            log_dieu(LOG_EXIT_SYS, "build environment") ;

        if (chdir(pwd) < 0)
            log_dieu(LOG_EXIT_SYS, "chdir to: ", pwd) ;

        m = 0 ;
        newargv[m++] = config_script ;

        if (res->regex.configure > 0)
            newargv[m++] = res->sa.s + res->regex.configure ;

        newargv[m++] = 0 ;

        log_info("launch script configure of module: ", name) ;

        pid = child_spawn0(newargv[0], newargv, newenv) ;

        if (waitpid_nointr(pid, &wstat, 0) < 0)
            log_dieusys(LOG_EXIT_SYS, "wait for: ", config_script) ;

        if (wstat)
            log_dieu(LOG_EXIT_SYS, "run: ", config_script) ;
    }

    stralloc_free(&env) ;
}

void parse_module(resolve_service_t *res, resolve_service_t *ares, unsigned int *areslen, ssexec_t *info, uint8_t force)
{
    log_flow() ;

    int r, insta = -1 ;
    unsigned int residx = 0 ;
    size_t pos = 0, pathlen = 0 ;
    char *name = res->sa.s + res->name ;
    char *src = res->sa.s + res->path.frontend ;
    char path[SS_MAX_PATH_LEN] ;
    char ainsta[strlen(name)] ;
    stralloc list = STRALLOC_ZERO ;
    resolve_wrapper_t_ref wres = 0 ;

    log_trace("parse module: ", name) ;

    insta = instance_check(name) ;
    instance_splitname_to_char(ainsta, name, insta, 0) ;

    size_t prefixlen = strlen(ainsta) ;
    size_t len = prefixlen + SS_MAX_SERVICE_NAME ;

    char prefix[len + 1] ;
    auto_strings(prefix, ainsta) ;
    //prefix[insta] = '-' ;
    prefix[insta] = 0 ;

    if (!getuid()) {

        auto_strings(path, SS_SERVICE_ADMDIR, name) ;

    } else {

        if (!parse_module_ownerhome(path))
            log_dieusys(LOG_EXIT_SYS, "unable to find the home directory of the user") ;

        pathlen = strlen(path) ;
        auto_strings(path + pathlen, SS_SERVICE_USERDIR, name) ;
    }

    uint8_t conf = res->environ.env_overwrite ;

    /** Check the validity of the module directory.
     * The frontend file of the module must be located in
     * a directory with a name in the format @template_name:
     *      - frontend file -> scandir@
     *      - directory name containing the frontend service file -> @scandir */
    parse_module_check_name(src, name) ;

    /** check mandatory directories
     * res->frontend/module_name/{configure,service,service@} */
    parse_module_check_dir(src, SS_MODULE_CONFIG_DIR) ;
    parse_module_check_dir(src, SS_MODULE_SERVICE) ;
    parse_module_check_dir(src, SS_MODULE_SERVICE_INSTANCE) ;

    r = scan_mode(path, S_IFDIR) ;
    if (r == -1) {
        errno = EEXIST ;
        log_dieusys(LOG_EXIT_SYS, "conflicting format of: ", path) ;

    } else if (!r) {

        if (!hiercopy(src, path))
            log_dieusys(LOG_EXIT_SYS, "copy: ", src, " to: ", path) ;

    } else {

        /** Must reconfigure all services of the module */
        if (force < 2) {

            log_warn("skip configuration of the module: ", name, " -- already configured") ;
            goto deps ;
        }

        if (rm_rf(path) < 0)
            log_dieusys (LOG_EXIT_SYS, "remove: ", path) ;

        if (!hiercopy(src, path))
            log_dieusys(LOG_EXIT_SYS,"copy: ", src, " to: ", path) ;
    }

    pathlen = strlen(path) ;
    /** remove the original service frontend file inside the copied directory
     * to avoid double frontend service file for a same service.*/
    auto_strings(path + pathlen, "/", ainsta) ;

    if (unlink(path) < 0)
        log_dieusys(LOG_EXIT_ZERO, "unlink: ", path) ;

    path[pathlen] = 0 ;

    /** contents */
    get_list(&list, path, name, S_IFREG) ;
    regex_replace(&list, res) ;

    /** directories */
    get_list(&list, path, name, S_IFDIR) ;
    regex_rename(&list, res, res->regex.directories) ;

    /** filename */
    get_list(&list, path, name, S_IFREG) ;
    regex_rename(&list, res, res->regex.files) ;

    /** configure script */
    regex_configure(res, info, path, name) ;

    deps:

    list.len = 0 ;

    if (!auto_stra(&list, path))
        log_die_nomem("stralloc") ;

    char t[list.len] ;

    sastr_to_char(t, &list) ;

    list.len = 0 ;
    char const *exclude[3] = { SS_MODULE_CONFIG_DIR + 1, SS_MODULE_SERVICE_INSTANCE + 1, 0 } ;
    if (!sastr_dir_get_recursive(&list, t, exclude, S_IFREG, 1))
        log_dieusys(LOG_EXIT_SYS, "get file(s) of module: ", name) ;

    char l[list.len] ;
    size_t llen = list.len ;

    sastr_to_char(l, &list) ;

    list.len = 0 ;

    for (pos = 0 ; pos < llen ; pos += strlen(l + pos) + 1) {

        char *dname = l + pos ;
        char ainsta[pathlen + SS_MODULE_SERVICE_INSTANCE_LEN + 1 + SS_MAX_SERVICE_NAME + 1] ;
        size_t namelen = strlen(dname) ;
        char bname[namelen] ;

        if (!ob_basename(bname,dname))
            log_dieu(LOG_EXIT_SYS, "find basename of: ", dname) ;

        if (instance_check(bname) > 0) {
            auto_strings(ainsta, path, SS_MODULE_SERVICE_INSTANCE, "/", bname) ;
            dname = ainsta ;
        }

        if (!sastr_add_string(&list, bname))
            log_die_nomem("stralloc") ;

        if (!strcmp(name, bname))
            log_die(LOG_EXIT_SYS, "cyclic call detected -- ", name, " call ", bname) ;

        /** nothing to do with the exit code */
        parse_frontend(dname, ares, areslen, info, force, conf, &residx, path) ;

        wres = resolve_set_struct(DATA_SERVICE, &ares[residx]) ;

        ares[residx].inmodule = resolve_add_string(wres, prefix - 1) ;
    }

    char deps[list.len] ;

    sastr_to_char(deps, &list) ;

    llen = list.len ;

    list.len = 0 ;

    /* rebuild the dependencies list incorporating the services defined inside
     * the module. */

    if (res->dependencies.ndepends)
        if (!sastr_clean_string(&list, res->sa.s + res->dependencies.depends))
            log_dieu(LOG_EXIT_SYS, "clean string") ;

    for (pos = 0 ; pos < llen ; pos += strlen(deps + pos) + 1)
        if (!sastr_add_string(&list, deps + pos))
            log_die_nomem("stralloc") ;

    wres = resolve_set_struct(DATA_SERVICE, res) ;

    res->dependencies.depends = parse_compute_list(wres, &list, &res->dependencies.ndepends, 0) ;

    free(wres) ;
    stralloc_free(&list) ;
}
