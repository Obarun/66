/*
 * enum_parser.h
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

#ifndef SS_ENUM_PARSER_H
#define SS_ENUM_PARSER_H

#include <stdint.h>
#include <linux/capability.h> // _LINUX_CAPABILITY_VERSION_3

#include <66/enum_struct.h>

#define ENUM_EXPECTED(id, str) E_PARSER_EXPECT_##id
#define STR_EXPECTED(id, str) [E_PARSER_EXPECT_##id] = str
#define KEY_EXPECTED(idy, str) { .name = &enum_str_parser_expected[E_PARSER_EXPECT_##idy], .id = E_PARSER_EXPECT_##idy, .expected = 0 }

#define EXPECTED_TEMPLATE(macro) \
    macro(LINE,     "line"), \
    macro(BRACKET,  "bracket"), \
    macro(UINT,     "uint"), \
    macro(SLASH,    "slash"), \
    macro(QUOTE,    "quote"), \
    macro(KEYVAL,   "keyval")

enum resolve_parser_enum_expected_e
{
    EXPECTED_TEMPLATE(ENUM_EXPECTED),
    E_PARSER_EXPECT_ENDOFKEY
} ;
typedef enum resolve_parser_enum_expected_e resolve_parser_enum_expected_t ;

#define ENUM_SECTION(id, str) E_PARSER_SECTION_##id
#define STR_SECTION(id, str) [E_PARSER_SECTION_##id] = str
#define KEY_SECTION(idy, str) { .name = &enum_str_parser_section[E_PARSER_SECTION_##idy], .id = E_PARSER_SECTION_##idy, .expected = 0 }

#define SECTION_TEMPLATE(macro) \
    macro(MAIN,         "Main"), \
    macro(START,        "Start"), \
    macro(STOP,         "Stop"), \
    macro(LOGGER,       "Logger"), \
    macro(ENVIRONMENT,  "Environment"), \
    macro(REGEX,        "Regex"), \
    macro(EXECUTE,      "Execute")

enum resolve_parser_enum_section_e
{
    SECTION_TEMPLATE(ENUM_SECTION),
    E_PARSER_SECTION_ENDOFKEY
} ;
typedef enum resolve_parser_enum_section_e resolve_parser_enum_section_t ;

#define ENUM_SECTION_MAIN(id, str, exp) E_PARSER_SECTION_MAIN_##id
#define STR_SECTION_MAIN(id, str, exp) [E_PARSER_SECTION_MAIN_##id] = str
#define KEY_SECTION_MAIN(idy, str, exp) { .name = &enum_str_parser_section_main[E_PARSER_SECTION_MAIN_##idy], .id = E_PARSER_SECTION_MAIN_##idy, .expected = exp }

#define SECTION_MAIN_TEMPLATE(macro) \
    macro(TYPE,             "Type",             E_PARSER_EXPECT_LINE), \
    macro(VERSION,          "Version",          E_PARSER_EXPECT_LINE), \
    macro(DESCRIPTION,      "Description",      E_PARSER_EXPECT_QUOTE), \
    macro(DEPENDS,          "Depends",          E_PARSER_EXPECT_BRACKET), \
    macro(REQUIREDBY,       "RequiredBy",       E_PARSER_EXPECT_BRACKET), \
    macro(OPTSDEPS,         "OptsDepends",      E_PARSER_EXPECT_BRACKET), \
    macro(CONTENTS,         "Contents",         E_PARSER_EXPECT_BRACKET), \
    macro(OPTIONS,          "Options",          E_PARSER_EXPECT_BRACKET), \
    macro(NOTIFY,           "Notify",           E_PARSER_EXPECT_UINT), \
    macro(USER,             "User",             E_PARSER_EXPECT_BRACKET), \
    macro(TIMESTART,        "TimeoutStart",     E_PARSER_EXPECT_UINT), \
    macro(TIMESTOP,         "TimeoutStop",      E_PARSER_EXPECT_UINT), \
    macro(DEATH,            "MaxDeath",         E_PARSER_EXPECT_UINT), \
    macro(COPYFROM,         "CopyFrom",         E_PARSER_EXPECT_BRACKET), \
    macro(SIGNAL,           "DownSignal",       E_PARSER_EXPECT_UINT), \
    macro(FLAGS,            "Flags",            E_PARSER_EXPECT_BRACKET), \
    macro(INTREE,           "InTree",           E_PARSER_EXPECT_LINE), \
    macro(STDIN,            "StdIn",            E_PARSER_EXPECT_LINE), \
    macro(STDOUT,           "StdOut",           E_PARSER_EXPECT_LINE), \
    macro(STDERR,           "StdErr",           E_PARSER_EXPECT_LINE), \
    macro(PROVIDE,          "Provide",          E_PARSER_EXPECT_BRACKET), \
    macro(CONFLICT,         "Conflict",         E_PARSER_EXPECT_BRACKET)

enum resolve_parser_enum_section_main_e
{
    SECTION_MAIN_TEMPLATE(ENUM_SECTION_MAIN),
    E_PARSER_SECTION_MAIN_ENDOFKEY
} ;
typedef enum resolve_parser_enum_section_main_e resolve_parser_enum_section_main_t ;

#define ENUM_SECTION_STARTSTOP(id, str, exp) E_PARSER_SECTION_STARTSTOP_##id
#define STR_SECTION_STARTSTOP(id, str, exp) [E_PARSER_SECTION_STARTSTOP_##id] = str
#define KEY_SECTION_STARTSTOP(idy, str, exp) { .name = &enum_str_parser_section_startstop[E_PARSER_SECTION_STARTSTOP_##idy], .id = E_PARSER_SECTION_STARTSTOP_##idy, .expected = exp }

#define SECTION_STARTSTOP_TEMPLATE(macro) \
    macro(BUILD, "Build",   E_PARSER_EXPECT_LINE), \
    macro(RUNAS, "RunAs",   E_PARSER_EXPECT_LINE), \
    macro(EXEC,  "Execute", E_PARSER_EXPECT_BRACKET)

enum resolve_parser_enum_section_startstop_e
{
    SECTION_STARTSTOP_TEMPLATE(ENUM_SECTION_STARTSTOP),
    E_PARSER_SECTION_STARTSTOP_ENDOFKEY
} ;
typedef enum resolve_parser_enum_section_startstop_e resolve_parser_enum_section_startstop_t ;

#define ENUM_SECTION_LOGGER(id, str, exp) E_PARSER_SECTION_LOGGER_##id
#define STR_SECTION_LOGGER(id, str, exp) [E_PARSER_SECTION_LOGGER_##id] = str
#define KEY_SECTION_LOGGER(idy, str, exp) { .name = &enum_str_parser_section_logger[E_PARSER_SECTION_LOGGER_##idy], .id = E_PARSER_SECTION_LOGGER_##idy, .expected = exp }

#define SECTION_LOGGER_TEMPLATE(macro) \
    macro(BUILD,         "Build",           E_PARSER_EXPECT_LINE), \
    macro(RUNAS,         "RunAs",           E_PARSER_EXPECT_LINE), \
    macro(EXEC,          "Execute",         E_PARSER_EXPECT_BRACKET), \
    macro(DESTINATION,   "Destination",     E_PARSER_EXPECT_SLASH), \
    macro(BACKUP,        "Backup",          E_PARSER_EXPECT_UINT), \
    macro(MAXSIZE,       "MaxSize",         E_PARSER_EXPECT_UINT), \
    macro(TIMESTAMP,     "Timestamp",       E_PARSER_EXPECT_LINE), \
    macro(TIMESTART,     "TimeoutStart",    E_PARSER_EXPECT_UINT), \
    macro(TIMESTOP,      "TimeoutStop",     E_PARSER_EXPECT_UINT)

enum resolve_parser_enum_section_logger_e
{
    SECTION_LOGGER_TEMPLATE(ENUM_SECTION_LOGGER),
    E_PARSER_SECTION_LOGGER_ENDOFKEY
} ;
typedef enum resolve_parser_enum_section_logger_e resolve_parser_enum_section_logger_t ;

#define ENUM_SECTION_ENVIRON(id, str, exp) E_PARSER_SECTION_ENVIRON_##id
#define STR_SECTION_ENVIRON(id, str, exp) [E_PARSER_SECTION_ENVIRON_##id] = str
#define KEY_SECTION_ENVIRON(idy, str, exp) { .name = &enum_str_parser_section_environ[E_PARSER_SECTION_ENVIRON_##idy], .id = E_PARSER_SECTION_ENVIRON_##idy, .expected = exp }

#define SECTION_ENVIRON_TEMPLATE(macro) \
    macro(ENVAL,        "Enval",        E_PARSER_EXPECT_KEYVAL), \
    macro(IMPORTFILE,   "ImportFile",   E_PARSER_EXPECT_BRACKET)


enum resolve_parser_enum_section_environ_e
{
    SECTION_ENVIRON_TEMPLATE(ENUM_SECTION_ENVIRON),
    E_PARSER_SECTION_ENVIRON_ENDOFKEY
} ;
typedef enum resolve_parser_enum_section_environ_e resolve_parser_enum_section_environ_t ;

#define ENUM_SECTION_REGEX(id, str, exp) E_PARSER_SECTION_REGEX_##id
#define STR_SECTION_REGEX(id, str, exp) [E_PARSER_SECTION_REGEX_##id] = str
#define KEY_SECTION_REGEX(idy, str, exp) { .name = &enum_str_parser_section_regex[E_PARSER_SECTION_REGEX_##idy], .id = E_PARSER_SECTION_REGEX_##idy, .expected = exp }

#define SECTION_REGEX_TEMPLATE(macro) \
    macro(CONFIGURE,    "Configure",    E_PARSER_EXPECT_QUOTE), \
    macro(DIRECTORIES,  "Directories",  E_PARSER_EXPECT_BRACKET), \
    macro(FILES,        "Files",        E_PARSER_EXPECT_BRACKET), \
    macro(INFILES,      "InFiles",      E_PARSER_EXPECT_BRACKET)

enum resolve_parser_enum_section_regex_e
{
    SECTION_REGEX_TEMPLATE(ENUM_SECTION_REGEX),
    E_PARSER_SECTION_REGEX_ENDOFKEY
} ;
typedef enum resolve_parser_enum_section_regex_e resolve_parser_enum_section_regex_t ;

#define ENUM_SECTION_EXECUTE(id, str, exp) E_PARSER_SECTION_EXECUTE_##id
#define STR_SECTION_EXECUTE(id, str, exp) [E_PARSER_SECTION_EXECUTE_##id] = str
#define KEY_SECTION_EXECUTE(idy, str, exp) { .name = &enum_str_parser_section_execute[E_PARSER_SECTION_EXECUTE_##idy], .id = E_PARSER_SECTION_EXECUTE_##idy, .expected = exp }

#define SECTION_EXECUTE_TEMPLATE(macro) \
    macro(LIMITAS,          "LimitAS",          E_PARSER_EXPECT_UINT), \
    macro(LIMITCORE,        "LimitCORE",        E_PARSER_EXPECT_UINT), \
    macro(LIMITCPU,         "LimitCPU",         E_PARSER_EXPECT_UINT), \
    macro(LIMITDATA,        "LimitDATA",        E_PARSER_EXPECT_UINT), \
    macro(LIMITFSIZE,       "LimitFSIZE",       E_PARSER_EXPECT_UINT), \
    macro(LIMITLOCKS,       "LimitLOCKS",       E_PARSER_EXPECT_UINT), \
    macro(LIMITMEMLOCK,     "LimitMEMLOCK",     E_PARSER_EXPECT_UINT), \
    macro(LIMITMSGQUEUE,    "LimitMSGQUEUE",    E_PARSER_EXPECT_UINT), \
    macro(LIMITNICE,        "LimitNICE",        E_PARSER_EXPECT_UINT), \
    macro(LIMITNOFILE,      "LimitNOFILE",      E_PARSER_EXPECT_UINT), \
    macro(LIMITNPROC,       "LimitNPROC",       E_PARSER_EXPECT_UINT), \
    macro(LIMITRTPRIO,      "LimitRTPRIO",      E_PARSER_EXPECT_UINT), \
    macro(LIMITRTTIME,      "LimitRTTIME",      E_PARSER_EXPECT_UINT), \
    macro(LIMITSIGPENDING,  "LimitSIGPENDING",  E_PARSER_EXPECT_UINT), \
    macro(LIMITSTACK,       "LimitSTACK",       E_PARSER_EXPECT_UINT), \
    macro(BLOCK_PRIVILEGES, "BlockPrivileges",  E_PARSER_EXPECT_LINE), \
    macro(UMASK,            "UMask",            E_PARSER_EXPECT_UINT), \
    macro(NICE,             "Nice",             E_PARSER_EXPECT_UINT), \
    macro(CHDIR,            "ChangeDirectory",  E_PARSER_EXPECT_LINE), \
    macro(CAPS_BOUND,       "CapsBound",        E_PARSER_EXPECT_BRACKET), \
    macro(CAPS_AMBIENT,     "CapsAmbient",      E_PARSER_EXPECT_BRACKET)

enum resolve_parser_enum_section_execute_e
{
    SECTION_EXECUTE_TEMPLATE(ENUM_SECTION_EXECUTE),
    E_PARSER_SECTION_EXECUTE_ENDOFKEY
} ;
typedef enum resolve_parser_enum_section_execute_e resolve_parser_enum_section_execute_t ;

#define ENUM_IO_TYPE(id, str, exp) E_PARSER_IO_TYPE_##id
#define STR_IO_TYPE(id, str, exp) [E_PARSER_IO_TYPE_##id] = str
#define KEY_IO_TYPE(idy, str, exp) { .name = &enum_str_parser_io_type[E_PARSER_IO_TYPE_##idy], .id = E_PARSER_IO_TYPE_##idy, .expected = exp }

#define IO_TYPE_TEMPLATE(macro) \
    macro(TTY,      "tty",      E_PARSER_EXPECT_LINE), \
    macro(FILE,     "file",     E_PARSER_EXPECT_LINE), \
    macro(CONSOLE,  "console",  E_PARSER_EXPECT_LINE), \
    macro(S6LOG,    "s6log",    E_PARSER_EXPECT_LINE), \
    macro(SYSLOG,   "syslog",   E_PARSER_EXPECT_LINE), \
    macro(INHERIT,  "inherit",  E_PARSER_EXPECT_LINE), \
    macro(NULL,     "null",     E_PARSER_EXPECT_LINE), \
    macro(PARENT,   "parent",   E_PARSER_EXPECT_LINE), \
    macro(CLOSE,    "close",    E_PARSER_EXPECT_LINE), \
    macro(NOTSET,   "notset",   E_PARSER_EXPECT_LINE)

enum resolve_parser_enum_io_type_e
{
    IO_TYPE_TEMPLATE(ENUM_IO_TYPE),
    E_PARSER_IO_TYPE_ENDOFKEY
} ;
typedef enum resolve_parser_enum_io_type_e resolve_parser_enum_io_type_t ;

#define ENUM_TYPE(id, str, exp) E_PARSER_TYPE_##id
#define STR_TYPE(id, str, exp) [E_PARSER_TYPE_##id] = str
#define KEY_TYPE(idy, str, exp) { .name = &enum_str_parser_type[E_PARSER_TYPE_##idy], .id = E_PARSER_TYPE_##idy, .expected = exp }

#define TYPE_TEMPLATE(macro) \
    macro(CLASSIC, "classic", E_PARSER_EXPECT_LINE), \
    macro(ONESHOT, "oneshot", E_PARSER_EXPECT_LINE), \
    macro(MODULE,  "module",  E_PARSER_EXPECT_LINE)

enum resolve_parser_enum_type_e
{
    TYPE_TEMPLATE(ENUM_TYPE),
    E_PARSER_TYPE_ENDOFKEY
} ;
typedef enum resolve_parser_enum_type_e resolve_parser_enum_type_t ;

#define ENUM_OPTS(id, str, exp) E_PARSER_OPTS_##id
#define STR_OPTS(id, str, exp) [E_PARSER_OPTS_##id] = str
#define KEY_OPTS(idy, str, exp) { .name = &enum_str_parser_opts[E_PARSER_OPTS_##idy], .id = E_PARSER_OPTS_##idy, .expected = exp }

#define OPTS_TEMPLATE(macro) \
    macro(LOGGER, "log", E_PARSER_EXPECT_BRACKET)

enum resolve_parser_enum_opts_e
{
    OPTS_TEMPLATE(ENUM_OPTS),
    E_PARSER_OPTS_ENDOFKEY
} ;
typedef enum resolve_parser_enum_opts_e resolve_parser_enum_opts_t ;

#define ENUM_FLAGS(id, str, exp) E_PARSER_FLAGS_##id
#define STR_FLAGS(id, str, exp) [E_PARSER_FLAGS_##id] = str
#define KEY_FLAGS(idy, str, exp) { .name = &enum_str_parser_flags[E_PARSER_FLAGS_##idy], .id = E_PARSER_FLAGS_##idy, .expected = exp }

#define FLAGS_TEMPLATE(macro) \
    macro(DOWN,     "down",     E_PARSER_EXPECT_BRACKET), \
    macro(EARLIER,  "earlier",  E_PARSER_EXPECT_BRACKET)

enum resolve_parser_enum_flags_e
{
    FLAGS_TEMPLATE(ENUM_FLAGS),
    E_PARSER_FLAGS_ENDOFKEY
} ;
typedef enum resolve_parser_enum_flags_e resolve_parser_enum_flags_t ;

#define ENUM_BUILD(id, str, exp) E_PARSER_BUILD_##id
#define STR_BUILD(id, str, exp) [E_PARSER_BUILD_##id] = str
#define KEY_BUILD(idy, str, exp) { .name = &enum_str_parser_build[E_PARSER_BUILD_##idy], .id = E_PARSER_BUILD_##idy, .expected = exp }

#define BUILD_TEMPLATE(macro) \
    macro(AUTO,     "auto",     E_PARSER_EXPECT_LINE), \
    macro(CUSTOM,   "custom",   E_PARSER_EXPECT_LINE)

enum resolve_parser_enum_build_e
{
    BUILD_TEMPLATE(ENUM_BUILD),
    E_PARSER_BUILD_ENDOFKEY
} ;
typedef enum resolve_parser_enum_build_e resolve_parser_enum_build_t ;

#define ENUM_TIME(id, str, exp) E_PARSER_TIME_##id
#define STR_TIME(id, str, exp) [E_PARSER_TIME_##id] = str
#define KEY_TIME(idy, str, exp) { .name = &enum_str_parser_time[E_PARSER_TIME_##idy], .id = E_PARSER_TIME_##idy, .expected = exp }

#define TIME_TEMPLATE(macro) \
    macro(TAI,  "tai",  E_PARSER_EXPECT_LINE), \
    macro(ISO,  "iso",  E_PARSER_EXPECT_LINE), \
    macro(NONE, "none", E_PARSER_EXPECT_LINE)


enum resolve_parser_enum_time_e
{
    TIME_TEMPLATE(ENUM_TIME),
    E_PARSER_TIME_ENDOFKEY
} ;
typedef enum resolve_parser_enum_time_e resolve_parser_enum_time_t ;

#define ENUM_SEED(id, str, exp) E_PARSER_SEED_##id
#define STR_SEED(id, str, exp) [E_PARSER_SEED_##id] = str
#define KEY_SEED(idy, str, exp) { .name = &enum_str_parser_seed[E_PARSER_SEED_##idy], .id = E_PARSER_SEED_##idy, .expected = exp }

#define SEED_TEMPLATE(macro) \
    macro(DEPENDS,      "depends",      E_PARSER_EXPECT_LINE), \
    macro(REQUIREDBY,   "requiredby",   E_PARSER_EXPECT_LINE), \
    macro(ENABLE,       "enable",       E_PARSER_EXPECT_LINE), \
    macro(ALLOW,        "allow",        E_PARSER_EXPECT_LINE), \
    macro(DENY,         "deny",         E_PARSER_EXPECT_LINE), \
    macro(CURRENT,      "current",      E_PARSER_EXPECT_LINE), \
    macro(GROUPS,       "groups",       E_PARSER_EXPECT_LINE), \
    macro(CONTENTS,     "contents",     E_PARSER_EXPECT_LINE)

enum resolve_parser_enum_seed_e
{
    SEED_TEMPLATE(ENUM_SEED),
    E_PARSER_SEED_ENDOFKEY
} ;
typedef enum resolve_parser_enum_seed_e resolve_parser_enum_seed_t ;

#define ENUM_MANDATORY(id, str) E_PARSER_MANDATORY_##id
#define STR_MANDATORY(id, str) [E_PARSER_MANDATORY_##id] = str
#define KEY_MANDATORY(idy, str) { .name = &enum_str_parser_mandatory[E_PARSER_MANDATORY_##idy], .id = E_PARSER_MANDATORY_##idy, .expected = 0 }

#define MANDATORY_TEMPLATE(macro) \
    macro(NEED,     "need"), \
    macro(OPTS,     "opts"), \
    macro(CUSTOM,   "custom")

enum resolve_parser_enum_mandatory_e
{
    MANDATORY_TEMPLATE(ENUM_MANDATORY),
    E_PARSER_MANDATORY_ENDOFKEY
} ;
typedef enum resolve_parser_enum_mandatory_e resolve_parser_enum_mandatory_t ;

// Valid key for field CapsBound and CapsAmbient
#define ENUM_CAPS(id, str) E_PARSER_CAPS_##id
#define STR_CAPS(id, str) [E_PARSER_CAPS_##id] = str
#define KEY_CAPS(idy, str) { .name = &enum_str_parser_caps[E_PARSER_CAPS_##idy], .id = E_PARSER_CAPS_##idy, .expected = 0 }

#define CAPS_TEMPLATE_COMMON(macro) \
    macro(CHOWN,            "CAP_CHOWN"), \
    macro(DAC_OVERRIDE,     "CAP_DAC_OVERRIDE"), \
    macro(DAC_READ_SEARCH,  "CAP_DAC_READ_SEARCH"), \
    macro(FOWNER,           "CAP_FOWNER"), \
    macro(FSETID,           "CAP_FSETID"), \
    macro(KILL,             "CAP_KILL"), \
    macro(SETGID,           "CAP_SETGID"), \
    macro(SETUID,           "CAP_SETUID"), \
    macro(SETPCAP,          "CAP_SETPCAP"), \
    macro(LINUX_IMMUTABLE,  "CAP_LINUX_IMMUTABLE"), \
    macro(NET_BIND_SERVICE, "CAP_NET_BIND_SERVICE"), \
    macro(NET_BROADCAST,    "CAP_NET_BROADCAST"), \
    macro(NET_ADMIN,        "CAP_NET_ADMIN"), \
    macro(NET_RAW,          "CAP_NET_RAW"), \
    macro(IPC_LOCK,         "CAP_IPC_LOCK"), \
    macro(IPC_OWNER,        "CAP_IPC_OWNER"), \
    macro(SYS_MODULE,       "CAP_SYS_MODULE"), \
    macro(SYS_RAWIO,        "CAP_SYS_RAWIO"), \
    macro(SYS_CHROOT,       "CAP_SYS_CHROOT"), \
    macro(SYS_PTRACE,       "CAP_SYS_PTRACE"), \
    macro(SYS_PACCT,        "CAP_SYS_PACCT"), \
    macro(SYS_ADMIN,        "CAP_SYS_ADMIN"), \
    macro(SYS_BOOT,         "CAP_SYS_BOOT"), \
    macro(SYS_NICE,         "CAP_SYS_NICE"), \
    macro(SYS_RESOURCE,     "CAP_SYS_RESOURCE"), \
    macro(SYS_TIME,         "CAP_SYS_TIME"), \
    macro(SYS_TTY_CONFIG,   "CAP_SYS_TTY_CONFIG"), \
    macro(MKNOD,            "CAP_MKNOD"), \
    macro(LEASE,            "CAP_LEASE"), \
    macro(AUDIT_WRITE,      "CAP_AUDIT_WRITE"), \
    macro(AUDIT_CONTROL,    "CAP_AUDIT_CONTROL"), \
    macro(SETFCAP,          "CAP_SETFCAP"), \
    macro(MAC_OVERRIDE,     "CAP_MAC_OVERRIDE"), \
    macro(MAC_ADMIN,        "CAP_MAC_ADMIN"), \
    macro(SYSLOG,           "CAP_SYSLOG"), \
    macro(WAKE_ALARM,       "CAP_WAKE_ALARM"), \
    macro(BLOCK_SUSPEND,    "CAP_BLOCK_SUSPEND"), \
    macro(AUDIT_READ,       "CAP_AUDIT_READ")

#ifdef _LINUX_CAPABILITY_VERSION_3
#define CAPS_TEMPLATE_LINUX(macro) , \
    macro(PERFMON,          "CAP_PERFMON"), \
    macro(BPF,              "CAP_BPF"), \
    macro(CHECKPOINT_RESTORE, "CAP_CHECKPOINT_RESTORE")
#else
#define CAPS_TEMPLATE_LINUX(macro) /* Empty if condition not met */
#endif

