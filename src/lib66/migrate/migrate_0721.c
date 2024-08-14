/*
 * migrate_0721.c
 *
 * Copyright (c) 2024 Eric Vidal <eric@obarun.org>
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
#include <stdint.h>
#include <fcntl.h>

#include <oblibs/log.h>
#include <oblibs/stack.h>
#include <oblibs/lexer.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>
#include <oblibs/files.h>
#include <oblibs/directory.h>
#include <oblibs/lexer.h>

#include <skalibs/stralloc.h>
#include <skalibs/cdb.h>


#include <66/ssexec.h>
#include <66/tree.h>
#include <66/resolve.h>
#include <66/parse.h>
#include <66/enum.h>
#include <66/constants.h>
#include <66/utils.h>
#include <66/write.h>
#include <66/instance.h>
#include <66/module.h>

#include <66/migrate_0721.h>

typedef struct field_s_0721 field_t_0721 ;
struct field_s_0721
{
    char const *regex ;
    char const *by ;
} ;

typedef struct conf_s conf_t ;
struct conf_s
{
    char name[SS_MAX_SERVICE_NAME + 1] ;
    char treename[SS_MAX_TREENAME + 1] ;
    char frontend[SS_MAX_PATH_LEN + 1] ;
    short toenable ;
} ;

static void conf_init(conf_t *conf)
{
    memset(conf->name, 0, sizeof(char) * SS_MAX_SERVICE_NAME) ;
    memset(conf->treename, 0, sizeof(char) * SS_MAX_TREENAME) ;
    memset(conf->frontend, 0, sizeof(char) * SS_MAX_PATH_LEN) ;
    conf->toenable = 0 ;

}

static void set_info(ssexec_t *info)
{
    log_flow() ;

    int r ;

    set_treeinfo(info) ;

    r = set_livedir(&info->live) ;
    if (!r)
        log_die_nomem("stralloc") ;
    if(r < 0)
        log_die(LOG_EXIT_SYS, "live: ", info->live.s, " must be an absolute path") ;

    if (!stralloc_copy(&info->scandir, &info->live))
        log_die_nomem("stralloc") ;

    r = set_livescan(&info->scandir, info->owner) ;
    if (!r)
        log_die_nomem("stralloc") ;
    if(r < 0)
        log_die(LOG_EXIT_SYS, "scandir: ", info->scandir.s, " must be an absolute path") ;

    if (!set_environment(&info->environment, info->owner))
        log_dieusys(LOG_EXIT_ZERO, "set environment") ;
}

static int resolve_find_cdb_0721(stralloc *result, cdb const *c, char const *key)
{
    uint32_t x = 0 ;
    size_t klen = strlen(key) ;
    cdb_data cdata ;

    result->len = 0 ;

    int r = cdb_find(c, &cdata, key, klen) ;
    if (r == -1)
        log_warnusys_return(LOG_EXIT_LESSONE,"search on cdb key: ",key) ;

    if (!r)
        log_warn_return(LOG_EXIT_ZERO,"unknown cdb key: ",key) ;

    char pack[cdata.len + 1] ;
    memcpy(pack,cdata.s, cdata.len) ;
    pack[cdata.len] = 0 ;

    uint32_unpack_big(pack, &x) ;

    if (!auto_stra(result,pack))
        log_warnusys_return(LOG_EXIT_LESSONE,"stralloc") ;

    return x ;
}

static void tree_resolve_master_read_cdb_0721(cdb *c, resolve_tree_master_t *mres)
{
    log_flow() ;

    _alloc_sa_(tmp) ;
    resolve_wrapper_t_ref wres ;
    uint32_t x ;

    wres = resolve_set_struct(DATA_TREE_MASTER, mres) ;

    resolve_init(wres) ;

    /* name */
    resolve_find_cdb_0721(&tmp,c,"name") ;
    mres->name = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* allow */
    resolve_find_cdb_0721(&tmp,c,"allow") ;
    mres->allow = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* current */
    resolve_find_cdb_0721(&tmp,c,"current") ;
    mres->current = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* contents */
    resolve_find_cdb_0721(&tmp,c,"contents") ;
    mres->contents = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* nallow */
    x = resolve_find_cdb_0721(&tmp,c,"nallow") ;
    mres->nallow = x ;

    /* ncontents */
    x = resolve_find_cdb_0721(&tmp,c,"ncontents") ;
    mres->ncontents = x ;

    free(wres) ;
}

