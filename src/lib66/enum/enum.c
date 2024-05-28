/*
 * get_enum.c
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

#include <66/enum.h>

#include <string.h>

#include <oblibs/log.h>

char const *enum_str_section[] = {
    "main" ,
    "start" ,
    "stop" ,
    "logger" ,
    "environment" ,
    "regex" ,
    0
} ;

char const *enum_str_key_section_main[] = {
    "@type" ,
    "@version" ,
    "@description" ,
    "@depends" ,
    "@requiredby",
    "@optsdepends" ,
    "@contents" ,
    "@options" ,
    "@notify" ,
    "@user" ,
    "@timeout-finish" ,
    "@timeout-kill" ,
    "@timeout-up" ,
    "@timeout-down" ,
    "@maxdeath" ,
    "@hiercopy" ,
    "@down-signal" ,
    "@flags" ,
    "@intree" ,
    0
} ;


char const *enum_str_key_section_startstop[] = {
    "@build" ,
    "@runas" ,
    "@shebang" ,
    "@execute" ,
    0
} ;

char const *enum_str_key_section_logger[] = {
    "@build" ,
    "@runas" ,
    "@shebang" ,
    "@execute" ,
    "@destination" ,
    "@backup" ,
    "@maxsize" ,
    "@timestamp" ,
    "@timeout-finish" ,
    "@timeout-kill" ,
    "@depends" ,
    0
} ;

char const *enum_str_key_section_environ[] = {
    "@environ" ,
    0
} ;

char const *enum_str_key_section_regex[] = {
    "@configure" ,
    "@directories" ,
    "@files" ,
    "@infiles" ,
    0
} ;

char const *enum_str_type[] = {
    "classic" ,
    "oneshot" ,
    "module" ,
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
    0
} ;

char const *enum_str_flags[] = {
    "down" ,
    "earlier" ,
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
    "custom" ,
    0
} ;

char const *enum_str_time[] = {
    "tai" ,
    "iso" ,
    "none" ,
    0
} ;

char const *enum_str_seed[] = {

    "depends" ,
    "requiredby" ,
    "enable" ,
    "allow" ,
    "deny" ,
    "current" ,
    "groups" ,
    "contents" ,
    0
} ;

ssize_t get_enum_by_key(key_description_t const *list, char const *key)
{
    int i = 0 ;
    for(; list[i].name ; i++) {
        if(!strcmp(key, *list[i].name))
            return i ;
    }
    return -1 ;
}

char const *get_key_by_enum(key_description_t const *list, int const key)
{
    return *list[key].name ;
}
