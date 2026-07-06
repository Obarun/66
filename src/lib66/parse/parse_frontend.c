/*
 * parse_frontend.c
 *
 * Copyright (c) 2022 Eric Vidal <eric@obarun.org>
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
#include <string.h>
#include <stdlib.h> //free
#include <sys/stat.h>


#include <oblibs/log.h>
#include <oblibs/files.h>
#include <oblibs/sbl.h>
#include <oblibs/string.h>
#include <oblibs/strbuf.h>

#include <66/utils.h>
#include <66/constants.h>
#include <66/ssexec.h>
#include <66/service.h>
#include <66/config.h>
#include <66/resolve.h>
#include <66/enum_parser.h>
#include <66/state.h> // service_is_g flag
#include <66/parse.h>
#include <66/module.h>
#include <66/instance.h>

static void parse_read_instance(strbuf *frontend, char const *svsrc, char const *sv, int insta)
{
    log_flow() ;

    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    uint8_t exlen = 0 ; // see service_frontend_path file and compute_exclude()
    char const *exclude[1] = { 0 } ;

    if (!instance_splitname(&sa, sv, insta, SS_INSTANCE_TEMPLATE))
        log_die(LOG_EXIT_SYS, "split instance service of: ", sv) ;

    log_trace("read frontend service at: ", svsrc, sa.s) ;

    if (read_svfile(frontend, sa.s, svsrc) <= 0) {

        char instaname[sa.len + 1] ;
        auto_strings(instaname, sa.s) ;
        sa.len = 0 ;
        /** in module the template service may not exist e.g.
         * module which call another module. In this case
         * follow the classic way */
        int r = service_frontend_path(&sa, sv, getuid(), 0, exclude, exlen) ;
        if (r < 1)
            log_dieu(LOG_EXIT_SYS, "get frontend service file of: ", sv) ;

        char svsrc[sa.len + 1] ;

        if (!ob_dirname(svsrc, sa.s))
            log_dieu(LOG_EXIT_SYS, "get dirname of: ", sa.s) ;

        if (read_svfile(frontend, instaname, svsrc) <= 0)
            log_dieusys(LOG_EXIT_SYS, "read frontend service at: ", svsrc, instaname) ;
    }

}

/* @sv -> name of the service to parse with
 * the path of the frontend file source
 * @Die on fail
 * @Return 1 on success
 * @Return 2 -> already parsed */

