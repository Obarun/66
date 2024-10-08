/*
 * enum.h
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

#ifndef SS_ENUM_H
#define SS_ENUM_H

#include <sys/types.h> //ssize_t

typedef enum enum_section_e enum_section_t, *enum_section_t_ref ;
enum enum_section_e
{
    SECTION_MAIN = 0 ,
    SECTION_START ,
    SECTION_STOP ,
    SECTION_LOG ,
    SECTION_ENV ,
    SECTION_REGEX ,
    SECTION_ENDOFKEY
} ;

extern char const *enum_str_section[] ;

typedef enum enum_key_section_main_e enum_key_section_main_t, *enum_key_section_main_t_ref ;
enum enum_key_section_main_e
{
    KEY_MAIN_TYPE = 0 ,
    KEY_MAIN_VERSION ,
    KEY_MAIN_DESCRIPTION ,
    KEY_MAIN_DEPENDS ,
    KEY_MAIN_REQUIREDBY ,
    KEY_MAIN_OPTSDEPS ,
    KEY_MAIN_CONTENTS ,
    KEY_MAIN_OPTIONS ,
    KEY_MAIN_NOTIFY ,
    KEY_MAIN_USER ,
    KEY_MAIN_T_START ,
    KEY_MAIN_T_STOP ,
    KEY_MAIN_DEATH ,
    KEY_MAIN_COPYFROM ,
    KEY_MAIN_SIGNAL ,
    KEY_MAIN_FLAGS ,
    KEY_MAIN_INTREE ,
    KEY_MAIN_STDIN ,
    KEY_MAIN_STDOUT ,
    KEY_MAIN_STDERR ,
    KEY_MAIN_ENDOFKEY
} ;

extern char const *enum_str_key_section_main[] ;

typedef enum enum_key_section_startstop_e enum_key_section_startstop_t, *enum_key_section_startstop_t_ref ;
enum enum_key_section_startstop_e
{
    KEY_STARTSTOP_BUILD = 0 ,
    KEY_STARTSTOP_RUNAS ,
    KEY_STARTSTOP_EXEC ,
    KEY_STARTSTOP_ENDOFKEY
} ;

extern char const *enum_str_key_section_startstop[] ;

typedef enum enum_key_section_logger_e enum_key_section_logger_t, *enum_key_section_logger_t_ref ;
enum enum_key_section_logger_e
{
    KEY_LOGGER_BUILD = 0 ,
    KEY_LOGGER_RUNAS ,
    KEY_LOGGER_EXEC ,
    KEY_LOGGER_DESTINATION ,
    KEY_LOGGER_BACKUP ,
    KEY_LOGGER_MAXSIZE ,
    KEY_LOGGER_TIMESTP ,
    KEY_LOGGER_T_START ,
    KEY_LOGGER_T_STOP ,
    KEY_LOGGER_ENDOFKEY
} ;

extern char const *enum_str_key_section_logger[] ;

typedef enum enum_key_section_environ_e enum_key_section_environ_t, *enum_key_section_environ_e_ref ;
enum enum_key_section_environ_e
{
    KEY_ENVIRON_ENVAL = 0 ,
    KEY_ENVIRON_ENDOFKEY
} ;

extern char const *enum_str_key_section_environ[] ;

typedef enum enum_key_section_regex_e enum_key_section_regex_t, *enum_key_section_regex_t_ref ;
enum enum_key_section_regex_e
{
    KEY_REGEX_CONFIGURE = 0 ,
    KEY_REGEX_DIRECTORIES ,
    KEY_REGEX_FILES ,
    KEY_REGEX_INFILES ,
    KEY_REGEX_ENDOFKEY
} ;

extern char const *enum_str_key_section_regex[] ;

typedef enum enum_io_type_e enum_io_type_t, *enum_io_type_t_ref ;
enum enum_io_type_e
{
    IO_TYPE_TTY = 0,
    IO_TYPE_FILE,
    IO_TYPE_CONSOLE,
    IO_TYPE_S6LOG,
    IO_TYPE_SYSLOG,
    IO_TYPE_INHERIT,
    IO_TYPE_NULL,
    IO_TYPE_PARENT,
    IO_TYPE_CLOSE,
    IO_TYPE_NOTSET,
    IO_TYPE_ENDOFKEY
} ;

extern char const *enum_io_type[] ;

typedef enum enum_type_e enum_type_t, *enum_type_t_ref ;
enum enum_type_e
{
    TYPE_CLASSIC = 0 ,
    TYPE_ONESHOT ,
    TYPE_MODULE ,
    TYPE_ENDOFKEY

} ;

extern char const *enum_str_type[] ;

typedef enum enum_opts_e enum_opts_t, *enum_opts_t_ref ;
enum enum_opts_e
{
    OPTS_LOGGER = 0 ,
    OPTS_ENDOFKEY
} ;

extern char const *enum_str_opts[] ;

typedef enum enum_flags_e enum_flags_t, *enum_flags_t_ref ;
enum enum_flags_e
{
    FLAGS_DOWN = 0 ,
    FLAGS_EARLIER,
    FLAGS_ENDOFKEY
} ;

extern char const *enum_str_flags[] ;

typedef enum enum_build_e enum_build_t, *enum_build_t_ref ;
enum enum_build_e
{
    BUILD_AUTO = 0 ,
    BUILD_CUSTOM ,
    BUILD_ENDOFKEY
} ;

extern char const *enum_str_build[] ;

typedef enum enum_time_e enum_time_t, *enum_time_t_ref ;
enum enum_time_e
{
    TIME_TAI = 0 ,
    TIME_ISO ,
    TIME_NONE ,
    TIME_ENDOFKEY
} ;

extern char const *enum_str_time[] ;

typedef enum enum_seed_e enum_seed_t, *enum_seed_t_ref ;
enum enum_seed_e
{

    SEED_DEPENDS = 0 ,
    SEED_REQUIREDBY ,
    SEED_ENABLE ,
    SEED_ALLOW ,
    SEED_DENY ,
    SEED_CURRENT ,
    SEED_GROUPS ,
    SEED_CONTENTS ,
    SEED_ENDOFKEY

} ;

extern char const *enum_str_seed[] ;

typedef enum enum_expected_e enum_expected_t, *enum_expected_t_ref ;
enum enum_expected_e
{
    EXPECT_LINE = 0 ,
    EXPECT_BRACKET ,
    EXPECT_UINT ,
    EXPECT_SLASH ,
    EXPECT_QUOTE ,
    EXPECT_KEYVAL ,
    EXPECT_ENDOFKEY
} ;

extern char const *enum_str_expected[] ;

typedef struct key_description_s key_description_t ;
struct key_description_s
{
    char const **name ;
    int const id ;
    int const expected ;
} ;

static key_description_t const list_section[] =
{
    { .name = &enum_str_section[SECTION_MAIN], .id = SECTION_MAIN, .expected = 0 } ,
    { .name = &enum_str_section[SECTION_START], .id = SECTION_START, .expected = 0 } ,
    { .name = &enum_str_section[SECTION_STOP], .id = SECTION_STOP, .expected = 0 } ,
    { .name = &enum_str_section[SECTION_LOG], .id = SECTION_LOG, .expected = 0 } ,
    { .name = &enum_str_section[SECTION_ENV], .id = SECTION_ENV, .expected = 0 } ,
    { .name = &enum_str_section[SECTION_REGEX], .id = SECTION_REGEX, .expected = 0 } ,
    { .name = 0 }
} ;

static key_description_t const list_section_main[] =
{
    { .name = &enum_str_key_section_main[KEY_MAIN_TYPE], .id = KEY_MAIN_TYPE, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_main[KEY_MAIN_VERSION], .id = KEY_MAIN_VERSION, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_main[KEY_MAIN_DESCRIPTION], .id = KEY_MAIN_DESCRIPTION, .expected = EXPECT_QUOTE },
    { .name = &enum_str_key_section_main[KEY_MAIN_DEPENDS], .id = KEY_MAIN_DEPENDS, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_REQUIREDBY], .id = KEY_MAIN_REQUIREDBY, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_OPTSDEPS], .id = KEY_MAIN_OPTSDEPS, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_CONTENTS], .id = KEY_MAIN_CONTENTS, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_OPTIONS], .id = KEY_MAIN_OPTIONS, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_NOTIFY], .id = KEY_MAIN_NOTIFY, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_main[KEY_MAIN_USER], .id = KEY_MAIN_USER, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_T_START], .id = KEY_MAIN_T_START, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_main[KEY_MAIN_T_STOP], .id = KEY_MAIN_T_STOP, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_main[KEY_MAIN_DEATH], .id = KEY_MAIN_DEATH, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_main[KEY_MAIN_COPYFROM], .id = KEY_MAIN_COPYFROM, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_SIGNAL], .id = KEY_MAIN_SIGNAL, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_main[KEY_MAIN_FLAGS], .id = KEY_MAIN_FLAGS, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_INTREE], .id = KEY_MAIN_INTREE, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_main[KEY_MAIN_STDIN], .id = KEY_MAIN_STDIN, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_main[KEY_MAIN_STDOUT], .id = KEY_MAIN_STDOUT, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_main[KEY_MAIN_STDERR], .id = KEY_MAIN_STDERR, .expected = EXPECT_LINE },
    { .name = 0 }
} ;

static key_description_t const list_section_startstop[] =
{
    { .name = &enum_str_key_section_startstop[KEY_STARTSTOP_BUILD], .id = KEY_STARTSTOP_BUILD, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_startstop[KEY_STARTSTOP_RUNAS], .id = KEY_STARTSTOP_RUNAS, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_startstop[KEY_STARTSTOP_EXEC], .id = KEY_STARTSTOP_EXEC, .expected = EXPECT_BRACKET },
    { .name = 0 }
} ;

static key_description_t const list_section_logger[] =
{
    { .name = &enum_str_key_section_logger[KEY_LOGGER_BUILD], .id = KEY_LOGGER_BUILD, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_RUNAS], .id = KEY_LOGGER_RUNAS, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_EXEC], .id = KEY_LOGGER_EXEC, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_DESTINATION], .id = KEY_LOGGER_DESTINATION, .expected = EXPECT_SLASH },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_BACKUP], .id = KEY_LOGGER_BACKUP, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_MAXSIZE], .id = KEY_LOGGER_MAXSIZE, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_TIMESTP], .id = KEY_LOGGER_TIMESTP, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_T_START], .id = KEY_LOGGER_T_START, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_T_STOP], .id = KEY_LOGGER_T_STOP, .expected = EXPECT_UINT },
    { .name = 0 }
} ;

static key_description_t const list_section_environment[] =
{
    { .name = &enum_str_key_section_environ[KEY_ENVIRON_ENVAL], .id = KEY_ENVIRON_ENVAL, .expected = EXPECT_KEYVAL },
    { .name = 0 }
} ;

static key_description_t const list_section_regex[] =
{
    { .name = &enum_str_key_section_regex[KEY_REGEX_CONFIGURE], .id = KEY_REGEX_CONFIGURE, .expected = EXPECT_QUOTE },
    { .name = &enum_str_key_section_regex[KEY_REGEX_DIRECTORIES], .id = KEY_REGEX_DIRECTORIES, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_regex[KEY_REGEX_FILES], .id = KEY_REGEX_FILES, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_regex[KEY_REGEX_INFILES], .id = KEY_REGEX_INFILES, .expected = EXPECT_BRACKET },
    { .name = 0 }
} ;

static key_description_t const list_io_type[] =
{
    { .name = &enum_io_type[IO_TYPE_TTY], .id = IO_TYPE_TTY, .expected = EXPECT_LINE },
    { .name = &enum_io_type[IO_TYPE_FILE], .id = IO_TYPE_FILE, .expected = EXPECT_LINE },
    { .name = &enum_io_type[IO_TYPE_CONSOLE], .id = IO_TYPE_CONSOLE, .expected = EXPECT_LINE },
    { .name = &enum_io_type[IO_TYPE_S6LOG], .id = IO_TYPE_S6LOG, .expected = EXPECT_LINE },
    { .name = &enum_io_type[IO_TYPE_SYSLOG], .id = IO_TYPE_SYSLOG, .expected = EXPECT_LINE },
    { .name = &enum_io_type[IO_TYPE_INHERIT], .id = IO_TYPE_INHERIT, .expected = EXPECT_LINE },
    { .name = &enum_io_type[IO_TYPE_NULL], .id = IO_TYPE_NULL, .expected = EXPECT_LINE },
    { .name = &enum_io_type[IO_TYPE_PARENT], .id = IO_TYPE_PARENT, .expected = EXPECT_LINE },
    { .name = &enum_io_type[IO_TYPE_CLOSE], .id = IO_TYPE_CLOSE, .expected = EXPECT_LINE },
    { .name = &enum_io_type[IO_TYPE_NOTSET], .id = IO_TYPE_NOTSET, .expected = EXPECT_LINE },
    { .name = 0 }
} ;

static key_description_t const list_type[] =
{
    { .name = &enum_str_type[TYPE_CLASSIC], .id = TYPE_CLASSIC, .expected = EXPECT_LINE } ,
    { .name = &enum_str_type[TYPE_ONESHOT], .id = TYPE_ONESHOT, .expected = EXPECT_LINE } ,
    { .name = &enum_str_type[TYPE_MODULE], .id = TYPE_MODULE, .expected = EXPECT_LINE } ,
    { .name = 0 }
} ;

static key_description_t const list_opts[] =
{
    { .name = &enum_str_opts[OPTS_LOGGER], .id = OPTS_LOGGER, .expected = EXPECT_BRACKET } ,
    { .name = 0 }
} ;

static key_description_t const list_flags[] =
{
    { .name = &enum_str_flags[FLAGS_DOWN], .id = FLAGS_DOWN, .expected = EXPECT_BRACKET } ,
    { .name = &enum_str_flags[FLAGS_EARLIER], .id = FLAGS_EARLIER, .expected = EXPECT_BRACKET } ,
    { .name = 0 }
} ;

static key_description_t const list_build[] =
{
    { .name = &enum_str_build[BUILD_AUTO], .id = BUILD_AUTO, .expected = EXPECT_LINE } ,
    { .name = &enum_str_build[BUILD_CUSTOM], .id = BUILD_CUSTOM, .expected = EXPECT_LINE } ,
    { .name = 0 }
} ;

static key_description_t const list_timestamp[] =
{
    { .name = &enum_str_time[TIME_TAI], .id = TIME_TAI, .expected = EXPECT_LINE } ,
    { .name = &enum_str_time[TIME_ISO], .id = TIME_ISO, .expected = EXPECT_LINE } ,
    { .name = &enum_str_time[TIME_NONE], .id = TIME_NONE, .expected = EXPECT_LINE } ,
    { .name = 0 }
} ;

static key_description_t const list_seed[] =
{
    { .name = &enum_str_seed[SEED_DEPENDS], .id = SEED_DEPENDS, .expected = EXPECT_LINE } ,
    { .name = &enum_str_seed[SEED_REQUIREDBY], .id = SEED_REQUIREDBY, .expected = EXPECT_LINE } ,
    { .name = &enum_str_seed[SEED_ENABLE], .id = SEED_ENABLE, .expected = EXPECT_LINE } ,
    { .name = &enum_str_seed[SEED_ALLOW], .id = SEED_ALLOW, .expected = EXPECT_LINE } ,
    { .name = &enum_str_seed[SEED_DENY], .id = SEED_DENY, .expected = EXPECT_LINE } ,
    { .name = &enum_str_seed[SEED_CURRENT], .id = SEED_CURRENT, .expected = EXPECT_LINE } ,
    { .name = &enum_str_seed[SEED_GROUPS], .id = SEED_GROUPS, .expected = EXPECT_LINE } ,
    { .name = &enum_str_seed[SEED_CONTENTS], .id = SEED_CONTENTS, .expected = EXPECT_LINE } ,
    { .name = 0 }
} ;

typedef enum enum_mandatory_e enum_mandatory_t, *enum_mandatory_t_ref ;
enum enum_mandatory_e
{
    MANDATORY_NEED = 0 ,
    MANDATORY_OPTS ,
    MANDATORY_CUSTOM ,
    MANDATORY_ENDOFKEY
} ;

extern char const *enum_str_mandatory[]  ;

typedef enum actions_e actions_t, *actions_t_ref ;
enum actions_e
{
    ACTION_COMMON = 0 ,
    ACTION_EXECRUN ,
    ACTION_EXECFINISH ,
    ACTION_EXECLOG ,
    ACTION_EXECUP ,
    ACTION_EXECDOWN,
    ACTION_ENVIRON,
    ACTION_REGEX ,
    ACTION_SKIP
} ;

extern ssize_t get_enum_by_key(key_description_t const *list, char const *key) ;
extern char const *get_key_by_enum(key_description_t const *list, int const key) ;
extern const char **get_enum_str(const int sid) ;
extern const key_description_t *get_enum_list(const int sid) ;

#endif