/** Experimental
    macro(PIDFD_OPEN,       "CAP_PIDFD_OPEN"), \
    macro(PIDFD_GETFD,      "CAP_PIDFD_GETFD")
*/

#define CAPS_TEMPLATE(macro) \
    CAPS_TEMPLATE_COMMON(macro) \
    CAPS_TEMPLATE_LINUX(macro)

enum resolve_parser_enum_caps_e
{
    CAPS_TEMPLATE(ENUM_CAPS),
    E_PARSER_CAPS_ENDOFKEY
} ;
typedef enum resolve_parser_enum_caps_e resolve_parser_enum_caps_t ;

#define CATEGORY_PARSER(id) E_PARSER_CATEGORY_##id

#define PARSER_CATEGORY(macro) \
    macro(SECTION_MAIN), \
    macro(SECTION_STARTSTOP), \
    macro(SECTION_LOGGER), \
    macro(SECTION_ENVIRON), \
    macro(SECTION_REGEX), \
    macro(SECTION_EXECUTE), \
    macro(SECTION), \
    macro(IO_TYPE), \
    macro(TYPE), \
    macro(OPTS), \
    macro(FLAGS), \
    macro(BUILD), \
    macro(TIME), \
    macro(SEED), \
    macro(EXPECTED), \
    macro(MANDATORY), \
    macro(CAPS)

