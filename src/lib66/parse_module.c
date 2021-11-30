/*
 * parse_module.c
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

#include <66/parser.h>

#include <string.h>
#include <stdio.h> //rename
#include <unistd.h> //chdir

#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/environ.h>
#include <oblibs/files.h>
#include <oblibs/mill.h>
#include <oblibs/directory.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/env.h>
#include <skalibs/bytestr.h>//byte_count
#include <skalibs/genalloc.h>

#include <66/resolve.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/environ.h>

#define SS_MODULE_CONFIG_DIR "/configure"
#define SS_MODULE_CONFIG_DIR_LEN (sizeof SS_MODULE_CONFIG_DIR - 1)
#define SS_MODULE_CONFIG_SCRIPT "configure"
#define SS_MODULE_CONFIG_SCRIPT_LEN (sizeof SS_MODULE_CONFIG_SCRIPT - 1)
#define SS_MODULE_SERVICE "/service"
#define SS_MODULE_SERVICE_LEN (sizeof SS_MODULE_SERVICE - 1)
#define SS_MODULE_SERVICE_INSTANCE "/service@"
#define SS_MODULE_SERVICE_INSTANCE_LEN (sizeof SS_MODULE_SERVICE_INSTANCE - 1)

static int check_dir(char const *src,char const *dir)
{
    log_flow() ;

    int r ;
    size_t srclen = strlen(src) ;
    size_t dirlen = strlen(dir) ;

    char tsrc[srclen + dirlen + 1] ;
    auto_strings(tsrc,src,dir) ;

    r = scan_mode(tsrc,S_IFDIR) ;
    if (r < 0) { errno = EEXIST ; log_warnusys_return(LOG_EXIT_ZERO,"conflicting format of: ",tsrc) ; }
    if (!r) {

        if (!dir_create_parent(tsrc,0755))
            log_warnusys_return(LOG_EXIT_ZERO,"create directory: ",tsrc) ;
    }

    return 1 ;
}

static int get_list(stralloc *list, stralloc *sdir,size_t len, char const *svname, mode_t mode)
{
    log_flow() ;

    sdir->len = len ;
    if (!auto_stra(sdir,SS_MODULE_SERVICE)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

    char const *exclude[1] = { 0 } ;
    if (!sastr_dir_get_recursive(list,sdir->s,exclude,mode,1))
        log_warnusys_return(LOG_EXIT_ZERO,"get file(s) of module: ",svname) ;

    sdir->len = len ;

    if (!auto_stra(sdir,SS_MODULE_SERVICE_INSTANCE)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

    if (!sastr_dir_get_recursive(list,sdir->s,exclude,mode,1))
        log_warnusys_return(LOG_EXIT_ZERO,"get file(s) of module: ",svname) ;

    return 1 ;
}

static int rebuild_list(sv_alltype *alltype,stralloc *list,stralloc *sv_all_type, stralloc *module_service)
{
    log_flow() ;

    size_t pos, id, did, nid, dnid ;
    sv_alltype_ref sv ;
    id = alltype->cname.idga, nid = alltype->cname.nga ;
    for (; nid ; id += strlen(deps.s + id) + 1, nid--)
    {
        char *deps_name = deps.s + id ;
        /** if we check the dependencies of a module which was declared
         * inside the main module, the recursive operation enter on infinite loop.
         * So, break it by dependency addition comparison */
        if (sastr_cmp(sv_all_type,deps_name) == -1) {

            for (pos = 0 ; pos < genalloc_len(sv_alltype,&gasv) ; pos++) {

                sv = &genalloc_s(sv_alltype,&gasv)[pos] ;
                int type = sv->cname.itype == TYPE_MODULE ? 1 : 0 ;
                char *n = keep.s + sv->cname.name ;

                if (!strcmp(n,deps_name)) {

                    if (!stralloc_catb(list,keep.s + sv->src,strlen(keep.s + sv->src) + 1))
                        return 0 ;

                    if (!sv->cname.nga)
                        continue ;

                    did = type ? sv->cname.idcontents : sv->cname.idga, dnid = type ? sv->cname.ncontents : sv->cname.nga ;

                    for (;dnid; did += strlen(deps.s + did) + 1, dnid--) {

                        if (sastr_cmp(list,deps.s + did) >= 0)
                            continue ;

                        if (!rebuild_list(sv,list,sv_all_type,module_service))
                            log_warnu(LOG_EXIT_ZERO,"rebuild dependencies list of: ",deps.s + did) ;
                    }
                }
            }

            if (!stralloc_catb(sv_all_type,deps_name,strlen(deps_name) + 1))
                return 0 ;
        }

        if (!stralloc_catb(module_service,deps_name,strlen(deps_name) + 1))
            return 0 ;
    }

    if (!sastr_sortndrop_element(module_service))
        return 0 ;

    return 1 ;
}

