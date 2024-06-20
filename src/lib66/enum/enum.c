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
    "Main" ,
    "Start" ,
    "Stop" ,
    "Logger" ,
    "Environment" ,
    "Regex" ,
    0
} ;

char const *enum_str_key_section_main[] = {
    "Type" ,
    "Version" ,
    "Description" ,
    "Depends" ,
    "RequiredBy",
    "OptsDepends" ,
    "Contents" ,
    "Options" ,
    "Notify" ,
    "User" ,
    "TimeoutStart" ,
    "TimeoutStop" ,
    "MaxDeath" ,
    "Hiercopy" ,
    "DownSignal" ,
    "Flags" ,
    "InTree" ,
    0
} ;


char const *enum_str_key_section_startstop[] = {
    "Build" ,
    "RunAs" ,
    "Execute" ,
    0
} ;

char const *enum_str_key_section_logger[] = {
    "Build" ,
    "RunAs" ,
    "Execute" ,
    "Destination" ,
    "Backup" ,
    "Maxsize" ,
    "Timestamp" ,
    "TimeoutStart" ,
    "TimeoutStop" ,
    "Depends" ,
    0
} ;

char const *enum_str_key_section_environ[] = {
    "Environ" ,
    0
} ;

char const *enum_str_key_section_regex[] = {
    "Configure" ,
    "Directories" ,
    "Files" ,
    "InFiles" ,
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

const key_description_t *get_enum_list(const int sid)
{
    switch (sid) {

        case SECTION_MAIN:
            return list_section_main ;

        case SECTION_START:
            return list_section_startstop ;

        case SECTION_STOP:
            return list_section_startstop ;

        case SECTION_LOG:
            return list_section_logger ;

        case SECTION_ENV:
            return list_section_environment ;

        case SECTION_REGEX:
            return list_section_regex ;

        default:
            errno = EINVAL ;
            return 0 ;
    }

}

const char **get_enum_str(const int sid)
{
    switch (sid) {

        case SECTION_MAIN:
            return enum_str_key_section_main ;

        case SECTION_START:
            return enum_str_key_section_startstop ;

        case SECTION_STOP:
            return enum_str_key_section_startstop ;

        case SECTION_LOG:
            return enum_str_key_section_logger ;

        case SECTION_ENV:
            return enum_str_key_section_environ ;

        case SECTION_REGEX:
            return enum_str_key_section_regex ;

        default:
            errno = EINVAL ;
            return 0 ;
    }
}