/*
 * migrate_0802.h
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

#ifndef SS_SERVICE_0802_H
#define SS_SERVICE_0802_H

#include <stdint.h>
#include <66/ssexec.h>

typedef struct resolve_service_addon_path_s_0802 resolve_service_addon_path_t_0802, *resolve_service_addon_path_t_ref_0802 ;
struct resolve_service_addon_path_s_0802
{
    uint32_t home ; // string, /var/lib/66 or /home/user/.66
    uint32_t frontend ;  // string, /home/<user>/.66/service or /etc/66/service or /usr/lib/66/service
    uint32_t servicedir ; //string, /var/lib/66/system/service/svc/service_name
} ;

#define RESOLVE_SERVICE_ADDON_PATH_ZERO_0802 { 0,0,0 }

typedef struct resolve_service_addon_dependencies_s_0802 resolve_service_addon_dependencies_t_0802, *resolve_service_addon_dependencies_t_ref_0802 ;
struct resolve_service_addon_dependencies_s_0802
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

#define RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO_0802 { 0,0,0,0,0,0,0,0 }

typedef struct resolve_service_addon_timeout_s_0802 resolve_service_addon_timeout_t_0802, *resolve_service_addon_timeout_t_ref_0802 ;
struct resolve_service_addon_timeout_s_0802
{
    uint32_t start ; // integer
    uint32_t stop ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO_0802 { 0,0 }

typedef struct resolve_service_addon_scripts_s_0802 resolve_service_addon_scripts_t_0802, *resolve_service_addon_scritps_t_ref_0802 ;
struct resolve_service_addon_scripts_s_0802
{
    uint32_t run ; // string, launch before Execute
    uint32_t run_user ; // string, Execute field
    uint32_t build ; // string, custom or execline
    uint32_t runas ; // string
} ;

#define RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO_0802 { 0,0,0,0 }

typedef struct resolve_service_addon_execute_s_0802 resolve_service_addon_execute_t_0802, *resolve_service_addon_execute_t_ref_0802 ;
struct resolve_service_addon_execute_s_0802
{
    resolve_service_addon_scripts_t_0802 run ;
    resolve_service_addon_scripts_t_0802 finish ;
    resolve_service_addon_timeout_t_0802 timeout ;
    uint32_t down ; // integer
    uint32_t downsignal ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_EXECUTE_ZERO_0802 { \
    RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO_0802, \
    RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO_0802, \
    RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO_0802, \
    0, \
    0 \
}

typedef struct resolve_service_addon_live_s_0802 resolve_service_addon_live_t_0802, *resolve_service_addon_live_t_ref_0802 ;
struct resolve_service_addon_live_s_0802
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

#define RESOLVE_SERVICE_ADDON_LIVE_ZERO_0802 { 0,0,0,0,0,0,0,0,0,0 }

typedef struct resolve_service_addon_logger_s_0802 resolve_service_addon_logger_t_0802, *resolve_service_addon_logger_t_ref_0802 ;
struct resolve_service_addon_logger_s_0802
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
    resolve_service_addon_execute_t_0802 execute ;
    resolve_service_addon_timeout_t_0802 timeout ;
} ;

#define RESOLVE_SERVICE_ADDON_LOGGER_ZERO_0802 { \
    0,3,1000000,3,1, \
    RESOLVE_SERVICE_ADDON_EXECUTE_ZERO_0802, \
    RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO_0802 \
}

typedef struct resolve_service_addon_environ_s_0802 resolve_service_addon_environ_t_0802, *resolve_service_addon_environ_t_ref_0802 ;
struct resolve_service_addon_environ_s_0802
{
    uint32_t env ; // string
    uint32_t envdir ; // string, /etc/66/conf or /home/user/.66/conf
    uint32_t env_overwrite ; // integer, overwrite the environment
} ;

#define RESOLVE_SERVICE_ADDON_ENVIRON_ZERO_0802 { 0,0,0 }

typedef struct resolve_service_addon_regex_s_0802 resolve_service_addon_regex_t_0802, *resolve_service_addon_regex_t_ref_0802 ;
struct resolve_service_addon_regex_s_0802
{
    uint32_t configure ; // string
    uint32_t directories ; // string
    uint32_t files ; // string
    uint32_t infiles ; // string
    uint32_t ndirectories ; // integer
    uint32_t nfiles ; // integer
    uint32_t ninfiles ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_REGEX_ZERO_0802 { 0,0,0,0,0,0,0 }

/**
 * Stdin: default -> /dev/null
 * StdOut: default -> /var/log/66/<service>/current through s6-log
 * StdErr: default -> inherit from StdOut
 **/