static void tree_resolve_read_cdb_0721(cdb *c, resolve_tree_t *tres)
{
    log_flow() ;

    _alloc_sa_(tmp) ;
    resolve_wrapper_t_ref wres ;
    uint32_t x ;

    wres = resolve_set_struct(DATA_TREE, tres) ;

    resolve_init(wres) ;

    /* name */
    resolve_find_cdb_0721(&tmp,c,"name") ;
    tres->name = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* enabled */
    x = resolve_find_cdb_0721(&tmp,c,"enabled") ;
    tres->enabled = x ;

    /* depends */
    resolve_find_cdb_0721(&tmp,c,"depends") ;
    tres->depends = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* requiredby */
    resolve_find_cdb_0721(&tmp,c,"requiredby") ;
    tres->requiredby = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* allow */
    resolve_find_cdb_0721(&tmp,c,"allow") ;
    tres->allow = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* groups */
    resolve_find_cdb_0721(&tmp,c,"groups") ;
    tres->groups = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* contents */
    resolve_find_cdb_0721(&tmp,c,"contents") ;
    tres->contents = tmp.len ? resolve_add_string(wres,tmp.s) : 0 ;

    /* ndepends */
    x = resolve_find_cdb_0721(&tmp,c,"ndepends") ;
    tres->ndepends = x ;

    /* nrequiredby */
    x = resolve_find_cdb_0721(&tmp,c,"nrequiredby") ;
    tres->nrequiredby = x ;

    /* nallow */
    x = resolve_find_cdb_0721(&tmp,c,"nallow") ;
    tres->nallow = x ;

    /* ngroups */
    x = resolve_find_cdb_0721(&tmp,c,"ngroups") ;
    tres->ngroups = x ;

    /* ncontents */
    x = resolve_find_cdb_0721(&tmp,c,"ncontents") ;
    /** bugs fix: the field ncontents do not match
     * the number of service associated to the tree. */
    if (x > 0) {

        _alloc_stk_(stk, strlen(tres->sa.s + tres->contents)) ;
        if (!stack_string_clean(&stk, tres->sa.s + tres->contents))
            log_dieu(LOG_EXIT_SYS, "clean string") ;

        tres->ncontents = stack_count_element(&stk) ;
    }

    /* init */
    x = resolve_find_cdb_0721(&tmp,c,"init") ;
    tres->init = x ;

    /* supervised */
    x = resolve_find_cdb_0721(&tmp,c,"supervised") ;
    tres->supervised = x ;

    free(wres) ;

}

static void migrate_tree_0721(ssexec_t *info)
{
    log_flow() ;

    size_t pos = 0 ;
    int fd ;
    cdb c = CDB_ZERO ;
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wmres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;
    resolve_wrapper_t_ref wtres = 0 ;
    _alloc_stk_(path, info->base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + 2) ;

    /** migrate the Master resolve file of trees*/
    auto_strings(path.s, info->base.s, SS_SYSTEM, SS_RESOLVE, "/") ;

    /** Check if we are not on the current format already.
     * The update can crash at some point and relaunched. In that
     * case the resolve file can be already written with the current
     * format. */
    if (resolve_read_g(wmres, info->base.s, SS_MASTER + 1) <= 0) {

        mres = tree_resolve_master_zero ;

        if (resolve_open_cdb(&fd, &c, path.s, SS_MASTER + 1) <= 0)
            log_dieusys(LOG_EXIT_SYS, "open resolve master file") ;

        log_trace("upgrading master resolve file") ;
        tree_resolve_master_read_cdb_0721(&c, &mres) ;

        if (!resolve_write_g(wmres, info->base.s, SS_MASTER + 1))
            log_dieu(LOG_EXIT_SYS, "write resolve master file") ;

        cdb_free(&c) ;

    } else log_trace("master resolve already migrated -- ignoring it") ;

    close(fd) ;

    /** migrate the all resolve file of trees */
    if (mres.ncontents) {

        _alloc_stk_(stk, strlen(mres.sa.s + mres.contents) + 1) ;
        if (!stack_string_clean(&stk, mres.sa.s + mres.contents))
            log_dieu(LOG_EXIT_SYS, "convert string") ;

        FOREACH_STK(&stk, pos) {

            c = cdb_zero ;
            tres = tree_resolve_zero ;
            wtres = resolve_set_struct(DATA_TREE, &tres) ;

            if (resolve_read_g(wtres, info->base.s, stk.s + pos) <= 0) {

                if (resolve_open_cdb(&fd, &c, path.s, stk.s + pos) <= 0)
                    log_dieusys(LOG_EXIT_SYS, "open resolve file of tree: ", stk.s + pos) ;

                log_trace("upgrading resolve file of tree: ", stk.s + pos) ;
                tree_resolve_read_cdb_0721(&c, &tres) ;

                if (!resolve_write_g(wtres, info->base.s, tres.sa.s + tres.name))
                    log_dieu(LOG_EXIT_SYS, "write resolve file of tree: ", tres.sa.s + tres.name) ;

                close(fd) ;

            } else log_trace("resolve file of tree: ", stk.s + pos, " already migrated -- ignoring it") ;

            resolve_free(wtres) ;
        }
    }

    resolve_free(wmres) ;
}

