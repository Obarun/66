/*
 * ssexec_tree.c
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

#include <66/tree.h>

#include <string.h>
#include <stdint.h>//uintx_t
#include <sys/stat.h>
#include <stdio.h>//rename
#include <pwd.h>

#include <oblibs/obgetopt.h>
#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>
#include <oblibs/graph.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/bytestr.h>//byte_count
#include <skalibs/posixplz.h>//unlink_void

#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/db.h>
#include <66/enum.h>
#include <66/state.h>
#include <66/service.h>
#include <66/resolve.h>

#include <s6/supervise.h>
#include <s6-rc/s6rc-servicedir.h>
#include <s6-rc/s6rc-constants.h>


/**
 *
 *
 *
 * -o depends=boot,system:requiredby=graphics
 *
 * depends/requiredby automatically enable the trees found at field except for boot
 *
 * -S options dissappear but it kept as it with a warn to user for the compatibility.
 *
 * -G for groups
 *
 *
 * */
#define TREE_COLON_DELIM ':'
#define TREE_COMMA_DELIM ','
#define TREE_MAXOPTS 4
#define tree_checkopts(n) if (n >= TREE_MAXOPTS) log_die(LOG_EXIT_USER, "too many -o options")
stralloc TREE_SAOPTS = STRALLOC_ZERO ;

static char const *cleantree = 0 ;
static graph_t GRAPH = GRAPH_ZERO ;

typedef struct tree_opts_map_s tree_opts_map_t ;
struct tree_opts_map_s
{
    char const *str ;
    int const id ;
} ;

typedef enum enum_tree_opts_e enum_tree_opts_t, *enum_tree_opts_t_ref ;
enum enum_ns_opts_e
{
    TREE_OPTS_DEPENDS = 0,
    TREE_OPTS_REQUIREDBY,
    TREE_OPTS_RENAME,
    TREE_OPTS_ENDOFKEY
} ;

tree_opts_map_t const tree_opts_table[] =
{
    { .str = "depends", .id = TREE_OPTS_DEPENDS },
    { .str = "requiredby", .id = TREE_OPTS_REQUIREDBY },
    { .str = "rename", .id = TREE_OPTS_RENAME },
    { .str = 0 }
} ;

typedef struct tree_opts_s tree_opts_t, *tree_opts_t_ref ;
struct tree_opts_s
{
    int depends ; //index of depends at TREE_SAOPTS
    int requiredby ; //index of requiredby at TREE_SAOPTS
    int rename ; //index of rename at TREE_SAOPTS
} ;
#define TREE_OPTS_ZERO { -1, -1, -1 }

static void cleanup(void)
{
    log_flow() ;

    if (cleantree) {

        log_trace("removing: ",cleantree,"...") ;
        rm_rf(cleantree) ;
    }
}

static void auto_dir(char const *dst,mode_t mode)
{
    log_flow() ;

    log_trace("create directory: ",dst) ;
    if (!dir_create_parent(dst,mode))
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"create directory: ",dst) ;
}

static void auto_create(char *strings,char const *str, size_t len)
{
    log_flow() ;

    auto_strings(strings + len, str) ;
    auto_dir(strings,0755) ;
}

static void auto_check(char *dst)
{
    log_flow() ;

    int r = scan_mode(dst,S_IFDIR) ;

    if (r == -1)
        log_diesys_nclean(LOG_EXIT_SYS, &cleanup, "conflicting format for: ", dst) ;

    if (!r)
        auto_dir(dst,0755) ;
}

static void inline auto_stralloc(stralloc *sa,char const *str)
{
    log_flow() ;

    if (!auto_stra(sa,str))
        log_die_nomem("stralloc") ;
}

static ssize_t tree_get_key(char *table,char const *str)
{
    ssize_t pos = -1 ;

    pos = get_len_until(str,'=') ;

    if (pos == -1)
        return -1 ;

    auto_strings(table,str) ;

    table[pos] = 0 ;

    pos++ ; // remove '='

    return pos ;
}