int parse_frontend(char const *sv,
                   hash_t *hres,
                   ssexec_t *info,
                   uint8_t force,
                   uint8_t conf,
                   char const *forced_directory,
                   char const *main,
                   char const *inns,
                   char const *intree,
                   resolve_service_t *moduleres)
{
    log_flow() ;

    int insta, isparsed ;
    uint8_t opt_tree_forced = 0 ;
    size_t svlen = strlen(sv) ;
    char svname[svlen + 1], svsrc[svlen + 1], instaname[svlen + 1] ;
    _cleanup_strbuf_ strbuf sa = STRBUF_ZERO ;
    struct resolve_hash_s *hash ;

    if (!ob_basename(svname, sv))
        log_dieu(LOG_EXIT_SYS, "get basename of: ", sv) ;

    if (!ob_dirname(svsrc, sv))
        log_dieu(LOG_EXIT_SYS, "get dirname of: ", sv) ;

    hash = resolve_hash_search(hres, svname) ;
    if (hash != NULL)
        log_warn_return(2, "ignoring: ", svname, " service -- already appended to the selection") ;

    if (inns) {
        char n[strlen(inns) + 1 + strlen(svname) + 1] ;
        auto_strings(n, inns, ":", svname) ;

        hash = resolve_hash_search(hres, n) ;
        if (hash != NULL)
            log_warn_return(2, "ignoring: ", n, " service -- already appended to the selection") ;
    }

    log_trace("parse service: ", sv) ;

    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_SERVICE, &res) ;
    resolve_enum_table_t table = E_TABLE_PARSER_SECTION_MAIN_ZERO ;

    resolve_init(wres) ;

    insta = instance_check(svname) ;

    if (insta > 0) {

        auto_strings(instaname, svname) ;
        instaname[insta + 1] = 0 ;

        parse_read_instance(&sa, svsrc, svname, insta) ;

    } else {

        log_trace("read frontend service at: ", sv) ;

        if (read_svfile(&sa, svname, svsrc) <= 0)
            log_dieu(LOG_EXIT_SYS, "read frontend service at: ", sv) ;
    }

    if (!identifier_replace(&sa, svname))
        log_dieu(LOG_EXIT_SYS, "replace regex for service: ", svname) ;

    _alloc_sbl_(store, sa.len + 1) ;

    isparsed = service_is_g(svname, STATE_FLAGS_ISPARSED) ;
    if (isparsed == -1)
        log_dieusys(LOG_EXIT_SYS, "get information of service: ", svname, " -- please make a bug report") ;
    else if (!isparsed)
        isparsed = STATE_FLAGS_FALSE ;

    if (isparsed == STATE_FLAGS_TRUE && !force) {
        resolve_free(wres) ;
        log_warn_return(2, "ignoring service: ", svname, " -- already parsed") ;
    }

    /** the execute addon: run/finish scripts, timeouts, caps and the supervision
     * scalars (notify/maxdeath/maxdeathtime). Present on every service. */
    resolve_service_addon_execute_t execaddon = RESOLVE_SERVICE_ADDON_EXECUTE_ZERO ;
    {
        resolve_wrapper_t_ref exwres = resolve_set_struct(DATA_SERVICE_EXECUTE, &execaddon) ;
        resolve_init(exwres) ;
        free(exwres) ;
    }

    /** the dependencies addon: depends/requiredby/optsdeps/contents/provide/conflict + counts. */
    resolve_service_addon_dependencies_t depaddon = RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO ;
    {
        resolve_wrapper_t_ref depwres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &depaddon) ;
        resolve_init(depwres) ;
        free(depwres) ;
    }

    {

        table.u.parser.id = E_PARSER_SECTION_MAIN_TYPE ;

        if (!parse_get_value_of_key(&store, sa.s, table))
            log_dieu(LOG_EXIT_SYS, "get field ", enum_to_key(table.u.parser.list, table.u.parser.id), " of service: ", svname) ;

        if (!parse_store_main(&res, &execaddon, &depaddon, &store, table))
            log_dieu(LOG_EXIT_SYS, "store field type of service: ", svname) ;
    }

    if (!info->opt_tree) {

        store.len = 0 ;

        table.u.parser.id = E_PARSER_SECTION_MAIN_INTREE ;
        /** search for the intree field.
         * This field is not mandatory, do not crash if it not found */
        if (parse_get_value_of_key(&store, sa.s, table)) {

            if (!parse_store_main(&res, &execaddon, &depaddon, &store, table))
                log_dieu(LOG_EXIT_SYS, "store field intree of service: ", svname) ;

            info->treename.len = 0 ;
            info->opt_tree = 1 ;
            opt_tree_forced = 1 ;

            if (!auto_strbuf(&info->treename, res.sa.s + res.intree))
                log_die_nomem("strbuf") ;
        }
    }

    if (inns) {

        char n[strlen(inns) + 1 + strlen(svname) + 1] ;
        auto_strings(n, inns,":",svname) ;

        res.name = resolve_add_string(wres, n) ;
        res.inns = resolve_add_string(wres, inns) ;

    } else {

        res.name = resolve_add_string(wres, svname) ;
    }

    res.owner = info->owner ;
    res.ownerstr = resolve_add_string(wres, info->ownerstr) ;
    res.path.home = resolve_add_string(wres, info->base.s) ;

    {
        char *realname = 0 ;
        if (insta > 0) {
            realname = instaname ;
        } else {
            realname = svname ;
        }

        if (inns) {

            char *tmpath = strstr(sv, SS_MODULE_FRONTEND) ;
            char *result = tmpath + SS_MODULE_FRONTEND_LEN ;
            char *path = moduleres->sa.s + moduleres->path.frontend ;
            char pdir[strlen(path)] ;
            char tdir[strlen(result)] ;

            if (!ob_dirname(pdir, path))
                log_dieu(LOG_EXIT_SYS, "get dirname of: ", path) ;

            if (!ob_dirname(tdir, result))
                log_dieu(LOG_EXIT_SYS, "get dirname of: ", result) ;

            char frontend[strlen(pdir) + SS_MODULE_FRONTEND_LEN + strlen(tdir) + strlen(realname) + 2] ;

            auto_strings(frontend, pdir, SS_MODULE_FRONTEND + 1, tdir,  realname) ;

            res.path.frontend = resolve_add_string(wres, frontend) ;

        } else {

            char frontend[svlen + 1] ;

            auto_strings(frontend, svsrc, realname) ;

            res.path.frontend = resolve_add_string(wres, frontend) ;
        }
    }

    /** contents of directory should be listed by service_frontend_path
     * except for module type */
    if (scan_mode(sv, S_IFDIR) == 1 && res.type != E_PARSER_TYPE_MODULE) {
        resolve_free(wres) ;
        return 1 ;
    }

    /** pass 1: lex the frontend once into the (section,key) store; consumed by
     * parse_environ (right below, before parse_module needs it) and parse_limit. */
    parse_store_t st ;
    if (!parse_store_build(&st, sa.s))
        log_die(LOG_EXIT_SYS, "build parse store of service: ", svname) ;

    resolve_service_addon_environ_t environaddon = RESOLVE_SERVICE_ADDON_ENVIRON_ZERO ;
    {
        uint8_t has_environ = 0 ;
        if (!parse_environ(&st, &res, &environaddon, conf, &has_environ)) {
            parse_store_free(&st) ;
            log_die(LOG_EXIT_SYS, "parse environment of service: ", svname) ;
        }
        res.has_environ = has_environ ;
    }

    /** the parent's logger config; the effective has_logger is refined below,
     * once the io is resolved. */
    resolve_service_addon_logger_t loggeraddon = RESOLVE_SERVICE_ADDON_LOGGER_ZERO ;
    {
        uint8_t has_logger = 0 ;
        if (!parse_logger(&st, &res, &loggeraddon, &has_logger)) {
            parse_store_free(&st) ;
            log_die(LOG_EXIT_SYS, "parse logger of service: ", svname) ;
        }
        res.has_logger = has_logger ;
    }

    if (!parse_contents(&res, &execaddon, &depaddon, sa.s))
        log_dieu(LOG_EXIT_SYS, "parse file of service: ", svname) ;

    if (!parse_mandatory(&res, &loggeraddon, &execaddon, info))
        log_die(LOG_EXIT_SYS, "some mandatory field is missing for service: ", svname) ;

    /** try to create the tree if not exist yet with
     * the help of the seed files */
    set_treeinfo(info) ;

    res.treename = resolve_add_string(wres, info->treename.s) ;

    if (inns && intree)
        res.intree = resolve_add_string(wres, intree) ;

    if (opt_tree_forced)
        info->opt_tree = 0 ;

    /** append the depends list with the optional dependencies list */
    if (depaddon.noptsdeps) {

        resolve_wrapper_t_ref depwres = resolve_set_struct(DATA_SERVICE_DEPENDENCIES, &depaddon) ;

        if (depaddon.ndepends) {
            size_t len = strlen(depaddon.sa.s + depaddon.depends) ;
            char t[len + strlen(depaddon.sa.s + depaddon.optsdeps) + 2] ;
            auto_strings(t, depaddon.sa.s + depaddon.depends, " ", depaddon.sa.s + depaddon.optsdeps) ;
            depaddon.depends = resolve_add_string(depwres, t) ;

        } else {

            depaddon.depends = resolve_add_string(depwres, depaddon.sa.s + depaddon.optsdeps) ;
        }
        depaddon.ndepends += depaddon.noptsdeps ;

        free(depwres) ;
    }

    /** parse interdependences if the service was never parsed */
    if (isparsed == STATE_FLAGS_FALSE) {

        if (!parse_interdependences(svname, depaddon.sa.s + depaddon.depends, depaddon.ndepends, hres, info, force, conf, forced_directory, main, inns, intree, moduleres))
            log_dieu(LOG_EXIT_SYS, "parse dependencies of service: ", svname) ;
    }

    resolve_service_addon_regex_t regexaddon = RESOLVE_SERVICE_ADDON_REGEX_ZERO ;
    {
        uint8_t has_regex = 0 ;
        if (!parse_regex(&st, &res, &regexaddon, &has_regex)) {
            parse_store_free(&st) ;
            log_die(LOG_EXIT_SYS, "parse regex of service: ", svname) ;
        }
        res.has_regex = has_regex ;
    }

    if (res.type == E_PARSER_TYPE_MODULE)
        parse_module(&res, hres, info, force, conf, &environaddon, &depaddon, &regexaddon) ;

    parse_compute_resolve(&res, &execaddon, info) ;
    res.has_execute = 1 ;

    resolve_service_addon_io_t ioaddon = RESOLVE_SERVICE_ADDON_IO_ZERO ;
    {
        uint8_t has_io = 0 ;
        if (!parse_io(&st, &res, &ioaddon, &has_io, info)) {
            parse_store_free(&st) ;
            log_die(LOG_EXIT_SYS, "parse io of service: ", svname) ;
        }
        res.has_io = has_io ;
        /* logger "effective": keep it only if the resolved io is 66log-bound.
         * The io machine no longer mutates has_logger -- the orchestrator does. */
        if (res.has_logger && ioaddon.fdin.type != E_PARSER_IO_TYPE_66LOG && ioaddon.fdout.type != E_PARSER_IO_TYPE_66LOG)
            res.has_logger = 0 ;
    }

    if ((res.has_logger && ioaddon.fdin.type == E_PARSER_IO_TYPE_66LOG) &&
        (!res.inns && res.type != E_PARSER_TYPE_MODULE)) {

        parse_validator_t validator ;
        if (!parse_validator_init(&validator, sa.s))
            log_dieu(LOG_EXIT_SYS, "init parser validator of service: ", svname) ;

        parse_create_logger(&validator, hres, &res, &ioaddon, &loggeraddon, &execaddon, &depaddon, info) ;
    }

    resolve_service_addon_limit_t limitaddon = RESOLVE_SERVICE_ADDON_LIMIT_ZERO ;
    {
        uint8_t has_limit = 0 ;
        if (!parse_limit(&st, &limitaddon, &has_limit)) {
            parse_store_free(&st) ;
            log_die(LOG_EXIT_SYS, "parse limits of service: ", svname) ;
        }
        res.has_limit = has_limit ;
    }

    /** a service has a dependencies addon iff any relation is set. */
    res.has_dependencies = (depaddon.ndepends || depaddon.nrequiredby || depaddon.noptsdeps ||
                            depaddon.ncontents || depaddon.nprovide || depaddon.nconflict) ? 1 : 0 ;

    hash = resolve_hash_search(hres, res.sa.s + res.name) ;
    if (hash == NULL) {

        if (resolve_hash_count(hres) > SS_MAX_SERVICE)
            log_die(LOG_EXIT_SYS, "too many services to parse -- compile again 66 changing the --max-service options") ;

        log_trace("add service: ", res.sa.s + res.name, " to the service selection") ;
        char *name = res.sa.s + res.name ; // resolve_hash_add + log_dieu doesn't accept res.sa.s + res.name
        if (!resolve_hash_add(hres, name, res))
            log_dieu(LOG_EXIT_SYS, "append service selection with: ", name) ;

        if (res.has_limit) {
            hash = resolve_hash_search(hres, name) ;
            hash->limit = limitaddon ;
        }

        if (res.has_environ) {
            hash = resolve_hash_search(hres, name) ;
            hash->environ = environaddon ;
        }

        if (res.has_io) {
            hash = resolve_hash_search(hres, name) ;
            hash->io = ioaddon ;
        }

        if (res.has_logger) {
            hash = resolve_hash_search(hres, name) ;
            hash->logger = loggeraddon ;
        }

        if (res.has_execute) {
            hash = resolve_hash_search(hres, name) ;
            hash->execute = execaddon ;
        }

        if (res.has_dependencies) {
            hash = resolve_hash_search(hres, name) ;
            hash->dependencies = depaddon ;
        }

        if (res.has_regex) {
            hash = resolve_hash_search(hres, name) ;
            hash->regex = regexaddon ;
        }
    }

    parse_store_free(&st) ;
    free(wres) ;
    return 1 ;
}