typedef struct IO_type_s_0802 IO_type_t_0802, *IO_type_t_ref_0802 ;
struct IO_type_s_0802 {
    uint32_t type  ; // integer, flags
    uint32_t destination ; // string
} ;

#define IO_TYPE_ZERO_0802 { 0, 0 }

typedef struct IO_redirection_s_0802 IO_redirection_t_0802, *IO_redirection_t_ref_0802 ;
struct IO_redirection_s_0802
{
    IO_type_t_0802 fdin ; // default close
    IO_type_t_0802 fdout ; // default s6-log
    IO_type_t_0802 fderr ; // default inherit
} ;

#define IO_REDIRECTION_ZERO_0802 { \
    { IO_TYPE_NOTSET_0802, 0 }, \
    { IO_TYPE_NOTSET_0802, 0 }, \
    { IO_TYPE_NOTSET_0802, 0 } \
}

typedef struct resolve_service_s_0802 resolve_service_t_0802, *resolve_service_t_ref_0802 ;
struct resolve_service_s_0802
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
    uint32_t hiercopy ; // string
    uint32_t intree ; // string
    uint32_t ownerstr ; // string
    uint32_t owner ; // integer, uid of the owner
    uint32_t treename ; // string
    uint32_t user ; // string
    uint32_t inns ; // string, name of the namespace(module) which depend on
    uint32_t enabled ; // integer, 0 not enabled
    uint32_t islog ; // integer, 0 not a logger service

    resolve_service_addon_path_t_0802 path ;
    resolve_service_addon_dependencies_t_0802 dependencies ;
    resolve_service_addon_execute_t_0802 execute ;
    resolve_service_addon_live_t_0802 live ;
    resolve_service_addon_logger_t_0802 logger ;
    resolve_service_addon_environ_t_0802 environ ;
    resolve_service_addon_regex_t_0802 regex ;

    IO_redirection_t_0802 io ;
} ;

#define RESOLVE_SERVICE_ZERO_0802 { STRALLOC_ZERO, 0, \
                               0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0, \
                               RESOLVE_SERVICE_ADDON_PATH_ZERO_0802, \
                               RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO_0802, \
                               RESOLVE_SERVICE_ADDON_EXECUTE_ZERO_0802, \
                               RESOLVE_SERVICE_ADDON_LIVE_ZERO_0802, \
                               RESOLVE_SERVICE_ADDON_LOGGER_ZERO_0802, \
                               RESOLVE_SERVICE_ADDON_ENVIRON_ZERO_0802, \
                               RESOLVE_SERVICE_ADDON_REGEX_ZERO_0802, \
                               IO_REDIRECTION_ZERO_0802 }

enum resolve_service_enum_e_0802
{
    E_RESOLVE_SERVICE_RVERSION_0802 = 0,
    E_RESOLVE_SERVICE_NAME_0802,
    E_RESOLVE_SERVICE_DESCRIPTION_0802,
    E_RESOLVE_SERVICE_VERSION_0802,
    E_RESOLVE_SERVICE_TYPE_0802,
    E_RESOLVE_SERVICE_NOTIFY_0802,
    E_RESOLVE_SERVICE_MAXDEATH_0802,
    E_RESOLVE_SERVICE_EARLIER_0802,
    E_RESOLVE_SERVICE_HIERCOPY_0802,
    E_RESOLVE_SERVICE_INTREE_0802,
    E_RESOLVE_SERVICE_OWNERSTR_0802,
    E_RESOLVE_SERVICE_OWNER_0802,
    E_RESOLVE_SERVICE_TREENAME_0802,
    E_RESOLVE_SERVICE_USER_0802,
    E_RESOLVE_SERVICE_INNS_0802,
    E_RESOLVE_SERVICE_ENABLED_0802,
    E_RESOLVE_SERVICE_ISLOG_0802,

    // path
    E_RESOLVE_SERVICE_HOME_0802,
    E_RESOLVE_SERVICE_FRONTEND_0802,
    E_RESOLVE_SERVICE_SERVICEDIR_0802,

    // dependencies
    E_RESOLVE_SERVICE_DEPENDS_0802,
    E_RESOLVE_SERVICE_REQUIREDBY_0802,
    E_RESOLVE_SERVICE_OPTSDEPS_0802,
    E_RESOLVE_SERVICE_CONTENTS_0802,
    E_RESOLVE_SERVICE_NDEPENDS_0802,
    E_RESOLVE_SERVICE_NREQUIREDBY_0802,
    E_RESOLVE_SERVICE_NOPTSDEPS_0802,
    E_RESOLVE_SERVICE_NCONTENTS_0802,