int sanitize_tree(ssexec_t *info, char const *tree)
{
    log_flow() ;

    ssize_t r ;
    size_t baselen = info->base.len ;
    size_t treelen = strlen(tree) ;
    uid_t log_uid ;
    gid_t log_gid ;
    char dst[baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + 1 + treelen + 1] ;
    auto_strings(dst,info->base.s, SS_SYSTEM) ;

    /** base is /var/lib/66 or $HOME/.66*/
    /** this verification is made in case of
     * first use of 66-*** tools */
    auto_check(dst) ;
    /** create extra directory for service part */
    if (!info->owner) {

        auto_strings(dst,info->base.s, SS_SYSTEM, SS_RESOLVE) ;
        auto_check(dst) ;

        auto_check(SS_LOGGER_SYSDIR) ;

        if (!youruid(&log_uid,SS_LOGGER_RUNNER) ||
        !yourgid(&log_gid,log_uid))
            log_dieusys(LOG_EXIT_SYS,"get uid and gid of: ",SS_LOGGER_RUNNER) ;

        if (chown(SS_LOGGER_SYSDIR,log_uid,log_gid) == -1)
            log_dieusys(LOG_EXIT_SYS,"chown: ",SS_LOGGER_RUNNER) ;

        auto_check(SS_SERVICE_SYSDIR) ;
        auto_check(SS_SERVICE_ADMDIR) ;
        auto_check(SS_SERVICE_ADMCONFDIR) ;
        auto_check(SS_MODULE_SYSDIR) ;
        auto_check(SS_MODULE_ADMDIR) ;
        auto_check(SS_SCRIPT_SYSDIR) ;
        auto_check(SS_SEED_ADMDIR) ;
        auto_check(SS_SEED_SYSDIR) ;

    } else {

        size_t extralen ;
        stralloc extra = STRALLOC_ZERO ;
        if (!set_ownerhome(&extra,info->owner))
            log_dieusys(LOG_EXIT_SYS,"set home directory") ;

        extralen = extra.len ;
        if (!auto_stra(&extra, SS_USER_DIR, SS_SYSTEM))
            log_die_nomem("stralloc") ;
        auto_check(extra.s) ;

        extra.len = extralen ;
        auto_stralloc(&extra,SS_LOGGER_USERDIR) ;
        auto_check(extra.s) ;

        extra.len = extralen ;
        auto_stralloc(&extra,SS_SERVICE_USERDIR) ;
        auto_check(extra.s) ;

        extra.len = extralen ;
        auto_stralloc(&extra,SS_SERVICE_USERCONFDIR) ;
        auto_check(extra.s) ;

        extra.len = extralen ;
        auto_stralloc(&extra,SS_MODULE_USERDIR) ;
        auto_check(extra.s) ;

        extra.len = extralen ;
        auto_stralloc(&extra,SS_SCRIPT_USERDIR) ;
        auto_check(extra.s) ;

        extra.len = extralen ;
        auto_stralloc(&extra,SS_SEED_USERDIR) ;
        auto_check(extra.s) ;

        stralloc_free(&extra) ;
    }

    auto_strings(dst,info->base.s, SS_SYSTEM, SS_RESOLVE) ;
    auto_check(dst) ;

    auto_strings(dst + baselen, SS_TREE_CURRENT) ;
    auto_check(dst) ;
    auto_strings(dst + baselen,SS_SYSTEM, SS_BACKUP) ;
    auto_check(dst) ;
    auto_strings(dst + baselen + SS_SYSTEM_LEN, SS_STATE) ;

    if (!scan_mode(dst,S_IFREG)) {

        auto_strings(dst + baselen,SS_SYSTEM) ;
        if(!file_create_empty(dst,SS_STATE + 1,0644))
            log_dieusys(LOG_EXIT_SYS,"create ",dst,SS_STATE) ;
    }

    auto_strings(dst + baselen + SS_SYSTEM_LEN,"/",tree) ;
    r = scan_mode(dst,S_IFDIR) ;
    if (r == -1) log_die(LOG_EXIT_SYS,"invalid directory: ",dst) ;
    /** we have one, keep it*/
    info->tree.len = 0 ;
    if (!auto_stra(&info->tree,dst))
        log_die_nomem("stralloc") ;

    if (!r) return 0 ;

    return 1 ;
}

