/*
 * parser_utils.c
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

#include <66/parser.h>

#include <string.h>
#include <unistd.h>//getuid
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>

#include <oblibs/string.h>
#include <oblibs/files.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/mill.h>
#include <oblibs/environ.h>
#include <oblibs/sastr.h>

#include <skalibs/sig.h>
#include <skalibs/genalloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/bytestr.h>
#include <skalibs/diuint32.h>
#include <skalibs/djbunix.h>

#include <66/config.h>
#include <66/constants.h>
#include <66/enum.h>
#include <66/environ.h> //env_clean_with_comment
#include <66/utils.h>//MYUID

stralloc keep = STRALLOC_ZERO ;//sv_alltype data
stralloc deps = STRALLOC_ZERO ;//sv_name depends
genalloc gasv = GENALLOC_ZERO ;//sv_alltype general

/**********************************
 *      function helper declaration
 * *******************************/

void section_setsa(int id, stralloc_ref *p,section_t *sa) ;
int section_get_skip(char const *s,size_t pos,int nline) ;
int section_get_id(stralloc *secname, char const *string,size_t *pos,int *id) ;
int key_get_next_id(stralloc *sa, char const *string,size_t *pos) ;
int get_clean_val(keynocheck *ch) ;
int get_enum(char const *string, keynocheck *ch) ;
int get_timeout(keynocheck *ch,uint32_t *ui) ;
int get_uint(keynocheck *ch,uint32_t *ui) ;
int check_valid_runas(keynocheck *check) ;
void parse_err(int ierr,keynocheck *check) ;
int parse_line(stralloc *sa, size_t *pos) ;
int parse_bracket(stralloc *sa,size_t *pos) ;

/**********************************
 *      freed function
 * *******************************/

void sv_alltype_free(sv_alltype *sv)
{
    log_flow() ;

    stralloc_free(&sv->saenv) ;
    *&sv->saenv = stralloc_zero ;
}

void keynocheck_free(keynocheck *nocheck)
{
    log_flow() ;

    stralloc_free(&nocheck->val) ;
    *nocheck = keynocheck_zero ;
}

void section_free(section_t *sec)
{
    log_flow() ;

    stralloc_free(&sec->main) ;
    stralloc_free(&sec->start) ;
    stralloc_free(&sec->stop) ;
    stralloc_free(&sec->logger) ;
    stralloc_free(&sec->environment) ;
}

void freed_parser(void)
{
    log_flow() ;

    stralloc_free(&keep) ;
    stralloc_free(&deps) ;
    for (unsigned int i = 0 ; i < genalloc_len(sv_alltype,&gasv) ; i++)
        sv_alltype_free(&genalloc_s(sv_alltype,&gasv)[i]) ;
    genalloc_free(sv_alltype,&gasv) ;
}

/**********************************
 *      Mill utilities
 * *******************************/

parse_mill_t MILL_FIRST_BRACKET = \
{ \
    .search = "(", .searchlen = 1, \
    .end = ")", .endlen = 1, \
    .inner.debug = "first_bracket" } ;

parse_mill_t MILL_GET_AROBASE_KEY = \
{ \
    .open = '@', .close = '=', .keepopen = 1, \
    .flush = 1, \
    .forceclose = 1, .skip = " \t\r", .skiplen = 3, \
    .forceskip = 1, .inner.debug = "get_arobase_key" } ;

parse_mill_t MILL_GET_COMMENTED_KEY = \
{ \
    .search = "#", .searchlen = 1, \
    .end = "@", .endlen = 1,
    .inner.debug = "get_commented_key" } ;

parse_mill_t MILL_GET_SECTION_NAME = \
{ \
    .open = '[', .close = ']', \
    .forceclose = 1, .forceskip = 1, \
    .skip = " \t\r", .skiplen = 3, \
    .inner.debug = "get_section_name" } ;

/**********************************
 *      parser split function
 * *******************************/

int section_get_range(section_t *sasection,stralloc *src)
{
    log_flow() ;

    if (!src->len) return 0 ;
    size_t pos = 0, start = 0 ;
    int r, n = 0, id = -1, skip = 0 ;
    stralloc secname = STRALLOC_ZERO ;
    stralloc_ref psasection = 0 ;
    stralloc cp = STRALLOC_ZERO ;
    /** be clean */
    wild_zero_all(&MILL_GET_LINE) ;
    wild_zero_all(&MILL_GET_SECTION_NAME) ;
    r = mill_string(&cp,src,&MILL_GET_LINE) ;
    if (r == -1 || !r) goto err ;
    if (!sastr_rebuild_in_nline(&cp) ||
    !stralloc_0(&cp)) goto err ;

    while (pos  < cp.len)
    {
        if(secname.len && n)
        {
            skip = section_get_skip(cp.s,pos,MILL_GET_SECTION_NAME.inner.nline) ;
            id = get_enum_by_key(secname.s) ;
            section_setsa(id,&psasection,sasection) ;
            if (skip) sasection->idx[id] = 1 ;
        }
        if (!section_get_id(&secname,cp.s,&pos,&id)) goto err ;
        if (!secname.len && !n)  goto err ;
        if (!n)
        {
            skip = section_get_skip(cp.s,pos,MILL_GET_SECTION_NAME.inner.nline) ;
            section_setsa(id,&psasection,sasection) ;
            if (skip) sasection->idx[id] = 1 ;
            start = pos ;

            if (!section_get_id(&secname,cp.s,&pos,&id)) goto err ;
            if(skip)
            {
                r = get_rlen_until(cp.s,'\n',pos-1) ;//-1 to retrieve the end of previous line
                if (r == -1) goto err ;
                if (!stralloc_catb(psasection,cp.s+start,(r-start))) goto err ;
                if (!stralloc_0(psasection)) goto err ;
            }
            n++ ;
            start = pos ;
        }
        else
        {
            if (skip)
            {
                /** end of file do not contain section, avoid to remove the len of it if in the case*/
                if (secname.len)
                {
                    r = get_rlen_until(cp.s,'\n',pos-1) ;//-1 to retrieve the end of previous line
                    if (r == -1) goto err ;
                    if (!stralloc_catb(psasection,cp.s+start,(r - start))) goto err ;

                }
                else if (!stralloc_catb(psasection,cp.s+start,cp.len - start)) goto err ;
                if (!stralloc_0(psasection)) goto err ;
            }
            start = pos ;
        }
    }
    stralloc_free(&secname) ;
    stralloc_free(&cp) ;
    return 1 ;
    err:
        stralloc_free(&secname) ;
        stralloc_free(&cp) ;
        return 0 ;
}

