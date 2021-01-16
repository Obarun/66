/*
 * 66-tree.c
 *
 * Copyright (c) 2018-2021-2019 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#include <66/tree.h>

#include <string.h>
#include <stdint.h>//uintx_t
#include <sys/stat.h>
#include <stdio.h>//rename

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>//byte_count

#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/db.h>
#include <66/enum.h>
#include <66/state.h>
#include <66/resolve.h>

#include <s6/s6-supervise.h>
#include <s6-rc/s6rc-servicedir.h>
#include <s6-rc/s6rc-constants.h>

#define USAGE "66-tree [ -h ] [ -z ] [ -v verbosity ] [ -l ] [ -n|R ] [ -a|d ] [ -c ] [ -S after_tree ] [ -E|D ] [ -U ] [ -C clone ] tree"

static stralloc reslive = STRALLOC_ZERO ;
static char const *cleantree = 0 ;

static inline void info_help (void)
{
  static char const *help =
"66-tree <options> tree\n"
"\n"
"options :\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -n: create a new empty tree\n"
"   -a: allow user(s) at tree\n"
"   -d: deny user(s) at tree\n"
"   -c: set tree as default\n"
"   -S: start the tree after after_tree\n"
"   -E: enable the tree\n"
"   -D: disable the tree\n"
"   -R: remove the tree\n"
"   -C: clone the tree\n"
"   -U: unsupervise the tree\n"
;

 if (buffer_putsflush(buffer_1, help) < 0)
    log_dieusys(111,"write to stdout") ;
}

static void cleanup(void)
{
    log_flow() ;

    if (cleantree)
    {
        log_trace("removing: ",cleantree,"...") ;
        rm_rf(cleantree) ;
    }
}

static void auto_string(char *strings,char const *str,size_t baselen)
{
    log_flow() ;

    size_t slen = strlen(str) ;
    memcpy(strings + baselen,str,slen) ;
    strings[baselen+slen] = 0 ;
}

static void auto_dir(char const *dst,mode_t mode)
{
    log_flow() ;

    log_trace("create directory: ",dst) ;
    if (!dir_create_parent(dst,mode))
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"create directory: ",dst) ;
}

static void auto_create(char *strings,char const *str,size_t baselen,mode_t mode)
{
    log_flow() ;

    auto_string(strings,str,baselen) ;
    auto_dir(strings,mode) ;
}

static void auto_check(char *dst,char const *str,size_t baselen,mode_t mode)
{
    log_flow() ;

    auto_string(dst,str,baselen) ;
    if (!scan_mode(dst,mode)) auto_dir(dst,0755) ;
}

static void auto_check_one(char *dst,mode_t mode)
{
    log_flow() ;

    if (!scan_mode(dst,mode)) auto_dir(dst,0755) ;
}

static void inline auto_stralloc(stralloc *sa,char const *str)
{
    log_flow() ;

    if (!stralloc_cats(sa,str)) log_die_nomem("stralloc") ;
}

static void inline auto_stralloc0(stralloc *sa)
{
    log_flow() ;

    if (!stralloc_0(sa)) log_die_nomem("stralloc") ;
    sa->len-- ;
}

static void inline auto_stralloc_0(stralloc *sa,char const *str)
{
    log_flow() ;

    auto_stralloc(sa,str) ;
    auto_stralloc0(sa) ;
}

int sanitize_tree(stralloc *dstree, char const *base, char const *tree,uid_t owner)
{
    log_flow() ;

    ssize_t r ;
    size_t baselen = strlen(base) ;
    size_t treelen = strlen(tree) ;
    char dst[baselen + SS_SYSTEM_LEN + 1 + treelen + 1] ;
    auto_string(dst,base,0) ;

    /** base is /var/lib/66 or $HOME/.66*/
    /** this verification is made in case of
     * first use of 66-*** tools */
    auto_check(dst,SS_SYSTEM,baselen,0755) ;
    /** create extra directory for service part */
    if (!owner)
    {
        auto_check_one(SS_LOGGER_SYSDIR,0755) ;
        auto_check_one(SS_SERVICE_SYSDIR,0755) ;
        auto_check_one(SS_SERVICE_ADMDIR,0755) ;
        auto_check_one(SS_SERVICE_ADMCONFDIR,0755) ;
        auto_check_one(SS_MODULE_SYSDIR,0755) ;
        auto_check_one(SS_MODULE_ADMDIR,0755) ;
        auto_check_one(SS_SCRIPT_SYSDIR,0755) ;
    }
    else
    {
        size_t extralen ;
        stralloc extra = STRALLOC_ZERO ;
        if (!set_ownerhome(&extra,owner))
            log_dieusys(LOG_EXIT_SYS,"set home directory") ;

        extralen = extra.len ;
        auto_stralloc(&extra,SS_USER_DIR) ;
        auto_stralloc_0(&extra,SS_SYSTEM) ;
        auto_check_one(extra.s,0755) ;
        extra.len = extralen ;
        auto_stralloc_0(&extra,SS_LOGGER_USERDIR) ;
        auto_check_one(extra.s,0755) ;
        extra.len = extralen ;
        auto_stralloc_0(&extra,SS_SERVICE_USERDIR) ;
        auto_check_one(extra.s,0755) ;
        extra.len = extralen ;
        auto_stralloc_0(&extra,SS_SERVICE_USERCONFDIR) ;
        auto_check_one(extra.s,0755) ;
        extra.len = extralen ;
        auto_stralloc_0(&extra,SS_MODULE_USERDIR) ;
        auto_check_one(extra.s,0755) ;
        extra.len = extralen ;
        auto_stralloc_0(&extra,SS_SCRIPT_USERDIR) ;
        auto_check_one(extra.s,0755) ;
        stralloc_free(&extra) ;
    }

    auto_check(dst,SS_TREE_CURRENT,baselen,0755) ;
    auto_string(dst,SS_SYSTEM,baselen) ;
    auto_check(dst,SS_BACKUP,baselen + SS_SYSTEM_LEN,0755) ;
    auto_string(dst,SS_STATE,baselen + SS_SYSTEM_LEN) ;
    if (!scan_mode(dst,S_IFREG))
    {
        auto_string(dst,SS_SYSTEM,baselen) ;
        if(!file_create_empty(dst,SS_STATE + 1,0644))
            log_dieusys(LOG_EXIT_SYS,"create ",dst,SS_STATE) ;
    }
    auto_string(dst,"/",baselen + SS_SYSTEM_LEN) ;
    auto_string(dst,tree,baselen + SS_SYSTEM_LEN + 1) ;
    r = scan_mode(dst,S_IFDIR) ;
    if (r == -1) log_die(LOG_EXIT_SYS,"invalid directory: ",dst) ;
    /** we have one, keep it*/
    if (!stralloc_cats(dstree,dst)) log_die_nomem("stralloc") ;
    if (!stralloc_0(dstree)) log_die_nomem("stralloc") ;

    if (!r) return 0 ;

    return 1 ;
}