enum resolve_parser_enum_category_e
{
    PARSER_CATEGORY(CATEGORY_PARSER),
    E_PARSER_CATEGORY_ENDOFKEY
} ;
typedef enum resolve_parser_enum_category_e resolve_parser_enum_category_t ;

typedef struct resolve_parser_enum_table_s resolve_parser_enum_table_t ;
struct resolve_parser_enum_table_s {
    resolve_parser_enum_category_t category;
    uint32_t id ; // key id
    uint32_t sid ; // section id
    key_description_t const *list ;
} ;

#define E_TABLE_PARSER_SECTION_MAIN_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_SECTION_MAIN, \
    .u.parser.list = enum_list_parser_section_main, \
    .u.parser.sid = E_PARSER_SECTION_MAIN \
}

#define E_TABLE_PARSER_SECTION_START_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_SECTION_STARTSTOP, \
    .u.parser.list = enum_list_parser_section_startstop, \
    .u.parser.sid = E_PARSER_SECTION_START \
}

#define E_TABLE_PARSER_SECTION_STOP_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_SECTION_STARTSTOP, \
    .u.parser.list = enum_list_parser_section_startstop, \
    .u.parser.sid = E_PARSER_SECTION_STOP \
}

#define E_TABLE_PARSER_SECTION_LOGGER_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_SECTION_LOGGER, \
    .u.parser.list = enum_list_parser_section_logger, \
    .u.parser.sid = E_PARSER_SECTION_LOGGER \
}