static void migrate_frontend_file_0721(const char *file, ssexec_t *info)
{
    field_t_0721 field[] = {

        { .regex = "[main]", .by = "[Main]" },
        { .regex = "[start]", .by = "[Start]" },
        { .regex = "[stop]", .by = "[Stop]" },
        { .regex = "[logger]", .by = "[Logger]" },
        { .regex = "[regex]", .by = "[Regex]" },
        { .regex = "[environment]", .by = "[Environment]" },

        { .regex = "@type" , .by="Type" },
        { .regex = "@version" , .by="Version" },
        { .regex = "@description" , .by="Description" },
        { .regex = "@depends" , .by="Depends" },
        { .regex = "@requiredby" , .by="RequiredBy" },
        { .regex = "@optsdepends" , .by="OptsDepends" },
        { .regex = "@contents" , .by="Contents" },
        { .regex = "@options" , .by="Options" },
        { .regex = "@notify" , .by="Notify" },
        { .regex = "@user" , .by="User" },
        { .regex = "@maxdeath" , .by="MaxDeath" },
        { .regex = "@hiercopy" , .by="Hiercopy" },
        { .regex = "@flags" , .by="Flags" },
        { .regex = "@intree" , .by="InTree" },

        { .regex = "@timeout-finish" , .by = "TimeoutStop" },
        { .regex = "@timeout-kill" , .by = "TimeoutStart" },
        { .regex = "@timeout-up" , .by = "TimeoutStart" },
        { .regex = "@timeout-down" , .by = "TimeoutStop" },
        { .regex = "@down-signal" , .by = "DownSignal" },

        { .regex = "@build" , .by = "Build" },
        { .regex = "@runas" , .by = "RunAs" },
        { .regex = "@shebang" , .by = "##@shebang this field was deprecated and removed from version 0.7.3.0" },
        { .regex = "@execute" , .by = "Execute" },

        { .regex = "@configure" , .by = "Configure" },
        { .regex = "@directories" , .by = "Directories" },
        { .regex = "@files" , .by = "Files" },
        { .regex = "@infiles" , .by = "InFiles" },

        { .regex = "@destination" , .by = "Destination" },
        { .regex = "@backup" , .by = "Backup" },
        { .regex = "@maxsize" , .by = "MaxSize" },
        { .regex = "@timestamp" , .by = "Timestamp" },
        { .regex = 0 }
    } ;

    int pos = 0 ;
    size_t len = strlen(file) ;
    ssize_t insta = get_rlen_until(file, '@', len) ;
    _alloc_sa_(frontend) ;
    _alloc_stk_(f, len + 1) ;

    auto_strings(f.s, file) ;

    if (insta > 0)
        f.s[insta + 1] = 0;

    /** service for user declare inside root directory are handled by root.*/
    if ((str_start_with(f.s, SS_SERVICE_SYSDIR_USER) || str_start_with(f.s, SS_SERVICE_ADMDIR_USER)) && info->owner) {

        return ;
    }
    log_trace("read frontend service file: ", f.s) ;
    if (!file_readputsa_g(&frontend, f.s))
        log_dieu(LOG_EXIT_SYS, "read system version file: ", f.s) ;

    while(field[pos].regex) {
        log_trace("replacing field: ", field[pos].regex, " by: ", field[pos].by) ;
        if (!sastr_replace(&frontend, field[pos].regex, field[pos].by))
            log_die(LOG_EXIT_ZERO, "replace regex character: ", field[pos].regex, " by: ", field[pos].by," for service: ", f.s) ;
        pos++ ;
    }

    log_trace("frontend result of migration process for: ", f.s, "\n", frontend.s) ;
    /** point of no return */
    log_trace("write frontend service file: ", f.s) ;
    if (!file_write_unsafe_g(f.s, frontend.s))
        log_dieusys(LOG_EXIT_SYS, "write frontend file: ", f.s) ;

}