void create_tree(char const *tree,char const *treename)
{
    log_flow() ;

    size_t newlen = 0 ;
    size_t treelen = strlen(tree) ;

    char dst[treelen + SS_SVDIRS_LEN + SS_DB_LEN + SS_SRC_LEN + 16 + 1] ;
    ss_resolve_t res = RESOLVE_ZERO ;
    ss_resolve_init(&res) ;

    memcpy(dst, tree, treelen) ;
    newlen = treelen ;
    dst[newlen] = 0 ;

    res.name = ss_resolve_add_string(&res,SS_MASTER+1) ;
    res.description = ss_resolve_add_string(&res,"inner bundle - do not use it") ;
    res.tree = ss_resolve_add_string(&res,dst) ;
    res.treename = ss_resolve_add_string(&res,treename) ;
    res.type = TYPE_BUNDLE ;
    res.disen = 1 ;

    auto_create(dst,SS_SVDIRS,newlen,0755) ;
    auto_create(dst,SS_RULES,newlen,0755) ;
    auto_string(dst,SS_SVDIRS,newlen) ;
    newlen = newlen + SS_SVDIRS_LEN ;
    auto_create(dst,SS_DB,newlen,0755) ;
    auto_create(dst,SS_SVC,newlen,0755) ;
    auto_create(dst,SS_RESOLVE,newlen,0755) ;
    dst[newlen] = 0 ;
    log_trace("write resolve file of inner bundle") ;
    if (!ss_resolve_write(&res,dst,SS_MASTER+1))
    {
        ss_resolve_free(&res) ;
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"write resolve file of inner bundle") ;
    }
    ss_resolve_free(&res) ;

    char sym[newlen + 1 + SS_SYM_SVC_LEN + 1] ;
    char dstsym[newlen + SS_SVC_LEN + 1] ;

    auto_string(sym,dst,0) ;
    auto_string(sym,"/",newlen) ;
    auto_string(sym,SS_SYM_SVC,newlen + 1) ;

    auto_string(dstsym,dst,0) ;
    auto_string(dstsym,SS_SVC,newlen) ;
    log_trace("point symlink: ",sym," to ",dstsym) ;
    if (symlink(dstsym,sym) < 0)
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"symlink: ", sym) ;

    auto_string(sym,SS_SYM_DB,newlen + 1) ;
    auto_string(dstsym,SS_DB,newlen) ;
    log_trace("point symlink: ",sym," to ",dstsym) ;
    if (symlink(dstsym,sym) < 0)
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"symlink: ", sym) ;

    auto_string(dst,SS_DB,newlen) ;
    newlen = newlen + SS_DB_LEN ;
    auto_create(dst,SS_SRC,newlen,0755) ;
    auto_string(dst,SS_SRC,newlen) ;
    newlen = newlen + SS_SRC_LEN ;
    auto_create(dst,SS_MASTER,newlen,0755) ;
    auto_string(dst,SS_MASTER,newlen) ;
    newlen = newlen + SS_MASTER_LEN ;
    log_trace("create file: ",dst,"/contents") ;
    if (!file_create_empty(dst,"contents",0644))
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"create: ",dst,"/contents") ;

    auto_string(dst,"/type",newlen) ;
    log_trace("create file: ",dst) ;
    if(!openwritenclose_unsafe(dst,"bundle\n",7))
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"write: ",dst) ;


}

