/* 
 * enum.h
 * 
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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

#include <sys/types.h>
#define ENUM_START 0

typedef enum enum_main_e enum_main_t, *enum_main_t_ref ;
enum enum_main_e
{
	ENUM_SECTION = 0 ,
	ENUM_KEY ,
	ENUM_TYPE ,
	ENUM_EXPECTED ,
	ENUM_OPTS ,
	ENUM_FLAGS ,
	ENUM_BUILD ,
	ENUM_MANDATORY ,
	ENUM_TIME ,
	ENUM_LOGOPTS ,
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
	SECTION_ENDOFKEY
} ;

extern char const *enum_str_section[] ;

typedef enum enum_key_e enum_key_t, *enum_key_t_ref ;
enum enum_key_e
{
	KEY_TYPE = 0 ,
	KEY_NAME ,
	KEY_DESCRIPTION ,
	KEY_CONTENTS ,
	KEY_DEPENDS ,
	KEY_OPTSDEPS ,
	KEY_EXTDEPS ,
	KEY_OPTIONS ,
	KEY_NOTIFY ,
	KEY_USER ,
	KEY_BUILD ,
	KEY_SIGNAL ,
	KEY_FLAGS ,
	KEY_RUNAS ,
	KEY_SHEBANG ,
	KEY_T_FINISH ,
	KEY_T_KILL ,
	KEY_T_UP ,
	KEY_T_DOWN ,
	KEY_DEATH ,
	KEY_HIERCOPY ,
	KEY_EXEC ,
	KEY_DESTINATION ,
	KEY_BACKUP ,
	KEY_MAXSIZE ,
	KEY_TIMESTP ,
	KEY_ENVAL ,
	KEY_ENDOFKEY
} ;

extern char const *enum_str_key[] ;

typedef enum enum_type_e enum_type_t, *enum_type_t_ref ;
enum enum_type_e
{
	TYPE_MODULES = 0 ,
	TYPE_CLASSIC ,
	TYPE_BUNDLE ,
	TYPE_LONGRUN ,
	TYPE_ONESHOT ,
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
	FLAGS_NOSETSID ,
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


typedef struct enum_all_enum_s enum_all_enum_t, *enum_all_enum_t_ref ;
struct enum_all_enum_s
{
	int const enum_all ;
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
	ACTION_SKIP
} ;

unsigned char const actions[SECTION_ENDOFKEY][TYPE_ENDOFKEY] ;
unsigned char const states[SECTION_ENDOFKEY][TYPE_ENDOFKEY] ;

typedef struct key_description_s key_description_t ;
struct key_description_s
{
	char const **name ;
	int const id ;
	int const expected ;
	int const mandatory ; 
} ;

typedef struct key_all_s key_all_t ;
struct key_all_s
{
	key_description_t const *list ;
} ;

static key_description_t const main_section_list[] =
{
	{ .name = &enum_str_key[KEY_TYPE], .id = KEY_TYPE, .expected = EXPECT_LINE, .mandatory = MANDATORY_NEED },
	{ .name = &enum_str_key[KEY_NAME], .id = KEY_NAME, .expected = EXPECT_LINE, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_DESCRIPTION], .id = KEY_DESCRIPTION, .expected = EXPECT_QUOTE, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_DEPENDS], .id = KEY_DEPENDS, .expected = EXPECT_BRACKET, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_OPTSDEPS], .id = KEY_OPTSDEPS, .expected = EXPECT_BRACKET, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_EXTDEPS], .id = KEY_EXTDEPS, .expected = EXPECT_BRACKET, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_CONTENTS], .id = KEY_CONTENTS, .expected = EXPECT_BRACKET, .mandatory = MANDATORY_BUNDLE },
	{ .name = &enum_str_key[KEY_OPTIONS], .id = KEY_OPTIONS, .expected = EXPECT_BRACKET, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_FLAGS], .id = KEY_FLAGS, .expected = EXPECT_BRACKET, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_NOTIFY], .id = KEY_NOTIFY, .expected = EXPECT_UINT, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_USER], .id = KEY_USER, .expected = EXPECT_BRACKET, .mandatory = MANDATORY_NEED },
	{ .name = &enum_str_key[KEY_T_FINISH], .id = KEY_T_FINISH, .expected = EXPECT_UINT, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_T_KILL], .id = KEY_T_KILL, .expected = EXPECT_UINT, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_T_UP], .id = KEY_T_UP, .expected = EXPECT_UINT, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_T_DOWN], .id = KEY_T_DOWN, .expected = EXPECT_UINT, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_DEATH], .id = KEY_DEATH, .expected = EXPECT_UINT, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_SIGNAL], .id = KEY_SIGNAL, .expected = EXPECT_UINT, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_HIERCOPY], .id = KEY_HIERCOPY, .expected = EXPECT_BRACKET, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_ENDOFKEY] } 
} ;

static key_description_t const startstop_section_list[] =
{
	{ .name = &enum_str_key[KEY_BUILD], .id = KEY_BUILD, .expected = EXPECT_LINE, .mandatory = MANDATORY_NEED },
	{ .name = &enum_str_key[KEY_RUNAS], .id = KEY_RUNAS, .expected = EXPECT_LINE, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_SHEBANG], .id = KEY_SHEBANG, .expected = EXPECT_QUOTE, .mandatory = MANDATORY_CUSTOM },
	{ .name = &enum_str_key[KEY_EXEC], .id = KEY_EXEC, .expected = EXPECT_BRACKET, .mandatory = MANDATORY_NEED },
	{ .name = &enum_str_key[KEY_ENDOFKEY] }
} ;

static key_description_t const logger_section_list[] =
{
	{ .name = &enum_str_key[KEY_DESTINATION], .id = KEY_DESTINATION, .expected = EXPECT_SLASH, .mandatory = MANDATORY_CUSTOM },
	{ .name = &enum_str_key[KEY_BUILD], .id = KEY_BUILD, .expected = EXPECT_LINE, .mandatory = MANDATORY_NEED },
	{ .name = &enum_str_key[KEY_RUNAS], .id = KEY_RUNAS, .expected = EXPECT_LINE, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_DEPENDS], .id = KEY_DEPENDS, .expected = EXPECT_BRACKET, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_SHEBANG], .id = KEY_SHEBANG, .expected = EXPECT_QUOTE, .mandatory = MANDATORY_CUSTOM },
	{ .name = &enum_str_key[KEY_T_FINISH], .id = KEY_T_FINISH, .expected = EXPECT_UINT, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_T_KILL], .id = KEY_T_KILL, .expected = EXPECT_UINT, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_BACKUP], .id = KEY_BACKUP, .expected = EXPECT_UINT, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_MAXSIZE], .id = KEY_MAXSIZE, .expected = EXPECT_UINT, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_TIMESTP], .id = KEY_TIMESTP, .expected = EXPECT_LINE, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_key[KEY_EXEC], .id = KEY_EXEC, .expected = EXPECT_BRACKET, .mandatory = MANDATORY_CUSTOM },
	{ .name = &enum_str_key[KEY_ENDOFKEY] }
} ;

static key_description_t const environment_section_list[] =
{
	{ .name = &enum_str_section[SECTION_ENV], .id = SECTION_ENV, .expected = EXPECT_KEYVAL, .mandatory = MANDATORY_OPTS },
	{ .name = &enum_str_section[SECTION_ENDOFKEY] }
} ;

static int const total_list_el[6] = { 19, 5, 5, 12, 2, 0 } ;

static key_all_t const total_list[] =
{
	{ .list = main_section_list },
	{ .list = startstop_section_list },
	{ .list = startstop_section_list },
	{ .list = logger_section_list },
	{ .list = environment_section_list },
	{ .list = 0 }
} ;


#endif