void create_tree(ssexec_t *info)
{
    log_flow() ;

    size_t newlen = 0 ;
    size_t treelen = info->tree.len ;

    char const *tree = info->tree.s, *treename = info->treename.s ;

    char dst[treelen + SS_SVDIRS_LEN + SS_DB_LEN + SS_SRC_LEN + 16 + 1] ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &res) ;
    resolve_init(wres) ;

    auto_strings(dst, tree) ;
    newlen = treelen ;

    res.name = resolve_add_string(wres,SS_MASTER+1) ;
    res.description = resolve_add_string(wres,"inner bundle - do not use it") ;
    res.tree = resolve_add_string(wres,dst) ;
    res.treename = resolve_add_string(wres,treename) ;
    res.type = TYPE_BUNDLE ;
    res.disen = 1 ;

    auto_create(dst, SS_SVDIRS, newlen) ;
    auto_create(dst, SS_RULES, newlen) ;
    auto_strings(dst + newlen, SS_SVDIRS) ;
    newlen = newlen + SS_SVDIRS_LEN ;
    auto_create(dst, SS_DB, newlen) ;
    auto_create(dst, SS_SVC, newlen) ;
    auto_create(dst, SS_RESOLVE, newlen) ;
    dst[newlen] = 0 ;
    log_trace("write resolve file of inner bundle") ;
    if (!resolve_write(wres,dst,SS_MASTER+1)) {

        resolve_free(wres) ;
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"write resolve file of inner bundle") ;
    }
    resolve_free(wres) ;

    char sym[newlen + 1 + SS_SYM_SVC_LEN + 1] ;
    char dstsym[newlen + SS_SVC_LEN + 1] ;

    auto_strings(sym,dst, "/", SS_SYM_SVC) ;
    auto_strings(dstsym, dst, SS_SVC) ;

    log_trace("point symlink: ",sym," to ",dstsym) ;
    if (symlink(dstsym,sym) < 0)
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"symlink: ", sym) ;

    auto_strings(sym + newlen + 1, SS_SYM_DB) ;
    auto_strings(dstsym + newlen, SS_DB) ;

    log_trace("point symlink: ",sym," to ",dstsym) ;
    if (symlink(dstsym,sym) < 0)
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"symlink: ", sym) ;

    auto_strings(dst + newlen, SS_DB) ;
    newlen = newlen + SS_DB_LEN ;
    auto_create(dst, SS_SRC, newlen) ;
    auto_strings(dst + newlen, SS_SRC) ;
    newlen = newlen + SS_SRC_LEN ;
    auto_create(dst, SS_MASTER, newlen) ;
    auto_strings(dst + newlen, SS_MASTER) ;
    newlen = newlen + SS_MASTER_LEN ;

    log_trace("create file: ",dst,"/contents") ;
    if (!file_create_empty(dst,"contents",0644))
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"create: ",dst,"/contents") ;

    auto_strings(dst + newlen,"/type") ;

    log_trace("create file: ",dst) ;
    if(!openwritenclose_unsafe(dst,"bundle\n",7))
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"write: ",dst) ;

}

void create_backupdir(ssexec_t *info)
{
    log_flow() ;

    int r ;

    size_t baselen = info->base.len ;
    size_t treenamelen = info->treename.len ;

    char const *base = info->base.s, *treename = info->treename.s ;

    char treetmp[baselen + 1 + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treenamelen + 1] ;

    auto_strings(treetmp, base, SS_SYSTEM, SS_BACKUP, "/", treename) ;

    r = scan_mode(treetmp,S_IFDIR) ;
    if (r || (r == -1)) {

        log_trace("remove existing backup: ",treetmp) ;
        if (rm_rf(treetmp) < 0)
            log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"remove: ",treetmp) ;
    }

    if (!r)
        auto_dir(treetmp,0755) ;
}