#define E_TABLE_PARSER_SECTION_ENVIRON_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_SECTION_ENVIRON, \
    .u.parser.list = enum_list_parser_section_environ, \
    .u.parser.sid = E_PARSER_SECTION_ENVIRONMENT \
}

#define E_TABLE_PARSER_SECTION_REGEX_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_SECTION_REGEX, \
    .u.parser.list = enum_list_parser_section_regex, \
    .u.parser.sid = E_PARSER_SECTION_REGEX \
}

#define E_TABLE_PARSER_SECTION_EXECUTE_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_SECTION_EXECUTE, \
    .u.parser.list = enum_list_parser_section_execute, \
    .u.parser.sid = E_PARSER_SECTION_EXECUTE \
}

#define E_TABLE_PARSER_SECTION { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_SECTION, \
    .u.parser.list = enum_list_parser_section, \
    .u.parser.sid = 0 \
}

#define E_TABLE_PARSER_IO_TYPE_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_IO_TYPE, \
    .u.parser.list = enum_list_parser_io_type, \
    .u.parser.sid = E_PARSER_SECTION_MAIN \
}

#define E_TABLE_PARSER_TYPE_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_TYPE, \
    .u.parser.list = enum_list_parser_type, \
    .u.parser.sid = E_PARSER_SECTION_MAIN \
}

