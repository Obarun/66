/* 
 * get_enum.c
 * 
 * Copyright (c) 2018-2019 Eric Vidal <eric@obarun.org>
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

#include <sys/types.h>
#include <oblibs/string.h>

char const *get_keybyid(key_enum_t key)
{
			//Section
	return	(key == MAIN) ? "main" :
			(key == START) ? "start" :
			(key == STOP) ? "stop" :
			(key == LOG) ? "logger" :
			(key == ENV) ? "environment" :
			//Main
			(key == TYPE ) ? "@type" :
			(key == NAME ) ? "@name" :
			(key == DESCRIPTION ) ? "@description" :
			(key == CONTENTS ) ? "@contents" :
			(key == DEPENDS ) ? "@depends" :
			(key == OPTIONS ) ? "@options" :
			(key == NOTIFY ) ? "@notify" :
			(key == USER ) ? "@user" :
			(key == BUILD ) ? "@build" :
			(key == SIGNAL) ? "@down-signal" :
			(key == FLAGS ) ? "@flags" :
			(key == RUNAS ) ? "@runas" :
			(key == SHEBANG ) ? "@shebang" :
			(key == T_FINISH ) ? "@timeout-finish" :
			(key == T_KILL ) ? "@timeout-kill" :
			(key == T_UP ) ? "@timeout-up" :
			(key == T_DOWN ) ? "@timeout-down" :
			(key == DEATH) ? "@maxdeath" :
			(key == EXEC ) ? "@execute" :
			(key == DESTINATION ) ? "@destination" :
			(key == BACKUP ) ? "@backup" :
			(key == MAXSIZE ) ? "@maxsize" :
			(key == TIMESTP ) ? "@timestamp" :
			//Service type	
			(key == CLASSIC ) ? "classic" :
			(key == BUNDLE ) ? "bundle" :
			(key == LONGRUN ) ? "longrun" :
			(key == ONESHOT ) ? "oneshot" :
			//Key expected
			(key == LINE ) ? "line" :
			(key == BRACKET ) ? "bracket" :
			(key == UINT ) ? "uint" :
			(key == SLASH ) ? "slash" :
			(key == QUOTE ) ? "quote" :
			//Options
			(key == LOGGER ) ? "log" :
			(key == ENVIR ) ? "env" :
			(key == DATA ) ? "data" :
			(key == PIPELINE ) ? "pipeline" :
			//Flags
			(key == DOWN ) ? "down" :
			(key == NOSETSID ) ? "nosetsid" :
			//Build
			(key == AUTO ) ? "auto" :
			(key == CUSTOM ) ? "custom" :
			//Mandatory
			(key == NEED ) ? "need" :
			(key == OPTS ) ? "opts" :
			//Time
			(key == TAI ) ? "tai" :
			(key == ISO ) ? "iso" :
			//logger
			(key == PRODUCER ) ? "producer-for" :
			(key == CONSUMER ) ? "consumer-for" :
			"unknown" ;
}

ssize_t get_enumbyid(char const *str, key_enum_t key_el)
{
	key_enum_t i  = 0 ;
	
	for (;i<key_el;i++)
		if(obstr_equal(str,get_keybyid(i)))	return i ;
		
	return -1 ;
}


int const key_enum_el = ENDOFKEY - MAIN ;	
int const key_enum_section_el = TYPE - MAIN ;

int const key_enum_main_el = CLASSIC - TYPE ;
int const key_enum_svtype_el = LINE - CLASSIC ;
int const key_enum_expect_el = LOGGER - LINE ;
int const key_enum_options_el = DOWN - LOGGER ;
int const key_enum_flags_el = AUTO - DOWN ;
int const key_enum_build_el = NEED - AUTO ;
int const key_enum_mandatory_el = TAI - NEED ;
int const key_enum_time_el = ENDOFKEY - TAI ;

