/*
 * service.h
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

#ifndef SS_SERVICE_H
#define SS_SERVICE_H

#include <stdint.h>

#include <oblibs/graph.h>

#include <skalibs/stralloc.h>
#include <skalibs/cdb.h>
#include <skalibs/cdbmake.h>

#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/hash.h>

typedef struct resolve_service_addon_path_s resolve_service_addon_path_t, *resolve_service_addon_path_t_ref ;
struct resolve_service_addon_path_s
{
    uint32_t home ; // string, /var/lib/66 or /home/user/.66
    uint32_t frontend ;  // string, /home/<user>/.66/service or /etc/66/service or /usr/lib/66/service
    uint32_t servicedir ; //string, /var/lib/66/system/service/svc/service_name
} ;

#define RESOLVE_SERVICE_ADDON_PATH_ZERO { 0,0,0 }

typedef struct resolve_service_addon_dependencies_s resolve_service_addon_dependencies_t, *resolve_service_addon_dependencies_t_ref ;
struct resolve_service_addon_dependencies_s
{
    uint32_t depends ; // string
    uint32_t requiredby ; // string,
    uint32_t optsdeps ; // string, optional dependencies
    uint32_t contents ; // string

    uint32_t ndepends ; // integer
    uint32_t nrequiredby ; // integer
    uint32_t noptsdeps ; // integer
    uint32_t ncontents ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO { 0,0,0,0,0,0,0,0 }

typedef struct resolve_service_addon_timeout_s resolve_service_addon_timeout_t, *resolve_service_addon_timeout_t_ref ;
struct resolve_service_addon_timeout_s
{
    uint32_t kill ; // integer
    uint32_t finish ; // integer
    uint32_t up ; // integer
    uint32_t down ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO { 0,0,0,0 }

typedef struct resolve_service_addon_scripts_s resolve_service_addon_scripts_t, *resolve_service_addon_scritps_t_ref ;
struct resolve_service_addon_scripts_s
{
    uint32_t run ; // string, launch before Execute
    uint32_t run_user ; // string, Execute field
    uint32_t build ; // string, custom or execline
    uint32_t shebang ; // string
    uint32_t runas ; // string
} ;

#define RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO { 0,0,0,0,0 }

typedef struct resolve_service_addon_execute_s resolve_service_addon_execute_t, *resolve_service_addon_execute_t_ref ;
struct resolve_service_addon_execute_s
{
    resolve_service_addon_scripts_t run ;
    resolve_service_addon_scripts_t finish ;
    resolve_service_addon_timeout_t timeout ;
    uint32_t down ; // integer
    uint32_t downsignal ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_EXECUTE_ZERO { \
    RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO, \
    RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO, \
    RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO, \
    0, \
    0 \
}

typedef struct resolve_service_addon_live_s resolve_service_addon_live_t, *resolve_service_addon_live_t_ref ;
struct resolve_service_addon_live_s
{
    uint32_t livedir ; // string, /run/66
    uint32_t status ; //string, /var/lib/66/system/.resolve/service/service_name/state/status
    uint32_t servicedir ; // string, /run/66/state/uid/service_name
    uint32_t scandir ; // string, /run/66/state/uid/service_name/scandir/service_name -> /var/lib/66/system/service/svc/service_name -> /run/66/scandir/uid
    uint32_t statedir ; // string, /run/66/state/uid/service_name/state -> /var/lib/66/system/service/svc/service_name/state
    uint32_t eventdir ; // string, /run/66/state/uid/service_name/event -> /var/lib/66/system/service/svc/service_name/event
    uint32_t notifdir ; // string, /run/66/state/uid/service_name/notif -> /var/lib/66/system/service/svc/service_name/notif
    uint32_t supervisedir ; // string, /run/66/state/uid/service_name/supervise -> /var/lib/66/system/service/svc/service_name/supervise
    uint32_t fdholderdir ; // string, /run/66/state/uid/service_name/scandir/fdholder
    uint32_t oneshotddir ; // string, /run/66/state/uid/service_name/scandir/oneshotd
} ;

#define RESOLVE_SERVICE_ADDON_LIVE_ZERO { 0,0,0,0,0,0,0,0,0,0 }

typedef struct resolve_service_addon_logger_s resolve_service_addon_logger_t, *resolve_service_addon_logger_t_ref ;
struct resolve_service_addon_logger_s
{
    // logger
    uint32_t name ; // string, typically "name-log" or 0 if it's the resolve of the logger
    uint32_t destination ; // string
    uint32_t backup ; // integer
    uint32_t maxsize ; // integer
    /** integer, default 3 which mean not touched, in this case the value configured
     * at compilation take precedence */
    uint32_t timestamp ;
    uint32_t want ; // 1 want, 0 do not want. Want by default
    resolve_service_addon_execute_t execute ;
    resolve_service_addon_timeout_t timeout ;

} ;

