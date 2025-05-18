/*
 * service.h
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <stdbool.h>

#include <oblibs/hash.h>

#include <skalibs/stralloc.h>
#include <skalibs/cdb.h>
#include <skalibs/cdbmake.h>

#include <66/ssexec.h>
#include <66/resolve.h>
#include <66/graph.h>
#include <66/config.h>
#include <66/enum_service.h>

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
    uint32_t provide ; // string
    uint32_t conflict ; // string

    uint32_t ndepends ; // integer
    uint32_t nrequiredby ; // integer
    uint32_t noptsdeps ; // integer
    uint32_t ncontents ; // integer
    uint32_t nprovide ; // integer
    uint32_t nconflict ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO { 0,0,0,0,0,0,0,0,0,0,0,0 }

typedef struct resolve_service_addon_timeout_s resolve_service_addon_timeout_t, *resolve_service_addon_timeout_t_ref ;
struct resolve_service_addon_timeout_s
{
    uint32_t start ; // integer
    uint32_t stop ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO { 0,0 }

typedef struct resolve_service_addon_scripts_s resolve_service_addon_scripts_t, *resolve_service_addon_scritps_t_ref ;
struct resolve_service_addon_scripts_s
{
    uint32_t run ; // string, launch before Execute
    uint32_t run_user ; // string, Execute field
    uint32_t build ; // string, custom or execline
    uint32_t runas ; // string
} ;

#define RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO { 0,0,0,0 }

typedef struct resolve_service_addon_execute_s resolve_service_addon_execute_t, *resolve_service_addon_execute_t_ref ;
struct resolve_service_addon_execute_s
{
    resolve_service_addon_scripts_t run ;
    resolve_service_addon_scripts_t finish ;
    resolve_service_addon_timeout_t timeout ;
    uint32_t down ; // integer
    uint32_t downsignal ; // integer
    uint32_t blockprivileges ; // integer
    uint32_t umask ; // integer
    uint32_t want_umask ; // integer
    uint32_t nice ; // integer
    uint32_t want_nice ; // integer
    uint32_t chdir ; // string
    uint32_t capsbound ; // string
    uint32_t capsambient ; // string
    uint32_t ncapsbound ; // integer
    uint32_t ncapsambient ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_EXECUTE_ZERO { \
    RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO, \
    RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO, \
    RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 \
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
    uint32_t backup ; // integer
    uint32_t maxsize ; // integer
    /** integer, default 3 which mean not touched, in this case the value configured
     * at compilation take precedence */
    uint32_t timestamp ; // integer
    uint32_t want ; // 1 want, 0 do not want. Want by default
    resolve_service_addon_execute_t execute ;
    resolve_service_addon_timeout_t timeout ;
} ;

#define RESOLVE_SERVICE_ADDON_LOGGER_ZERO { \
    0,3,1000000,3,1, \
    RESOLVE_SERVICE_ADDON_EXECUTE_ZERO, \
    RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO \
}

typedef struct resolve_service_addon_environ_s resolve_service_addon_environ_t, *resolve_service_addon_environ_t_ref ;
struct resolve_service_addon_environ_s
{
    uint32_t env ; // string
    uint32_t envdir ; // string, /etc/66/conf or /home/user/.66/conf
    uint32_t env_overwrite ; // integer, overwrite the environment
    uint32_t importfile ; // string, field ImportFile
    uint32_t nimportfile ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_ENVIRON_ZERO { 0,0,0,0,0 }

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

/**
 * Stdin: default -> /dev/null
 * StdOut: default -> /var/log/66/<service>/current through s6-log
 * StdErr: default -> inherit from StdOut
 **/
typedef struct resolve_service_addon_io_type_s resolve_service_addon_io_type_t, *resolve_service_addon_io_type_t_ref ;
struct resolve_service_addon_io_type_s {

    uint32_t type  ; // integer, flags
    uint32_t destination ; // string
} ;

#define RESOLVE_SERVICE_ADDON_IO_TYPE_ZERO { 0, 0 }

typedef struct resolve_service_addon_io_s resolve_service_addon_io_t, *resolve_service_addon_io_t_ref ;
struct resolve_service_addon_io_s
{
    resolve_service_addon_io_type_t fdin ; // default close
    resolve_service_addon_io_type_t fdout ; // default s6-log
    resolve_service_addon_io_type_t fderr ; // default inherit
} ;

#define RESOLVE_SERVICE_ADDON_IO_ZERO { \
    { E_PARSER_IO_TYPE_NOTSET, 0 }, \
    { E_PARSER_IO_TYPE_NOTSET, 0 }, \
    { E_PARSER_IO_TYPE_NOTSET, 0 } \
}

typedef struct resolve_service_addon_limit_s resolve_service_addon_limit_t, *resolve_service_addon_limit_t_ref;
struct resolve_service_addon_limit_s
{
    // all integer
    uint64_t limitas ;        // RLIMIT_AS (address space/virtual memory)
    uint64_t limitcore ;      // RLIMIT_CORE (core dump size)
    uint64_t limitcpu ;       // RLIMIT_CPU (CPU time)
    uint64_t limitdata ;      // RLIMIT_DATA (data segment size)
    uint64_t limitfsize ;     // RLIMIT_FSIZE (file size)
    uint64_t limitlocks ;     // RLIMIT_LOCKS (file locks)
    uint64_t limitmemlock ;   // RLIMIT_MEMLOCK (locked memory)
    uint64_t limitmsgqueue ;  // RLIMIT_MSGQUEUE (POSIX message queues)
    uint64_t limitnice ;      // RLIMIT_NICE (nice value/priority)
    uint64_t limitnofile ;    // RLIMIT_NOFILE (open file descriptors)
    uint64_t limitnproc ;     // RLIMIT_NPROC (number of processes)
    uint64_t limitrtprio ;    // RLIMIT_RTPRIO (real-time priority)
    uint64_t limitrttime ;    // RLIMIT_RTTIME (real-time CPU time)
    uint64_t limitsigpending ;// RLIMIT_SIGPENDING (queued signals)
    uint64_t limitstack ;     // RLIMIT_STACK (stack size)
} ;