static void get_config(conf_t *lconf, conf_t *conf, size_t *nservice, const char *name, const char *frontend, ssexec_t *info)
{
    int fd ;
    _alloc_sa_(sa) ;
    _alloc_stk_(path, info->base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1 + strlen(name) + SS_RESOLVE_LEN + 2) ;

    resolve_service_t_0721 res_0721 = RESOLVE_SERVICE_ZERO_0721 ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res_0721) ;
    cdb c = CDB_ZERO ;

    resolve_init(wres) ;

    auto_strings(path.s, info->base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE, "/", name, SS_RESOLVE, "/") ;

    if (resolve_open_cdb(&fd, &c, path.s, name) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file of service: ", name) ;

    conf->toenable = resolve_find_cdb_0721(&sa, &c, "enabled") ;
    if (conf->toenable)
        log_trace("service ", name, " is marked enabled") ;


    sa.len = 0 ;
    resolve_find_cdb_0721(&sa, &c,"treename") ;
    auto_strings(conf->treename, sa.s) ;
    log_trace("service ", name, " is associated to tree: ", conf->treename) ;

    auto_strings(conf->name, name) ;

    auto_strings(conf->frontend, frontend) ;
    log_trace("service ",name, " declares its frontend file at: ", conf->frontend) ;

    lconf[(*nservice)++] = *conf ;

    close(fd) ;
    cdb_free(&c) ;
    resolve_free(wres) ;
}

static void migrate_user_service(ssexec_t *info)
{
    char const *exclude[3] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1, 0 } ;
    _alloc_sa_(sa) ;
    size_t syslen = strlen(SS_SERVICE_SYSDIR_USER), admlen = strlen(SS_SERVICE_ADMDIR_USER), pos = 0 ;
    _alloc_stk_(path, (syslen > admlen ? syslen : admlen) + 1) ;

    {
        auto_strings(path.s, SS_SERVICE_SYSDIR_USER) ;

        if (!sastr_dir_get_recursive(&sa, path.s, exclude, S_IFLNK, 1))
            log_dieu(LOG_EXIT_SYS, "get resolve files") ;

        FOREACH_SASTR(&sa, pos)
            migrate_frontend_file_0721(sa.s + pos, info) ;
    }
    {
        auto_strings(path.s, SS_SERVICE_ADMDIR_USER) ;

        sa.len = 0 ;
        if (!sastr_dir_get_recursive(&sa, path.s, exclude, S_IFLNK, 1))
            log_dieu(LOG_EXIT_SYS, "get resolve files") ;

        FOREACH_SASTR(&sa, pos)
            migrate_frontend_file_0721(sa.s + pos, info) ;
    }
}