int key_get_range(genalloc *ga, section_t *sasection)
{
    log_flow() ;

    int r ;
    size_t pos = 0, fakepos = 0 ;
    uint8_t found = 0 ;
    stralloc sakey = STRALLOC_ZERO ;
    stralloc_ref psasection ;
    key_all_t const *list = total_list ;

    for (int i = 0 ; i < SECTION_ENDOFKEY ; i++)
    {
        if (sasection->idx[i])
        {
            if (i == SECTION_ENV)
            {
                pos = 0 ;
                keynocheck nocheck = KEYNOCHECK_ZERO ;
                nocheck.idsec = i ;
                nocheck.idkey = KEY_ENVIRON_ENVAL ;
                nocheck.expected = EXPECT_KEYVAL ;
                section_setsa(i,&psasection,sasection) ;
                if (!stralloc_cats(&nocheck.val,psasection->s+1)) goto err ;//+1 remove the first '\n'
                if (!stralloc_cats(&nocheck.val,"\n") ||
                !stralloc_0(&nocheck.val)) goto err ;
                nocheck.val.len-- ;

                if (!genalloc_append(keynocheck,ga,&nocheck)) goto err ;
            }
            else
            {
                section_setsa(i,&psasection,sasection) ;
                pos = 0 ;
                size_t blen = psasection->len ;
                while (pos < blen)
                {
                    keynocheck nocheck = KEYNOCHECK_ZERO ;
                    sakey.len = 0 ;
                    r = mill_element(&sakey,psasection->s,&MILL_GET_AROBASE_KEY,&pos) ;
                    if (r == -1) goto err ;
                    if (!r) break ; //end of string
                    fakepos = get_rlen_until(psasection->s,'\n',pos) ;
                    r = mill_element(&sakey,psasection->s,&MILL_GET_COMMENTED_KEY,&fakepos) ;
                    if (r == -1) goto err ;
                    if (r) continue ;
                    if (!stralloc_cats(&nocheck.val,psasection->s+pos)) goto err ;
                    if (!stralloc_0(&nocheck.val)) goto err ;
                    for (int j = 0 ; j < total_list_el[i]; j++)
                    {
                        found = 0 ;
                        if (*list[i].list[j].name && obstr_equal(sakey.s,*list[i].list[j].name))
                        {
                            nocheck.idsec = i ;
                            nocheck.idkey = list[i].list[j].id ;
                            nocheck.expected = list[i].list[j].expected ;
                            found = 1 ;
                            switch(list[i].list[j].expected)
                            {
                                case EXPECT_QUOTE:
                                    if (!sastr_get_double_quote(&nocheck.val))
                                    {
                                        parse_err(6,&nocheck) ;
                                        goto err ;
                                    }
                                    if (!stralloc_0(&nocheck.val)) goto err ;
                                    break ;
                                case EXPECT_BRACKET:
                                    if (!parse_bracket(&nocheck.val,&pos))
                                    {
                                        parse_err(6,&nocheck) ;
                                        goto err ;
                                    }
                                    if (nocheck.val.len == 1)
                                    {
                                        parse_err(9,&nocheck) ;
                                        goto err ;
                                    }
                                    break ;
                                case EXPECT_LINE:
                                case EXPECT_UINT:
                                case EXPECT_SLASH:
                                    if (!parse_line(&nocheck.val,&pos))
                                    {
                                        parse_err(7,&nocheck) ;
                                        goto err ;
                                    }
                                    if (nocheck.val.len == 1)
                                    {
                                        parse_err(9,&nocheck) ;
                                        goto err ;
                                    }
                                    break ;
                                default:
                                    return 0 ;
                            }
                            if (!genalloc_append(keynocheck,ga,&nocheck)) goto err ;
                            break ;
                        }
                    }
                    if (!found && r >=0)
                    {
                        log_warn("unknown key: ",sakey.s," : in section: ",get_key_by_enum(ENUM_SECTION,i)) ;
                        keynocheck_free(&nocheck) ;
                        goto err ;
                    }
                }
            }
        }
    }

    stralloc_free(&sakey) ;
    return 1 ;
    err:
        stralloc_free(&sakey) ;
        return 0 ;
}

