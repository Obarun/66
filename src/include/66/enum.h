/* 
 * enum.h
 * 
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
 * 
 * All rights reserved.
 * 
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */
 
#ifndef ENUM_H
#define ENUM_H


#include <sys/types.h>

typedef enum key_enum_e key_enum_t, *key_enum_t_ref ;
enum key_enum_e
{
	//Section
	MAIN = 0 ,
	START ,
	STOP ,
	LOG ,
	ENV ,
	//Main
	TYPE ,// = 5
	NAME ,
	DESCRIPTION ,
	CONTENTS ,
	DEPENDS ,
	OPTIONS ,
	NOTIFY ,
	USER ,
	PRODUCER ,
	CONSUMER ,
	BUILD ,
	SIGNAL ,
	FLAGS ,
	RUNAS ,
	SHEBANG ,
	T_KILL ,
	T_FINISH ,
	T_UP ,
	T_DOWN ,
	DEATH ,
	EXEC ,
	DESTINATION ,
	BACKUP ,
	MAXSIZE ,
	TIMESTP ,
	ENVAL ,
	//Service type
	CLASSIC, // = 31
	BUNDLE ,
	LONGRUN ,
	ONESHOT ,
	//Key expected
	LINE , // = 35
	BRACKET ,
	UINT ,
	SLASH ,
	QUOTE ,
	KEYVAL ,
	//Options
	LOGGER , // = 36
	PIPELINE ,
	DATA ,
	//Flags
	DOWN , // = 44
	NOSETSID ,
	ENVIR ,
	//Build
	AUTO , // = 45
	CUSTOM ,
	//Mandatory
	NEED , // = 49
	OPTS ,
	//Time
	TAI , // = 50
	ISO ,
	ENDOFKEY
} ;
#define PIPELINE_NAME "pipeline-name"

extern int const key_enum_el ;
extern int const key_enum_section_el ;

extern int const key_enum_main_el ;
extern int const key_enum_svtype_el ;
extern int const key_enum_expect_el ;
extern int const key_enum_options_el ;
extern int const key_enum_flags_el ;
extern int const key_enum_build_el ;
extern int const key_enum_mandatory_el ;
extern int const key_enum_time_el ;

typedef struct key_description_s key_description_t ;
struct key_description_s
{
	char const *name ;
	key_enum_t const expected ;
	key_enum_t const mandatory ; 
} ;

typedef struct key_all_s key_all_t ;
struct key_all_s
{
	key_description_t const *list ;
} ;

static key_description_t const main_section_list[] =
{
	{ .name = "@type",  .expected = LINE, .mandatory = NEED },
	{ .name = "@name", .expected = LINE, .mandatory = NEED },
	{ .name = "@description", .expected = QUOTE, .mandatory = OPTS },
	{ .name = "@depends", .expected = BRACKET, .mandatory = OPTS },
	{ .name = "@contents", .expected = BRACKET, .mandatory = BUNDLE },
	{ .name = "@options", .expected = BRACKET, .mandatory = OPTS },
	{ .name = "@flags", .expected = BRACKET, .mandatory = OPTS },
	{ .name = "@notify", .expected = UINT, .mandatory = OPTS },
	{ .name = "@user", .expected = BRACKET, .mandatory = NEED },
	{ .name = "@timeout-finish", .expected = UINT, .mandatory = OPTS },
	{ .name = "@timeout-kill", .expected = UINT, .mandatory = OPTS },
	{ .name = "@timeout-up", .expected = UINT, .mandatory = OPTS },
	{ .name = "@timeout-down", .expected = UINT, .mandatory = OPTS },
	{ .name = "@maxdeath", .expected = UINT, .mandatory = OPTS },
	{ .name = "@down-signal", .expected = UINT, .mandatory = OPTS },
	{ .name = 0 } 
} ;

static key_description_t const startstop_section_list[] =
{
	{ .name = "@build", .expected = LINE, .mandatory = NEED },
	{ .name = "@runas", .expected = LINE, .mandatory = OPTS },
	{ .name = "@shebang", .expected = QUOTE, .mandatory = CUSTOM },
	{ .name = "@execute", .expected = BRACKET, .mandatory = NEED },
	{ .name = 0 }
} ;

static key_description_t const logger_section_list[] =
{
	{ .name = "@destination", .expected = SLASH, .mandatory = CUSTOM },
	{ .name = "@build",  .expected = LINE, .mandatory = NEED },
	{ .name = "@runas", .expected = LINE, .mandatory = OPTS },
	{ .name = "@shebang", .expected = QUOTE, .mandatory = CUSTOM },
	{ .name = "@timeout-finish", .expected = UINT, .mandatory = OPTS },
	{ .name = "@timeout-kill", .expected = UINT, .mandatory = OPTS },
	{ .name = "@backup", .expected = UINT, .mandatory = OPTS },
	{ .name = "@maxsize", .expected = UINT, .mandatory = OPTS },
	{ .name = "@timestamp", .expected = LINE, .mandatory = OPTS },
	{ .name = "@execute", .expected = BRACKET, .mandatory = CUSTOM },
	{ .name = 0 }
} ;

static key_description_t const environment_section_list[] =
{
	{ .name = "environment", .expected = KEYVAL, .mandatory = OPTS },
	{ .name = 0 }
} ;

static int const total_list_el[6] = { 16, 5, 5, 11, 2, 0 } ;

static key_all_t const total_list[] =
{
	{ .list = main_section_list },
	{ .list = startstop_section_list },
	{ .list = startstop_section_list },
	{ .list = logger_section_list },
	{ .list = environment_section_list },
	{ .list = 0 }
} ;

/** Compare @str with function @func on @key_el enum table
 * @Return the index of the table
 * @Return -1 on fail*/
extern ssize_t get_enumbyid(char const *str,key_enum_t key_el) ;

/**@Return the string of the @key */ 
extern char const *get_keybyid(key_enum_t key) ;

#endif