void parse_uid_list(uid_t *uids, char const *str)
{
    size_t pos = 0 ;
    stralloc sa = STRALLOC_ZERO ;

    if (!sastr_clean_string_wdelim(&sa, str, ','))
        log_dieu(LOG_EXIT_SYS,"parse uid list") ;

    uid_t owner = MYUID ;
    /** special case, we don't know which user want to use
     *  the service, we need a general name to allow all user
     *  the term "user" is took here to allow the current user*/
    ssize_t p = sastr_cmp(&sa,"user") ;

    FOREACH_SASTR(&sa, pos) {

        if (pos == (size_t)p) {

            struct passwd *pw = getpwuid(owner);

            if (!pw) {

                if (!errno) errno = ESRCH ;
                    log_dieu(LOG_EXIT_ZERO,"get user name") ;
            }

            if (!scan_uidlist(pw->pw_name, uids))
                log_dieu(LOG_EXIT_USER,"scan account: ",pw->pw_name) ;

            continue ;
        }

        if (!scan_uidlist(sa.s + pos, uids))
            log_dieu(LOG_EXIT_USER,"scan account: ",sa.s + pos) ;

    }
}

/** @what -> 0 deny
 * @what -> 1 allow */
void set_rules(char const *tree, uid_t *uids, uint8_t what)
{
    log_flow() ;

    log_trace("set ", !what ? "denied" : "allowed"," user for tree: ",tree,"..." ) ;

    int r ;
    size_t treelen = strlen(tree), uidn = uids[0], pos = 0 ;
    uid_t owner = MYUID ;
    char pack[256] ;
    char tmp[treelen + SS_RULES_LEN + 1] ;

    auto_strings(tmp, tree, SS_RULES) ;

    if (!uidn && what) {

        uids[0] = 1 ;
        uids[1] = owner ;
        uidn++ ;
    }

    //allow
    if (what) {

        for (; pos < uidn ; pos++) {

            uint32_pack(pack,uids[pos+1]) ;
            pack[uint_fmt(pack,uids[pos+1])] = 0 ;
            log_trace("create file: ",pack," at ",tmp) ;
            if(!file_create_empty(tmp,pack,0644) && errno != EEXIST)
                log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"set permissions access") ;

            log_trace("user: ",pack," is allowed for tree: ",tree) ;
        }
        goto announce ;
    }
    //else deny
    for (pos = 0 ; pos < uidn ; pos++) {

        if (owner == uids[pos+1]) continue ;
        uint32_pack(pack,uids[pos+1]) ;
        pack[uint_fmt(pack,uids[pos+1])] = 0 ;
        char ut[treelen + SS_RULES_LEN + 1 + uint_fmt(pack,uids[pos+1]) + 1] ;
        memcpy(ut,tmp,treelen + SS_RULES_LEN) ;
        memcpy(ut + treelen + SS_RULES_LEN,"/",1) ;
        memcpy(ut + treelen + SS_RULES_LEN + 1,pack,uint_fmt(pack,uids[pos+1])) ;
        ut[treelen + SS_RULES_LEN + 1 + uint_fmt(pack,uids[pos + 1])] = 0 ;
        r = scan_mode(ut,S_IFREG) ;
        if (r == 1) {

            log_trace("unlink: ",ut) ;
            r = unlink(ut) ;
            if (r == -1)
                log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"set permissions access") ;
        }

        log_trace("user: ",pack," is denied for tree: ",tree) ;
    }
    announce:
    log_info("Permissions rules set successfully for tree: ",tree) ;
}

/** @action -> 0 disable
 * @action -> 1 enable */
void tree_enable_disable(ssexec_t *info, uint8_t action)
{
    log_flow() ;

    int r ;
    char const *base = info->base.s, *dst = info->tree.s , *tree = info->treename.s ;

    log_trace(!action ? "disable " : "enable ",dst,"...") ;
    r  = tree_cmd_state(VERBOSITY,!action ? "-d" : "-a", tree) ;
    if (!r)

        log_dieusys(LOG_EXIT_SYS,!action ? "disable: " : "enable: ",dst," at: ",base,SS_SYSTEM,SS_STATE) ;

    else if (r == 1) {

        log_info(!action ? "Disabled" : "Enabled"," successfully tree: ",tree) ;

    }

    else log_info("Already ",!action ? "disabled" : "enabled"," tree: ",tree) ;

}

