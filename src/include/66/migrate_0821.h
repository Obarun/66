/*
 * migrate_0821.h
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 */

#ifndef SS_SERVICE_0821_H
#define SS_SERVICE_0821_H

#include <stdint.h>

#include <oblibs/strbuf.h>

#include <66/ssexec.h>

typedef struct resolve_service_addon_path_s_0821 resolve_service_addon_path_t_0821, *resolve_service_addon_path_t_ref_0821 ;
struct resolve_service_addon_path_s_0821
{
    uint32_t home ;
    uint32_t frontend ;
    uint32_t servicedir ;
} ;

#define RESOLVE_SERVICE_ADDON_PATH_ZERO_0821 { 0,0,0 }

typedef struct resolve_service_addon_dependencies_s_0821 resolve_service_addon_dependencies_t_0821, *resolve_service_addon_dependencies_t_ref_0821 ;
struct resolve_service_addon_dependencies_s_0821
{
    uint32_t depends ;
    uint32_t requiredby ;
    uint32_t optsdeps ;
    uint32_t contents ;
    uint32_t provide ;
    uint32_t conflict ;

    uint32_t ndepends ;
    uint32_t nrequiredby ;
    uint32_t noptsdeps ;
    uint32_t ncontents ;
    uint32_t nprovide ;
    uint32_t nconflict ;
} ;

#define RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO_0821 { 0,0,0,0,0,0,0,0,0,0,0,0 }

typedef struct resolve_service_addon_timeout_s_0821 resolve_service_addon_timeout_t_0821, *resolve_service_addon_timeout_t_ref_0821 ;
struct resolve_service_addon_timeout_s_0821
{
    uint32_t start ;
    uint32_t stop ;
} ;

#define RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO_0821 { 0,0 }

typedef struct resolve_service_addon_scripts_s_0821 resolve_service_addon_scripts_t_0821, *resolve_service_addon_scritps_t_ref_0821 ;
struct resolve_service_addon_scripts_s_0821
{
    uint32_t run ;
    uint32_t run_user ;
    uint32_t build ;
    uint32_t runas ;
} ;

#define RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO_0821 { 0,0,0,0 }

typedef struct resolve_service_addon_execute_s_0821 resolve_service_addon_execute_t_0821, *resolve_service_addon_execute_t_ref_0821 ;
struct resolve_service_addon_execute_s_0821
{
    resolve_service_addon_scripts_t_0821 run ;
    resolve_service_addon_scripts_t_0821 finish ;
    resolve_service_addon_timeout_t_0821 timeout ;
    uint32_t down ;
    uint32_t downsignal ;
    uint32_t blockprivileges ;
    uint32_t umask ;
    uint32_t want_umask ;
    uint32_t nice ;
    uint32_t want_nice ;
    uint32_t chdir ;
    uint32_t capsbound ;
    uint32_t capsambient ;
    uint32_t ncapsbound ;
    uint32_t ncapsambient ;
} ;

#define RESOLVE_SERVICE_ADDON_EXECUTE_ZERO_0821 { \
    RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO_0821, \
    RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO_0821, \
    RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO_0821, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 \
}

typedef struct resolve_service_addon_live_s_0821 resolve_service_addon_live_t_0821, *resolve_service_addon_live_t_ref_0821 ;
struct resolve_service_addon_live_s_0821
{
    uint32_t livedir ;
    uint32_t status ;
    uint32_t servicedir ;
    uint32_t scandir ;
    uint32_t statedir ;
    uint32_t eventdir ;
    uint32_t notifdir ;
    uint32_t supervisedir ;
    uint32_t fdholderdir ;
    uint32_t oneshotddir ;
} ;

#define RESOLVE_SERVICE_ADDON_LIVE_ZERO_0821 { 0,0,0,0,0,0,0,0,0,0 }

typedef struct resolve_service_addon_logger_s_0821 resolve_service_addon_logger_t_0821, *resolve_service_addon_logger_t_ref_0821 ;
struct resolve_service_addon_logger_s_0821
{
    uint32_t name ;
    uint32_t backup ;
    uint32_t maxsize ;
    uint32_t timestamp ;
    uint32_t want ;
    resolve_service_addon_execute_t_0821 execute ;
} ;

#define RESOLVE_SERVICE_ADDON_LOGGER_ZERO_0821 { \
    0,3,1000000,3,1, \
    RESOLVE_SERVICE_ADDON_EXECUTE_ZERO_0821 \
}

typedef struct resolve_service_addon_environ_s_0821 resolve_service_addon_environ_t_0821, *resolve_service_addon_environ_t_ref_0821 ;
struct resolve_service_addon_environ_s_0821
{
    uint32_t env ;
    uint32_t envdir ;
    uint32_t env_overwrite ;
    uint32_t importfile ;
    uint32_t nimportfile ;
} ;