    // execute
    E_RESOLVE_SERVICE_RUN_0802,
    E_RESOLVE_SERVICE_RUN_USER_0802,
    E_RESOLVE_SERVICE_RUN_BUILD_0802,
    E_RESOLVE_SERVICE_RUN_RUNAS_0802,
    E_RESOLVE_SERVICE_FINISH_0802,
    E_RESOLVE_SERVICE_FINISH_USER_0802,
    E_RESOLVE_SERVICE_FINISH_BUILD_0802,
    E_RESOLVE_SERVICE_FINISH_RUNAS_0802,
    E_RESOLVE_SERVICE_TIMEOUTSTART_0802,
    E_RESOLVE_SERVICE_TIMEOUTSTOP_0802,
    E_RESOLVE_SERVICE_DOWN_0802,
    E_RESOLVE_SERVICE_DOWNSIGNAL_0802,

    // live
    E_RESOLVE_SERVICE_LIVEDIR_0802,
    E_RESOLVE_SERVICE_STATUS_0802,
    E_RESOLVE_SERVICE_SERVICEDIR_LIVE_0802,
    E_RESOLVE_SERVICE_SCANDIR_0802,
    E_RESOLVE_SERVICE_STATEDIR_0802,
    E_RESOLVE_SERVICE_EVENTDIR_0802,
    E_RESOLVE_SERVICE_NOTIFDIR_0802,
    E_RESOLVE_SERVICE_SUPERVISEDIR_0802,
    E_RESOLVE_SERVICE_FDHOLDERDIR_0802,
    E_RESOLVE_SERVICE_ONESHOTDDIR_0802,

    // logger
    E_RESOLVE_SERVICE_LOGNAME_0802,
    E_RESOLVE_SERVICE_LOGDESTINATION_0802,
    E_RESOLVE_SERVICE_LOGBACKUP_0802,
    E_RESOLVE_SERVICE_LOGMAXSIZE_0802,
    E_RESOLVE_SERVICE_LOGTIMESTAMP_0802,
    E_RESOLVE_SERVICE_LOGWANT_0802,
    E_RESOLVE_SERVICE_LOGRUN_0802,
    E_RESOLVE_SERVICE_LOGRUN_USER_0802,
    E_RESOLVE_SERVICE_LOGRUN_BUILD_0802,
    E_RESOLVE_SERVICE_LOGRUN_RUNAS_0802,
    E_RESOLVE_SERVICE_LOGTIMEOUTSTART_0802,
    E_RESOLVE_SERVICE_LOGTIMEOUTSTOP_0802,

    // environment
    E_RESOLVE_SERVICE_ENV_0802,
    E_RESOLVE_SERVICE_ENVDIR_0802,
    E_RESOLVE_SERVICE_ENV_OVERWRITE_0802,

    // regex
    E_RESOLVE_SERVICE_REGEX_CONFIGURE_0802,
    E_RESOLVE_SERVICE_REGEX_DIRECTORIES_0802,
    E_RESOLVE_SERVICE_REGEX_FILES_0802,
    E_RESOLVE_SERVICE_REGEX_INFILES_0802,
    E_RESOLVE_SERVICE_REGEX_NDIRECTORIES_0802,
    E_RESOLVE_SERVICE_REGEX_NFILES_0802,
    E_RESOLVE_SERVICE_REGEX_NINFILES_0802,

    // IO
    E_RESOLVE_SERVICE_STDIN_0802,
    E_RESOLVE_SERVICE_STDINDEST_0802,
    E_RESOLVE_SERVICE_STDOUT_0802,
    E_RESOLVE_SERVICE_STDOUTDEST_0802,
    E_RESOLVE_SERVICE_STDERR_0802,
    E_RESOLVE_SERVICE_STDERRDEST_0802,
    E_RESOLVE_SERVICE_ENDOFKEY
} ;
typedef enum resolve_service_enum_e_0802 resolve_service_enum_t_0802, *resolve_service_enum_t_ref_0802;

enum enum_io_type_e_0802
{
    IO_TYPE_TTY_0802 = 0,
    IO_TYPE_FILE_0802,
    IO_TYPE_CONSOLE_0802,
    IO_TYPE_S6LOG_0802,
    IO_TYPE_SYSLOG_0802,
    IO_TYPE_INHERIT_0802,
    IO_TYPE_NULL_0802,
    IO_TYPE_PARENT_0802,
    IO_TYPE_CLOSE_0802,
    IO_TYPE_NOTSET_0802,
    IO_TYPE_ENDOFKEY
} ;
typedef enum enum_io_type_e_0802 enum_io_type_t_0802, *enum_io_type_t_ref_0802 ;

#endif