void tree_modify_resolve(resolve_service_t *res, resolve_service_enum_t field,char const *regex,char const *by)
{
    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;
    resolve_service_t modif = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &modif) ;

    log_trace("modify field: ", resolve_service_field_table[field].field," of service: ",res->sa.s + res->name) ;

    if (!service_resolve_copy(&modif,res))
        log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"copy resolve file of: ", res->sa.s + res->name) ;

    if (!service_resolve_field_to_sa(&sa,&modif, field))
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"get copy of field: ",resolve_service_field_table[field].field) ;

    if (sa.len) {
        if (!sastr_replace(&sa,regex,by))
            log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"replace field: ",resolve_service_field_table[field].field) ;

        if (!stralloc_0(&sa))
            log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"stralloc") ;

        sa.len-- ;
    }

    if (!service_resolve_modify_field(&modif,field,sa.s))
        log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"modify field: ",resolve_service_field_table[field].field) ;

    if (!service_resolve_copy(res,&modif))
        log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"copy resolve file of: ",res->sa.s + res->name) ;

    stralloc_free(&sa) ;

    resolve_free(wres) ;
}

void tree_remove(ssexec_t *info)
{
    log_flow() ;

    char const *base = info->base.s, *dst = info->tree.s, *tree = info->treename.s ;
    int r ;

    log_trace("delete: ",dst,"..." ) ;

    if (rm_rf(dst) < 0) log_dieusys(LOG_EXIT_SYS,"delete: ", dst) ;

    size_t treelen = strlen(tree) ;
    size_t baselen = strlen(base) ;
    char treetmp[baselen + SS_SYSTEM_LEN + SS_BACKUP_LEN + 1 + treelen  + 1] ;

    auto_strings(treetmp, base, SS_SYSTEM, SS_BACKUP, "/", tree) ;

    r = scan_mode(treetmp,S_IFDIR) ;
    if (r || (r < 0)) {

        log_trace("delete backup of tree: ",treetmp,"...") ;
        if (rm_rf(treetmp) < 0) log_dieusys(LOG_EXIT_SYS,"delete: ",treetmp) ;

    }

    tree_enable_disable(info,0) ;

    log_info("Deleted successfully: ",tree) ;
}

#include <stdio.h>

static void tree_parse_depends(char const *treename, char const *str)
{
    size_t pos = 0 ;
    char *sv = 0 ;
    stralloc sa = STRALLOC_ZERO ;

    if (!sastr_clean_string_wdelim(&sa, str, TREE_COMMA_DELIM))
        log_dieu(LOG_EXIT_SYS,"clean sub options") ;

    FOREACH_SASTR(&sa, pos) {

        sv = sa.s + pos ;

        if (!graph_vertex_add(&GRAPH, sv))
            log_die(LOG_EXIT_SYS,"add vertex: ", sv) ;

        if (!graph_edge_add_g(&GRAPH, treename, sv))
            log_die(LOG_EXIT_SYS,"add edge: ", sv, " to vertex: ", treename) ;

    }
    stralloc_free(&sa) ;
}