int check_mandatory(sv_alltype *service, section_t *sasection)
{
    log_flow() ;

    if (service->cname.description < 0)
        log_warn_return(LOG_EXIT_ZERO,"key @description at section [start] must be set") ;

    if (!service->user[0])
        log_warn_return(LOG_EXIT_ZERO,"key @user at section [start] must be set") ;

    if (service->cname.version < 0)
        log_warn_return(LOG_EXIT_ZERO,"key @version at section [main] must be set") ;

    if (service->opts[2] && !sasection->idx[SECTION_ENV])
        log_warn_return(LOG_EXIT_ZERO,"options env was asked -- section environment must be set") ;

    switch (service->cname.itype)
    {
        case TYPE_BUNDLE:
            if (service->cname.idga < 0)
                log_warn_return(LOG_EXIT_ZERO,"bundle type detected -- key @contents must be set") ;
            break ;
        case TYPE_ONESHOT:
            if ((service->type.oneshot.up.build == BUILD_CUSTOM) && (service->type.oneshot.up.shebang < 0))
                    log_warn_return(LOG_EXIT_ZERO,"custom build asked on section [start] -- key @shebang must be set") ;

            if (service->type.oneshot.up.exec < 0)
                    log_warn_return(LOG_EXIT_ZERO,"key @execute at section [start] must be set") ;

            if (sasection->idx[SECTION_STOP])
            {
                if ((service->type.oneshot.down.build == BUILD_CUSTOM) && (service->type.oneshot.down.shebang < 0))
                    log_warn_return(LOG_EXIT_ZERO,"custom build asked on section [stop] -- key @shebang must be set") ;
                if (service->type.oneshot.down.exec < 0)
                    log_warn_return(LOG_EXIT_ZERO,"key @execute at section [stop] must be set") ;
            }
            break ;
        case TYPE_CLASSIC:
        case TYPE_LONGRUN:
            if ((service->type.classic_longrun.run.build == BUILD_CUSTOM) && (service->type.classic_longrun.run.shebang < 0))
                    log_warn_return(LOG_EXIT_ZERO,"custom build asked on section [start] -- key @shebang must be set") ;

            if (service->type.classic_longrun.run.exec < 0)
                    log_warn_return(LOG_EXIT_ZERO,"key @execute at section [start] must be set") ;

            if (sasection->idx[SECTION_STOP])
            {
                if ((service->type.classic_longrun.finish.build == BUILD_CUSTOM) && (service->type.classic_longrun.finish.shebang < 0))
                    log_warn_return(LOG_EXIT_ZERO,"custom build asked on section [stop] -- key @shebang must be set") ;
                if (service->type.classic_longrun.finish.exec < 0)
                    log_warn_return(LOG_EXIT_ZERO,"key @execute at section [stop] must be set") ;
            }
            if (sasection->idx[SECTION_LOG])
            {
                if (service->type.classic_longrun.log.run.build == BUILD_CUSTOM)
                {
                    if (service->type.classic_longrun.log.run.shebang < 0)
                        log_warn_return(LOG_EXIT_ZERO,"custom build asked on section [logger] -- key @shebang must be set") ;
                    if (service->type.classic_longrun.log.run.exec < 0)
                        log_warn_return(LOG_EXIT_ZERO,"custom build asked on section [logger] -- key @execute must be set") ;
                }
            }
            break ;
        case TYPE_MODULE:
            /*if (!sasection->idx[SECTION_REGEX])
                log_warn_return(LOG_EXIT_ZERO,"section [regex] must be set") ;
            if (service->type.module.iddir < 0)
                log_warn_return(LOG_EXIT_ZERO,"key @directories at section [regex] must be set") ;
            if (service->type.module.idfiles < 0)
                log_warn_return(LOG_EXIT_ZERO,"key @files at section [regex] must be set") ;
            if (service->type.module.start_infiles < 0)
                log_warn_return(LOG_EXIT_ZERO,"key @infiles at section [regex] must be set") ;*/
            break ;
        /** really nothing to do here */
        default: break ;
    }
    return 1 ;
}

int nocheck_toservice(keynocheck *nocheck,int svtype, sv_alltype *service)
{
    log_flow() ;

    int p = svtype ;
    int ste = 0 ;

    unsigned char const actions[SECTION_ENDOFKEY][TYPE_ENDOFKEY] = {
        // CLASSIC,             BUNDLE,             LONGRUN,            ONESHOT             MODULES
        { ACTION_COMMON,        ACTION_COMMON,      ACTION_COMMON,      ACTION_COMMON,      ACTION_COMMON }, // main
        { ACTION_EXECRUN,       ACTION_SKIP,        ACTION_EXECRUN,     ACTION_EXECUP,      ACTION_SKIP }, // start
        { ACTION_EXECFINISH,    ACTION_SKIP,        ACTION_EXECFINISH,  ACTION_EXECDOWN,    ACTION_SKIP }, // stop
        { ACTION_EXECLOG,       ACTION_SKIP,        ACTION_EXECLOG,     ACTION_SKIP,        ACTION_SKIP }, // log
        { ACTION_ENVIRON,       ACTION_SKIP,        ACTION_ENVIRON,     ACTION_ENVIRON,     ACTION_ENVIRON }, // env
        { ACTION_SKIP,          ACTION_SKIP,        ACTION_SKIP,        ACTION_SKIP,        ACTION_REGEX } // regex
    } ;

    unsigned char const states[ACTION_SKIP + 1][TYPE_ENDOFKEY] = {
        // CLASSIC,         BUNDLE,         LONGRUN,        ONESHOT         MODULES
        { SECTION_START,    ACTION_SKIP,    SECTION_START,  SECTION_START,  SECTION_REGEX }, // action_common
        { SECTION_STOP,     ACTION_SKIP,    SECTION_STOP,   ACTION_SKIP,    ACTION_SKIP }, // action_execrun
        { SECTION_LOG,      ACTION_SKIP,    SECTION_LOG,    ACTION_SKIP,    ACTION_SKIP }, // action_execfinish
        { SECTION_ENV,      ACTION_SKIP,    SECTION_ENV,    ACTION_SKIP,    ACTION_SKIP }, // action_log
        { ACTION_SKIP,      ACTION_SKIP,    ACTION_SKIP,    SECTION_STOP,   ACTION_SKIP }, // action_execup
        { ACTION_SKIP,      ACTION_SKIP,    ACTION_SKIP,    SECTION_ENV,    ACTION_SKIP }, // action_execdown
        { ACTION_SKIP,      ACTION_SKIP,    ACTION_SKIP,    ACTION_SKIP,    ACTION_SKIP }, // action_environ
        { ACTION_SKIP,      ACTION_SKIP,    ACTION_SKIP,    ACTION_SKIP,    SECTION_ENV }, // action_regex
        { ACTION_SKIP,      ACTION_SKIP,    ACTION_SKIP,    ACTION_SKIP,    ACTION_SKIP } // action_skip
    } ;

    while (ste < 8)
    {
        unsigned int action = actions[ste][p] ;
        ste = states[action][p] ;

        switch (action) {
            case ACTION_COMMON:
                if (nocheck->idsec == SECTION_MAIN)
                    if (!keep_common(service,nocheck,svtype))
                        return 0 ;
                break ;
            case ACTION_EXECRUN:
                if (nocheck->idsec == SECTION_START)
                    if (!keep_runfinish(&service->type.classic_longrun.run,nocheck))
                        return 0 ;
                break ;
            case ACTION_EXECFINISH:
                if (nocheck->idsec == SECTION_STOP)
                    if (!keep_runfinish(&service->type.classic_longrun.finish,nocheck))
                        return 0 ;
                break ;
            case ACTION_EXECLOG:
                if (nocheck->idsec == SECTION_LOG)
                    if (!keep_logger(&service->type.classic_longrun.log,nocheck))
                        return 0 ;
                break ;
            case ACTION_EXECUP:
                if (nocheck->idsec == SECTION_START)
                    if (!keep_runfinish(&service->type.oneshot.up,nocheck))
                        return 0 ;
                break ;
            case ACTION_EXECDOWN:
                if (nocheck->idsec == SECTION_STOP)
                    if (!keep_runfinish(&service->type.oneshot.down,nocheck))
                        return 0 ;
                break ;
            case ACTION_ENVIRON:
                if (nocheck->idsec == SECTION_ENV)
                    if (!keep_environ(service,nocheck))
                        return 0 ;
                break ;
            case ACTION_REGEX:
                if (nocheck->idsec == SECTION_REGEX)
                    if (!keep_regex(&service->type.module,nocheck))
                        return 0 ;
                break ;
            case ACTION_SKIP:
                break ;
            default: log_warn_return(LOG_EXIT_ZERO,"unknown action") ;
        }
    }

    return 1 ;
}


