/*
 * parser.c
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

#include <oblibs/string.h>
#include <oblibs/types.h>
#include <oblibs/log.h>
#include <oblibs/sastr.h>
#include <oblibs/files.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>//environ

#include <66/resolve.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/environ.h>

/* @sv -> name of the service to parse with
 * the path of the frontend file source
 * @Return 0 on fail
 * @Return 1 on success
 * @Return 2 -> already parsed */
int parse_service_before(ssexec_t *info,stralloc *parsed_list,stralloc *tree_list, char const *sv,unsigned int *nbsv, stralloc *sasv,uint8_t force,uint8_t conf,uint8_t disable_module,char const *directory_forced)
{
    log_flow() ;

    log_trace("start parse process of service: ",sv) ;

    if (sastr_cmp(parsed_list,sv) >= 0)
    {
        log_warn("ignoring: ",sv," service: already parsed") ;
        sasv->len = 0 ;
        return 2 ;
    }

    int insta ;
    uint8_t exist = 0 ;
    size_t svlen = strlen(sv), svnamelen ;
    char svname[svlen + 1], svsrc[svlen + 1] ;
    if (!ob_basename(svname,sv)) log_warnu_return(LOG_EXIT_ZERO,"get basename of: ",sv) ;
    if (!ob_dirname(svsrc,sv)) log_warnu_return(LOG_EXIT_ZERO,"get dirname of: ",sv) ;

    svnamelen = strlen(svname) ;
    char tree[info->tree.len + SS_SVDIRS_LEN + 1] ;
    auto_strings(tree,info->tree.s,SS_SVDIRS) ;

    int r = parse_service_check_enabled(tree,svname,force,&exist) ;
    if (!r) { sasv->len = 0 ; return 2 ; }

    sv_alltype sv_before = SV_ALLTYPE_ZERO ;
    sasv->len = 0 ;

    insta = instance_check(svname) ;
    if (!insta)
    {
        log_warn_return(LOG_EXIT_ZERO, "invalid instance name: ",svname) ;
    }
    else if (insta > 0)
    {
        stralloc tmp = STRALLOC_ZERO ;
        instance_splitname(&tmp,svname,insta,SS_INSTANCE_TEMPLATE) ;
        log_trace("read service file of: ",svsrc,tmp.s) ;
        if (read_svfile(sasv,tmp.s,svsrc) <= 0) return 0 ;
        stralloc_free(&tmp) ;
    }
    else {
        log_trace("read service file of: ",svsrc,svname) ;
        if (read_svfile(sasv,svname,svsrc) <= 0) return 0 ;
    }

    if (!get_svtype(&sv_before,sasv->s))
        log_warn_return (LOG_EXIT_ZERO,"invalid value for key: ",get_key_by_enum(ENUM_KEY_SECTION_MAIN,KEY_MAIN_TYPE)," in service file: ",svsrc,svname) ;


    sv_before.cname.name = keep.len ;
    if (!stralloc_catb(&keep,svname,svnamelen + 1)) return 0 ;

    /** contents of directory should be listed by ss_resolve_src_path
     * execpt for module type */
    if (scan_mode(sv,S_IFDIR) == 1 && sv_before.cname.itype != TYPE_MODULE) return 1 ;

    if (insta > 0)
    {
        if (!instance_create(sasv,svname,SS_INSTANCE_REGEX,insta))
            log_warn_return(LOG_EXIT_ZERO,"create instance service: ",svname) ;

        /** ensure that we have an empty line at the end of the string*/
        if (!stralloc_cats(sasv,"\n")) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
        if (!stralloc_0(sasv)) log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
    }

    if (!parser(&sv_before,sasv,svname,sv_before.cname.itype)) return 0 ;

    if ((sv_before.cname.itype > TYPE_CLASSIC && force > 1) || !exist)
        if (!parse_service_all_deps(info,parsed_list,tree_list,&sv_before,sv,nbsv,sasv,force,conf,directory_forced)) return 0 ;

    if (sv_before.cname.itype == TYPE_MODULE)
    {
        r = parse_module(&sv_before,info,parsed_list,tree_list,svname,svsrc,nbsv,sasv,force,conf) ;
        if (!r) return 0 ;
        else if (r == 2)
        {
            sasv->len = 0 ;
            goto add ;
        }

        if (force > 1 && exist && disable_module)
        {
            char const *newargv[4] ;
            unsigned int m = 0 ;

            newargv[m++] = "fake_name" ;
            newargv[m++] = svname ;
            newargv[m++] = 0 ;
            if (ssexec_disable(m,newargv,(const char *const *)environ,info))
                log_warnu_return(LOG_EXIT_ZERO,"disable module: ",svname) ;
        }
    }
    add:
    if (!parse_add_service(parsed_list,&sv_before,sv,nbsv,info->owner,conf)) return 0 ;

    return 1 ;
}

