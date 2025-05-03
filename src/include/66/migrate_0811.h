/*
 * migrate_0811.h
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

#ifndef SS_SERVICE_0811_H
#define SS_SERVICE_0811_H

#include <stdint.h>
#include <66/ssexec.h>
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

typedef struct resolve_service_addon_path_s_0811 resolve_service_addon_path_t_0811, *resolve_service_addon_path_t_ref_0811 ;
struct resolve_service_addon_path_s_0811
{
    uint32_t home ; // string, /var/lib/66 or /home/user/.66
    uint32_t frontend ;  // string, /home/<user>/.66/service or /etc/66/service or /usr/lib/66/service
    uint32_t servicedir ; //string, /var/lib/66/system/service/svc/service_name
} ;

#define RESOLVE_SERVICE_ADDON_PATH_ZERO_0811 { 0,0,0 }

typedef struct resolve_service_addon_dependencies_s_0811 resolve_service_addon_dependencies_t_0811, *resolve_service_addon_dependencies_t_ref_0811 ;
struct resolve_service_addon_dependencies_s_0811
{
    uint32_t depends ; // string
    uint32_t requiredby ; // string,
    uint32_t optsdeps ; // string, optional dependencies
    uint32_t contents ; // string
    uint32_t provide ; // string

    uint32_t ndepends ; // integer
    uint32_t nrequiredby ; // integer
    uint32_t noptsdeps ; // integer
    uint32_t ncontents ; // integer
    uint32_t nprovide ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO_0811 { 0,0,0,0,0,0,0,0,0,0 }

typedef struct resolve_service_addon_timeout_s_0811 resolve_service_addon_timeout_t_0811, *resolve_service_addon_timeout_t_ref_0811 ;
struct resolve_service_addon_timeout_s_0811
{
    uint32_t start ; // integer
    uint32_t stop ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO_0811 { 0,0 }

typedef struct resolve_service_addon_scripts_s_0811 resolve_service_addon_scripts_t_0811, *resolve_service_addon_scritps_t_ref_0811 ;
struct resolve_service_addon_scripts_s_0811
{
    uint32_t run ; // string, launch before Execute
    uint32_t run_user ; // string, Execute field
    uint32_t build ; // string, custom or execline
    uint32_t runas ; // string
} ;

#define RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO_0811 { 0,0,0,0 }

typedef struct resolve_service_addon_execute_s_0811 resolve_service_addon_execute_t_0811, *resolve_service_addon_execute_t_ref_0811 ;
struct resolve_service_addon_execute_s_0811
{
    resolve_service_addon_scripts_t_0811 run ;
    resolve_service_addon_scripts_t_0811 finish ;
    resolve_service_addon_timeout_t_0811 timeout ;
    uint32_t down ; // integer
    uint32_t downsignal ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_EXECUTE_ZERO_0811 { \
    RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO_0811, \
    RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO_0811, \
    RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO_0811, \
    0, \
    0 \
}

typedef struct resolve_service_addon_live_s_0811 resolve_service_addon_live_t_0811, *resolve_service_addon_live_t_ref_0811 ;
struct resolve_service_addon_live_s_0811
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

#define RESOLVE_SERVICE_ADDON_LIVE_ZERO_0811 { 0,0,0,0,0,0,0,0,0,0 }

typedef struct resolve_service_addon_logger_s_0811 resolve_service_addon_logger_t_0811, *resolve_service_addon_logger_t_ref_0811 ;
struct resolve_service_addon_logger_s_0811
{
    // logger
    uint32_t name ; // string, typically "name-log" or 0 if it's the resolve of the logger
    //uint32_t destination ; // string
    uint32_t backup ; // integer
    uint32_t maxsize ; // integer
    /** integer, default 3 which mean not touched, in this case the value configured
     * at compilation take precedence */
    uint32_t timestamp ; // integer
    uint32_t want ; // 1 want, 0 do not want. Want by default
    resolve_service_addon_execute_t_0811 execute ;
    resolve_service_addon_timeout_t_0811 timeout ;
} ;