static void tree_parse_options(tree_opts_t *opts, char const *str)
{

    size_t pos = 0, len = 0 ;
    ssize_t r ;
    char *line = 0, *key = 0, *val = 0 ;
    tree_opts_map_t const *t ;

    stralloc sa = STRALLOC_ZERO ;

    if (!sastr_clean_string_wdelim(&sa,str,TREE_COLON_DELIM))
        log_dieu(LOG_EXIT_SYS,"clean options") ;

    unsigned int n = sastr_len(&sa), nopts = 0 , old ;

    tree_checkopts(n) ;

    FOREACH_SASTR(&sa, pos) {

        line = sa.s + pos ;
        t = tree_opts_table ;
        old = nopts ;

        for (; t->str ; t++) {

            len = strlen(line) ;
            char tmp[len + 1] ;

            r = tree_get_key(tmp,line) ;
            if (r == -1)
                log_die(LOG_EXIT_USER,"invalid key: ", line) ;

            key = tmp ;
            val = line + r ;

            if (!strcmp(key, t->str)) {

                switch(t->id) {

                    case TREE_OPTS_DEPENDS :

                            opts->depends = TREE_SAOPTS.len ;
                            if (!sastr_add_string(&TREE_SAOPTS, val))
                                log_die_nomem("stralloc") ;

                            break ;

                    case TREE_OPTS_REQUIREDBY :

                            opts->requiredby = TREE_SAOPTS.len ;
                            if (!sastr_add_string(&TREE_SAOPTS, val))
                                log_die_nomem("stralloc") ;

                        break ;

                    case TREE_OPTS_RENAME:

                            opts->rename = TREE_SAOPTS.len ;
                            if (!sastr_add_string(&TREE_SAOPTS, val))
                                log_die_nomem("stralloc") ;

                        break ;

                    default :

                        break ;
                }
                nopts++ ;
            }
        }

        if (old == nopts)
            log_die(LOG_EXIT_SYS,"invalid option: ",line) ;
    }
    stralloc_free(&sa) ;
}