/** return 1 on success
 * return 0 on failure
 * return 2 on already enabled
 * @svname do not contents the path of the frontend file*/

int parse_module(sv_alltype *alltype, ssexec_t *info, stralloc *parsed_list, uint8_t force)
{
    log_flow() ;

    int r, err = 1, insta = -1, minsta = -1, svtype = -1, from_ext_insta = 0, already_parsed = 0 ;
    size_t pos = 0, id, nid, newlen ;
    stralloc sdir = STRALLOC_ZERO ; // service dir
    stralloc list = STRALLOC_ZERO ;
    stralloc tmp = STRALLOC_ZERO ;
    stralloc addonsv = STRALLOC_ZERO ;
    char *sv = keep.s + alltype->cname.name ;
    char src[strlen(keep.s + alltype->src) + 1] ;
    uint8_t conf = alltype->overwrite_conf ;

    if (!ob_dirname(src, keep.s + alltype->src))
        log_dieu(LOG_EXIT_SYS,"get dirname of: ", keep.s + alltype->src) ;

    log_trace("start parse process of module: ",sv) ;

    /** should be always right,
     * be paranoid and check it */
    insta = instance_check(sv) ;
    if (insta <= 0)
        log_die(LOG_EXIT_SYS, "invalid module instance name: ",sv);

    minsta = insta ;

    if (!ss_resolve_module_path(&sdir, &tmp, sv, src, info->owner))
        log_dieu(LOG_EXIT_SYS,"resolve source of module: ",sv);

    /** check mandatory directories:
     * module/module_name, module/module_name/{configure,service,service@} */
    if (!check_dir(tmp.s, "")) return 0 ;
    if (!check_dir(tmp.s, SS_MODULE_CONFIG_DIR)) return 0 ;
    if (!check_dir(tmp.s, SS_MODULE_SERVICE)) return 0 ;
    if (!check_dir(tmp.s, SS_MODULE_SERVICE_INSTANCE)) return 0 ;

    newlen = sdir.len ;

    char permanent_sdir[sdir.len + 2] ;
    auto_strings(permanent_sdir, sdir.s, "/") ;

    r = scan_mode(sdir.s, S_IFDIR) ;
    if (r < 0) { errno = EEXIST ; log_dieusys(LOG_EXIT_SYS,"conflicting format of: ", sdir.s) ; }
    else if (!r) {

        if (!hiercopy(tmp.s, sdir.s))
            log_dieusys(LOG_EXIT_SYS, "copy: ", tmp.s, " to: ", sdir.s) ;

    } else {

        /** Must reconfigure all services of the module */
        if (force < 2) {

            log_warn("skip configuration of the module: ", sv, " -- already configured") ;
            err = 2 ;
            goto make_deps ;
        }

        if (rm_rf(sdir.s) < 0)
            log_dieusys (LOG_EXIT_SYS, "remove: ", sdir.s) ;

        if (!hiercopy(tmp.s, sdir.s))
            log_dieusys(LOG_EXIT_SYS,"copy: ", tmp.s, " to: ", sdir.s) ;
    }

    /** regex file content */
    list.len = 0 ;

    if (!get_list(&list, &sdir, newlen, sv, S_IFREG)) return 0 ;
    if (!regex_replace(&list, alltype, sv)) return 0 ;

    /* regex directories name */
    if (!get_list(&list, &sdir, newlen, sv, S_IFDIR)) return 0 ;
    if (!regex_rename(&list, alltype->type.module.iddir, alltype->type.module.ndir, sdir.s)) return 0 ;

    /* regex files name */
    list.len = 0 ;

    if (!get_list(&list,&sdir, newlen, sv, S_IFREG)) return 0 ;
    if (!regex_rename(&list, alltype->type.module.idfiles, alltype->type.module.nfiles, sdir.s)) return 0 ;

    /* launch configure script */
    if (!regex_configure(alltype, info, permanent_sdir, sv, conf)) return 0 ;

    make_deps:

    tmp.len = 0 ;
    list.len = 0 ;

    if (!auto_stra(&tmp, permanent_sdir, SS_MODULE_SERVICE + 1))
        log_die_nomem("stralloc") ;

    /** get all services */
    char const *exclude[1] = { 0 } ;
    if (!sastr_dir_get_recursive(&list, tmp.s, exclude, S_IFREG, 1))
        log_dieusys(LOG_EXIT_SYS, "get file(s) of module: ", sv) ;

    /** add addon services */
    if (alltype->type.module.naddservices > 0) {

        id = alltype->type.module.idaddservices, nid = alltype->type.module.naddservices ;
        for (; nid ; id += strlen(keep.s + id) + 1, nid--) {

            char *name = keep.s + id ;
            if (ss_resolve_src_path(&list, name, info->owner,0) < 1)
                    log_die(LOG_EXIT_SYS, "resolve source path of: ", name) ;
        }
    }

    tmp.len = 0 ;
    sdir.len = 0 ;

    /** remake the depends field.
     * incoporate the module services dependencies inside the list to parse
     * and each dependency of each module service dependency.
     * Do it recursively. */
    if (!rebuild_list(alltype, &list, &tmp, &sdir))
        log_dieu(LOG_EXIT_SYS,"rebuild dependencies list of: ", sv) ;

    /** parse all services of the modules */
    for (pos = 0 ; pos < list.len ; pos += strlen(list.s + pos) + 1) {

        insta = 0, svtype = -1, from_ext_insta = 0, already_parsed = 0 ;
        char *svname = list.s + pos ;
        size_t len = strlen(svname) ;
        char bname[len + 1] ;
        char *pbname = 0 ;

        addonsv.len = 0 ;

        if (sastr_cmp(parsed_list, svname) >= 0) {

            /** already parsed ? */
            size_t idx = 0 ;
            for (; idx < genalloc_len(sv_alltype, &gasv) ; idx++) {

                char *name = keep.s + genalloc_s(sv_alltype, &gasv)[idx].src ;

                if (!strcmp(name, svname)) {

                    svname = name ;
                    break ;
                }
            }
            already_parsed = 1 ;
        }

        if (!ob_basename(bname,svname))
            log_dieu(LOG_EXIT_SYS,"find basename of: ", svname) ;

        pbname = bname ;

        /** detect cyclic call. Sub-module cannot call it itself*/
        if (!strcmp(sv, bname))
            log_die(LOG_EXIT_SYS,"cyclic call detected -- ", sv, " call ", bname) ;

        insta = instance_check(bname) ;
        if (!insta) log_die(LOG_EXIT_SYS,"invalid instance name: ", svname) ;
        if (insta > 0) {

            /** we can't know the origin of the instanciated service.
             * Search first at service@ directory, if it not found
             * pass through the classic ss_resolve_src_path() */

            pbname = bname ;
            if (!already_parsed) {

                int found = 0 ;
                size_t l = strlen(permanent_sdir) ;
                char tmp[l + SS_MODULE_SERVICE_INSTANCE_LEN + 2] ;
                auto_strings(tmp, permanent_sdir, SS_MODULE_SERVICE_INSTANCE + 1, "/") ;

                r = ss_resolve_src(&addonsv, pbname, tmp, &found) ;

                if (r == -1) log_dieusys(LOG_EXIT_SYS,"parse source directory: ", tmp) ;
                if (!r) {

                    if (ss_resolve_src_path(&addonsv, pbname, info->owner, 0) < 1)
                        log_dieu(LOG_EXIT_SYS,"resolve source path of: ", pbname) ;
                }
                svname = addonsv.s ;
            }
            from_ext_insta++ ;
            len = strlen(svname) ;
        }

        if (!already_parsed)
            start_parser(svname, info, 0, permanent_sdir) ;

        char ext_insta[len + 1] ;
        if (from_ext_insta) {

            size_t len = strlen(svname) ;
            r = get_rlen_until(svname, '@', len) + 1 ;
            size_t newlen = len - (len - r) ;
            auto_strings(ext_insta, svname) ;
            ext_insta[newlen] = 0 ;
            svname = ext_insta ;
        }

        /** we want the configuration file for each service inside
         * the configuration directory of the module.
         * In case of sub-module, we skip it.
         * Also, we skip every dependency of the sub-module,
         * each module contains its own services.*/
        char *version = keep.s + alltype->cname.version ;

        {
            stralloc tmpenv = STRALLOC_ZERO ;
            /** SS_MAX_SERVICE is the maximum of services set at compile time
             * that can be supervised by s6-svscan. 500 is the default which
             * should be large enough for the majority of the cases */
            size_t pos, spos, id, nid, idmodule[SS_MAX_SERVICE] = { 0 } ;
            sv_alltype_ref svref ;

            if (!env_resolve_conf(&tmpenv, sv, info->owner))
                log_dieu(LOG_EXIT_SYS,"get path of the configuration file") ;

            if (!auto_stra(&tmpenv, "/"))
                log_die_nomem("stralloc") ;

            /** search first for all sub-modules */
            for (pos = 0 ; pos < genalloc_len(sv_alltype,&gasv); pos++) {

                if (genalloc_s(sv_alltype,&gasv)[pos].cname.itype == TYPE_MODULE) {

                    idmodule[pos] = 1 ;

                    svref = &genalloc_s(sv_alltype,&gasv)[pos] ;

                    id = svref->cname.idcontents, nid = svref->cname.ncontents ;
                    for (;nid; id += strlen(deps.s + id) + 1, nid--) {

                        char *name = deps.s + id ;

                        for (spos = 0 ; spos < genalloc_len(sv_alltype,&gasv);spos++)
                            if (!strcmp(name,keep.s + genalloc_s(sv_alltype,&gasv)[spos].cname.name))
                                idmodule[spos] = 1 ;
                    }
                }
            }

            for (pos = 0 ; pos < genalloc_len(sv_alltype,&gasv); pos++) {

                if (!genalloc_s(sv_alltype,&gasv)[pos].opts[2] ||
                idmodule[pos]) continue ;
                char *n = keep.s + genalloc_s(sv_alltype,&gasv)[pos].cname.name ;

                if (!strcmp(n,bname)) {

                    genalloc_s(sv_alltype,&gasv)[pos].srconf = keep.len ;
                    if (!auto_stra(&tmpenv,version,"/",bname)) log_warn_return(LOG_EXIT_ZERO,"stralloc") ;

                    if (!stralloc_catb(&keep,tmpenv.s,strlen(tmpenv.s) + 1))
                        log_die_nomem("stralloc") ;
                    break ;
                }
            }
            stralloc_free(&tmpenv) ;
        }

        svtype = get_svtype_from_file(svname) ;
        if (svtype == -1) log_dieu(LOG_EXIT_SYS,"get svtype of: ",svname) ;

        if (sastr_cmp(&sdir,pbname) == -1)
            if (!stralloc_catb(&sdir,pbname,strlen(pbname) + 1))
                log_die_nomem("stralloc") ;

        if (sastr_cmp(&tmp,pbname) == -1)
            if (svtype != TYPE_CLASSIC)
                if (!stralloc_catb(&tmp,pbname,strlen(pbname) + 1))
                    log_die_nomem("stralloc") ;
    }

    /** prefix each service inside the module with the name of the module */
    list.len = 0 ;
    if (!instance_splitname(&list,sv,minsta,SS_INSTANCE_NAME))
            log_dieu(LOG_EXIT_SYS, "split instance service: ",sv) ;

    list.len-- ; //instance_splitname close the string

    if (!auto_stra(&list,"-"))
        log_die_nomem("stralloc") ;

    alltype->cname.idga = deps.len ;
    alltype->cname.nga = 0 ;
    for (pos = 0 ;pos < tmp.len ; pos += strlen(tmp.s + pos) + 1) {

        if (!stralloc_catb(&deps,list.s, list.len) ||
            !stralloc_catb(&deps,tmp.s + pos,strlen(tmp.s + pos) + 1))
                log_die_nomem("stralloc") ;

        alltype->cname.nga++ ;
    }

    alltype->cname.idcontents = deps.len ;
    for (pos = 0 ;pos < sdir.len ; pos += strlen(sdir.s + pos) + 1) {

        if (!stralloc_catb(&deps,list.s, list.len) ||
            !stralloc_catb(&deps,sdir.s + pos,strlen(sdir.s + pos) + 1))
                log_die_nomem("stralloc") ;

        alltype->cname.ncontents++ ;
    }

    tmp.len = 0 ;
    if (!auto_stra(&tmp,permanent_sdir,SS_MODULE_CONFIG_DIR + 1))
        log_die_nomem("stralloc") ;

    if (rm_rf(tmp.s) < 0)
        log_dieusys(LOG_EXIT_SYS,"remove: ",tmp.s) ;

    stralloc_free(&sdir) ;
    stralloc_free(&list) ;
    stralloc_free(&tmp) ;
    stralloc_free(&addonsv) ;

    return err ;
}