#define RESOLVE_SERVICE_ADDON_LOGGER_ZERO { \
    0,0,3,1000000,3,1, \
    RESOLVE_SERVICE_ADDON_EXECUTE_ZERO, \
    RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO \
}

typedef struct resolve_service_addon_environ_s resolve_service_addon_environ_t, *resolve_service_addon_environ_t_ref ;
struct resolve_service_addon_environ_s
{
    uint32_t env ; // string
    uint32_t envdir ; // string, /etc/66/conf or /home/user/.66/conf
    uint32_t env_overwrite ; // integer, overwrite the environment

} ;

#define RESOLVE_SERVICE_ADDON_ENVIRON_ZERO { 0,0,0 }

typedef struct resolve_service_addon_regex_s resolve_service_addon_regex_t, *resolve_service_addon_regex_t_ref ;
struct resolve_service_addon_regex_s
{
    uint32_t configure ; // string
    uint32_t directories ; // string
    uint32_t files ; // string
    uint32_t infiles ; // string

    uint32_t ndirectories ; // integer
    uint32_t nfiles ; // integer
    uint32_t ninfiles ; // integer

} ;

#define RESOLVE_SERVICE_ADDON_REGEX_ZERO { 0,0,0,0,0,0,0 }


typedef struct resolve_service_s resolve_service_t, *resolve_service_t_ref ;
struct resolve_service_s
{
    uint32_t salen ;
    stralloc sa ;

    // configuration
    uint32_t name ; // string
    uint32_t description ; // string
    uint32_t version ;// string
    uint32_t type ; // integer
    uint32_t notify ; // integer
    uint32_t maxdeath ; // integer
    uint32_t earlier ; // integer
    uint32_t hiercopy ; // string
    uint32_t intree ; // string
    uint32_t ownerstr ; // string
    uint32_t owner ; // integer, uid of the owner
    uint32_t treename ; // string
    uint32_t user ; // string
    uint32_t inns ; // string, name of the namespace(module) which depend on
    uint32_t enabled ; // integer, 0 not enabled

    resolve_service_addon_path_t path ;
    resolve_service_addon_dependencies_t dependencies ;
    resolve_service_addon_execute_t execute ;
    resolve_service_addon_live_t live ;
    resolve_service_addon_logger_t logger ;
    resolve_service_addon_environ_t environ ;
    resolve_service_addon_regex_t regex ;

} ;

#define RESOLVE_SERVICE_ZERO { 0,STRALLOC_ZERO, \
                               0,0,0,0,0,5,0,0,0,0,0,0,0,0,0, \
                               RESOLVE_SERVICE_ADDON_PATH_ZERO, \
                               RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO, \
                               RESOLVE_SERVICE_ADDON_EXECUTE_ZERO, \
                               RESOLVE_SERVICE_ADDON_LIVE_ZERO, \
                               RESOLVE_SERVICE_ADDON_LOGGER_ZERO, \
                               RESOLVE_SERVICE_ADDON_ENVIRON_ZERO, \
                               RESOLVE_SERVICE_ADDON_REGEX_ZERO }