#define E_TABLE_PARSER_OPTS_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_OPTS, \
    .u.parser.list = enum_list_parser_opts, \
    .u.parser.sid = E_PARSER_SECTION_MAIN \
}

#define E_TABLE_PARSER_FLAGS_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_FLAGS, \
    .u.parser.list = enum_list_parser_flags, \
    .u.parser.sid = E_PARSER_SECTION_MAIN \
}

#define E_TABLE_PARSER_BUILD_START_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_BUILD, \
    .u.parser.list = enum_list_parser_build, \
    .u.parser.sid = E_PARSER_SECTION_START \
}

#define E_TABLE_PARSER_BUILD_STOP_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_BUILD, \
    .u.parser.list = enum_list_parser_build, \
    .u.parser.sid = E_PARSER_SECTION_STOP \
}

#define E_TABLE_PARSER_TIME_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_TIME, \
    .u.parser.list = enum_list_parser_time, \
    .u.parser.sid = E_PARSER_SECTION_LOGGER \
}

#define E_TABLE_PARSER_SEED_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_SEED, \
    .u.parser.list = enum_list_parser_seed \
}

#define E_TABLE_PARSER_EXPECTED_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_EXPECTED, \
    .u.parser.list = enum_list_parser_expected \
}

#define E_TABLE_PARSER_MANDATORY_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_MANDATORY, \
    .u.parser.list = enum_list_parser_mandatory \
}