/* helper */

/* 0 filename undefine
 * -1 system error
 * should return at least 2 meaning :: no file define*/
int regex_get_file_name(char *filename,char const *str)
{
    log_flow() ;

    int r ;
    size_t pos = 0 ;
    stralloc kp = STRALLOC_ZERO ;

    parse_mill_t MILL_GET_COLON = {
    .open = ':', .close = ':',
    .skip = " \t\r", .skiplen = 3,
    .forceclose = 1,
    .inner.debug = "get_colon" } ;

    r = mill_element(&kp,str,&MILL_GET_COLON,&pos) ;
    if (r == -1) goto err ;

    auto_strings(filename,kp.s) ;

    stralloc_free(&kp) ;
    return pos ;
    err:
        stralloc_free(&kp) ;
        return -1 ;
}

int regex_get_replace(char *replace, char const *str)
{
    log_flow() ;

    int pos = get_len_until(str,'=') ;
    if (!pos || pos == -1) return 0 ;
    char tmp[pos + 1] ;
    memcpy(tmp,str,pos) ;
    tmp[pos] = 0 ;
    auto_strings(replace,tmp) ;
    return 1 ;
}

int regex_get_regex(char *regex, char const *str)
{
    log_flow() ;

    size_t len = strlen(str) ;
    int pos = get_len_until(str,'=') ;
    if (!pos || pos == -1) return 0 ;
    pos++ ; // remove '='
    char tmp[len + 1] ;
    memcpy(tmp,str + pos,len-pos) ;
    tmp[len-pos] = 0 ;
    auto_strings(regex,tmp) ;
    return 1 ;
}

