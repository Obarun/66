/*
 * enum_service.h
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef SS_ENUM_SERVICE_H
#define SS_ENUM_SERVICE_H

#include <stdint.h>
#include <66/enum_struct.h>

// configuration

#define ENUM_SERVICE_CONFIG(id, str) E_RESOLVE_SERVICE_CONFIG_##id
#define STR_SERVICE_CONFIG(id, str) [E_RESOLVE_SERVICE_CONFIG_##id] = str
#define KEY_SERVICE_CONFIG(idy, str) { .name = &enum_str_service_config[E_RESOLVE_SERVICE_CONFIG_##idy], .id = E_RESOLVE_SERVICE_CONFIG_##idy }

#define CONFIG_TEMPLATE(macro) \
    macro(RVERSION,     "rversion"), \
    macro(NAME,         "name"), \
    macro(DESCRIPTION,  "description"), \
    macro(VERSION,      "version"), \
    macro(TYPE,         "type"), \
    macro(NOTIFY,       "notify"), \
    macro(MAXDEATH,     "maxdeath"), \
    macro(EARLIER,      "earlier"), \
    macro(COPYFROM,     "copyfrom"), \
    macro(INTREE,       "intree"), \
    macro(OWNERSTR,     "ownerstr"), \
    macro(OWNER,        "owner"), \
    macro(TREENAME,     "treename"), \
    macro(USER,         "user"), \
    macro(INNS,         "inns"), \
    macro(ENABLED,      "enabled"), \
    macro(ISLOG,        "islog")

enum resolve_service_enum_config_e
{
    CONFIG_TEMPLATE(ENUM_SERVICE_CONFIG),
    E_RESOLVE_SERVICE_CONFIG_ENDOFKEY
} ;
typedef enum resolve_service_enum_config_e resolve_service_enum_config_t ;

// path

#define ENUM_SERVICE_PATH(id, str) E_RESOLVE_SERVICE_PATH_##id
#define STR_SERVICE_PATH(id, str) [E_RESOLVE_SERVICE_PATH_##id] = str
#define KEY_SERVICE_PATH(idy, str) { .name = &enum_str_service_path[E_RESOLVE_SERVICE_PATH_##idy], .id = E_RESOLVE_SERVICE_PATH_##idy }

#define PATH_TEMPLATE(macro) \
    macro(HOME,         "home"), \
    macro(FRONTEND,     "frontend"), \
    macro(SERVICEDIR,   "servicedir")

enum resolve_service_enum_path_e
{
    PATH_TEMPLATE(ENUM_SERVICE_PATH),
    E_RESOLVE_SERVICE_PATH_ENDOFKEY
} ;
typedef enum resolve_service_enum_path_e resolve_service_enum_path_t ;

// dependencies

#define ENUM_SERVICE_DEPS(id, str) E_RESOLVE_SERVICE_DEPS_##id
#define STR_SERVICE_DEPS(id, str) [E_RESOLVE_SERVICE_DEPS_##id] = str
#define KEY_SERVICE_DEPS(idy, str) { .name = &enum_str_service_deps[E_RESOLVE_SERVICE_DEPS_##idy], .id = E_RESOLVE_SERVICE_DEPS_##idy }

#define DEPS_TEMPLATE(macro) \
    macro(DEPENDS,      "depends"), \
    macro(REQUIREDBY,   "requiredby"), \
    macro(OPTSDEPS,     "optsdeps"), \
    macro(CONTENTS,     "contents"), \
    macro(PROVIDE,      "provide"), \
    macro(CONFLICT,     "conflict"), \
    macro(NDEPENDS,     "ndepends"), \
    macro(NREQUIREDBY,  "nrequiredby"), \
    macro(NOPTSDEPS,    "noptsdeps"), \
    macro(NCONTENTS,    "ncontents"), \
    macro(NPROVIDE,     "nprovide"), \
    macro(NCONFLICT,    "nconflict")

enum resolve_service_enum_deps_e
{
    DEPS_TEMPLATE(ENUM_SERVICE_DEPS),
    E_RESOLVE_SERVICE_DEPS_ENDOFKEY
} ;
typedef enum resolve_service_enum_deps_e resolve_service_enum_deps_t ;

// execute

#define ENUM_SERVICE_EXECUTE(id, str) E_RESOLVE_SERVICE_EXECUTE_##id
#define STR_SERVICE_EXECUTE(id, str) [E_RESOLVE_SERVICE_EXECUTE_##id] = str
#define KEY_SERVICE_EXECUTE(idy, str) { .name = &enum_str_service_execute[E_RESOLVE_SERVICE_EXECUTE_##idy], .id = E_RESOLVE_SERVICE_EXECUTE_##idy }

#define EXECUTE_TEMPLATE(macro) \
    macro(RUN,              "run"), \
    macro(RUN_USER,         "run_user"), \
    macro(RUN_BUILD,        "run_build"), \
    macro(RUN_RUNAS,        "run_runas"), \
    macro(FINISH,           "finish"), \
    macro(FINISH_USER,      "finish_user"), \
    macro(FINISH_BUILD,     "finish_build"), \
    macro(FINISH_RUNAS,     "finish_runas"), \
    macro(TIMEOUTSTART,     "timeoutstart"), \
    macro(TIMEOUTSTOP,      "timeoutstop"), \
    macro(DOWN,             "down"), \
    macro(DOWNSIGNAL,       "downsignal"), \
    macro(BLOCK_PRIVILEGES, "blockprivileges"), \
    macro(UMASK,            "umask"), \
    macro(WANT_UMASK,       "want_umask"), \
    macro(NICE,             "nice"), \
    macro(WANT_NICE,        "want_nice"), \
    macro(CHDIR,            "chdir"), \
    macro(CAPS_BOUND,       "capsbound"), \
    macro(CAPS_AMBIENT,     "capsambient")

enum resolve_service_enum_execute_e
{
    EXECUTE_TEMPLATE(ENUM_SERVICE_EXECUTE),
    E_RESOLVE_SERVICE_EXECUTE_ENDOFKEY
} ;
typedef enum resolve_service_enum_execute_e resolve_service_enum_execute_t ;

// live

#define ENUM_SERVICE_LIVE(id, str) E_RESOLVE_SERVICE_LIVE_##id
#define STR_SERVICE_LIVE(id, str) [E_RESOLVE_SERVICE_LIVE_##id] = str
#define KEY_SERVICE_LIVE(idy, str) { .name = &enum_str_service_live[E_RESOLVE_SERVICE_LIVE_##idy], .id = E_RESOLVE_SERVICE_LIVE_##idy }

#define LIVE_TEMPLATE(macro) \
    macro(LIVEDIR,          "livedir"), \
    macro(STATUS,           "status"), \
    macro(SERVICEDIR,       "servicedir"), \
    macro(SCANDIR,          "scandirdir"), \
    macro(STATEDIR,         "statedir"), \
    macro(EVENTDIR,         "eventdir"), \
    macro(NOTIFDIR,         "notifdir"), \
    macro(SUPERVISEDIR,     "supervisedir"), \
    macro(FDHOLDERDIR,      "fdholderdir"), \
    macro(ONESHOTDDIR,      "oneshotddir")

enum resolve_service_enum_live_e
{
    LIVE_TEMPLATE(ENUM_SERVICE_LIVE),
    E_RESOLVE_SERVICE_LIVE_ENDOFKEY
} ;
typedef enum resolve_service_enum_live_e resolve_service_enum_live_t ;

// logger

#define ENUM_SERVICE_LOGGER(id, str) E_RESOLVE_SERVICE_LOGGER_##id
#define STR_SERVICE_LOGGER(id, str) [E_RESOLVE_SERVICE_LOGGER_##id] = str
#define KEY_SERVICE_LOGGER(idy, str) { .name = &enum_str_service_logger[E_RESOLVE_SERVICE_LOGGER_##idy], .id = E_RESOLVE_SERVICE_LOGGER_##idy }

#define LOGGER_TEMPLATE(macro) \
    macro(LOGNAME,          "logname"), \
    macro(LOGDESTINATION,   "logdestination"), \
    macro(LOGBACKUP,        "logbackup"), \
    macro(LOGMAXSIZE,       "logmaxsize"), \
    macro(LOGTIMESTAMP,     "logtimestamp"), \
    macro(LOGWANT,          "logwant"), \
    macro(LOGRUN,           "logrun"), \
    macro(LOGRUN_USER,      "logrun_user"), \
    macro(LOGRUN_BUILD,     "logrun_build"), \
    macro(LOGRUN_RUNAS,     "logrun_runas"), \
    macro(LOGTIMEOUTSTART,  "logtimeoutstart"), \
    macro(LOGTIMEOUTSTOP,   "logtimeoutstop")

enum resolve_service_enum_logger_e
{
    LOGGER_TEMPLATE(ENUM_SERVICE_LOGGER),
    E_RESOLVE_SERVICE_LOGGER_ENDOFKEY
} ;
typedef enum resolve_service_enum_logger_e resolve_service_enum_logger_t ;

#define ENUM_SERVICE_ENVIRON(id, str) E_RESOLVE_SERVICE_ENVIRON_##id
#define STR_SERVICE_ENVIRON(id, str) [E_RESOLVE_SERVICE_ENVIRON_##id] = str
#define KEY_SERVICE_ENVIRON(idy, str) { .name = &enum_str_service_environ[E_RESOLVE_SERVICE_ENVIRON_##idy], .id = E_RESOLVE_SERVICE_ENVIRON_##idy }

#define ENVIRON_TEMPLATE(macro) \
    macro(ENV,           "env"), \
    macro(ENVDIR,        "envdir"), \
    macro(ENV_OVERWRITE, "env_overwrite"), \
    macro(IMPORTFILE,    "importfile"), \
    macro(NIMPORTFILE,   "nimportfile")


enum resolve_service_enum_environ_e
{
    ENVIRON_TEMPLATE(ENUM_SERVICE_ENVIRON),
    E_RESOLVE_SERVICE_ENVIRON_ENDOFKEY
} ;
typedef enum resolve_service_enum_environ_e resolve_service_enum_environ_t ;

// regex

#define ENUM_SERVICE_REGEX(id, str) E_RESOLVE_SERVICE_REGEX_##id
#define STR_SERVICE_REGEX(id, str) [E_RESOLVE_SERVICE_REGEX_##id] = str
#define KEY_SERVICE_REGEX(idy, str) { .name = &enum_str_service_regex[E_RESOLVE_SERVICE_REGEX_##idy], .id = E_RESOLVE_SERVICE_REGEX_##idy }

#define REGEX_TEMPLATE(macro) \
    macro(CONFIGURE,    "configure"), \
    macro(DIRECTORIES,  "directories"), \
    macro(FILES,        "files"), \
    macro(INFILES,      "infiles"), \
    macro(NDIRECTORIES, "ndirectories"), \
    macro(NFILES,       "nfiles"), \
    macro(NINFILES,     "ninfiles")

enum resolve_service_enum_regex_e
{
    REGEX_TEMPLATE(ENUM_SERVICE_REGEX),
    E_RESOLVE_SERVICE_REGEX_ENDOFKEY
} ;
typedef enum resolve_service_enum_regex_e resolve_service_enum_regex_t ;

// io

#define ENUM_SERVICE_IO(id, str) E_RESOLVE_SERVICE_IO_##id
#define STR_SERVICE_IO(id, str) [E_RESOLVE_SERVICE_IO_##id] = str
#define KEY_SERVICE_IO(idy, str) { .name = &enum_str_service_io[E_RESOLVE_SERVICE_IO_##idy], .id = E_RESOLVE_SERVICE_IO_##idy }

#define IO_TEMPLATE(macro) \
    macro(STDIN,        "stdintype"), \
    macro(STDINDEST,    "stdindest"), \
    macro(STDOUT,       "stdouttype"), \
    macro(STDOUTDEST,   "stdoutdest"), \
    macro(STDERR,       "stderrtype"), \
    macro(STDERRDEST,   "stderrdest")

enum resolve_service_enum_io_e
{
    IO_TEMPLATE(ENUM_SERVICE_IO),
    E_RESOLVE_SERVICE_IO_ENDOFKEY
} ;
typedef enum resolve_service_enum_io_e resolve_service_enum_io_t ;

// limit

#define ENUM_SERVICE_LIMIT(id, str) E_RESOLVE_SERVICE_LIMIT_##id
#define STR_SERVICE_LIMIT(id, str) [E_RESOLVE_SERVICE_LIMIT_##id] = str
#define KEY_SERVICE_LIMIT(idy, str) { .name = &enum_str_service_limit[E_RESOLVE_SERVICE_LIMIT_##idy], .id = E_RESOLVE_SERVICE_LIMIT_##idy }

#define LIMIT_TEMPLATE(macro) \
    macro(AS,          "limitas"), \
    macro(CORE,        "limitcore"), \
    macro(CPU,         "limitcpu"), \
    macro(DATA,        "limitdata"), \
    macro(FSIZE,       "limitfsize"), \
    macro(LOCKS,       "limitlocks"), \
    macro(MEMLOCK,     "limitmemlock"), \
    macro(MSGQUEUE,    "limitmsgqueue"), \
    macro(NICE,        "limitnice"), \
    macro(NOFILE,      "limitnofile"), \
    macro(NPROC,       "limitnproc"), \
    macro(RTPRIO,      "limitrtprio"), \
    macro(RTTIME,      "limitrttime"), \
    macro(SIGPENDING,  "limitsigpending"), \
    macro(STACK,       "limitstack")

enum resolve_service_enum_limit_e
{
    LIMIT_TEMPLATE(ENUM_SERVICE_LIMIT),
    E_RESOLVE_SERVICE_LIMIT_ENDOFKEY
} ;
typedef enum resolve_service_enum_limit_e resolve_service_enum_limit_t ;

// Category
#define CATEGORY_SERVICE(id) E_RESOLVE_SERVICE_CATEGORY_##id

#define SERVICE_CATEGORY(macro) \
    macro(CONFIG), \
    macro(PATH), \
    macro(DEPS), \
    macro(EXECUTE), \
    macro(LIVE), \
    macro(LOGGER), \
    macro(ENVIRON), \
    macro(REGEX), \
    macro(IO) , \
    macro(LIMIT)

enum resolve_service_enum_category_e
{
    SERVICE_CATEGORY(CATEGORY_SERVICE),
    E_RESOLVE_SERVICE_CATEGORY_ENDOFKEY
} ;
typedef enum resolve_service_enum_category_e resolve_service_enum_category_t ;

typedef struct resolve_service_enum_table_s resolve_service_enum_table_t ;
struct resolve_service_enum_table_s
{
    resolve_service_enum_category_t category ;
    uint32_t id ;
    key_description_t const *list ;
} ;

#define E_TABLE_SERVICE_CONFIG_ZERO { \
    .category = E_RESOLVE_CATEGORY_SERVICE, \
    .u.service.category = E_RESOLVE_SERVICE_CATEGORY_CONFIG, \
    .u.service.list = enum_list_service_config \
}

#define E_TABLE_SERVICE_PATH_ZERO { \
    .category = E_RESOLVE_CATEGORY_SERVICE, \
    .u.service.category = E_RESOLVE_SERVICE_CATEGORY_PATH, \
    .u.service.list = enum_list_service_path \
}

#define E_TABLE_SERVICE_DEPS_ZERO { \
    .category = E_RESOLVE_CATEGORY_SERVICE, \
    .u.service.category = E_RESOLVE_SERVICE_CATEGORY_DEPS, \
    .u.service.list = enum_list_service_deps \
}

#define E_TABLE_SERVICE_EXECUTE_ZERO { \
    .category = E_RESOLVE_CATEGORY_SERVICE, \
    .u.service.category = E_RESOLVE_SERVICE_CATEGORY_EXECUTE, \
    .u.service.list = enum_list_service_execute \
}

#define E_TABLE_SERVICE_LIVE_ZERO { \
    .category = E_RESOLVE_CATEGORY_SERVICE, \
    .u.service.category = E_RESOLVE_SERVICE_CATEGORY_LIVE, \
    .u.service.list = enum_list_service_live \
}

#define E_TABLE_SERVICE_LOGGER_ZERO { \
    .category = E_RESOLVE_CATEGORY_SERVICE, \
    .u.service.category = E_RESOLVE_SERVICE_CATEGORY_LOGGER, \
    .u.service.list = enum_list_service_logger \
}

#define E_TABLE_SERVICE_ENVIRON_ZERO { \
    .category = E_RESOLVE_CATEGORY_SERVICE, \
    .u.service.category = E_RESOLVE_SERVICE_CATEGORY_ENVIRON, \
    .u.service.list = enum_list_service_environ \
}

#define E_TABLE_SERVICE_REGEX_ZERO { \
    .category = E_RESOLVE_CATEGORY_SERVICE, \
    .u.service.category = E_RESOLVE_SERVICE_CATEGORY_REGEX, \
    .u.service.list = enum_list_service_regex \
}

#define E_TABLE_SERVICE_IO_ZERO { \
    .category = E_RESOLVE_CATEGORY_SERVICE, \
    .u.service.category = E_RESOLVE_SERVICE_CATEGORY_IO, \
    .u.service.list = enum_list_service_io \
}

#define E_TABLE_SERVICE_LIMIT_ZERO { \
    .category = E_RESOLVE_CATEGORY_SERVICE, \
    .u.service.category = E_RESOLVE_SERVICE_CATEGORY_LIMIT, \
    .u.service.list = enum_list_service_limit \
}

extern const char *enum_str_service_config[] ;
extern const char *enum_str_service_path[] ;
extern const char *enum_str_service_deps[] ;
extern const char *enum_str_service_execute[] ;
extern const char *enum_str_service_live[] ;
extern const char *enum_str_service_logger[] ;
extern const char *enum_str_service_environ[] ;
extern const char *enum_str_service_regex[] ;
extern const char *enum_str_service_io[] ;
extern const char *enum_str_service_limit[] ;
extern key_description_t const enum_list_service_config[] ;
extern key_description_t const enum_list_service_path[] ;
extern key_description_t const enum_list_service_deps[] ;
extern key_description_t const enum_list_service_execute[] ;
extern key_description_t const enum_list_service_live[] ;
extern key_description_t const enum_list_service_logger[] ;
extern key_description_t const enum_list_service_environ[] ;
extern key_description_t const enum_list_service_regex[] ;
extern key_description_t const enum_list_service_io[] ;
extern key_description_t const enum_list_service_limit[] ;
extern key_description_t const *enum_get_list_service(resolve_service_enum_table_t table) ;

#endif