#define E_TABLE_PARSER_CAPS_ZERO { \
    .category = E_RESOLVE_CATEGORY_PARSER, \
    .u.parser.category = E_PARSER_CATEGORY_CAPS, \
    .u.parser.list = enum_list_parser_caps, \
    .u.parser.sid = E_PARSER_SECTION_EXECUTE \
}

extern char const *enum_str_parser_expected[] ;
extern char const *enum_str_parser_section[] ;
extern char const *enum_str_parser_section_main[] ;
extern char const *enum_str_parser_section_startstop[] ;
extern char const *enum_str_parser_section_logger[] ;
extern char const *enum_str_parser_section_environ[] ;
extern char const *enum_str_parser_section_regex[] ;
extern char const *enum_str_parser_section_execute[] ;
extern char const *enum_str_parser_io_type[] ;
extern char const *enum_str_parser_type[] ;
extern char const *enum_str_parser_opts[] ;
extern char const *enum_str_parser_flags[] ;
extern char const *enum_str_parser_build[] ;
extern char const *enum_str_parser_time[] ;
extern char const *enum_str_parser_seed[] ;
extern char const *enum_str_parser_mandatory[] ;
extern char const *enum_str_parser_caps[] ;
extern key_description_t const enum_list_parser_expected[] ;
extern key_description_t const enum_list_parser_section[] ;
extern key_description_t const enum_list_parser_section_main[] ;
extern key_description_t const enum_list_parser_section_startstop[] ;
extern key_description_t const enum_list_parser_section_logger[] ;
extern key_description_t const enum_list_parser_section_environ[] ;
extern key_description_t const enum_list_parser_section_regex[] ;
extern key_description_t const enum_list_parser_section_execute[] ;
extern key_description_t const enum_list_parser_io_type[] ;
extern key_description_t const enum_list_parser_type[] ;
extern key_description_t const enum_list_parser_opts[] ;
extern key_description_t const enum_list_parser_flags[] ;
extern key_description_t const enum_list_parser_build[] ;
extern key_description_t const enum_list_parser_time[] ;
extern key_description_t const enum_list_parser_seed[] ;
extern key_description_t const enum_list_parser_mandatory[] ;
extern key_description_t const enum_list_parser_caps[] ;
extern key_description_t const *enum_get_list_parser(resolve_parser_enum_table_t table) ;

#endif