#define RESOLVE_SERVICE_ADDON_ENVIRON_ZERO_0821 { 0,0,0,0,0 }

typedef struct resolve_service_addon_regex_s_0821 resolve_service_addon_regex_t_0821, *resolve_service_addon_regex_t_ref_0821 ;
struct resolve_service_addon_regex_s_0821
{
    uint32_t configure ;
    uint32_t directories ;
    uint32_t files ;
    uint32_t infiles ;

    uint32_t ndirectories ;
    uint32_t nfiles ;
    uint32_t ninfiles ;
} ;

#define RESOLVE_SERVICE_ADDON_REGEX_ZERO_0821 { 0,0,0,0,0,0,0 }

typedef struct resolve_service_addon_io_type_s_0821 resolve_service_addon_io_type_t_0821, *resolve_service_addon_io_type_t_ref_0821 ;
struct resolve_service_addon_io_type_s_0821 {

    uint32_t type ;
    uint32_t destination ;
} ;

#define RESOLVE_SERVICE_ADDON_IO_TYPE_ZERO_0821 { 0, 0 }

typedef struct resolve_service_addon_io_s_0821 resolve_service_addon_io_t_0821, *resolve_service_addon_io_t_ref_0821 ;
struct resolve_service_addon_io_s_0821
{
    resolve_service_addon_io_type_t_0821 fdin ;
    resolve_service_addon_io_type_t_0821 fdout ;
    resolve_service_addon_io_type_t_0821 fderr ;
} ;

#define E_PARSER_IO_TYPE_NOTSET_0821 9

#define RESOLVE_SERVICE_ADDON_IO_ZERO_0821 { \
    { E_PARSER_IO_TYPE_NOTSET_0821, 0 }, \
    { E_PARSER_IO_TYPE_NOTSET_0821, 0 }, \
    { E_PARSER_IO_TYPE_NOTSET_0821, 0 } \
}

typedef struct resolve_service_addon_limit_s_0821 resolve_service_addon_limit_t_0821, *resolve_service_addon_limit_t_ref_0821 ;
struct resolve_service_addon_limit_s_0821
{
    uint64_t limitas ;
    uint64_t limitcore ;
    uint64_t limitcpu ;
    uint64_t limitdata ;
    uint64_t limitfsize ;
    uint64_t limitlocks ;
    uint64_t limitmemlock ;
    uint64_t limitmsgqueue ;
    uint64_t limitnice ;
    uint64_t limitnofile ;
    uint64_t limitnproc ;
    uint64_t limitrtprio ;
    uint64_t limitrttime ;
    uint64_t limitsigpending ;
    uint64_t limitstack ;
} ;

#define RESOLVE_SERVICE_ADDON_LIMIT_ZERO_0821 { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }

typedef struct resolve_service_s_0821 resolve_service_t_0821, *resolve_service_t_ref_0821 ;
struct resolve_service_s_0821
{
    strbuf sa ;
    uint32_t rversion ;

    // configuration
    uint32_t name ;
    uint32_t description ;
    uint32_t version ;
    uint32_t type ;
    uint32_t notify ;
    uint32_t maxdeath ;
    uint32_t earlier ;
    uint32_t copyfrom ;
    uint32_t intree ;
    uint32_t ownerstr ;
    uint32_t owner ;
    uint32_t treename ;
    uint32_t user ;
    uint32_t inns ;
    uint32_t enabled ;
    uint32_t islog ;

    resolve_service_addon_path_t_0821 path ;
    resolve_service_addon_dependencies_t_0821 dependencies ;
    resolve_service_addon_execute_t_0821 execute ;
    resolve_service_addon_live_t_0821 live ;
    resolve_service_addon_logger_t_0821 logger ;
    resolve_service_addon_environ_t_0821 environ ;
    resolve_service_addon_regex_t_0821 regex ;
    resolve_service_addon_io_t_0821 io ;
    resolve_service_addon_limit_t_0821 limit ;
} ;

#define RESOLVE_SERVICE_ZERO_0821 { STRBUF_ZERO, 0, \
                               0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0, \
                               RESOLVE_SERVICE_ADDON_PATH_ZERO_0821, \
                               RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO_0821, \
                               RESOLVE_SERVICE_ADDON_EXECUTE_ZERO_0821, \
                               RESOLVE_SERVICE_ADDON_LIVE_ZERO_0821, \
                               RESOLVE_SERVICE_ADDON_LOGGER_ZERO_0821, \
                               RESOLVE_SERVICE_ADDON_ENVIRON_ZERO_0821, \
                               RESOLVE_SERVICE_ADDON_REGEX_ZERO_0821, \
                               RESOLVE_SERVICE_ADDON_IO_ZERO_0821, \
                               RESOLVE_SERVICE_ADDON_LIMIT_ZERO_0821 }

#endif