void create_backupdir(char const *base, char const *treename)
{
    log_flow() ;

    int r ;

    size_t baselen = strlen(base) - 1 ;//remove the last slash
    size_t treenamelen = strlen(treename) ;

    char treetmp[baselen + 1 + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treenamelen + 1] ;

    auto_string(treetmp,base,0) ;
    auto_string(treetmp,"/",baselen) ;
    auto_string(treetmp,SS_SYSTEM,baselen + 1) ;
    auto_string(treetmp,SS_BACKUP,baselen + 1 + SS_SYSTEM_LEN) ;
    auto_string(treetmp,"/",baselen + 1 + SS_SYSTEM_LEN + SS_BACKUP_LEN) ;
    auto_string(treetmp,treename,baselen + 1 + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1) ;

    r = scan_mode(treetmp,S_IFDIR) ;
    if (r || (r == -1))
    {
        log_trace("remove existing backup: ",treetmp) ;
        if (rm_rf(treetmp) < 0)
            log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"remove: ",treetmp) ;
    }
    if (!r) auto_dir(treetmp,0755) ;
}

/** @what -> 0 deny
 * @what -> 1 allow */
void set_rules(char const *tree,uid_t *uids, size_t uidn,uint8_t what)
{
    log_flow() ;

    log_trace("set ", !what ? "denied" : "allowed"," user for tree: ",tree,"..." ) ;

    int r ;
    size_t treelen = strlen(tree) ;

    char pack[256] ;
    char tmp[treelen + SS_RULES_LEN + 1] ;

    auto_string(tmp,tree,0) ;
    auto_string(tmp,SS_RULES,treelen) ;

    if (!uidn && what)
    {
        uids[0] = 1 ;
        uids[1] = MYUID ;
        uidn++ ;
    }
    if (what) //allow
    {
        for (size_t i = 0 ; i < uidn ; i++)
        {
            uint32_pack(pack,uids[i+1]) ;
            pack[uint_fmt(pack,uids[i+1])] = 0 ;
            log_trace("create file: ",pack," at ",tmp) ;
            if(!file_create_empty(tmp,pack,0644) && errno != EEXIST)
                log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"set permissions access") ;

            log_trace("user: ",pack," is allowed for tree: ",tree) ;
        }
        return ;
    }
    //else deny
    for (size_t i = 0 ; i < uidn ; i++)
    {
        if (MYUID == uids[i+1]) continue ;
        uint32_pack(pack,uids[i+1]) ;
        pack[uint_fmt(pack,uids[i+1])] = 0 ;
        char ut[treelen + SS_RULES_LEN + 1 + uint_fmt(pack,uids[i+1]) + 1] ;
        memcpy(ut,tmp,treelen + SS_RULES_LEN) ;
        memcpy(ut + treelen + SS_RULES_LEN,"/",1) ;
        memcpy(ut + treelen + SS_RULES_LEN + 1,pack,uint_fmt(pack,uids[i+1])) ;
        ut[treelen + SS_RULES_LEN + 1 + uint_fmt(pack,uids[i + 1])] = 0 ;
        r = scan_mode(tmp,S_IFREG) ;
        if (r == 1)
        {
            log_trace("unlink: ",ut) ;
            r = unlink(ut) ;
            if (r == -1)
                log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"set permissions access") ;
        }
        log_trace("user: ",pack," is denied for tree: ",tree) ;
    }
}

