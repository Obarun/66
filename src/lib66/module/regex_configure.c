/*
 * regex_configure.c
 *
 * Copyright (c) 2023 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/sbl.h>
#include <oblibs/environ.h>
#include <oblibs/strbuf.h>
#include <oblibs/spawn.h>
#include <oblibs/process.h>
#include <oblibs/directory.h>
#include <oblibs/io.h>
#include <oblibs/fd.h>

#include <66/module.h>
#include <66/resolve.h>
#include <66/environ.h>
#include <66/utils.h>
#include <66/constants.h>

static int stage_environ(char const *dir, resolve_service_t *res, resolve_service_addon_environ_t *e)
{
    int r ;
    char const *svconf = e->sa.s + e->envdir ;
    char const *version = res->sa.s + res->version ;
    _cleanup_strbuf_ strbuf previous = STRBUF_ZERO ;
    char current[strlen(svconf) + 1 + strlen(version) + 1] ;

    auto_strings(current, svconf, "/", version) ;

    r = scan_mode(current, FLAGS_DIR) ;
    if (!r && errno != ENOENT)
        return -1 ;

    if (r > 0 && !tree_copy(current, dir))
        return -1 ;

    r = env_find_current_version(&previous, svconf) ;
    if (r < 0)
        return -1 ;

    char pv[previous.len + 1] ;

    if (r && !ob_basename(pv, previous.s))
        return -1 ;

    return env_write_version(dir, r ? pv : 0, res, e) ;
}

static int script_environ(strbuf *env, resolve_service_t *res, resolve_service_addon_environ_t *e, ssexec_t *info, char const *path, char const *name)
{
    char verbo[U32_FMT] ;
    _cleanup_strbuf_ strbuf clean = STRBUF_ZERO ;

    verbo[u32_fmt(verbo, VERBOSITY)] = 0 ;

    if (!auto_strbuf(env, \
        "MOD_NAME=", name, "\n", \
        "MOD_BASE=", res->sa.s + res->path.home, "\n", \
        "MOD_LIVE=", info->live.s, "\n", \
        "MOD_SCANDIR=", info->scandir.s, "\n", \
        "MOD_TREENAME=", res->sa.s + res->treename, "\n", \
        "MOD_OWNER=", res->sa.s + res->ownerstr, "\n", \
        "MOD_COLOR=", info->opt_color ? "1" : "0", "\n", \
        "MOD_VERBOSITY=", verbo, "\n", \
        "MOD_MODULE_DIR=", path, "\n", \
        "MOD_SKEL_DIR=", SS_SKEL_DIR, "\n", \
        "MOD_SERVICE_SYSDIR=", SS_SERVICE_SYSDIR, "\n", \
        "MOD_SERVICE_ADMDIR=", SS_SERVICE_ADMDIR, "\n", \
        "MOD_SERVICE_ADMCONFDIR=", SS_SERVICE_ADMCONFDIR, "\n", \
        "MOD_SCRIPT_SYSDIR=", SS_SCRIPT_SYSDIR, "\n", \
        "MOD_ENVIRONMENT_ADMDIR=", SS_ENVIRONMENT_ADMDIR, "\n", \
        "MOD_USER_DIR=", SS_USER_DIR, "\n", \
        "MOD_SERVICE_USERDIR=", SS_SERVICE_USERDIR, "\n", \
        "MOD_SERVICE_USERCONFDIR=", SS_SERVICE_USERCONFDIR, "\n", \
        "MOD_SCRIPT_USERDIR=", SS_SCRIPT_USERDIR, "\n", \
        "MOD_ENVIRONMENT_USERDIR=", SS_ENVIRONMENT_USERDIR, "\n"))
            return -1 ;

    /** sbl_clean_string forbids aliasing its source with the destination
     * buffer: clean into a separate strbuf, then copy back. */
    if (!sbl_clean_string(&clean, env->s) || !strbuf_copy(env, &clean))
        return -1 ;

    /** environment is not mandatory */
    if (res->has_environ && e->env > 0) {

        int r = 1 ;
        _cleanup_strbuf_ strbuf tmp = STRBUF_ZERO ;

        if (!dir_create_tmp(&tmp, "/tmp", name))
            return -1 ;

        if (stage_environ(tmp.s, res, e) < 0 || !environ_merge_dir_g(env, tmp.s))
            r = -1 ;

        tmpdir_destroy(tmp.s) ;

        if (r < 0)
            return -1 ;
    }

    return 1 ;
}

int regex_configure(resolve_service_t *res, resolve_service_addon_regex_t *rx, resolve_service_addon_environ_t *e, ssexec_t *info, char const *path, char const *name)
{
    log_flow() ;

    int wstat, fd ;
    pid_t pid ;
    size_t clen = rx->configure > 0 ? 1 : 0 ;
    size_t pathlen = strlen(path) ;

    _cleanup_strbuf_ strbuf env = STRBUF_ZERO ;

    char const *newargv[2 + clen] ;
    unsigned int m = 0 ;

    char pwd[pathlen + 1 + SS_MODULE_CONFIG_DIR_LEN + 1] ;
    auto_strings(pwd, path, "/", SS_MODULE_CONFIG_DIR + 1) ;

    char config_script[pathlen + 1 + SS_MODULE_CONFIG_DIR_LEN + 1 + SS_MODULE_CONFIG_SCRIPT_LEN + 1] ;
    auto_strings(config_script, path, "/", SS_MODULE_CONFIG_DIR + 1, "/", SS_MODULE_CONFIG_SCRIPT) ;

    ssize_t script = scan_mode(config_script, FLAGS_REG) ;
    if (!script && errno != ENOENT)
        return -1 ;

    if (script <= 0)
        return 1 ;

    if (script_environ(&env, res, e, info, path, name) < 0)
        return -1 ;

    newargv[m++] = config_script ;

    if (rx->configure > 0)
        newargv[m++] = rx->sa.s + rx->configure ;

    newargv[m++] = 0 ;

    // the script runs in its directory, the command comes back to its own after
    // oblibs spawn lacks a chdir action (posix_spawn_file_actions_addchdir_np): chdir around it
    fd = io_open(".", O_RDONLY|O_DIRECTORY|O_CLOEXEC) ;
    if (fd < 0)
        return -1 ;

    if (chdir(pwd) < 0) {
        close_fd(fd) ;
        return -1 ;
    }

    log_info("Launch configure script of module: ", name) ;

    pid = spawn_path_merge(newargv[0], newargv, (char const *const *)environ, env.s, env.len) ;

    int r = pid > 0 && process_wait(pid, &wstat) >= 0 ? 1 : -1 ;

    if (chdir_fd(fd) < 0)
        r = -1 ;

    close_fd(fd) ;

    if (r < 0)
        return -1 ;

    if (wstat) {
        log_warnu("run configure script of module: ", name, " -- it exited with a failure: ", config_script) ;
        return (errno = ECANCELED, -1) ;
    }

    return 1 ;
}