int regex_replace(stralloc *list,sv_alltype *alltype,char const *svname)
{
    log_flow() ;

    int r ;
    size_t in = 0, pos, inlen ;

    stralloc tmp = STRALLOC_ZERO ;

    for (; in < list->len; in += strlen(list->s + in) + 1)
    {
        tmp.len = 0 ;
        char *str = list->s + in ;
        size_t len = strlen(str) ;
        char bname[len + 1] ;
        char dname[len + 1] ;
        if (!ob_basename(bname,str)) log_warnu_return(LOG_EXIT_ZERO,"get basename of: ",str) ;
        if (!ob_dirname(dname,str)) log_warnu_return(LOG_EXIT_ZERO,"get dirname of: ",str) ;

        log_trace("read service file of: ",dname,bname) ;
        r = read_svfile(&tmp,bname,dname) ;
        if (!r) log_warnusys_return(LOG_EXIT_ZERO,"read file: ",str) ;
        if (r == -1) continue ;

        pos = alltype->type.module.start_infiles, inlen = alltype->type.module.end_infiles ;
        for (; pos < inlen ; pos += strlen(keep.s + pos) + 1)
        {
            int all = 0, fpos = 0 ;
            char filename[512] = { 0 } ;
            char replace[512] = { 0 } ;
            char regex[512] = { 0 } ;
            char const *line = keep.s + pos ;

            if (strlen(line) >= 511) log_warn_return(LOG_EXIT_ZERO,"limit exceeded in service: ", svname) ;
            if ((line[0] != ':') || (get_sep_before(line + 1,':','=') < 0))
                log_warn_return(LOG_EXIT_ZERO,"bad format in line: ",line," of key @infiles field") ;

            fpos = regex_get_file_name(filename,line) ;

            if (fpos == -1)  log_warnu_return(LOG_EXIT_ZERO,"get filename of line: ",line) ;
            else if (fpos < 3) all = 1 ;

            if (!regex_get_replace(replace,line+fpos)) log_warnu_return(LOG_EXIT_ZERO,"replace string of line: ",line) ;

            if (!regex_get_regex(regex,line+fpos)) log_warnu_return(LOG_EXIT_ZERO,"regex string of line: ",line) ;

            if (obstr_equal(bname,filename) || all)
            {
                if (!sastr_replace_all(&tmp,replace,regex))
                    log_warnu_return(LOG_EXIT_ZERO,"replace: ",replace," by: ", regex," in file: ",str) ;

                if (!stralloc_0(&tmp))
                    log_warnusys_return(LOG_EXIT_ZERO,"stralloc") ;

                tmp.len-- ;

                if (!file_write_unsafe(dname,bname,tmp.s,tmp.len))
                    log_warnusys_return(LOG_EXIT_ZERO,"write: ",dname,"/","filename") ;
            }
        }
    }
    stralloc_free(&tmp) ;

    return 1 ;
}