void tree_unsupervise(stralloc *live, char const *tree, char const *treename,uid_t owner,char const *const *envp)
{
    log_flow() ;

    int r, wstat ;
    pid_t pid ;
    size_t treenamelen = strlen(treename) ;
    size_t newlen ;

    stralloc scandir = STRALLOC_ZERO ;
    stralloc livestate = STRALLOC_ZERO ;
    stralloc livetree = STRALLOC_ZERO ;
    stralloc list = STRALLOC_ZERO ;
    stralloc realsym = STRALLOC_ZERO ;

    /** set what we need */
    char prefix[treenamelen + 2] ;
    auto_string(prefix,treename,0) ;
    auto_string(prefix,"-",treenamelen) ;

    auto_stralloc(&scandir,live->s) ;
    r = set_livescan(&scandir,owner) ;
    if (!r) log_dieusys(LOG_EXIT_SYS,"set livescan directory") ;

    auto_stralloc(&livestate,live->s) ;
    r = set_livestate(&livestate,owner) ;
    if (!r) log_dieusys(LOG_EXIT_SYS,"set livestate directory") ;

    auto_stralloc(&livetree,live->s) ;
    r = set_livetree(&livetree,owner) ;
    if (!r) log_dieusys(LOG_EXIT_SYS,"set livetree directory") ;
    auto_stralloc(&livetree,"/") ;
    newlen = livetree.len ;
    auto_stralloc0(&livetree) ;

    auto_stralloc(&livestate,"/") ;
    auto_stralloc(&livestate,treename) ;
    auto_stralloc0(&livestate) ;
    /** works begin */
    if (scan_mode(livestate.s,S_IFDIR))
        if (!sastr_dir_get(&list,livestate.s,SS_LIVETREE_INIT,S_IFREG)) log_dieusys(LOG_EXIT_SYS,"get service list at: ",livestate.s) ;

    if (!list.len)
    {
        /** No service to unsupervise but we need to
         * delete the livestate anyway*/
        log_warn("No service to unsupervise: ",livestate.s) ;
        /** pass through s6rc_servicedir_unsupervise just in case */
        goto follow ;
    }

    {
        size_t i = 0, len = sastr_len(&list) ;
        char const *newargv[9 + len + 1] ;
        unsigned int m = 0 ;
        char fmt[UINT_FMT] ;
        fmt[uint_fmt(fmt, VERBOSITY)] = 0 ;

        newargv[m++] = SS_BINPREFIX "66-stop" ;
        newargv[m++] = "-v" ;
        newargv[m++] = fmt ;
        newargv[m++] = "-l" ;
        newargv[m++] = live->s ;
        newargv[m++] = "-t" ;
        newargv[m++] = treename ;
        newargv[m++] = "-u" ;

        len = list.len ;
        for (;i < len; i += strlen(list.s + i) + 1)
            newargv[m++] = list.s+i ;

        newargv[m++] = 0 ;

        pid = child_spawn0(newargv[0],newargv,envp) ;
        if (waitpid_nointr(pid,&wstat, 0) < 0)
            log_dieusys(LOG_EXIT_SYS,"wait for: ",newargv[0]) ;
        /** we don't want to die here, we try we can do to stop correctly
         * service list, in any case we want to unsupervise */
        if (wstat) log_warnusys("stop services") ;
    }

    follow:
    if (db_find_compiled_state(livetree.s,treename) >=0)
    {
        list.len = 0 ;
        livetree.len = newlen ;
        auto_stralloc(&livetree,treename) ;
        auto_stralloc(&livetree,SS_SVDIRS) ;
        auto_stralloc0(&livetree) ;
        if (!sastr_dir_get(&list,livetree.s,"",S_IFDIR)) log_dieusys(LOG_EXIT_SYS,"get service list at: ",livetree.s) ;
        livetree.len = newlen ;
        auto_stralloc(&livetree,treename) ;
        auto_stralloc0(&livetree) ;

        size_t i = 0, len = list.len ;
        for (;i < len; i += strlen(list.s + i) + 1)
            s6rc_servicedir_unsupervise(livetree.s,prefix,list.s + i,0) ;

        r = sarealpath(&realsym,livetree.s) ;
        if (r < 0 ) log_dieusys(LOG_EXIT_SYS,"find realpath of: ",livetree.s) ;
        auto_stralloc0(&realsym) ;
        if (rm_rf(realsym.s) < 0) log_dieusys(LOG_EXIT_SYS,"remove: ", realsym.s) ;
        if (rm_rf(livetree.s) < 0) log_dieusys(LOG_EXIT_SYS,"remove: ", livetree.s) ;
        livetree.len = newlen ;
        auto_stralloc(&livetree,treename) ;
        auto_stralloc0(&livetree) ;
        /** remove the symlink itself */
        unlink_void(livetree.s) ;
    }

    if (scandir_send_signal(scandir.s,"an") <= 0) log_dieusys(LOG_EXIT_SYS,"reload scandir: ",scandir.s) ;
    /** remove /run/66/state/uid/treename directory */
    log_trace("delete: ",livestate.s,"..." ) ;
    if (rm_rf(livestate.s) < 0) log_dieusys(LOG_EXIT_SYS,"delete ",livestate.s) ;

    log_info("Unsupervised successfully tree: ",treename) ;

    stralloc_free(&scandir) ;
    stralloc_free(&livestate) ;
    stralloc_free(&livetree) ;
    stralloc_free(&list) ;
    stralloc_free(&realsym) ;
}

