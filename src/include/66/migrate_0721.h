/*
 * migrate_0721.h
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

#ifndef SS_MIGRATE_0721_H
#define SS_MIGRATE_0721_H

#include <stdint.h>

#include <oblibs/lexer.h>

#include <66/ssexec.h>


typedef struct resolve_service_addon_path_s_0721 resolve_service_addon_path_t_0721, *resolve_service_addon_path_t_ref_0721 ;
struct resolve_service_addon_path_s_0721
{
    uint32_t home ; // string, /var/lib/66 or /home/user/.66
    uint32_t frontend ;  // string, /home/<user>/.66/service or /etc/66/service or /usr/lib/66/service
    uint32_t servicedir ; //string, /var/lib/66/system/service/svc/service_name
} ;

#define RESOLVE_SERVICE_ADDON_PATH_ZERO_0721 { 0,0,0 }

typedef struct resolve_service_addon_dependencies_s_0721 resolve_service_addon_dependencies_t_0721, *resolve_service_addon_dependencies_t_ref_0721 ;
struct resolve_service_addon_dependencies_s_0721
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

#define RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO_0721 { 0,0,0,0,0,0,0,0 }

typedef struct resolve_service_addon_timeout_s_0721 resolve_service_addon_timeout_t_0721, *resolve_service_addon_timeout_t_ref_0721 ;
struct resolve_service_addon_timeout_s_0721
{
    uint32_t kill ; // integer
    uint32_t finish ; // integer
    uint32_t up ; // integer
    uint32_t down ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO_0721 { 0,0,0,0 }

typedef struct resolve_service_addon_scripts_s_0721 resolve_service_addon_scripts_t_0721, *resolve_service_addon_scritps_t_ref_0721 ;
struct resolve_service_addon_scripts_s_0721
{
    uint32_t run ; // string, launch before @execute
    uint32_t run_user ; // string, @execute field
    uint32_t build ; // string, custom or execline
    uint32_t shebang ; // string
    uint32_t runas ; // string
} ;

#define RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO_0721 { 0,0,0,0,0 }

typedef struct resolve_service_addon_execute_s_0721 resolve_service_addon_execute_t_0721, *resolve_service_addon_execute_t_ref_0721 ;
struct resolve_service_addon_execute_s_0721
{
    resolve_service_addon_scripts_t_0721 run ;
    resolve_service_addon_scripts_t_0721 finish ;
    resolve_service_addon_timeout_t_0721 timeout ;
    uint32_t down ; // integer
    uint32_t downsignal ; // integer
} ;

#define RESOLVE_SERVICE_ADDON_EXECUTE_ZERO_0721 { \
    RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO_0721, \
    RESOLVE_SERVICE_ADDON_SCRIPTS_ZERO_0721, \
    RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO_0721, \
    0, \
    0 \
}

typedef struct resolve_service_addon_live_s_0721 resolve_service_addon_live_t_0721, *resolve_service_addon_live_t_ref_0721 ;
struct resolve_service_addon_live_s_0721
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

#define RESOLVE_SERVICE_ADDON_LIVE_ZERO_0721 { 0,0,0,0,0,0,0,0,0,0 }

typedef struct resolve_service_addon_logger_s_0721 resolve_service_addon_logger_t_0721, *resolve_service_addon_logger_t_ref_0721 ;
struct resolve_service_addon_logger_s_0721
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
    resolve_service_addon_execute_t_0721 execute ;
    resolve_service_addon_timeout_t_0721 timeout ;

} ;

#define RESOLVE_SERVICE_ADDON_LOGGER_ZERO_0721 { \
    0,0,3,1000000,3,1, \
    RESOLVE_SERVICE_ADDON_EXECUTE_ZERO_0721, \
    RESOLVE_SERVICE_ADDON_TIMEOUT_ZERO_0721 \
}

typedef struct resolve_service_addon_environ_s_0721 resolve_service_addon_environ_t_0721, *resolve_service_addon_environ_t_ref_0721 ;
struct resolve_service_addon_environ_s_0721
{
    uint32_t env ; // string
    uint32_t envdir ; // string, /etc/66/conf or /home/user/.66/conf
    uint32_t env_overwrite ; // integer, overwrite the environment

} ;

#define RESOLVE_SERVICE_ADDON_ENVIRON_ZERO_0721 { 0,0,0 }

typedef struct resolve_service_addon_regex_s_0721 resolve_service_addon_regex_t_0721, *resolve_service_addon_regex_t_ref_0721 ;
struct resolve_service_addon_regex_s_0721
{
    uint32_t configure ; // string
    uint32_t directories ; // string
    uint32_t files ; // string
    uint32_t infiles ; // string

    uint32_t ndirectories ; // integer
    uint32_t nfiles ; // integer
    uint32_t ninfiles ; // integer

} ;

#define RESOLVE_SERVICE_ADDON_REGEX_ZERO_0721 { 0,0,0,0,0,0,0 }

typedef struct resolve_service_s_0721 resolve_service_t_0721, *resolve_service_t_ref_0721 ;
struct resolve_service_s_0721
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

    resolve_service_addon_path_t_0721 path ;
    resolve_service_addon_dependencies_t_0721 dependencies ;
    resolve_service_addon_execute_t_0721 execute ;
    resolve_service_addon_live_t_0721 live ;
    resolve_service_addon_logger_t_0721 logger ;
    resolve_service_addon_environ_t_0721 environ ;
    resolve_service_addon_regex_t_0721 regex ;

} ;

#define RESOLVE_SERVICE_ZERO_0721 { 0,STRALLOC_ZERO, \
                               0,0,0,0,0,5,0,0,0,0,0,0,0,0,0, \
                               RESOLVE_SERVICE_ADDON_PATH_ZERO_0721, \
                               RESOLVE_SERVICE_ADDON_DEPENDENCIES_ZERO_0721, \
                               RESOLVE_SERVICE_ADDON_EXECUTE_ZERO_0721, \
                               RESOLVE_SERVICE_ADDON_LIVE_ZERO_0721, \
                               RESOLVE_SERVICE_ADDON_LOGGER_ZERO_0721, \
                               RESOLVE_SERVICE_ADDON_ENVIRON_ZERO_0721, \
                               RESOLVE_SERVICE_ADDON_REGEX_ZERO_0721 }

typedef enum resolve_service_enum_e_0721 resolve_service_enum_t_0721, *resolve_service_enum_t_ref_0721;
enum resolve_service_enum_e_0721
{
    E_RESOLVE_SERVICE_NAME_0721 = 0,
    E_RESOLVE_SERVICE_DESCRIPTION_0721,
    E_RESOLVE_SERVICE_VERSION_0721,
    E_RESOLVE_SERVICE_TYPE_0721,
    E_RESOLVE_SERVICE_NOTIFY_0721,
    E_RESOLVE_SERVICE_MAXDEATH_0721,
    E_RESOLVE_SERVICE_EARLIER_0721,
    E_RESOLVE_SERVICE_HIERCOPY_0721,
    E_RESOLVE_SERVICE_INTREE_0721,
    E_RESOLVE_SERVICE_OWNERSTR_0721,
    E_RESOLVE_SERVICE_OWNER_0721,
    E_RESOLVE_SERVICE_TREENAME_0721,
    E_RESOLVE_SERVICE_USER_0721,
    E_RESOLVE_SERVICE_INNS_0721,
    E_RESOLVE_SERVICE_ENABLED_0721,

    // path
    E_RESOLVE_SERVICE_HOME_0721,
    E_RESOLVE_SERVICE_FRONTEND_0721,
    E_RESOLVE_SERVICE_SERVICEDIR_0721,

    // dependencies
    E_RESOLVE_SERVICE_DEPENDS_0721,
    E_RESOLVE_SERVICE_REQUIREDBY_0721,
    E_RESOLVE_SERVICE_OPTSDEPS_0721,
    E_RESOLVE_SERVICE_CONTENTS_0721,
    E_RESOLVE_SERVICE_NDEPENDS_0721,
    E_RESOLVE_SERVICE_NREQUIREDBY_0721,
    E_RESOLVE_SERVICE_NOPTSDEPS_0721,
    E_RESOLVE_SERVICE_NCONTENTS_0721,

    // execute
    E_RESOLVE_SERVICE_RUN_0721,
    E_RESOLVE_SERVICE_RUN_USER_0721,
    E_RESOLVE_SERVICE_RUN_BUILD_0721,
    E_RESOLVE_SERVICE_RUN_SHEBANG_0721,
    E_RESOLVE_SERVICE_RUN_RUNAS_0721,
    E_RESOLVE_SERVICE_FINISH_0721,
    E_RESOLVE_SERVICE_FINISH_USER_0721,
    E_RESOLVE_SERVICE_FINISH_BUILD_0721,
    E_RESOLVE_SERVICE_FINISH_SHEBANG_0721,
    E_RESOLVE_SERVICE_FINISH_RUNAS_0721,
    E_RESOLVE_SERVICE_TIMEOUTKILL_0721,
    E_RESOLVE_SERVICE_TIMEOUTFINISH_0721,
    E_RESOLVE_SERVICE_TIMEOUTUP_0721,
    E_RESOLVE_SERVICE_TIMEOUTDOWN_0721,
    E_RESOLVE_SERVICE_DOWN_0721,
    E_RESOLVE_SERVICE_DOWNSIGNAL_0721,

    // live
    E_RESOLVE_SERVICE_LIVEDIR_0721,
    E_RESOLVE_SERVICE_STATUS_0721,
    E_RESOLVE_SERVICE_SERVICEDIR_LIVE_0721,
    E_RESOLVE_SERVICE_SCANDIR_0721,
    E_RESOLVE_SERVICE_STATEDIR_0721,
    E_RESOLVE_SERVICE_EVENTDIR_0721,
    E_RESOLVE_SERVICE_NOTIFDIR_0721,
    E_RESOLVE_SERVICE_SUPERVISEDIR_0721,
    E_RESOLVE_SERVICE_FDHOLDERDIR_0721,
    E_RESOLVE_SERVICE_ONESHOTDDIR_0721,

    // logger
    E_RESOLVE_SERVICE_LOGNAME_0721,
    E_RESOLVE_SERVICE_LOGDESTINATION_0721,
    E_RESOLVE_SERVICE_LOGBACKUP_0721,
    E_RESOLVE_SERVICE_LOGMAXSIZE_0721,
    E_RESOLVE_SERVICE_LOGTIMESTAMP_0721,
    E_RESOLVE_SERVICE_LOGWANT_0721,
    E_RESOLVE_SERVICE_LOGRUN_0721,
    E_RESOLVE_SERVICE_LOGRUN_USER_0721,
    E_RESOLVE_SERVICE_LOGRUN_BUILD_0721,
    E_RESOLVE_SERVICE_LOGRUN_SHEBANG_0721,
    E_RESOLVE_SERVICE_LOGRUN_RUNAS_0721,
    E_RESOLVE_SERVICE_LOGTIMEOUTKILL_0721,
    E_RESOLVE_SERVICE_LOGTIMEOUTFINISH_0721,

    // environment
    E_RESOLVE_SERVICE_ENV_0721,
    E_RESOLVE_SERVICE_ENVDIR_0721,
    E_RESOLVE_SERVICE_ENV_OVERWRITE_0721,

    // regex
    E_RESOLVE_SERVICE_REGEX_CONFIGURE_0721,
    E_RESOLVE_SERVICE_REGEX_DIRECTORIES_0721,
    E_RESOLVE_SERVICE_REGEX_FILES_0721,
    E_RESOLVE_SERVICE_REGEX_INFILES_0721,
    E_RESOLVE_SERVICE_REGEX_NDIRECTORIES_0721,
    E_RESOLVE_SERVICE_REGEX_NFILES_0721,
    E_RESOLVE_SERVICE_REGEX_NINFILES_0721,
    E_RESOLVE_SERVICE_ENDOFKEY_0721

} ;

typedef enum enum_key_section_startstop_e_0721 enum_key_section_startstop_t_0721, *enum_key_section_startstop_t_ref_0721 ;
enum enum_key_section_startstop_e_0721
{
    KEY_STARTSTOP_BUILD_0721 = 0 ,
    KEY_STARTSTOP_RUNAS_0721 ,
    KEY_STARTSTOP_SHEBANG_0721 ,
    KEY_STARTSTOP_EXEC_0721 ,
    KEY_STARTSTOP_ENDOFKEY_0721
} ;

typedef enum enum_section_e_0721 enum_section_t_0721, *enum_section_t_ref_0721 ;
enum enum_section_e_0721
{
    SECTION_MAIN_0721 = 0 ,
    SECTION_START_0721 ,
    SECTION_STOP_0721 ,
    SECTION_LOG_0721 ,
    SECTION_ENV_0721 ,
    SECTION_REGEX_0721 ,
    SECTION_ENDOFKEY_0721
} ;

extern void migrate_0721(ssexec_t *info) ;

#endif