#define RESOLVE_SERVICE_ADDON_LOGGER_ZERO_0811 { \
    0,3,1000000,3,1, \
    RESOLVE_SERVICE_ADDON_EXECUTE_ZERO_0811, \
    RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO_0811 \
}

typedef struct resolve_service_addon_environ_s_0811 resolve_service_addon_environ_t_0811, *resolve_service_addon_environ_t_ref_0811 ;
struct resolve_service_addon_environ_s_0811
{
    uint32_t env ; // string
    uint32_t envdir ; // string, /etc/66/conf or /home/user/.66/conf
    uint32_t env_overwrite ; // integer, overwrite the environment
    uint32_t importfile ; // string, field ImportFile
    uint32_t nimportfile ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_ENVIRON_ZERO_0811 { 0,0,0,0,0 }

typedef struct resolve_service_addon_regex_s_0811 resolve_service_addon_regex_t_0811, *resolve_service_addon_regex_t_ref_0811 ;
struct resolve_service_addon_regex_s_0811
{
    uint32_t configure ; // string
    uint32_t directories ; // string
    uint32_t files ; // string
    uint32_t infiles ; // string

    uint32_t ndirectories ; // integer
    uint32_t nfiles ; // integer
    uint32_t ninfiles ; // integer

} ;

#define RESOLVE_SERVICE_ADDON_REGEX_ZERO_0811 { 0,0,0,0,0,0,0 }

/**
 * Stdin: default -> /dev/null
 * StdOut: default -> /var/log/66/<service>/current through s6-log
 * StdErr: default -> inherit from StdOut
 **/
typedef struct IO_type_s_0811 IO_type_t_0811, *IO_type_t_ref_0811 ;
struct IO_type_s_0811 {

    uint32_t type  ; // integer, flags
    uint32_t destination ; // string
} ;

#define IO_TYPE_ZERO_0811 { 0, 0 }

typedef struct IO_redirection_s_0811 IO_redirection_t_0811, *IO_redirection_t_ref_0811 ;
struct IO_redirection_s_0811
{
    IO_type_t_0811 fdin ; // default close
    IO_type_t_0811 fdout ; // default s6-log
    IO_type_t_0811 fderr ; // default inherit
} ;

#define E_PARSER_IO_TYPE_NOTSET_0811 9

#define IO_REDIRECTION_ZERO_0811 { \
    { E_PARSER_IO_TYPE_NOTSET_0811, 0 }, \
    { E_PARSER_IO_TYPE_NOTSET_0811, 0 }, \
    { E_PARSER_IO_TYPE_NOTSET_0811, 0 } \
}

typedef struct resolve_service_s_0811 resolve_service_t_0811, *resolve_service_t_ref_0811 ;
struct resolve_service_s_0811
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

    resolve_service_addon_path_t_0811 path ;
    resolve_service_addon_dependencies_t_0811 dependencies ;
    resolve_service_addon_execute_t_0811 execute ;
    resolve_service_addon_live_t_0811 live ;
    resolve_service_addon_logger_t_0811 logger ;
    resolve_service_addon_environ_t_0811 environ ;
    resolve_service_addon_regex_t_0811 regex ;

    IO_redirection_t_0811 io ;
} ;

#define RESOLVE_SERVICE_ZERO_0811 { STRALLOC_ZERO, 0, \
                               0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0, \
                               RESOLVE_SERVICE_ADDON_PATH_ZERO_0811, \
                               RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO_0811, \
                               RESOLVE_SERVICE_ADDON_EXECUTE_ZERO_0811, \
                               RESOLVE_SERVICE_ADDON_LIVE_ZERO_0811, \
                               RESOLVE_SERVICE_ADDON_LOGGER_ZERO_0811, \
                               RESOLVE_SERVICE_ADDON_ENVIRON_ZERO_0811, \
                               RESOLVE_SERVICE_ADDON_REGEX_ZERO_0811, \
                               IO_REDIRECTION_ZERO_0811 }

#endif