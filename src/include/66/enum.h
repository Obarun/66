/*
 * enum.h
 *
 * Copyright (c) 2018-2021 Eric Vidal <eric@obarun.org>
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

#define ENUM_START 0

typedef enum enum_main_e enum_main_t, *enum_main_t_ref ;
enum enum_main_e
{
    ENUM_SECTION = 0 ,
    ENUM_KEY_SECTION_MAIN ,
    ENUM_KEY_SECTION_STARTSTOP ,
    ENUM_KEY_SECTION_LOGGER ,
    ENUM_KEY_SECTION_ENVIRON ,
    ENUM_KEY_SECTION_REGEX ,
    ENUM_TYPE ,
    ENUM_EXPECTED ,
    ENUM_OPTS ,
    ENUM_FLAGS ,
    ENUM_BUILD ,
    ENUM_MANDATORY ,
    ENUM_TIME ,
    ENUM_LOGOPTS ,
    ENUM_SEED ,
    ENUM_ENDOFKEY
} ;

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
    KEY_MAIN_CONTENTS ,
    KEY_MAIN_DEPENDS ,
    KEY_MAIN_OPTSDEPS ,
    KEY_MAIN_EXTDEPS ,
    KEY_MAIN_OPTIONS ,
    KEY_MAIN_NOTIFY ,
    KEY_MAIN_USER ,
    KEY_MAIN_T_FINISH ,
    KEY_MAIN_T_KILL ,
    KEY_MAIN_T_UP ,
    KEY_MAIN_T_DOWN ,
    KEY_MAIN_DEATH ,
    KEY_MAIN_HIERCOPY ,
    KEY_MAIN_SIGNAL ,
    KEY_MAIN_FLAGS ,
    KEY_MAIN_INTREE ,
    KEY_MAIN_ENDOFKEY
} ;

extern char const *enum_str_key_section_main[] ;

typedef enum enum_key_section_startstop_e enum_key_section_startstop_t, *enum_key_section_startstop_t_ref ;
enum enum_key_section_startstop_e
{
    KEY_STARTSTOP_BUILD = 0 ,
    KEY_STARTSTOP_RUNAS ,
    KEY_STARTSTOP_SHEBANG ,
    KEY_STARTSTOP_EXEC ,
    KEY_STARTSTOP_ENDOFKEY
} ;

extern char const *enum_str_key_section_startstop[] ;

typedef enum enum_key_section_logger_e enum_key_section_logger_t, *enum_key_section_logger_t_ref ;
enum enum_key_section_logger_e
{
    KEY_LOGGER_BUILD = 0 ,
    KEY_LOGGER_RUNAS ,
    KEY_LOGGER_SHEBANG ,
    KEY_LOGGER_EXEC ,
    KEY_LOGGER_DESTINATION ,
    KEY_LOGGER_BACKUP ,
    KEY_LOGGER_MAXSIZE ,
    KEY_LOGGER_TIMESTP ,
    KEY_LOGGER_T_FINISH ,
    KEY_LOGGER_T_KILL ,
    KEY_LOGGER_DEPENDS ,
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
    KEY_REGEX_ADDSERVICES ,
    KEY_REGEX_ENDOFKEY
} ;

extern char const *enum_str_key_section_regex[] ;

typedef enum enum_type_e enum_type_t, *enum_type_t_ref ;
enum enum_type_e
{
    TYPE_CLASSIC = 0 ,
    TYPE_BUNDLE ,
    TYPE_LONGRUN ,
    TYPE_ONESHOT ,
    TYPE_MODULE ,
    TYPE_ENDOFKEY

} ;

extern char const *enum_str_type[] ;

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

typedef enum enum_opts_e enum_opts_t, *enum_opts_t_ref ;
enum enum_opts_e
{
    OPTS_LOGGER = 0 ,
    OPTS_ENVIR ,
    OPTS_HIERCOPY ,
    OPTS_PIPELINE ,
    OPTS_ENDOFKEY
} ;

extern char const *enum_str_opts[] ;

typedef enum enum_flags_e enum_flags_t, *enum_flags_t_ref ;
enum enum_flags_e
{
    FLAGS_DOWN = 0 ,
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

typedef enum enum_mandatory_e enum_mandatory_t, *enum_mandatory_t_ref ;
enum enum_mandatory_e
{
    MANDATORY_NEED = 0 ,
    MANDATORY_OPTS ,
    MANDATORY_BUNDLE ,
    MANDATORY_CUSTOM ,
    MANDATORY_ENDOFKEY
} ;

extern char const *enum_str_mandatory[]  ;

typedef enum enum_time_e enum_time_t, *enum_time_t_ref ;
enum enum_time_e
{
    TIME_TAI = 0 ,
    TIME_ISO ,
    TIME_NONE ,
    TIME_ENDOFKEY
} ;

extern char const *enum_str_time[] ;

typedef enum enum_logopts_e enum_logotps_t, *enum_logopts_t_ref ;
enum enum_logopts_e
{
    LOGOPTS_PRODUCER = 0 ,
    LOGOPTS_CONSUMER ,
    LOGOPTS_PIPE ,
    LOGOPTS_ENDOFKEY
} ;

extern char const *enum_str_logopts[] ;

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


typedef struct enum_all_enum_s enum_all_enum_t, *enum_all_enum_t_ref ;
struct enum_all_enum_s
{
    unsigned int const enum_all ;
    char const **str ;
} ;



extern ssize_t get_enum_by_key_one(char const *str, int const e) ;
extern ssize_t get_enum_by_key(char const *str) ;
extern char const *get_key_by_enum(int const e, int const key) ;
extern enum_all_enum_t enum_all[] ;

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

//extern unsigned char const actions[SECTION_ENDOFKEY][TYPE_ENDOFKEY] ;
//extern unsigned char const states[SECTION_ENDOFKEY][TYPE_ENDOFKEY] ;

typedef struct key_description_s key_description_t ;
struct key_description_s
{
    char const **name ;
    int const id ;
    int const expected ;
} ;

typedef struct key_all_s key_all_t ;
struct key_all_s
{
    key_description_t const *list ;
} ;

static key_description_t const main_section_list[] =
{
    { .name = &enum_str_key_section_main[KEY_MAIN_TYPE], .id = KEY_MAIN_TYPE, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_main[KEY_MAIN_VERSION], .id = KEY_MAIN_VERSION, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_main[KEY_MAIN_DESCRIPTION], .id = KEY_MAIN_DESCRIPTION, .expected = EXPECT_QUOTE },
    { .name = &enum_str_key_section_main[KEY_MAIN_CONTENTS], .id = KEY_MAIN_CONTENTS, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_DEPENDS], .id = KEY_MAIN_DEPENDS, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_OPTSDEPS], .id = KEY_MAIN_OPTSDEPS, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_EXTDEPS], .id = KEY_MAIN_EXTDEPS, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_OPTIONS], .id = KEY_MAIN_OPTIONS, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_NOTIFY], .id = KEY_MAIN_NOTIFY, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_main[KEY_MAIN_USER], .id = KEY_MAIN_USER, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_T_FINISH], .id = KEY_MAIN_T_FINISH, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_main[KEY_MAIN_T_KILL], .id = KEY_MAIN_T_KILL, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_main[KEY_MAIN_T_UP], .id = KEY_MAIN_T_UP, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_main[KEY_MAIN_T_DOWN], .id = KEY_MAIN_T_DOWN, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_main[KEY_MAIN_DEATH], .id = KEY_MAIN_DEATH, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_main[KEY_MAIN_HIERCOPY], .id = KEY_MAIN_HIERCOPY, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_SIGNAL], .id = KEY_MAIN_SIGNAL, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_main[KEY_MAIN_FLAGS], .id = KEY_MAIN_FLAGS, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_main[KEY_MAIN_INTREE], .id = KEY_MAIN_INTREE, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_main[KEY_MAIN_ENDOFKEY] }
} ;

static key_description_t const startstop_section_list[] =
{
    { .name = &enum_str_key_section_startstop[KEY_STARTSTOP_BUILD], .id = KEY_STARTSTOP_BUILD, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_startstop[KEY_STARTSTOP_RUNAS], .id = KEY_STARTSTOP_RUNAS, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_startstop[KEY_STARTSTOP_SHEBANG], .id = KEY_STARTSTOP_SHEBANG, .expected = EXPECT_QUOTE },
    { .name = &enum_str_key_section_startstop[KEY_STARTSTOP_EXEC], .id = KEY_STARTSTOP_EXEC, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_startstop[KEY_STARTSTOP_ENDOFKEY] }
} ;

static key_description_t const logger_section_list[] =
{
    { .name = &enum_str_key_section_logger[KEY_LOGGER_DESTINATION], .id = KEY_LOGGER_DESTINATION, .expected = EXPECT_SLASH },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_BUILD], .id = KEY_LOGGER_BUILD, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_RUNAS], .id = KEY_LOGGER_RUNAS, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_DEPENDS], .id = KEY_LOGGER_DEPENDS, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_SHEBANG], .id = KEY_LOGGER_SHEBANG, .expected = EXPECT_QUOTE },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_T_FINISH], .id = KEY_LOGGER_T_FINISH, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_T_KILL], .id = KEY_LOGGER_T_KILL, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_BACKUP], .id = KEY_LOGGER_BACKUP, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_MAXSIZE], .id = KEY_LOGGER_MAXSIZE, .expected = EXPECT_UINT },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_TIMESTP], .id = KEY_LOGGER_TIMESTP, .expected = EXPECT_LINE },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_EXEC], .id = KEY_LOGGER_EXEC, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_logger[KEY_LOGGER_ENDOFKEY] }
} ;

static key_description_t const environment_section_list[] =
{
    { .name = &enum_str_key_section_environ[KEY_ENVIRON_ENVAL], .id = KEY_ENVIRON_ENVAL, .expected = EXPECT_KEYVAL },
    { .name = &enum_str_key_section_environ[KEY_ENVIRON_ENDOFKEY] }
} ;

static key_description_t const regex_section_list[] =
{
    { .name = &enum_str_key_section_regex[KEY_REGEX_CONFIGURE], .id = KEY_REGEX_CONFIGURE, .expected = EXPECT_QUOTE },
    { .name = &enum_str_key_section_regex[KEY_REGEX_DIRECTORIES], .id = KEY_REGEX_DIRECTORIES, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_regex[KEY_REGEX_FILES], .id = KEY_REGEX_FILES, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_regex[KEY_REGEX_INFILES], .id = KEY_REGEX_INFILES, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_regex[KEY_REGEX_ADDSERVICES], .id = KEY_REGEX_ADDSERVICES, .expected = EXPECT_BRACKET },
    { .name = &enum_str_key_section_regex[KEY_REGEX_ENDOFKEY] }
} ;

static int const total_list_el[7] = { \
    KEY_MAIN_ENDOFKEY + 1, \
    KEY_STARTSTOP_ENDOFKEY + 1, \
    KEY_STARTSTOP_ENDOFKEY + 1, \
    KEY_LOGGER_ENDOFKEY + 1, \
    KEY_ENVIRON_ENDOFKEY + 1, \
    KEY_REGEX_ENDOFKEY + 1, \
    0 } ;

static key_all_t const total_list[] =
{
    { .list = main_section_list },
    { .list = startstop_section_list },
    { .list = startstop_section_list },
    { .list = logger_section_list },
    { .list = environment_section_list },
    { .list = regex_section_list },
    { .list = 0 }
} ;


#endif