int parse_service_all_deps(ssexec_t *info,stralloc *parsed_list, stralloc *tree_list, sv_alltype *sv_before,char const *sv, unsigned int *nbsv,stralloc *sasv,uint8_t force, uint8_t conf,char const *directory_forced)
{
    log_flow() ;

    stralloc rebuild = STRALLOC_ZERO ;

    if (!parse_service_deps(info,parsed_list,tree_list,sv_before,sv,nbsv,sasv,force,conf,directory_forced)) return 0 ;
    if (!parse_service_opts_deps(&rebuild,info,parsed_list,tree_list,sv_before,sv,nbsv,sasv,force,conf,KEY_MAIN_EXTDEPS,0)) return 0 ;
    if (!parse_service_opts_deps(&rebuild,info,parsed_list,tree_list,sv_before,sv,nbsv,sasv,force,conf,KEY_MAIN_OPTSDEPS,0)) return 0 ;

    if (rebuild.len)
    {
        size_t pos = 0 ;
        stralloc old = STRALLOC_ZERO ;

        //rebuild the dependencies list of the service
        int id = sv_before->cname.idga ;
        unsigned int nid = sv_before->cname.nga ;

        for (;nid; id += strlen(deps.s + id) + 1, nid--)
        {
            if (!stralloc_catb(&old,deps.s + id,strlen(deps.s + id) + 1))
                    log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
        }
        for (pos = 0 ; pos < rebuild.len ; pos += strlen(rebuild.s + pos) + 1)
        {
            if (!stralloc_catb(&old,rebuild.s + pos,strlen(rebuild.s + pos) + 1))
                log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
        }

        sv_before->cname.idga = deps.len ;
        sv_before->cname.nga = 0 ;

        for (pos = 0 ; pos < old.len ; pos += strlen(old.s + pos) + 1)
        {
            if (!stralloc_catb(&deps,old.s + pos,strlen(old.s + pos) + 1))
                log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;
            sv_before->cname.nga++ ;
        }
        stralloc_free(&old) ;
    }

    stralloc_free(&rebuild) ;

    return 1 ;
}

int parse_service_deps(ssexec_t *info,stralloc *parsed_list,stralloc *tree_list, sv_alltype *sv_before, char const *sv,unsigned int *nbsv,stralloc *sasv,uint8_t force,uint8_t conf,char const *directory_forced)
{
    log_flow() ;

    int r ;
    char *dname = 0 ;
    stralloc newsv = STRALLOC_ZERO ;

    if (sv_before->cname.nga)
    {
        size_t id = sv_before->cname.idga, nid = sv_before->cname.nga ;
        for (;nid; id += strlen(deps.s + id) + 1, nid--)
        {
            newsv.len = 0 ;
            if (sv_before->cname.itype != TYPE_BUNDLE)
            {
                log_trace("service: ",sv, " depends on: ",deps.s+id) ;
            }else log_trace("bundle: ",sv, " contents: ",deps.s+id," as service") ;
            dname = deps.s + id ;
            r = ss_resolve_src_path(&newsv,dname,info->owner,directory_forced) ;
            if (r < 1) goto err ;//don't warn here, the ss_revolve_src_path already warn user

            if (!parse_service_before(info,parsed_list,tree_list,newsv.s,nbsv,sasv,force,conf,0,directory_forced)) goto err ;
        }
    }
    else log_trace(sv,": haven't dependencies") ;
    stralloc_free(&newsv) ;
    return 1 ;
    err:
        stralloc_free(&newsv) ;
        return 0 ;
}