typedef enum resolve_service_enum_e resolve_service_enum_t, *resolve_service_enum_t_ref;
enum resolve_service_enum_e
{
    E_RESOLVE_SERVICE_NAME = 0,
    E_RESOLVE_SERVICE_DESCRIPTION,
    E_RESOLVE_SERVICE_VERSION,
    E_RESOLVE_SERVICE_TYPE,
    E_RESOLVE_SERVICE_NOTIFY,
    E_RESOLVE_SERVICE_MAXDEATH,
    E_RESOLVE_SERVICE_EARLIER,
    E_RESOLVE_SERVICE_HIERCOPY,
    E_RESOLVE_SERVICE_INTREE,
    E_RESOLVE_SERVICE_OWNERSTR,
    E_RESOLVE_SERVICE_OWNER,
    E_RESOLVE_SERVICE_TREENAME,
    E_RESOLVE_SERVICE_USER,
    E_RESOLVE_SERVICE_INNS,
    E_RESOLVE_SERVICE_ENABLED,

    // path
    E_RESOLVE_SERVICE_HOME,
    E_RESOLVE_SERVICE_FRONTEND,
    E_RESOLVE_SERVICE_SERVICEDIR,

    // dependencies
    E_RESOLVE_SERVICE_DEPENDS,
    E_RESOLVE_SERVICE_REQUIREDBY,
    E_RESOLVE_SERVICE_OPTSDEPS,
    E_RESOLVE_SERVICE_CONTENTS,
    E_RESOLVE_SERVICE_NDEPENDS,
    E_RESOLVE_SERVICE_NREQUIREDBY,
    E_RESOLVE_SERVICE_NOPTSDEPS,
    E_RESOLVE_SERVICE_NCONTENTS,

    // execute
    E_RESOLVE_SERVICE_RUN,
    E_RESOLVE_SERVICE_RUN_USER,
    E_RESOLVE_SERVICE_RUN_BUILD,
    E_RESOLVE_SERVICE_RUN_SHEBANG,
    E_RESOLVE_SERVICE_RUN_RUNAS,
    E_RESOLVE_SERVICE_FINISH,
    E_RESOLVE_SERVICE_FINISH_USER,
    E_RESOLVE_SERVICE_FINISH_BUILD,
    E_RESOLVE_SERVICE_FINISH_SHEBANG,
    E_RESOLVE_SERVICE_FINISH_RUNAS,
    E_RESOLVE_SERVICE_TIMEOUTKILL,
    E_RESOLVE_SERVICE_TIMEOUTFINISH,
    E_RESOLVE_SERVICE_TIMEOUTUP,
    E_RESOLVE_SERVICE_TIMEOUTDOWN,
    E_RESOLVE_SERVICE_DOWN,
    E_RESOLVE_SERVICE_DOWNSIGNAL,

    // live
    E_RESOLVE_SERVICE_LIVEDIR,
    E_RESOLVE_SERVICE_STATUS,
    E_RESOLVE_SERVICE_SERVICEDIR_LIVE,
    E_RESOLVE_SERVICE_SCANDIR,
    E_RESOLVE_SERVICE_STATEDIR,
    E_RESOLVE_SERVICE_EVENTDIR,
    E_RESOLVE_SERVICE_NOTIFDIR,
    E_RESOLVE_SERVICE_SUPERVISEDIR,
    E_RESOLVE_SERVICE_FDHOLDERDIR,
    E_RESOLVE_SERVICE_ONESHOTDDIR,

    // logger
    E_RESOLVE_SERVICE_LOGNAME,
    E_RESOLVE_SERVICE_LOGDESTINATION,
    E_RESOLVE_SERVICE_LOGBACKUP,
    E_RESOLVE_SERVICE_LOGMAXSIZE,
    E_RESOLVE_SERVICE_LOGTIMESTAMP,
    E_RESOLVE_SERVICE_LOGWANT,
    E_RESOLVE_SERVICE_LOGRUN,
    E_RESOLVE_SERVICE_LOGRUN_USER,
    E_RESOLVE_SERVICE_LOGRUN_BUILD,
    E_RESOLVE_SERVICE_LOGRUN_SHEBANG,
    E_RESOLVE_SERVICE_LOGRUN_RUNAS,
    E_RESOLVE_SERVICE_LOGTIMEOUTKILL,
    E_RESOLVE_SERVICE_LOGTIMEOUTFINISH,