/**********************************
 *      store
 * *******************************/
int keep_common(sv_alltype *service,keynocheck *nocheck,int svtype)
{
    log_flow() ;

    int r = 0 ;
    size_t pos = 0, *chlen = &nocheck->val.len ;
    char *chval = nocheck->val.s ;

    switch(nocheck->idkey){
        case KEY_MAIN_TYPE:
            r = get_enum(chval,nocheck) ;
            if (r == -1) return 0 ;
            service->cname.itype = r ;
            break ;
        case KEY_MAIN_NAME:
            /** name is already parsed */
            break ;
        case KEY_MAIN_DESCRIPTION:
            service->cname.description = keep.len ;
            if (!stralloc_catb(&keep,chval,*chlen + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
            break ;
        case KEY_MAIN_VERSION:
            service->cname.version = keep.len ;
            r = version_scan(&nocheck->val,chval,SS_CONFIG_VERSION_NDOT) ;
            if (r == -1) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
            if (!r) { parse_err(0,nocheck) ; return 0 ; }
            if (!stralloc_catb(&keep,chval,*chlen + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
            break ;
        case KEY_MAIN_OPTIONS:
            if (service->cname.itype == TYPE_BUNDLE)
                log_warn_return(LOG_EXIT_ZERO,"key: ",get_key_by_enum(ENUM_KEY_SECTION_MAIN,nocheck->idkey),": is not valid for type ",get_key_by_enum(ENUM_TYPE,service->cname.itype)) ;

            if (!get_clean_val(nocheck)) return 0 ;
            for (;pos < *chlen; pos += strlen(chval + pos)+1)
            {
                uint8_t reverse = chval[pos] == '!' ? 1 : 0 ;

                r = get_enum(chval + pos + reverse,nocheck) ;
                if (r == -1) return 0 ;
                if (svtype != TYPE_BUNDLE || svtype != TYPE_MODULE)
                {
                    /** set a logger by default */
                    if (reverse)
                        service->opts[0] = 0 ;/**0 means not enabled, 1 by default*/

                    if (svtype == TYPE_LONGRUN && r == OPTS_PIPELINE)
                        service->opts[1] = 1 ;
                }
                if (r == OPTS_ENVIR)
                {
                    stralloc saconf = STRALLOC_ZERO ;
                    if (!env_resolve_conf(&saconf,keep.s + service->cname.name,MYUID)) {
                        stralloc_free(&saconf) ;
                        return 0 ;
                    }
                    service->srconf = keep.len ;
                    if (!stralloc_catb(&keep,saconf.s,saconf.len + 1)) {
                        stralloc_free(&saconf) ;
                        return 0 ;
                    }
                    service->opts[2] = 1 ;
                    stralloc_free(&saconf) ;
                }
            }
            break ;
        case KEY_MAIN_FLAGS:
            if (service->cname.itype == TYPE_BUNDLE || service->cname.itype == TYPE_MODULE)
                log_warn_return(LOG_EXIT_ZERO,"key: ",get_key_by_enum(ENUM_KEY_SECTION_MAIN,nocheck->idkey),": is not valid for type ",get_key_by_enum(ENUM_TYPE,service->cname.itype)) ;

            if (!get_clean_val(nocheck)) return 0 ;
            for (;pos < *chlen; pos += strlen(chval + pos)+1)
            {
                r = get_enum(chval + pos,nocheck) ;
                if (r == -1) return 0 ;
                if (r == FLAGS_DOWN)
                    service->flags[0] = 1 ;/**0 means not enabled*/
                if (r == FLAGS_NOSETSID)
                    log_warn("deprecated file nosetsid -- ignoring") ;
            }
            break ;
        case KEY_MAIN_USER:
            if (!get_clean_val(nocheck)) return 0 ;
            {
                uid_t owner = MYUID ;
                if (!owner)
                {
                    if (sastr_find(&nocheck->val,"root") == -1)
                        log_warnu_return(LOG_EXIT_ZERO,"use the service -- permission denied") ;
                }
                /** special case, we don't know which user want to use
                 * the service, we need a general name to allow all user
                 * the term "user" is took here to allow the current user*/
                ssize_t p = sastr_cmp(&nocheck->val,"user") ;
                for (;pos < *chlen; pos += strlen(chval + pos)+1)
                {
                    if (pos == (size_t)p)
                    {
                        struct passwd *pw = getpwuid(owner);
                        if (!pw)
                        {
                            if (!errno) errno = ESRCH ;
                            log_warnu_return(LOG_EXIT_ZERO,"get user name") ;
                        }
                        scan_uidlist(pw->pw_name,(uid_t *)service->user) ;
                        continue ;
                    }
                    if (!scan_uidlist(chval + pos,(uid_t *)service->user))
                    {
                        parse_err(0,nocheck) ;
                        return 0 ;
                    }
                }
                uid_t nb = service->user[0] ;
                if (p == -1 && owner)
                {
                    int e = 0 ;
                    for (int i = 1; i < nb+1; i++)
                        if (service->user[i] == owner) e = 1 ;

                    if (!e)
                        log_warnu_return(LOG_EXIT_ZERO,"use the service -- permission denied") ;
                }
            }
            break ;
        case KEY_MAIN_HIERCOPY:
            if (service->cname.itype == TYPE_BUNDLE)
                log_warn_return(LOG_EXIT_ZERO,"key: ",get_key_by_enum(ENUM_KEY_SECTION_MAIN,nocheck->idkey),": is not valid for type ",get_key_by_enum(ENUM_TYPE,service->cname.itype)) ;

            if (!get_clean_val(nocheck)) return 0 ;
            {
                unsigned int idx = 0 ;
                for (;pos < *chlen; pos += strlen(chval + pos)+1)
                {
                    char *name = chval + pos ;
                    size_t namelen =  strlen(chval + pos) ;
                    service->hiercopy[idx+1] = keep.len ;
                    if (!stralloc_catb(&keep,name,namelen + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
                    service->hiercopy[0] = ++idx ;
                }
            }
            break ;
        case KEY_MAIN_DEPENDS:
            if ((service->cname.itype == TYPE_CLASSIC) || (service->cname.itype == TYPE_BUNDLE))
                log_warn_return(LOG_EXIT_ZERO,"key: ",get_key_by_enum(ENUM_KEY_SECTION_MAIN,nocheck->idkey),": is not valid for type ",get_key_by_enum(ENUM_TYPE,service->cname.itype)) ;

            if (!get_clean_val(nocheck)) return 0 ;
            service->cname.idga = deps.len ;
            for (;pos < *chlen; pos += strlen(chval + pos)+1)
            {
                /* allow to comment a service */
                if (chval[pos] == '#') continue ;
                if (!stralloc_catb(&deps,chval + pos,strlen(chval + pos) + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
                service->cname.nga++ ;
            }
            break ;
        case KEY_MAIN_OPTSDEPS:
            if ((service->cname.itype == TYPE_CLASSIC) || (service->cname.itype == TYPE_BUNDLE))
                log_warn_return(LOG_EXIT_ZERO,"key: ",get_key_by_enum(ENUM_KEY_SECTION_MAIN,nocheck->idkey),": is not valid for type ",get_key_by_enum(ENUM_TYPE,service->cname.itype)) ;
            if (!get_clean_val(nocheck)) return 0 ;
            service->cname.idopts = deps.len ;
            for (;pos < *chlen; pos += strlen(chval + pos)+1)
            {
                /* allow to comment a service */
                if (chval[pos] == '#') continue ;
                if (!stralloc_catb(&deps,chval + pos,strlen(chval + pos) + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
                service->cname.nopts++ ;
            }
            break ;
        case KEY_MAIN_EXTDEPS:
            if ((service->cname.itype == TYPE_CLASSIC) || (service->cname.itype == TYPE_BUNDLE))
                log_warn_return(LOG_EXIT_ZERO,"key: ",get_key_by_enum(ENUM_KEY_SECTION_MAIN,nocheck->idkey),": is not valid for type ",get_key_by_enum(ENUM_TYPE,service->cname.itype)) ;
            if (!get_clean_val(nocheck)) return 0 ;
            service->cname.idext = deps.len ;
            for (;pos < *chlen; pos += strlen(chval + pos)+1)
            {
                /* allow to comment a service */
                if (chval[pos] == '#') continue ;
                if (!stralloc_catb(&deps,chval + pos,strlen(chval + pos) + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
                service->cname.next++ ;
            }
            break ;
        case KEY_MAIN_CONTENTS:
            if (service->cname.itype != TYPE_BUNDLE)
                log_warn_return(LOG_EXIT_ZERO,"key: ",get_key_by_enum(ENUM_KEY_SECTION_MAIN,nocheck->idkey),": is not valid for type ",get_key_by_enum(ENUM_TYPE,service->cname.itype)) ;

            if (!get_clean_val(nocheck)) return 0 ;
            service->cname.idga = deps.len ;
            for (;pos < *chlen; pos += strlen(chval + pos) + 1)
            {
                /* allow to comment a service */
                if (chval[pos] == '#') continue ;
                if (!stralloc_catb(&deps,chval + pos,strlen(chval + pos) + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
                service->cname.nga++ ;
            }
            break ;
        case KEY_MAIN_T_KILL:
        case KEY_MAIN_T_FINISH:
        case KEY_MAIN_T_UP:
        case KEY_MAIN_T_DOWN:
            if (service->cname.itype == TYPE_BUNDLE || service->cname.itype == TYPE_MODULE)
                log_warn_return(LOG_EXIT_ZERO,"key: ",get_key_by_enum(ENUM_KEY_SECTION_MAIN,nocheck->idkey),": is not valid for type ",get_key_by_enum(ENUM_TYPE,service->cname.itype)) ;

            if (!get_timeout(nocheck,(uint32_t *)service->timeout)) return 0 ;
            break ;
        case KEY_MAIN_DEATH:
            if (service->cname.itype == TYPE_BUNDLE || service->cname.itype == TYPE_MODULE)
                log_warn_return(LOG_EXIT_ZERO,"key: ",get_key_by_enum(ENUM_KEY_SECTION_MAIN,nocheck->idkey),": is not valid for type ",get_key_by_enum(ENUM_TYPE,service->cname.itype)) ;

            if (!get_uint(nocheck,&service->death)) return 0 ;
            break ;
        case KEY_MAIN_NOTIFY:
            if (service->cname.itype == TYPE_BUNDLE || service->cname.itype == TYPE_MODULE)
                log_warn_return(LOG_EXIT_ZERO,"key: ",get_key_by_enum(ENUM_KEY_SECTION_MAIN,nocheck->idkey),": is not valid for type ",get_key_by_enum(ENUM_TYPE,service->cname.itype)) ;

            if (!get_uint(nocheck,&service->notification)) return 0 ;
            break ;
        case KEY_MAIN_SIGNAL:
            if (service->cname.itype == TYPE_BUNDLE || service->cname.itype == TYPE_MODULE)
                log_warn_return(LOG_EXIT_ZERO,"key: ",get_key_by_enum(ENUM_KEY_SECTION_MAIN,nocheck->idkey),": is not valid for type ",get_key_by_enum(ENUM_TYPE,service->cname.itype)) ;

            if (!sig0_scan(chval,&service->signal))
            {
                parse_err(3,nocheck) ;
                return 0 ;
            }
            break ;
        default: log_warn_return(LOG_EXIT_ZERO,"unknown key: ",get_key_by_enum(ENUM_KEY_SECTION_MAIN,nocheck->idkey)) ;

    }

    return 1 ;
}

int keep_runfinish(sv_exec *exec,keynocheck *nocheck)
{
    log_flow() ;

    int r = 0 ;
    size_t *chlen = &nocheck->val.len ;
    char *chval = nocheck->val.s ;

    switch(nocheck->idkey)
    {
        case KEY_STARTSTOP_BUILD:
            r = get_enum(chval,nocheck) ;
            if (r == -1) return 0 ;
            exec->build = r ;
            break ;
        case KEY_STARTSTOP_RUNAS:
            if (!check_valid_runas(nocheck)) return 0 ;
            exec->runas = keep.len ;
            if (!stralloc_catb(&keep,chval,*chlen + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
            break ;
        case KEY_STARTSTOP_SHEBANG:
            if (chval[0] != '/')
            {
                parse_err(4,nocheck) ;
                return 0 ;
            }
            exec->shebang = keep.len ;
            if (!stralloc_catb(&keep,chval,*chlen + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
            break ;
        case KEY_STARTSTOP_EXEC:
            exec->exec = keep.len ;
            if (!stralloc_catb(&keep,chval,*chlen + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
            break ;
        default: log_warn_return(LOG_EXIT_ZERO,"unknown key: ",get_key_by_enum(ENUM_KEY_SECTION_STARTSTOP,nocheck->idkey)) ;
    }
    return 1 ;
}

int keep_logger(sv_execlog *log,keynocheck *nocheck)
{
    log_flow() ;

    int r ;
    size_t pos = 0, *chlen = &nocheck->val.len ;
    char *chval = nocheck->val.s ;

    switch(nocheck->idkey){
        case KEY_LOGGER_BUILD:
            if (!keep_runfinish(&log->run,nocheck)) return 0 ;
            break ;
        case KEY_LOGGER_RUNAS:
            if (!keep_runfinish(&log->run,nocheck)) return 0 ;
            break ;
        case KEY_LOGGER_DEPENDS:
            if (!get_clean_val(nocheck)) return 0 ;
            log->idga = deps.len ;
            for (;pos < *chlen; pos += strlen(chval + pos) + 1)
            {
                if (!stralloc_catb(&deps,chval + pos,strlen(chval + pos) + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
                log->nga++ ;
            }
            break ;
        case KEY_LOGGER_SHEBANG:
            if (!keep_runfinish(&log->run,nocheck)) return 0 ;
            break ;
        case KEY_LOGGER_EXEC:
            if (!keep_runfinish(&log->run,nocheck)) return 0 ;
            break ;
        case KEY_LOGGER_T_KILL:
        case KEY_LOGGER_T_FINISH:
            if (!get_timeout(nocheck,(uint32_t *)log->timeout)) return 0 ;
            break ;
        case KEY_LOGGER_DESTINATION:
            if (chval[0] != '/')
            {
                parse_err(4,nocheck) ;
                return 0 ;
            }
            log->destination = keep.len ;
            if (!stralloc_catb(&keep,chval,*chlen + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
            break ;
        case KEY_LOGGER_BACKUP:
            if (!get_uint(nocheck,&log->backup)) return 0 ;
            break ;
        case KEY_LOGGER_MAXSIZE:
            if (!get_uint(nocheck,&log->maxsize)) return 0 ;
            break ;
        case KEY_LOGGER_TIMESTP:
            r = get_enum(chval,nocheck) ;
            if (r == -1) return 0 ;
            log->timestamp = r ;
            break ;
        default: log_warn_return(LOG_EXIT_ZERO,"unknown key: ",get_key_by_enum(ENUM_KEY_SECTION_LOGGER,nocheck->idkey)) ;
    }
    return 1 ;
}

int keep_environ(sv_alltype *service,keynocheck *nocheck)
{
    log_flow() ;

    stralloc tmp = STRALLOC_ZERO ;
    switch(nocheck->idkey){
        case KEY_ENVIRON_ENVAL:
            if (!env_clean_with_comment(&nocheck->val))
                log_warnu_return(LOG_EXIT_ZERO,"clean environment value") ;
            if (!auto_stra(&service->saenv,nocheck->val.s))
                log_warnu_return(LOG_EXIT_ZERO,"store environment value") ;
            break ;
        default: log_warn_return(LOG_EXIT_ZERO,"unknown key: ",get_key_by_enum(ENUM_KEY_SECTION_ENVIRON,nocheck->idkey)) ;
    }
    stralloc_free(&tmp) ;
    return 1 ;
}

int keep_regex(sv_module *module,keynocheck *nocheck)
{
    log_flow() ;

    size_t pos = 0, *chlen = &nocheck->val.len ;
    char *chval = nocheck->val.s ;

    switch(nocheck->idkey){
        case KEY_REGEX_CONFIGURE:
            module->configure = keep.len ;
            if (!stralloc_catb(&keep,chval,*chlen + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
            break ;
        case KEY_REGEX_DIRECTORIES:
            if (!get_clean_val(nocheck)) return 0 ;
            module->iddir = keep.len ;
            for (;pos < *chlen; pos += strlen(chval + pos) + 1)
            {
                /* allow to comment a service */
                if (chval[pos] == '#') continue ;
                if (!stralloc_catb(&keep,chval + pos,strlen(chval + pos) + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
                module->ndir++ ;
            }
            break ;
        case KEY_REGEX_FILES:
            if (!get_clean_val(nocheck)) return 0 ;
            module->idfiles = keep.len ;
            for (;pos < *chlen; pos += strlen(chval + pos) + 1)
            {
                /* allow to comment a service */
                if (chval[pos] == '#') continue ;
                if (!stralloc_catb(&keep,chval + pos,strlen(chval + pos) + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
                module->nfiles++ ;
            }
            break ;
        case KEY_REGEX_INFILES:
            if (!environ_get_clean_env(&nocheck->val))
                log_warnu_return(LOG_EXIT_ZERO,"clean key ",get_key_by_enum(ENUM_KEY_SECTION_REGEX,nocheck->idkey)," field") ;
            if (!environ_clean_nline(&nocheck->val))
                log_warnu_return(LOG_EXIT_ZERO,"clean lines of key ",get_key_by_enum(ENUM_KEY_SECTION_REGEX,nocheck->idkey)," field") ;
            if (!stralloc_0(&nocheck->val))
                log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

            if (!sastr_split_string_in_nline(&nocheck->val))
                log_warnu_return(LOG_EXIT_SYS,"split lines of key ",get_key_by_enum(ENUM_KEY_SECTION_REGEX,nocheck->idkey)," field") ;

            module->start_infiles = keep.len ;
            for (;pos < *chlen; pos += strlen(chval + pos) + 1)
                if (!stralloc_catb(&keep,chval + pos,strlen(chval + pos) + 1))
                    log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

            module->end_infiles = keep.len ;
            break ;
        case KEY_REGEX_ADDSERVICES:
            if (!get_clean_val(nocheck)) return 0 ;
            module->idaddservices = keep.len ;
            for (;pos < *chlen; pos += strlen(chval + pos) + 1)
            {
                /* allow to comment a service */
                if (chval[pos] == '#') continue ;
                if (!stralloc_catb(&keep,chval + pos,strlen(chval + pos) + 1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
                module->naddservices++ ;
            }
            break ;
        default: log_warn_return(LOG_EXIT_ZERO,"unknown key: ",get_key_by_enum(ENUM_KEY_SECTION_REGEX,nocheck->idkey)) ;
    }
    return 1 ;
}

/**********************************
 *      helper function
 * *******************************/
int add_pipe(sv_alltype *sv, stralloc *sa)
{
    log_flow() ;

    char *prodname = keep.s+sv->cname.name ;

    stralloc tmp = STRALLOC_ZERO ;

    sv->pipeline = sa->len ;
    if (!stralloc_cats(&tmp,SS_PIPE_NAME)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
    if (!stralloc_cats(&tmp,prodname)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
    if (!stralloc_0(&tmp)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

    if (!stralloc_catb(sa,tmp.s,tmp.len+1)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

    stralloc_free(&tmp) ;

    return 1 ;
}

int parse_line(stralloc *sa, size_t *pos)
{
    log_flow() ;

    if (!sa->len) return 0 ;
    int r = 0 ;
    size_t newpos = 0 ;
    stralloc kp = STRALLOC_ZERO ;
    wild_zero_all(&MILL_CLEAN_LINE) ;
    r = mill_element(&kp,sa->s,&MILL_CLEAN_LINE,&newpos) ;
    if (r == -1 || !r) goto err ;
    if (!stralloc_0(&kp)) goto err ;
    if (!stralloc_copy(sa,&kp)) goto err ;
    *pos += newpos - 1 ;
    stralloc_free(&kp) ;
    return 1 ;
    err:
        stralloc_free(&kp) ;
        return 0 ;
}

int parse_bracket(stralloc *sa,size_t *pos)
{
    log_flow() ;

    if (!sa->len) return 0 ;
    size_t newpos = 0 ;
    stralloc kp = STRALLOC_ZERO ;
    if (!key_get_next_id(&kp,sa->s,&newpos)) goto err ;
    if (!stralloc_0(&kp)) goto err ;
    if (!stralloc_copy(sa,&kp)) goto err ;
    *pos += newpos ;
    stralloc_free(&kp) ;
    return 1 ;
    err:
        stralloc_free(&kp) ;
        return 0 ;
}

void section_setsa(int id, stralloc_ref *p,section_t *sa)
{
    log_flow() ;

    switch(id)
    {
        case SECTION_MAIN: *p = &sa->main ; break ;
        case SECTION_START: *p = &sa->start ; break ;
        case SECTION_STOP: *p = &sa->stop ; break ;
        case SECTION_LOG: *p = &sa->logger ; break ;
        case SECTION_ENV: *p = &sa->environment ; break ;
        case SECTION_REGEX: *p = &sa->regex ; break ;
        default: break ;
    }
}

int section_get_skip(char const *s,size_t pos,int nline)
{
    log_flow() ;

    ssize_t r = -1 ;
    if (nline == 1)
    {
        r = get_sep_before(s,'#','[') ;
        if (r >= 0) return 0 ;
    }
    r = get_rlen_until(s,'\n',pos) ;
    if (r >= 0)
    {
        r = get_sep_before(s+r+1,'#','[') ;
        if (r >= 0) return 0 ;
    }
    return 1 ;
}

int section_get_id(stralloc *secname, char const *str,size_t *pos,int *id)
{
    log_flow() ;

    size_t len = strlen(str) ;
    size_t newpos = 0 ;
    (*id) = -1 ;

    while ((*id) < 0 && (*pos) < len)
    {
        secname->len = 0 ;
        newpos = 0 ;
        if (mill_element(secname,str+(*pos),&MILL_GET_SECTION_NAME,&newpos) == -1) return 0 ;
        if (secname->len)
        {
            if (!stralloc_0(secname)) return 0 ;
            (*id) = get_enum_by_key(secname->s) ;
        }
        (*pos) += newpos ;
    }
    return 1 ;
}

int key_get_next_id(stralloc *sa, char const *str,size_t *pos)
{
    log_flow() ;

    if (!str) return 0 ;
    int r = 0 ;
    size_t newpos = 0, len = strlen(str) ;
    stralloc kp = STRALLOC_ZERO ;
    wild_zero_all(&MILL_GET_AROBASE_KEY) ;
    wild_zero_all(&MILL_FIRST_BRACKET) ;
    int id = -1 ;
    r = mill_element(&kp,str,&MILL_FIRST_BRACKET,&newpos) ;
    if (r == -1 || !r) goto err ;
    *pos = newpos ;
    while (id == -1 && newpos < len)
    {
        kp.len = 0 ;
        r = mill_element(&kp,str,&MILL_GET_AROBASE_KEY,&newpos) ;
        if (r == -1) goto err ;
        if (!stralloc_0(&kp)) goto err ;
        id = get_enum_by_key(kp.s) ;
        //May confusing in case of instantiated service
        //if (id == -1 && kp.len > 1) log_warn("unknown key: ",kp.s,": at parenthesis parse") ;
    }
    newpos = get_rlen_until(str,')',newpos) ;
    if (newpos == -1) goto err ;
    if (!stralloc_catb(sa,str+*pos,newpos - *pos)) goto err ;
    *pos = newpos + 1 ; //+1 remove the last ')'
    stralloc_free(&kp) ;
    return 1 ;
    err:
        stralloc_free(&kp) ;
        return 0 ;
}

int get_clean_val(keynocheck *ch)
{
    log_flow() ;

    if (!sastr_clean_element(&ch->val))
    {
        parse_err(8,ch) ;
        return 0 ;
    }
    return 1 ;
}

int get_enum(char const *str, keynocheck *ch)
{
    log_flow() ;

    int r = get_enum_by_key(str) ;
    if (r == -1)
    {
        parse_err(0,ch) ;
        return -1 ;
    }
    return r ;
}

int get_timeout(keynocheck *ch,uint32_t *ui)
{
    log_flow() ;

    int time = 0 ;
    if ((ch->idkey == KEY_MAIN_T_KILL) || (ch->idkey == KEY_LOGGER_T_KILL)) time = 0 ;
    else if ((ch->idkey == KEY_MAIN_T_FINISH) || (ch->idkey == KEY_LOGGER_T_FINISH)) time = 1 ;
    else if (ch->idkey == KEY_MAIN_T_UP) time = 2 ;
    else if (ch->idkey == KEY_MAIN_T_DOWN) time = 3 ;
    if (scan_timeout(ch->val.s,ui,time) == -1)
    {
        parse_err(3,ch) ;
        return 0 ;
    }
    return 1 ;
}

int get_uint(keynocheck *ch,uint32_t *ui)
{
    log_flow() ;

    if (!uint32_scan(ch->val.s,ui))
    {
        parse_err(3,ch) ;
        return 0 ;
    }
    return 1 ;
}

int check_valid_runas(keynocheck *ch)
{
    log_flow() ;

    size_t len = strlen(ch->val.s) ;
    char file[len + 1] ;
    auto_strings(file,ch->val.s) ;

    char *colon ;
    colon = strchr(file,':') ;

    if (colon) {

        *colon = 0 ;

        uid_t uid ;
        gid_t gid ;
        size_t uid_strlen ;
        size_t gid_strlen ;
        static char uid_str[UID_FMT] ;
        static char gid_str[GID_FMT] ;

        /** on format :gid, get the uid of
         * the owner of the process */
        if (!*file) {

            uid = getuid() ;

        }
        else {

            if (get_uidbyname(file,&uid) == -1) {
                parse_err(0,ch) ;
                return 0 ;
            }

        }
        uid_strlen = uid_fmt(uid_str,uid) ;
        uid_str[uid_strlen] = 0 ;

        /** on format uid:, get the gid of
         * the owner of the process */
        if (!*(colon + 1)) {

            if (!yourgid(&gid,uid)) {
                parse_err(0,ch) ;
                return 0 ;
            }

        }
        else {

            if (get_gidbygroup(colon + 1,&gid) == -1) {
                parse_err(0,ch) ;
                return 0 ;
            }

        }
        gid_strlen = gid_fmt(gid_str,gid) ;
        gid_str[gid_strlen] = 0 ;

        ch->val.len = 0 ;
        if (!auto_stra(&ch->val,uid_str,":",gid_str))
            log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

    }
    else {

        int e = errno ;
        errno = 0 ;

        struct passwd *pw = getpwnam(ch->val.s);

        if (!pw) {

            if (!errno) errno = ESRCH ;
            parse_err(0,ch) ;
            return 0 ;
        }

        errno = e ;

    }

    return 1 ;
}


void parse_err(int ierr,keynocheck *check)
{
    log_flow() ;

    int idsec = check->idsec ;
    int idkey = check->idkey ;
    char const *section = get_key_by_enum(ENUM_SECTION,idsec) ;
    /* start stop enum are the same, enum_all must increase by one to match
     * the correct list */
    char const *key = get_key_by_enum(idsec < 2 ? idsec + 1 : idsec,idkey) ;

    switch(ierr)
    {
        case 0:
            log_warn("invalid value for key: ",key,": in section: ",section) ;
            break ;
        case 1:
            log_warn("multiple definition of key: ",key,": in section: ",section) ;
            break ;
        case 2:
            log_warn("same value for key: ",key,": in section: ",section) ;
            break ;
        case 3:
            log_warn("key: ",key,": must be an integrer value in section: ",section) ;
            break ;
        case 4:
            log_warn("key: ",key,": must be an absolute path in section: ",section) ;
            break ;
        case 5:
            log_warn("key: ",key,": must be set in section: ",section) ;
            break ;
        case 6:
            log_warn("invalid format of key: ",key,": in section: ",section) ;
            break ;
        case 7:
            log_warnu("parse key: ",key,": in section: ",section) ;
            break ;
        case 8:
            log_warnu("clean value of key: ",key,": in section: ",section) ;
            break ;
        case 9:
            log_warn("empty value of key: ",key,": in section: ",section) ;
            break ;
        default:
            log_warn("unknown parse_err number") ;
            break ;
    }
}

int get_svtype(sv_alltype *sv_before, char const *contents)
{
    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;

    if (!auto_stra(&sa,contents)) goto err ;

    if (!environ_get_val_of_key(&sa,get_key_by_enum(ENUM_KEY_SECTION_MAIN,KEY_MAIN_TYPE))) goto err ;

    if (!sastr_clean_element(&sa)) goto err ;
    sv_before->cname.itype = get_enum_by_key(sa.s) ;

    if (sv_before->cname.itype == -1) goto err ;

    stralloc_free(&sa) ;
    return 1 ;
    err:
        stralloc_free(&sa) ;
        return 0 ;
}

int get_svtype_from_file(char const *file)
{
    log_flow() ;

    stralloc tmp = STRALLOC_ZERO ;
    int svtype = -1 ;
    size_t len = strlen(file) ;
    char bname[len + 1] ;
    char dname[len + 1] ;
    if (!ob_basename(bname,file)) goto err ;
    if (!ob_dirname(dname,file)) goto err ;

    log_trace("read service file of: ",dname,bname) ;
    if (read_svfile(&tmp,bname,dname) <= 0) goto err ;

    if (!environ_get_val_of_key(&tmp,get_key_by_enum(ENUM_KEY_SECTION_MAIN,KEY_MAIN_TYPE))) goto err ;

    if (!sastr_clean_element(&tmp)) goto err ;
    svtype = get_enum_by_key(tmp.s) ;

    err:
    stralloc_free(&tmp) ;
    return svtype ;
}

int get_svname(sv_alltype *sv_before,char const *contents)
{
    log_flow() ;

    int r ;
    stralloc sa = STRALLOC_ZERO ;

    if (!auto_stra(&sa,contents)) goto err ;
    /** @name only exist on instantiated service if any */
    r = sastr_find(&sa,get_key_by_enum(ENUM_KEY_SECTION_MAIN,KEY_MAIN_NAME)) ;
    if (r == -1) { sv_before->cname.name == -1 ; goto freed ; }

    if (!environ_get_val_of_key(&sa,get_key_by_enum(ENUM_KEY_SECTION_MAIN,KEY_MAIN_NAME))) goto err ;

    if (!sastr_clean_element(&sa)) goto err ;
    sv_before->cname.name = get_enum_by_key(sa.s) ;

    if (sv_before->cname.name == -1) goto err ;

    freed:
    stralloc_free(&sa) ;
    return 1 ;
    err:
        stralloc_free(&sa) ;
        return 0 ;
}