static void migrate_service_0721(void)
{
    log_flow() ;

    int r ;
    size_t pos = 0, svpos = 0, nservice = 0 ;
    uint8_t exlen = 3, force = 1, conf = 0 ;
    char const *exclude[3] = { SS_MODULE_ACTIVATED + 1, SS_MODULE_FRONTEND + 1, 0 } ;
    conf_t lconf[SS_MAX_SERVICE + 1] ;
    ssexec_t info = SSEXEC_ZERO ;
    _alloc_sa_(sa) ;
    _alloc_sa_(frontend) ;
    struct resolve_hash_s *hres = NULL ;

    memset(lconf, 0, sizeof(conf_t) * SS_MAX_SERVICE) ;

    info.owner = getuid() ;
    info.ownerlen = uid_fmt(info.ownerstr, info.owner) ;
    info.ownerstr[info.ownerlen] = 0 ;

    if (!set_ownersysdir(&info.base, info.owner))
        log_dieusys(LOG_EXIT_SYS, "set owner directory") ;

    set_info(&info) ;

    /** migrate frontend of /usr/share/66/service/user
     * and /etc/66/service/user */
    migrate_user_service(&info) ;

    _alloc_stk_(path, info.base.len + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + 1) ;
    auto_strings(path.s, info.base.s, SS_SYSTEM, SS_RESOLVE, SS_SERVICE) ;

    if (!sastr_dir_get_recursive(&sa, path.s, exclude, S_IFLNK, 0))
        log_dieu(LOG_EXIT_SYS, "get resolve files") ;

    /** get old configuration of the service:
     * - associated tree name
     * - enabled or not. */
    {
        FOREACH_SASTR(&sa, pos) {

            conf_t conf ;

            conf_init(&conf) ;

            char *name = sa.s + pos ;
            frontend.len = 0 ;

            /** do not handle service inside ns.
             * The parse process will do it.
             * Same for logger service.*/
            r = str_contain(name, ":") ;
            if (r >= 0 || get_rstrlen_until(name, SS_LOG_SUFFIX) > 0)
                continue ;

            if (!service_frontend_path(&frontend, name, info.owner, 0, exclude, exlen))
                log_dieu(LOG_EXIT_USER, "find service frontend file of: ", name) ;

            get_config(lconf, &conf, &nservice, name, frontend.s, &info) ;

            migrate_frontend_file_0721(frontend.s, &info) ;

            {
                /** In case of module we need to migrate the frontend
                 * of each service inside the ns. */
                _alloc_stk_(module, frontend.len + 1 + SS_MODULE_FRONTEND_LEN + 1) ;

                if (!ob_dirname(module.s, frontend.s))
                    log_dieusys(LOG_EXIT_SYS, "get dirname of: ", frontend.s) ;

                auto_strings(module.s + strlen(module.s), SS_MODULE_FRONTEND + 1) ;

                r = access(module.s, F_OK) ;
                if (!r) {

                    char const *ex[1] = { 0 } ;
                    frontend.len = svpos = 0 ;

                    if (!sastr_dir_get_recursive(&frontend, module.s, ex, S_IFREG, 1))
                        log_dieu(LOG_EXIT_SYS, "get resolve files") ;

                    FOREACH_SASTR(&frontend, svpos)
                        migrate_frontend_file_0721(frontend.s + svpos, &info) ;
                }
            }
        }
    }

    /** parse each service */
    for (pos = 0 ; pos < nservice ; pos++)
        parse_service(&hres, lconf[pos].frontend, &info, force, conf) ;

    for (pos = 0 ; pos < nservice ; pos++) {

        if (lconf[pos].toenable) {

            info.opt_tree = 1 ;
            info.treename.len = 0 ;
            if (!auto_stra(&info.treename, lconf[pos].treename))
                log_die_nomem("stralloc") ;

            int argc = 3 ;
            int m = 0 ;
            char const *prog = PROG ;
            char const *newargv[argc] ;

            char const *help = info.help ;
            char const *usage = info.usage ;

            info.help = help_enable ;
            info.usage = usage_enable ;

            newargv[m++] = "enable" ;
            newargv[m++] = lconf[pos].name ;
            newargv[m] = 0 ;

            PROG = "enable" ;
            if (ssexec_enable(m, newargv, &info))
                log_dieu(LOG_EXIT_SYS, "enable service: ", lconf[pos].name) ;
            PROG = prog ;

            info.help = help ;
            info.usage = usage ;
        }
    }
    hash_free(&hres) ;
    ssexec_free(&info) ;
}

static void migrate_create_snap_0721(ssexec_t *info)
{
    int argc = 4 ;
    int m = 0 ;
    char const *prog = PROG ;
    char const *newargv[argc] ;

    char const *help = info->help ;
    char const *usage = info->usage ;

    info->help = help_snapshot_create ;
    info->usage = usage_snapshot_create ;

    newargv[m++] = "snapshot" ;
    newargv[m++] = "-s" ;
    newargv[m++] = "migrate@0.7.2.1" ;
    newargv[m] = 0 ;

    PROG = "snapshot" ;
    if (ssexec_snapshot_create(m, newargv, info))
        log_dieu(LOG_EXIT_SYS, "create snapshot migrate@0.7.2.1") ;
    PROG = prog ;

    info->help = help ;
    info->usage = usage ;

}
void migrate_0721(ssexec_t *info)
{
    log_flow() ;

    log_info("Upgrading system from version: 0.7.2.1 to: ", SS_VERSION) ;

    migrate_create_snap_0721(info) ;

    migrate_tree_0721(info) ;

    migrate_service_0721() ;

    log_info("System successfully upgraded to version: ", SS_VERSION) ;
}