int regex_rename(stralloc *list, int id, unsigned int nid, char const *sdir)
{
    log_flow() ;

    stralloc tmp = STRALLOC_ZERO ;
    size_t pos = id, len = nid, in ;

    pos = id, len = nid ;

    for (;len; pos += strlen(keep.s + pos) + 1,len--)
    {
        char *line = keep.s + pos ;
        char replace[512] = { 0 } ;
        char regex[512] = { 0 } ;
        if (!regex_get_replace(replace,line)) log_warnu_return(LOG_EXIT_ZERO,"replace string of line: ",line) ;
        if (!regex_get_regex(regex,line)) log_warnu_return(LOG_EXIT_ZERO,"regex string of line: ",line) ;

        for (in = 0 ; in < list->len; in += strlen(list->s + in) + 1)
        {
            tmp.len = 0 ;
            char *str = list->s + in ;
            size_t len = strlen(str) ;
            char dname[len + 1] ;
            if (!ob_dirname(dname,str)) log_warnu_return(LOG_EXIT_ZERO,"get dirname of: ",str) ;

            if (!sabasename(&tmp,str,len)) log_warnusys_return(LOG_EXIT_ZERO,"get basename of: ",str) ;
            if (!stralloc_0(&tmp)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

            if (!sastr_replace(&tmp,replace,regex))
                log_warnu_return(LOG_EXIT_ZERO,"replace: ",replace," by: ", regex," in file: ",str) ;

            if (!stralloc_0(&tmp))
                log_warnu_return(LOG_EXIT_ZERO,"stralloc") ;

            char new[len + tmp.len + 1] ;
            auto_strings(new,dname,tmp.s) ;
            /** do not try to rename the same directory */
            if (!obstr_equal(str,new))
            {
                log_trace("rename: ",str," to: ",new) ;
                if (rename(str,new) == -1)
                    log_warnusys_return(LOG_EXIT_ZERO,"rename: ",str," to: ",new) ;
            }
        }
    }
    stralloc_free(&tmp) ;

    return 1 ;
}

int regex_configure(sv_alltype *alltype,ssexec_t *info, char const *module_dir,char const *module_name,uint8_t conf)
{
    log_flow() ;

    int wstat, r ;
    pid_t pid ;
    size_t clen = alltype->type.module.configure > 0 ? 1 : 0 ;
    size_t module_dirlen = strlen(module_dir), n ;

    stralloc env = STRALLOC_ZERO ;

    char const *newargv[2 + clen] ;
    unsigned int m = 0 ;

    char pwd[module_dirlen + SS_MODULE_CONFIG_DIR_LEN + 1] ;
    auto_strings(pwd,module_dir,SS_MODULE_CONFIG_DIR + 1) ;

    char config_script[module_dirlen + SS_MODULE_CONFIG_DIR_LEN + 1 + SS_MODULE_CONFIG_SCRIPT_LEN + 1] ;
    auto_strings(config_script,module_dir,SS_MODULE_CONFIG_DIR + 1,"/",SS_MODULE_CONFIG_SCRIPT) ;

    r = scan_mode(config_script,S_IFREG) ;
    if (r > 0)
    {
        /** export ssexec_t info value on the environment */
        {
            char owner[UID_FMT];
            owner[uid_fmt(owner,info->owner)] = 0 ;
            char verbo[UINT_FMT];
            verbo[uid_fmt(verbo,VERBOSITY)] = 0 ;
            if (!auto_stra(&env, \
            "MOD_NAME=",module_name,"\n", \
            "MOD_BASE=",info->base.s,"\n", \
            "MOD_LIVE=",info->live.s,"\n", \
            "MOD_TREE=",info->tree.s,"\n", \
            "MOD_SCANDIR=",info->scandir.s,"\n", \
            "MOD_TREENAME=",info->treename.s,"\n", \
            "MOD_OWNER=",owner,"\n", \
            "MOD_COLOR=",info->opt_color ? "1" : "0","\n", \
            "MOD_VERBOSITY=",verbo,"\n", \
            "MOD_MODULE_DIR=",module_dir,"\n", \
            "MOD_SKEL_DIR=",SS_SKEL_DIR,"\n", \
            "MOD_SERVICE_SYSDIR=",SS_SERVICE_SYSDIR,"\n", \
            "MOD_SERVICE_ADMDIR=",SS_SERVICE_ADMDIR,"\n", \
            "MOD_SERVICE_ADMCONFDIR=",SS_SERVICE_ADMCONFDIR,"\n", \
            "MOD_MODULE_SYSDIR=",SS_MODULE_SYSDIR,"\n", \
            "MOD_MODULE_ADMDIR=",SS_MODULE_ADMDIR,"\n", \
            "MOD_SCRIPT_SYSDIR=",SS_SCRIPT_SYSDIR,"\n", \
            "MOD_USER_DIR=",SS_USER_DIR,"\n", \
            "MOD_SERVICE_USERDIR=",SS_SERVICE_USERDIR,"\n", \
            "MOD_SERVICE_USERCONFDIR=",SS_SERVICE_USERCONFDIR,"\n", \
            "MOD_MODULE_USERDIR=",SS_MODULE_USERDIR,"\n", \
            "MOD_SCRIPT_USERDIR=",SS_SCRIPT_USERDIR,"\n"))
                log_warnu_return(LOG_EXIT_ZERO,"append environment variables") ;
        }
        /** environment is not mandatory */
        if (alltype->opts[2] > 0)
        {
            stralloc oenv = STRALLOC_ZERO ;
            stralloc name = STRALLOC_ZERO ;
            stralloc dst = STRALLOC_ZERO ;

            if (!env_prepare_for_write(&name,&dst,&oenv,alltype,conf))
                return 0 ;

            if (!write_env(name.s,oenv.s,dst.s))
                log_warnu_return(LOG_EXIT_ZERO,"write environment") ;

            /** Reads all file from the directory */
            if (!environ_clean_envfile(&env,dst.s))
                log_warnu_return(LOG_EXIT_ZERO,"prepare environment") ;

            if (!environ_remove_unexport(&env,&env))
                log_warnu_return(LOG_EXIT_ZERO,"remove exclamation mark from environment variables") ;

            stralloc_free(&name) ;
            stralloc_free(&oenv) ;
            stralloc_free(&dst) ;
        }

        if (!sastr_split_string_in_nline(&env))
            log_warnu_return(LOG_EXIT_ZERO,"rebuild environment") ;

        n = env_len((const char *const *)environ) + 1 + byte_count(env.s,env.len,'\0') ;
        char const *newenv[n + 1] ;

        if (!env_merge (newenv, n ,(const char *const *)environ,env_len((const char *const *)environ),env.s, env.len))
            log_warnusys_return(LOG_EXIT_ZERO,"build environment") ;

        if (chdir(pwd) < 0)
            log_warnusys_return(LOG_EXIT_ZERO,"chdir to: ",pwd) ;

        m = 0 ;
        newargv[m++] = config_script ;

        if (alltype->type.module.configure > 0)
            newargv[m++] = keep.s + alltype->type.module.configure ;

        newargv[m++] = 0 ;

        log_info("launch script configure of module: ",module_name) ;

        pid = child_spawn0(newargv[0],newargv,newenv) ;

        if (waitpid_nointr(pid,&wstat, 0) < 0)
            log_warnusys_return(LOG_EXIT_ZERO,"wait for: ",config_script) ;

        if (wstat)
            log_warnu_return(LOG_EXIT_ZERO,"run: ",config_script) ;
    }

    stralloc_free(&env) ;

    return 1 ;
}