/** @action -> 0 disable
 * @action -> 1 enable */
void tree_enable_disable(char const *base, char const *dst, char const *tree,uint8_t action)
{
    log_flow() ;

    int r ;
    log_trace(!action ? "disable " : "enable ",dst,"...") ;
    r  = tree_cmd_state(VERBOSITY,!action ? "-d" : "-a", tree) ;
    if (!r) log_dieusys(LOG_EXIT_SYS,!action ? "disable: " : "enable: ",dst," at: ",base,SS_SYSTEM,SS_STATE) ;
    else if (r == 1)
    {
        log_info(!action ? "Disabled" : "Enabled"," successfully tree: ",tree) ;
    }
    else log_info("Already ",!action ? "disabled" : "enabled"," tree: ",tree) ;

}

void tree_modify_resolve(ss_resolve_t *res,ss_resolve_enum_t field,char const *regex,char const *by)
{
    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;
    ss_resolve_t modif = RESOLVE_ZERO ;

    log_trace("modify field: ",ss_resolve_field_table[field].field," of service: ",res->sa.s + res->name) ;

    if (!ss_resolve_copy(&modif,res))
        log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"copy resolve file of: ", res->sa.s + res->name) ;

    if (!ss_resolve_put_field_to_sa(&sa,&modif, field))
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"get copy field: ",ss_resolve_field_table[field].field) ;

    if (sa.len)
        if (!sastr_replace(&sa,regex,by))
            log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"replace field: ",ss_resolve_field_table[field].field) ;

    if (!ss_resolve_modify_field(&modif,field,sa.s))
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"modify field: ",ss_resolve_field_table[field].field) ;

    if (!ss_resolve_copy(res,&modif))
        log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"copy resolve file of: ",res->sa.s + res->name) ;

    stralloc_free(&sa) ;

    ss_resolve_free(&modif) ;
}

void tree_remove(char const *base,char const *dst,char const *tree)
{
    log_flow() ;

    log_trace("delete: ",dst,"..." ) ;

    int r ;

    if (rm_rf(dst) < 0) log_dieusys(LOG_EXIT_SYS,"delete: ", dst) ;

    size_t treelen = strlen(tree) ;
    size_t baselen = strlen(base) ;
    char treetmp[baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treelen  + 1] ;
    auto_string(treetmp,base,0) ;
    auto_string(treetmp,SS_SYSTEM,baselen) ;
    auto_string(treetmp,SS_BACKUP,baselen + SS_SYSTEM_LEN) ;
    auto_string(treetmp,"/",baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN) ;
    auto_string(treetmp,tree,baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1) ;

    r = scan_mode(treetmp,S_IFDIR) ;
    if (r || (r < 0))
    {
        log_trace("delete backup of tree: ",treetmp,"...") ;
        if (rm_rf(treetmp) < 0) log_dieusys(LOG_EXIT_SYS,"delete: ",treetmp) ;
    }

    tree_enable_disable(base,dst,tree,0) ;

    log_info("Deleted successfully: ",tree) ;
}

