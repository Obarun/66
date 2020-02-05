/* 
 * get_enum.c
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

#include <66/enum.h>

#include <stddef.h>
#include <oblibs/string.h>

char const *enum_str_section[] = {
	"main" ,
	"start" ,
	"stop" ,
	"logger" ,
	"environment" ,
	0
} ;

char const *enum_str_key[] = {
	"@type" ,
	"@name" ,
	"@description" ,
	"@contents" ,
	"@depends" ,
	"@optsdepends" ,
	"@extdepends" ,
	"@options" ,
	"@notify" ,
	"@user" ,
	"@build" ,
	"@down-signal" ,
	"@flags" ,
	"@runas" ,
	"@shebang" ,
	"@timeout-finish" ,
	"@timeout-kill" ,
	"@timeout-up" ,
	"@timeout-down" ,
	"@maxdeath" ,
	"@hiercopy" ,
	"@execute" ,
	"@destination" ,
	"@backup" ,
	"@maxsize" ,
	"@timestamp" ,
	"@enval" ,
	0
} ;

char const *enum_str_type[] = {
	"modules" ,
	"classic" ,
	"bundle" ,
	"longrun" ,
	"oneshot" ,
	0
} ;

char const *enum_str_expected[] = {
	"line" ,
	"bracket" ,
	"uint" ,
	"slash" ,
	"quote" ,
	"keyval" ,
	0
} ;

char const *enum_str_opts[] = {
	"log" ,
	"env" ,
	"hiercopy" ,
	"pipeline" ,
	0
} ;

char const *enum_str_flags[] = {
	"down" ,
	"nosetsid" ,
	0
} ;

char const *enum_str_build[] = {
	"auto" ,
	"custom" ,
	0
} ;

char const *enum_str_mandatory[] = {
	"need" ,
	"opts" ,
	"bundle" ,
	"custom" ,
	0
} ;

char const *enum_str_time[] = {
	"tai" ,
	"iso" ,
	"none" ,
	0
} ;

char const *enum_str_logopts[] = {
	"producer-for" ,
	"consumer-for" ,
	"pipeline-name" ,
	0
} ;

enum_all_enum_t enum_all[] = {

	[ENUM_SECTION] = { .enum_all = SECTION_ENDOFKEY - ENUM_START, .str = enum_str_section } ,
	[ENUM_KEY] = { .enum_all = KEY_ENDOFKEY - ENUM_START, .str = enum_str_key } ,
	[ENUM_TYPE] = { .enum_all = TYPE_ENDOFKEY - ENUM_START, .str = enum_str_type } ,
	[ENUM_EXPECTED] = { .enum_all = EXPECT_ENDOFKEY - ENUM_START, .str = enum_str_expected } ,
	[ENUM_OPTS] = { .enum_all = OPTS_ENDOFKEY- ENUM_START , .str = enum_str_opts } ,
	[ENUM_FLAGS] = { .enum_all = FLAGS_ENDOFKEY - ENUM_START , .str = enum_str_flags } ,
	[ENUM_BUILD] = { .enum_all = BUILD_ENDOFKEY - ENUM_START , .str = enum_str_build } ,
	[ENUM_MANDATORY] = { .enum_all = MANDATORY_ENDOFKEY - ENUM_START , .str = enum_str_mandatory } ,
	[ENUM_TIME] = { .enum_all = TIME_ENDOFKEY - ENUM_START , .str = enum_str_time } ,
	[ENUM_LOGOPTS] = { .enum_all = LOGOPTS_ENDOFKEY - ENUM_START , .str = enum_str_logopts } ,
	[ENUM_ENDOFKEY] = { 0 }

} ;

unsigned char const actions[SECTION_ENDOFKEY][TYPE_ENDOFKEY] = {

	// MODULES		>CLASSIC,			BUNDLE,			LONGRUN,			ONESHOT
    { ACTION_SKIP,	ACTION_COMMON,		ACTION_COMMON,	ACTION_COMMON,		ACTION_COMMON }, // main
    { ACTION_SKIP,	ACTION_EXECRUN, 	ACTION_SKIP,	ACTION_EXECRUN, 	ACTION_EXECUP }, // start
    { ACTION_SKIP,	ACTION_EXECFINISH, 	ACTION_SKIP,	ACTION_EXECFINISH,	ACTION_EXECDOWN }, // stop
    { ACTION_SKIP,	ACTION_EXECLOG,		ACTION_SKIP,	ACTION_EXECLOG, 	ACTION_SKIP }, // log
    { ACTION_SKIP,	ACTION_ENVIRON, 	ACTION_SKIP,	ACTION_ENVIRON, 	ACTION_ENVIRON }, // env
} ;

unsigned char const states[SECTION_ENDOFKEY][TYPE_ENDOFKEY] = {
	// MODULES		CLASSIC,		BUNDLE,			LONGRUN,		ONESHOT
    { ACTION_SKIP,	SECTION_START,	ACTION_SKIP,	SECTION_START,	SECTION_START }, // main
    { ACTION_SKIP,	SECTION_STOP,	ACTION_SKIP,	SECTION_STOP,	SECTION_STOP }, // start
    { ACTION_SKIP,	SECTION_LOG,	ACTION_SKIP,	SECTION_LOG,	SECTION_LOG }, // stop
    { ACTION_SKIP,	SECTION_ENV,	ACTION_SKIP,	SECTION_ENV,	SECTION_ENV }, // log
    { ACTION_SKIP,	ACTION_SKIP,	ACTION_SKIP,	ACTION_SKIP,	ACTION_SKIP }, // env
} ;

ssize_t get_enum_by_key_one(char const *str, int const e)
{
	int i = 0 ;
	enum_all_enum_t *key = enum_all ;
	for(; i < key[e].enum_all;i++)
		if(obstr_equal(str,key[e].str[i]))
			return i ;

	return -1 ;
}

ssize_t get_enum_by_key(char const *str)
{
	int i = 0, ret ;
	
	for (;i<ENUM_ENDOFKEY;i++)
	{
		ret = get_enum_by_key_one(str,i) ;
		if (ret >= 0) return ret ;
	}
	return -1 ;
}

char const *get_key_by_enum(int const e, int const key)
{
	return enum_all[e].str[key] ;
}