int ssexec_tree(int argc, char const *const *argv,char const *const *envp,ssexec_t *info)
{
    int r, current, create, allow, deny, enable, disable, remove, snap ;

    uid_t auids[256] = { 0 }, duids[256] = { 0 } ;

    char const *tree, *after_tree = 0 ;

    stralloc clone = STRALLOC_ZERO ;
    tree_seed_t seed = TREE_SEED_ZERO ;
    tree_opts_t opts = TREE_OPTS_ZERO ;

    current = create = allow = deny = enable = disable = remove = snap = 0 ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = getopt_args(argc,argv, ">" OPTS_TREE, &l) ;
            if (opt == -1) break ;
            if (opt == -2) log_die(LOG_EXIT_USER,"options must be set first") ;
            switch (opt)
            {

                case 'n' :  create = 1 ; break ;
                case 'a' :  parse_uid_list(auids, l.arg) ;
                            allow = 1 ;
                            break ;
                case 'd' :  parse_uid_list(duids, l.arg) ;
                            deny = 1 ;
                            break ;
                case 'c' :  current = 1 ; break ;
                case 'S' :  after_tree = l.arg ; break ;
                case 'E' :  enable = 1 ; if (disable) log_usage(usage_tree) ; break ;
                case 'D' :  disable = 1 ; if (enable) log_usage(usage_tree) ; break ;
                case 'R' :  remove = 1 ; if (create) log_usage(usage_tree) ; break ;
                case 'C' :  if (remove) log_usage(usage_tree) ;
                            if (!auto_stra(&clone,l.arg)) log_die_nomem("stralloc") ;
                            snap = 1 ;
                            break ;
                case 'o' :  tree_parse_options(&opts, l.arg) ;
                            break ;
                default :   log_usage(usage_tree) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1) log_usage(usage_tree) ;

    // make create the default option
    if (!current && !create && !allow && !deny && !enable && !disable && !remove && !snap && (opts.rename >= 0))
        create = 1 ;

    info->treename.len = 0 ;
    if (!auto_stra(&info->treename, argv[0]))
        log_die_nomem("stralloc") ;

    tree = info->treename.s ;

    log_trace("sanitize ",tree,"..." ) ;
    r = sanitize_tree(info,tree) ;

    if (!graph_vertex_add(&GRAPH, tree))
        log_dieu(LOG_EXIT_SYS,"add vertex: ",tree) ;

    if (opts.depends >= 0)
        tree_parse_depends(tree, TREE_SAOPTS.s + opts.depends) ;

    if(!r && create) {

        /** set cleanup */
        cleantree = info->tree.s ;
        log_trace("checking seed file: ",tree,"..." ) ;

        if (tree_seed_isvalid(tree)) {

            if (!tree_seed_setseed(&seed, tree, 0))
                log_dieu_nclean(LOG_EXIT_SYS, &cleanup, "parse seed file: ", tree) ;

            if (seed.depends) {

                tree_parse_depends(tree, saseed.s + seed.depends) ;

            }

            if (seed.requiredby) {

            }

            if (seed.enabled)
                enable = 1 ;

            if (seed.allow > 0) {

                parse_uid_list(auids, saseed.s + seed.allow) ;

                allow = 1 ;
            }

            if (seed.deny > 0) {

                parse_uid_list(duids, saseed.s + seed.deny) ;

                deny = 1 ;
            }

            if (seed.current)
                current = 1 ;

            if (seed.group) {

            }

            //if (seed.services)
        }


        log_trace("creating: ",info->tree.s,"..." ) ;
        create_tree(info) ;

        log_trace("creating backup directory for: ", tree,"...") ;
        create_backupdir(info) ;
        /** unset cleanup */
        cleantree = 0 ;

        // set permissions
        allow = 1 ;

        size_t dblen = info->tree.len ;
        char newdb[dblen + SS_SVDIRS_LEN + 1] ;
        auto_strings(newdb, info->tree.s, SS_SVDIRS) ;

        log_trace("compile: ",newdb,"/db/",tree,"..." ) ;
        if (!db_compile(newdb,info->tree.s,tree,envp))
            log_dieu_nclean(LOG_EXIT_SYS,&cleanup,"compile ",newdb,"/db/",tree) ;

        r = 1 ;
        create = 0 ;
        log_info("Created successfully tree: ",tree) ;
    }

    if ((!r && !create) || (!r && enable)) log_dieusys(LOG_EXIT_SYS,"find tree: ",info->tree.s) ;
    if (r && create) log_dieu(LOG_EXIT_USER,"create: ",info->tree.s,": already exist") ;



    if (enable) {
        /**
         *
         *
         * Also, check if depends of the tree are enabled.
         *
         *
         *
         * */


        if (!graph_matrix_build(&GRAPH))
            log_dieu(LOG_EXIT_SYS,"build the graph") ;

        if (!graph_matrix_analyze_cycle(&GRAPH))
            log_die(LOG_EXIT_SYS,"found cycle") ;

        if (!graph_matrix_sort(&GRAPH))
            log_dieu(LOG_EXIT_SYS,"sort the graph") ;

        graph_show_matrix(&GRAPH) ;

        for (unsigned int i = 0 ; i < GRAPH.sort_count ; i++)
            printf("%s\n",GRAPH.data.s + genalloc_s(graph_hash_t,&GRAPH.hash)[GRAPH.sort[i]].vertex) ;
        tree_enable_disable(info,1) ;

    }

    if (disable)
        tree_enable_disable(info,0) ;

    if (allow)
        set_rules(info->tree.s,auids,1) ;

    if (deny)
        set_rules(info->tree.s,duids,0) ;

    if(current) {

        log_trace("make: ",info->tree.s," as default ..." ) ;

        if (!tree_switch_current(info->base.s,tree))
            log_dieusys(LOG_EXIT_SYS,"set: ",info->tree.s," as default") ;

        log_info("Set successfully: ",tree," as default") ;
    }

    if (remove)
        tree_remove(info) ;

    if (snap) {

        stralloc salist = STRALLOC_ZERO ;
        resolve_service_t res = RESOLVE_SERVICE_ZERO ;
        resolve_wrapper_t_ref wres = resolve_set_struct(SERVICE_STRUCT, &res) ;
        char const *exclude[1] = { 0 } ;

        size_t syslen = info->base.len + SS_SYSTEM_LEN ;
        size_t treelen = info->treename.len ;
        size_t pos = 0 ;

        char system[syslen + 1] ;
        auto_strings(system,info->base.s,SS_SYSTEM) ;

        size_t clone_target_len = syslen + 1 + clone.len ;
        char clone_target[clone_target_len + 1] ;
        auto_strings(clone_target,system,"/",clone.s) ;

        r = scan_mode(clone_target,S_IFDIR) ;
        if ((r < 0) || r) log_die(LOG_EXIT_SYS,clone_target,": already exist") ;

        // clone main directory
        log_trace("clone: ",tree," as: ",clone.s,"..." ) ;
        if (!hiercopy(info->tree.s,clone_target))
            log_dieusys(LOG_EXIT_SYS,"copy: ",info->tree.s," to: ",clone_target) ;

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

        if (!sastr_dir_get(&salist,src_resolve,exclude,S_IFREG))
            log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"get resolve file at: ",src_resolve) ;

        char clone_res[clone_target_len + SS_SVDIRS_LEN + 1] ;
        auto_strings(clone_res,clone_target,SS_SVDIRS) ;

        FOREACH_SASTR(&salist, pos) {

            char *name = salist.s + pos ;

            if (!resolve_read(wres,clone_res,name))
                log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"read resolve file of: ",src_resolve,"/",name) ;

            tree_modify_resolve(&res,SERVICE_ENUM_RUNAT,tree,clone.s) ;
            tree_modify_resolve(&res,SERVICE_ENUM_TREENAME,tree,clone.s) ;
            tree_modify_resolve(&res,SERVICE_ENUM_TREE,tree,clone.s) ;
            tree_modify_resolve(&res,SERVICE_ENUM_STATE,tree,clone.s) ;

            if (!resolve_write(wres,clone_res,name))
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

        /** main and backup can be different,so rebuild the list
         * Also, a backup directory can be empty, check it first */
        r = scan_mode(src_resolve_backup,S_IFDIR) ;
        if (r == 1) {

            if (!sastr_dir_get(&salist,src_resolve_backup,exclude,S_IFREG))
                log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"get resolve file at: ",src_resolve_backup) ;

            pos = 0 ;
            FOREACH_SASTR(&salist, pos) {

                char *name = salist.s + pos ;

                if (!resolve_read(wres,clone_backup,name))
                    log_dieusys_nclean(LOG_EXIT_SYS,&cleanup,"read resolve file of: ",src_resolve_backup,"/",name) ;

                tree_modify_resolve(&res,SERVICE_ENUM_RUNAT,tree,clone.s) ;
                tree_modify_resolve(&res,SERVICE_ENUM_TREENAME,tree,clone.s) ;
                tree_modify_resolve(&res,SERVICE_ENUM_TREE,tree,clone.s) ;
                tree_modify_resolve(&res,SERVICE_ENUM_STATE,tree,clone.s) ;

                if (!resolve_write(wres,clone_backup,name))
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
        resolve_free(wres) ;

        log_info("Cloned successfully: ",tree," to ",clone.s) ;
    }

    if (after_tree) {

        stralloc contents = STRALLOC_ZERO ;
        stralloc tmp = STRALLOC_ZERO ;
        int befirst = obstr_equal(tree,after_tree) ;
        size_t baselen = info->base.len, pos = 0 ;
        char ste[baselen + SS_SYSTEM_LEN + 1] ;
        auto_strings(ste,info->base.s,SS_SYSTEM) ;

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

        if (tmp.len) {

            if (!sastr_rebuild_in_oneline(&tmp) ||
            !sastr_clean_element(&tmp))
                log_dieu(LOG_EXIT_SYS,"rebuild state list") ;

            pos = 0 ;
            FOREACH_SASTR(&tmp, pos) {

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

        } else {

            if (!auto_stra(&contents,tree,"\n"))
                log_dieusys(LOG_EXIT_SYS,"stralloc") ;
        }

        if (!file_write_unsafe(ste,SS_STATE + 1,contents.s,contents.len))
            log_dieusys(LOG_EXIT_ZERO,"write: ",ste,SS_STATE) ;

        log_info("Ordered successfully: ",tree," starts after tree: ",after_tree) ;
    }

    stralloc_free(&clone) ;
    stralloc_free(&TREE_SAOPTS) ;
    graph_free_all(&GRAPH) ;
    tree_seed_free();

    return 0 ;
}