int parse_service_opts_deps(stralloc *rebuild,ssexec_t *info,stralloc *parsed_list,stralloc *tree_list,sv_alltype *sv_before,char const *sv,unsigned int *nbsv,stralloc *sasv,uint8_t force,uint8_t conf,uint8_t mandatory,char const *directory_forced)
{
    log_flow() ;

    int r ;
    stralloc newsv = STRALLOC_ZERO ;

    size_t pos = 0 , baselen = strlen(info->base.s) + SS_SYSTEM_LEN ;
    uint8_t found = 0, ext = mandatory == KEY_MAIN_EXTDEPS ? 1 : 0 ;
    char *optname = 0 ;
    char btmp[baselen + 1] ;
    auto_strings(btmp,info->base.s,SS_SYSTEM) ;

    int idref = sv_before->cname.idopts ;
    unsigned int nref = sv_before->cname.nopts ;
    if (ext) {
        idref = sv_before->cname.idext ;
        nref = sv_before->cname.next ;
    }
    // only pass here for the first time
    if (!tree_list->len)
    {
        if (!sastr_dir_get(tree_list, btmp,SS_BACKUP + 1, S_IFDIR)) log_warnusys_return(LOG_EXIT_ZERO,"get list of tree at: ",btmp) ;
    }

    if (nref)
    {
        // may have no tree yet
        if (tree_list->len)
        {
            size_t id = idref, nid = nref ;
            for (;nid; id += strlen(deps.s + id) + 1, nid--)
            {

                newsv.len = 0 ;
                optname = deps.s + id ;

                for(pos = 0 ; pos < tree_list->len ; pos += strlen(tree_list->s + pos) +1 )
                {
                    found = 0 ;
                    char *tree = tree_list->s + pos ;
                    if (obstr_equal(tree,info->treename.s)) continue ;
                    size_t treelen = strlen(tree) ;
                    char tmp[baselen + 1 + treelen + SS_SVDIRS_LEN + 1] ;
                    auto_strings(tmp,btmp,"/",tree,SS_SVDIRS) ;

                    parse_service_check_enabled(tmp,optname,force,&found) ;
                    if (found)
                    {
                        log_trace(ext ? "external" : "optional"," service dependency: ",optname," is already enabled at tree: ",btmp,"/",tree) ;
                        break ;
                    }
                }
                if (!found)
                {
                    // -1 mean system error. If the service doesn't exist it return 0
                    r = ss_resolve_src_path(&newsv,optname,info->owner,directory_forced) ;
                    if (r == -1) {
                        goto err ; //don't warn here, the ss_revolve_src_path already warn user
                    }
                    else if (!r) {
                        if (ext) goto err ;
                    }
                    // be paranoid with the else if
                    else if (r == 1) {
                        if (!parse_service_before(info,parsed_list,tree_list,newsv.s,nbsv,sasv,force,conf,0,directory_forced))
                            goto err ;

                        // add the new deps
                        if (!stralloc_catb(rebuild,optname,strlen(optname) + 1))
                            log_warnsys_return(LOG_EXIT_ZERO,"stralloc") ;

                        // we only keep the first found on optsdepends
                        if (!ext) break ;
                    }
                }
            }
        }
    }

    stralloc_free(&newsv) ;
    return 1 ;
    err:
        stralloc_free(&newsv) ;
        return 0 ;
}

/** General helper */

int parse_service_check_enabled(char const *tree,char const *svname,uint8_t force,uint8_t *exist)
{
    log_flow() ;

    /** char const tree -> tree.s + SS_SVDIRS */
    size_t namelen = strlen(svname), newlen = strlen(tree) ;
    char tmp[newlen + SS_DB_LEN + SS_SRC_LEN + 1 + namelen + 1] ;
    auto_strings(tmp,tree,SS_DB,SS_SRC,"/",svname) ;
    /** search in db first, the most used */
    if (scan_mode(tmp,S_IFDIR) > 0)
    {
        (*exist) = 1 ;
        if (!force) goto found ;
    }
    /** svc */
    auto_string_from(tmp,newlen,SS_SVC,"/",svname) ;
    if (scan_mode(tmp,S_IFDIR) > 0)
    {
        (*exist) = 1 ;
        if (!force) goto found ;
    }

    return 1 ;
    found:
        log_info("ignoring: ",svname," service: already enabled") ;
        return 0 ;
}

int parse_add_service(stralloc *parsed_list,sv_alltype *sv_before,char const *service,unsigned int *nbsv,uid_t owner,uint8_t conf)
{
    log_flow() ;

    log_trace("add service: ",service) ;
    stralloc saconf = STRALLOC_ZERO ;
    size_t svlen = strlen(service) ;
    // keep overwrite_conf
    sv_before->overwrite_conf = conf ;
    // keep source of the frontend file
    sv_before->src = keep.len ;
    if (!stralloc_catb(&keep,service,svlen + 1)) goto err ;
    // keep service on current list
    if (!stralloc_catb(parsed_list,service,svlen + 1)) goto err ;
    if (!genalloc_append(sv_alltype,&gasv,sv_before)) goto err ;
    (*nbsv)++ ;
    stralloc_free(&saconf) ;
    return 1 ;
    err:
        stralloc_free(&saconf) ;
        return 0 ;
}