int main(int argc, char const *const *argv,char const *const *envp)
{
    int r, current, create, allow, deny, enable, disable, remove, snap, unsupervise ;

    uid_t owner ;

    size_t auidn = 0 ;
    uid_t auids[256] = { 0 } ;

    size_t duidn = 0 ;
    uid_t duids[256] = { 0 } ;

    char const *tree = 0 ;
    char const *after_tree = 0 ;

    stralloc base = STRALLOC_ZERO ;
    stralloc dstree = STRALLOC_ZERO ;
    stralloc clone = STRALLOC_ZERO ;
    stralloc live = STRALLOC_ZERO ;

    log_color = &log_color_disable ;

    current = create = allow = deny = enable = disable = remove = snap = unsupervise = 0 ;

    PROG = "66-tree" ;
    {
        subgetopt_t l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, "hv:l:na:d:cS:EDRC:Uz", &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
            switch (opt)
            {
                case 'h' :  info_help(); return 0 ;
                case 'v' :  if (!uint0_scan(l.arg, &VERBOSITY)) log_usage(USAGE) ; break ;
                case 'l' :  if (!stralloc_cats(&live,l.arg)) log_die_nomem("stralloc") ;
                            if (!stralloc_0(&live)) log_die_nomem("stralloc") ;
                            break ;
                case 'n' :  create = 1 ; break ;
                case 'a' :  if (!scan_uidlist_wdelim(l.arg,auids,',')) log_usage(USAGE) ;
                            auidn = auids[0] ;
                            allow = 1 ;
                            break ;
                case 'd' :  if (!scan_uidlist_wdelim(l.arg,duids,',')) log_usage(USAGE) ;
                            duidn = duids[0] ;
                            deny = 1 ;
                            break ;
                case 'c' :  current = 1 ; break ;
                case 'S' :  after_tree = l.arg ; break ;
                case 'E' :  enable = 1 ; if (disable) log_usage(USAGE) ; break ;
                case 'D' :  disable = 1 ; if (enable) log_usage (USAGE) ; break ;
                case 'R' :  remove = 1 ; if (create) log_usage(USAGE) ; break ;
                case 'C' :  if (remove) log_usage(USAGE) ;
                            if (!stralloc_cats(&clone,l.arg)) log_die_nomem("stralloc") ;
                            if (!stralloc_0(&clone)) log_die_nomem("stralloc") ;
                            snap = 1 ;
                            break ;
                case 'U' :  unsupervise = 1 ;
                            if (create)log_usage(USAGE) ;
                            log_1_warn("deprecated option -- please use 66-all unsupervise instead") ;
                            break ;
                case 'z' :  log_color = !isatty(1) ? &log_color_disable : &log_color_enable ; break ;
                default :   log_usage(USAGE) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc != 1) log_usage(USAGE) ;

    tree = argv[0] ;
    owner = MYUID ;

    if (!set_ownersysdir(&base, owner)) log_dieusys(LOG_EXIT_SYS,"set owner directory") ;

    r = set_livedir(&live) ;
    if (!r) log_die_nomem("stralloc") ;
    if(r < 0) log_dieu(LOG_EXIT_SYS,"livedir: ",live.s," must be an absolute path") ;

    log_trace("sanitize ",tree,"..." ) ;
    r = sanitize_tree(&dstree,base.s,tree,owner) ;

    if(!r && create)
    {
        /** set cleanup */
        cleantree = dstree.s ;
        log_trace("creating: ",dstree.s,"..." ) ;
        create_tree(dstree.s,tree) ;

        log_trace("creating backup directory for: ", tree,"...") ;
        create_backupdir(base.s,tree) ;
        /** unset cleanup */
        cleantree = 0 ;

        log_trace("set permissions rules for: ",dstree.s,"..." ) ;
            set_rules(dstree.s,auids,auidn,1) ;

        size_t dblen = dstree.len - 1 ;
        char newdb[dblen + SS_SVDIRS_LEN + 1] ;
        auto_string(newdb,dstree.s,0) ;
        auto_string(newdb,SS_SVDIRS,dblen) ;

        log_trace("compile: ",newdb,"/db/",tree,"..." ) ;
        if (!db_compile(newdb,dstree.s,tree,envp))
            log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"compile ",newdb,"/db/",tree) ;

        r = 1 ;
        create = 0 ;
        log_info("Created successfully tree: ",tree) ;
    }

    if ((!r && !create) || (!r && enable)) log_dieusys(LOG_EXIT_SYS,"find tree: ",dstree.s) ;
    if (r && create) log_dieu(LOG_EXIT_USER,"create: ",dstree.s,": already exist") ;

    if (enable)
        tree_enable_disable(base.s,dstree.s,tree,1) ;

    if (disable)
        tree_enable_disable(base.s,dstree.s,tree,0) ;

    if (auidn)
        set_rules(dstree.s,auids,auidn,1) ;

    if (duidn)
        set_rules(dstree.s,duids,duidn,0) ;

    if(current)
    {
        log_trace("make: ",dstree.s," as default ..." ) ;
        if (!tree_switch_current(base.s,tree)) log_dieusys(LOG_EXIT_SYS,"set: ",dstree.s," as default") ;
        log_info("Set successfully: ",tree," as default") ;
    }

    if (unsupervise)
        tree_unsupervise(&live,dstree.s,tree,owner,envp) ;

    if (remove)
        tree_remove(base.s,dstree.s,tree) ;

    if (snap)
    {
        stralloc salist = STRALLOC_ZERO ;
        ss_resolve_t res = RESOLVE_ZERO ;

        size_t syslen = base.len + SS_SYSTEM_LEN ;
        size_t treelen = strlen(tree) ;
        size_t pos = 0 ;

        char system[syslen + 1] ;
        auto_strings(system,base.s,SS_SYSTEM) ;

        size_t clone_target_len = syslen + 1 + clone.len ;
        char clone_target[clone_target_len + 1] ;
        auto_strings(clone_target,system,"/",clone.s) ;

        r = scan_mode(clone_target,S_IFDIR) ;
        if ((r < 0) || r) log_die(LOG_EXIT_SYS,clone_target,": already exist") ;

        // clone main directory
        log_trace("clone: ",tree," as: ",clone.s,"..." ) ;
        if (!hiercopy(dstree.s,clone_target))
            log_dieusys(LOG_EXIT_SYS,"copy: ",dstree.s," to: ",clone_target) ;

        // clone backup directory
        size_t clone_backup_len = syslen + SS_BACKUP_LEN + 1 + clone.len ;
        char clone_backup[clone_backup_len + 1] ;
        auto_strings(clone_backup,system,SS_BACKUP,"/",clone.s) ;

        char tree_backup[syslen + SS_BACKUP_LEN + 1 + treelen + 1] ;
        auto_strings(tree_backup,system,SS_BACKUP,"/",tree) ;

        /* make cleantree pointing to the clone to be able to remove it
         * in case of crach */
        cleantree = clone_target ;

        log_trace("clone backup of: ",tree," as: ",clone.s,"..." ) ;
        if (!hiercopy(tree_backup,clone_backup))
            log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"copy: ",tree_backup," to: ",clone_backup) ;

        // modify the resolve file to match the new name
        // main directory first
        char src_resolve[clone_target_len + SS_SVDIRS_LEN + SS_RESOLVE_LEN + 1] ;
        auto_strings(src_resolve,clone_target,SS_SVDIRS,SS_RESOLVE) ;

        if (!sastr_dir_get(&salist,src_resolve,"",S_IFREG))
            log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"get resolve file at: ",src_resolve) ;

        char clone_res[clone_target_len + SS_SVDIRS_LEN + 1] ;
        auto_strings(clone_res,clone_target,SS_SVDIRS) ;

        for (;pos < salist.len ; pos += strlen(salist.s + pos) + 1)
        {
            char *name = salist.s + pos ;

            if (!ss_resolve_read(&res,clone_res,name))
                log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"read resolve file of: ",src_resolve,"/",name) ;

            tree_modify_resolve(&res,SS_RESOLVE_ENUM_RUNAT,tree,clone.s) ;
            tree_modify_resolve(&res,SS_RESOLVE_ENUM_TREENAME,tree,clone.s) ;
            tree_modify_resolve(&res,SS_RESOLVE_ENUM_TREE,tree,clone.s) ;
            tree_modify_resolve(&res,SS_RESOLVE_ENUM_STATE,tree,clone.s) ;

            if (!ss_resolve_write(&res,clone_res,name))
                log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"write resolve file of: ",src_resolve,"/",name) ;
        }

        // rename db
        char clone_db_old[clone_target_len + SS_SVDIRS_LEN + SS_DB_LEN + 1 + treelen + 1] ;
        auto_strings(clone_db_old,clone_target,SS_SVDIRS,SS_DB,"/",tree) ;

        char clone_db_new[clone_target_len + SS_SVDIRS_LEN + SS_DB_LEN + 1 + clone.len + 1] ;
        auto_strings(clone_db_new,clone_target,SS_SVDIRS,SS_DB,"/",clone.s) ;

        log_trace("rename tree db: ",tree," as: ",clone.s,"..." ) ;
        if (rename(clone_db_old,clone_db_new) == -1)
            log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"rename: ",clone_db_old," to: ",clone_db_new) ;

        // backup directory
        char src_resolve_backup[clone_backup_len + SS_RESOLVE_LEN + 1] ;
        auto_strings(src_resolve_backup,clone_backup,SS_RESOLVE) ;

        salist.len = 0 ;

        /** main and backup can differs,so rebuild the list
         * Also, a backup directory can be empty, check it first */
        r = scan_mode(src_resolve_backup,S_IFDIR) ;
        if (r == 1)
        {
            if (!sastr_dir_get(&salist,src_resolve_backup,"",S_IFREG))
                log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"get resolve file at: ",src_resolve_backup) ;

            for (pos = 0 ; pos < salist.len ; pos += strlen(salist.s + pos) + 1)
            {
                char *name = salist.s + pos ;

                if (!ss_resolve_read(&res,clone_backup,name))
                    log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"read resolve file of: ",src_resolve_backup,"/",name) ;

                tree_modify_resolve(&res,SS_RESOLVE_ENUM_RUNAT,tree,clone.s) ;
                tree_modify_resolve(&res,SS_RESOLVE_ENUM_TREENAME,tree,clone.s) ;
                tree_modify_resolve(&res,SS_RESOLVE_ENUM_TREE,tree,clone.s) ;
                tree_modify_resolve(&res,SS_RESOLVE_ENUM_STATE,tree,clone.s) ;

                if (!ss_resolve_write(&res,clone_backup,name))
                    log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"write resolve file of: ",src_resolve,"/",name) ;
            }
            // rename db
            char clone_db_backup_old[clone_backup_len + SS_DB_LEN + 1 + treelen + 1] ;
            auto_strings(clone_db_backup_old,clone_backup,SS_DB,"/",tree) ;

            char clone_db_backup_new[clone_backup_len + SS_DB_LEN + 1 + clone.len + 1] ;
            auto_strings(clone_db_backup_new,clone_backup,SS_DB,"/",clone.s) ;

            log_trace("rename tree backup db: ",tree," as: ",clone.s,"..." ) ;
            if (rename(clone_db_backup_old,clone_db_backup_new) == -1)
                log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"rename: ",clone_db_backup_old," to: ",clone_db_backup_new) ;
        }

        stralloc_free(&salist) ;
        ss_resolve_free(&res) ;

        log_info("Cloned successfully: ",tree," to ",clone.s) ;
    }

    if (after_tree)
    {
        stralloc contents = STRALLOC_ZERO ;
        stralloc tmp = STRALLOC_ZERO ;
        int befirst = obstr_equal(tree,after_tree) ;
        size_t baselen = base.len, pos = 0 ;
        char ste[baselen + SS_SYSTEM_LEN + 1] ;
        auto_strings(ste,base.s,SS_SYSTEM) ;

        int enabled = tree_cmd_state(VERBOSITY,"-s",tree) ;
        if (enabled != 1)
            log_die(LOG_EXIT_USER,"tree: ",tree," is not enabled") ;

        enabled = tree_cmd_state(VERBOSITY,"-s",after_tree) ;
        if (enabled != 1)
            log_die(LOG_EXIT_USER,"tree: ",after_tree," is not enabled") ;

        r  = tree_cmd_state(VERBOSITY,"-d",tree) ;
        if (!r) log_dieusys(LOG_EXIT_SYS,"disable: ",tree," at: ",ste,SS_STATE) ;

        r = file_readputsa(&tmp,ste,SS_STATE + 1) ;
        if(!r) log_dieusys(LOG_EXIT_SYS,"open: ", ste,SS_STATE) ;

        /** if you have only one tree enabled, the state file will be
         * empty because we disable it before reading the file(see line 803).
         * This will make a crash at sastr_? call.
         * So write directly the name of the tree at state file. */

        if (tmp.len)
        {
            if (!sastr_rebuild_in_oneline(&tmp) ||
            !sastr_clean_element(&tmp))
                log_dieu(LOG_EXIT_SYS,"rebuild state list") ;

            for (pos = 0 ;pos < tmp.len; pos += strlen(tmp.s + pos) + 1)
            {
                char *name = tmp.s + pos ;

                /* e.g 66-tree -S root root -> meaning root need to
                 * be the first to start */
                if ((befirst) && (pos == 0))
                {
                    if (!auto_stra(&contents,tree,"\n"))
                        log_dieusys(LOG_EXIT_SYS,"stralloc") ;
                }
                if (!auto_stra(&contents,name,"\n"))
                    log_dieusys(LOG_EXIT_SYS,"stralloc") ;

                if (obstr_equal(name,after_tree))
                {
                    if (!auto_stra(&contents,tree,"\n"))
                        log_dieusys(LOG_EXIT_SYS,"stralloc") ;
                }
            }
        }
        else
        {
            if (!auto_stra(&contents,tree,"\n"))
                log_dieusys(LOG_EXIT_SYS,"stralloc") ;
        }

        if (!file_write_unsafe(ste,SS_STATE + 1,contents.s,contents.len))
            log_dieusys(LOG_EXIT_ZERO,"write: ",ste,SS_STATE) ;

        log_info("Ordered successfully: ",tree," starts after tree: ",after_tree) ;
    }

    stralloc_free(&reslive) ;
    stralloc_free(&live) ;
    stralloc_free(&base) ;
    stralloc_free(&dstree) ;
    stralloc_free(&clone) ;

    return 0 ;
}