#define RESOLVE_SERVICE_ADDON_LIMIT_ZERO { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }

typedef struct resolve_service_s resolve_service_t, *resolve_service_t_ref ;
struct resolve_service_s
{
    stralloc sa ;
    uint32_t rversion ; //string, version of 66 of the resolve file at write time

    // configuration
    uint32_t name ; // string
    uint32_t description ; // string
    uint32_t version ;// string
    uint32_t type ; // integer
    uint32_t notify ; // integer
    uint32_t maxdeath ; // integer
    uint32_t earlier ; // integer
    uint32_t copyfrom ; // string
    uint32_t intree ; // string
    uint32_t ownerstr ; // string
    uint32_t owner ; // integer, uid of the owner
    uint32_t treename ; // string
    uint32_t user ; // string
    uint32_t inns ; // string, name of the namespace(module) which depend on
    uint32_t enabled ; // integer, 0 not enabled
    uint32_t islog ; // integer, 0 not a logger service

    resolve_service_addon_path_t path ;
    resolve_service_addon_dependencies_t dependencies ;
    resolve_service_addon_execute_t execute ;
    resolve_service_addon_live_t live ;
    resolve_service_addon_logger_t logger ;
    resolve_service_addon_environ_t environ ;
    resolve_service_addon_regex_t regex ;
    resolve_service_addon_io_t io ;
    resolve_service_addon_limit_t limit ;
} ;

#define RESOLVE_SERVICE_ZERO { STRALLOC_ZERO, 0, \
                               0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0, \
                               RESOLVE_SERVICE_ADDON_PATH_ZERO, \
                               RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO, \
                               RESOLVE_SERVICE_ADDON_EXECUTE_ZERO, \
                               RESOLVE_SERVICE_ADDON_LIVE_ZERO, \
                               RESOLVE_SERVICE_ADDON_LOGGER_ZERO, \
                               RESOLVE_SERVICE_ADDON_ENVIRON_ZERO, \
                               RESOLVE_SERVICE_ADDON_REGEX_ZERO, \
                               RESOLVE_SERVICE_ADDON_IO_ZERO, \
                               RESOLVE_SERVICE_ADDON_LIMIT_ZERO }


extern const resolve_service_t service_resolve_zero ;

struct resolve_hash_s {
	char name[SS_MAX_SERVICE_NAME + 1] ; // name as key
	uint8_t visit ;
	resolve_service_t res ;
	UT_hash_handle hh ;

} ;

#define RESOLVE_HASH_ZERO { 0, 0, RESOLVE_SERVICE_ZERO, NULL }

extern int service_cmp_basedir(char const *dir) ;
extern int service_endof_dir(char const *dir, char const *name) ;
extern int service_frontend_path(stralloc *sasrc,char const *sv, uid_t owner,char const *directory_forced, char const **exclude, uint8_t exlen) ;
extern int service_frontend_src(stralloc *sasrc, char const *name, char const *src, char const **exclude) ;
extern int service_is_g(char const *name, uint32_t flag) ;
extern int service_get_treename(char *atree, char const *name) ;
extern void service_resolve_sanitize(resolve_service_t *res) ;
extern int service_resolve_get_field_tosa(stralloc *sa, resolve_service_t *res, resolve_service_enum_table_t table) ;
extern void service_resolve_modify_field(resolve_service_t *res, resolve_service_enum_table_t table, char const *data) ;
extern int service_resolve_read_cdb(cdb *c, resolve_service_t *res) ;
extern void service_resolve_write(resolve_service_t *res) ;
extern void service_resolve_write_remote(resolve_service_t *res, char const *dst, uint8_t force) ;
extern int service_resolve_write_cdb(cdbmaker *c, resolve_service_t *sres) ;
extern void service_enable_disable(service_graph_t *g, struct resolve_hash_s *hash, bool action, bool propagate, ssexec_t *info, stralloc *argv) ;
extern void service_switch_tree(resolve_service_t *res, char const *totreename, ssexec_t *info) ;
extern void service_db_migrate(resolve_service_t *old, resolve_service_t *new, char const *base, uint8_t requiredby) ;
extern int service_resolve_symlink(char const *base, char *path, char *name) ;

/* avoid circular dependencies by prototyping the ss_state_t instead
 * of calling the state.h header file*/
typedef struct ss_state_s ss_state_t, *ss_state_t_ref ;

/** Hash */
extern int hash_add(struct resolve_hash_s **hres, char const *name, resolve_service_t res) ;
extern struct resolve_hash_s *hash_search(struct resolve_hash_s **hres, char const *name) ;
extern int hash_count(struct resolve_hash_s **hres) ;
extern void hash_free(struct resolve_hash_s **hres) ;
extern void hash_reset_visit(struct resolve_hash_s *hres) ;

#endif