    // environment
    E_RESOLVE_SERVICE_ENV,
    E_RESOLVE_SERVICE_ENVDIR,
    E_RESOLVE_SERVICE_ENV_OVERWRITE,

    // regex
    E_RESOLVE_SERVICE_REGEX_CONFIGURE,
    E_RESOLVE_SERVICE_REGEX_DIRECTORIES,
    E_RESOLVE_SERVICE_REGEX_FILES,
    E_RESOLVE_SERVICE_REGEX_INFILES,
    E_RESOLVE_SERVICE_REGEX_NDIRECTORIES,
    E_RESOLVE_SERVICE_REGEX_NFILES,
    E_RESOLVE_SERVICE_REGEX_NINFILES,
    E_RESOLVE_SERVICE_ENDOFKEY

} ;

struct resolve_hash_s {
	char name[SS_MAX_SERVICE_NAME + 1] ; // name as key
	uint8_t visit ;
	resolve_service_t res ;
	UT_hash_handle hh ;

} ;
#define RESOLVE_HASH_ZERO { 0, 0, RESOLVE_SERVICE_ZERO, NULL }

extern resolve_field_table_t resolve_service_field_table[] ;

extern int service_cmp_basedir(char const *dir) ;
extern int service_endof_dir(char const *dir, char const *name) ;
extern int service_frontend_path(stralloc *sasrc,char const *sv, uid_t owner,char const *directory_forced, char const **exclude, uint8_t exlen) ;
extern int service_frontend_src(stralloc *sasrc, char const *name, char const *src, char const **exclude) ;
extern int service_is_g(char const *name, uint32_t flag) ;
extern int service_get_treename(char *atree, char const *name, uint32_t flag) ;
extern int service_resolve_copy(resolve_service_t *dst, resolve_service_t *res) ;
extern int service_resolve_get_field_tosa(stralloc *sa, resolve_service_t *res, resolve_service_enum_t field) ;
extern int service_resolve_modify_field(resolve_service_t *res, resolve_service_enum_t field, char const *data) ;
extern int service_resolve_read_cdb(cdb *c, resolve_service_t *res) ;
extern void service_resolve_write(resolve_service_t *res) ;
extern void service_resolve_write_remote(resolve_service_t *res, char const *dst, uint8_t force) ;
extern int service_resolve_write_cdb(cdbmaker *c, resolve_service_t *sres) ;
extern void service_enable_disable(graph_t *g, struct resolve_hash_s *hash, struct resolve_hash_s **hres, uint8_t action, uint8_t propagate, ssexec_t *info) ;
extern void service_switch_tree(resolve_service_t *res, char const *base, char const *totreename, ssexec_t *info) ;
extern void service_db_migrate(resolve_service_t *old, resolve_service_t *new, char const *base, uint8_t requiredby) ;
/* avoid circular dependencies by prototyping the ss_state_t instead
 * of calling the state.h header file*/
typedef struct ss_state_s ss_state_t, *ss_state_t_ref ;

/** Graph */
extern void service_graph_g(char const *slist, size_t slen, graph_t *graph, struct resolve_hash_s **hres, ssexec_t *info, uint32_t flag) ;
extern void service_graph_collect_list(graph_t *g, char const *alist, size_t alen, struct resolve_hash_s **hres, ssexec_t *info, uint32_t flag) ;
extern void service_graph_collect(graph_t *g, const char *name, struct resolve_hash_s **hres, ssexec_t *info, uint32_t flag) ;
extern void service_graph_compute(graph_t *g, struct resolve_hash_s **hres, uint32_t flag) ;

/** Hash */
extern int hash_add(struct resolve_hash_s **hres, char const *name, resolve_service_t res) ;
extern struct resolve_hash_s *hash_search(struct resolve_hash_s **hres, char const *name) ;
extern int hash_count(struct resolve_hash_s **hres) ;
extern void hash_free(struct resolve_hash_s **hres) ;

#endif
