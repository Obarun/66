/*
 * ssexec_tree_admin.c
 *
 * Copyright (c) 2018-2025 Eric Vidal <eric@obarun.org>
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
#include <stdlib.h>//free

#include <oblibs/log.h>
#include <oblibs/types.h>
#include <oblibs/directory.h>
#include <oblibs/files.h>
#include <oblibs/string.h>
#include <oblibs/sastr.h>
#include <oblibs/lexer.h>
#include <oblibs/stack.h>
#include <oblibs/account.h>
#include <oblibs/types.h>

#include <skalibs/sgetopt.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/bytestr.h>//byte_count
#include <skalibs/posixplz.h>//unlink_void

#include <66/config.h>
#include <66/utils.h>
#include <66/constants.h>
#include <66/enum_parser.h>
#include <66/state.h>
#include <66/service.h>
#include <66/resolve.h>
#include <66/graph.h>
#include <66/sanitize.h>
#include <66/enum_tree.h>

#include <s6/supervise.h>

#define TREE_COLON_DELIM ':'
#define TREE_COMMA_DELIM ','
#define TREE_MAXOPTS 10
#define tree_checkopts(n) if (n >= TREE_MAXOPTS) log_die(LOG_EXIT_USER, "too many -o options")

typedef struct tree_opts_map_s tree_opts_map_t ;
struct tree_opts_map_s
{
    char const *str ;
    int const id ;
} ;

enum enum_tree_opts_e
{
    TREE_OPTS_DEPENDS = 0,
    TREE_OPTS_REQUIREDBY,
    TREE_OPTS_ENABLE,
    TREE_OPTS_RENAME,
    TREE_OPTS_GROUPS,
    TREE_OPTS_NOSEED,
    TREE_OPTS_ALLOW,
    TREE_OPTS_DENY,
    TREE_OPTS_CLONE,
    TREE_OPTS_ENDOFKEY
} ;
typedef enum enum_tree_opts_e enum_tree_opts_t, *enum_tree_opts_t_ref ;

tree_opts_map_t const tree_opts_table[] =
{
    { .str = "depends",     .id = TREE_OPTS_DEPENDS },
    { .str = "requiredby",  .id = TREE_OPTS_REQUIREDBY },
    { .str = "enable",      .id = TREE_OPTS_ENABLE },
    { .str = "rename",      .id = TREE_OPTS_RENAME },
    { .str = "groups",      .id = TREE_OPTS_GROUPS },
    { .str = "noseed",      .id = TREE_OPTS_NOSEED },
    { .str = "allow",       .id = TREE_OPTS_ALLOW },
    { .str = "deny",        .id = TREE_OPTS_DENY },
    { .str = "clone",       .id = TREE_OPTS_CLONE },
    { .str = 0 }
} ;

typedef struct tree_what_s tree_what_t, *tree_what_t_ref ;
struct tree_what_s
{
    uint8_t create ;
    uint8_t depends ;
    uint8_t requiredby ;
    uint8_t allow ;
    uint8_t deny ;
    uint8_t enable ;
    uint8_t disable ;
    uint8_t remove ;
    uint8_t clone ;
    uint8_t groups ;
    uint8_t current ;
    uint8_t rename ;
    uint8_t noseed ;

    char gr[6] ;
    char sclone[100] ;
    uid_t auids[256] ;
    uid_t duids[256] ;
    uint8_t ndepends ; // only used if the term none is passed as dependencies
    uint8_t nrequiredby ; // only used if the term none is passed as dependencies

    uint8_t nopts ;
} ;
#define TREE_WHAT_ZERO { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, { 0 }, { 0 }, { 0 }, 1, 1, 0 }

tree_what_t what_init(void)
{
    log_flow() ;

    tree_what_t what = TREE_WHAT_ZERO ;

    memset(what.gr, 0, 6 * sizeof(char)); ;
    memset(what.auids, 0, 256 * sizeof(uid_t));
    memset(what.duids, 0, 256 * sizeof(uid_t)) ;

    return what ;
}

void tree_enable_disable(tree_graph_t *g, char const *base, char const *treename, uint8_t action) ;

static void check_identifier(char const *name)
{
    if (!memcmp(name, SS_MASTER + 1, 6))
        log_die(LOG_EXIT_USER,"tree name: ",name,": starts with reserved prefix Master") ;

    if (strlen(name) > SS_MAX_TREENAME)
        flog_die(LOG_EXIT_USER,"tree name is too long -- it can not exceed %d", SS_MAX_TREENAME) ;

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

static void tree_parse_options_groups(char *store, char const *str)
{
    log_flow() ;

    uid_t uid = getuid() ;

    if (strcmp(str, TREE_GROUPS_BOOT) &&
        strcmp(str, TREE_GROUPS_ADM) &&
        strcmp(str, TREE_GROUPS_USER) &&
        strcmp(str, "none"))
        log_die(LOG_EXIT_SYS, "invalid group: ", str) ;

    if (!uid && (!strcmp(str, TREE_GROUPS_USER)))
        log_die(LOG_EXIT_SYS, "Only regular user can use this group") ;

    else if (uid && (!strcmp(str, TREE_GROUPS_ADM) || !strcmp(str, TREE_GROUPS_BOOT)))
        log_die(LOG_EXIT_SYS, "Only root user can use this group") ;

    auto_strings(store, str) ;
}

void tree_parse_uid_list(uid_t *uids, char const *str)
{
    log_flow() ;

    size_t pos = 0 ;

    _alloc_stk_(stk, strlen(str) + 1) ;

    if (!lexer_trim_with_delim(&stk, str, TREE_COMMA_DELIM))
        log_dieu(LOG_EXIT_SYS,"parse options") ;

    uid_t owner = getuid() ;
    /** special case, we don't know which user want to use
     *  the tree, we need a general name to allow all users.
     *  The term "user" is took here to allow the current user*/
    ssize_t p = stack_retrieve_element(&stk, "user") ;

    FOREACH_STK(&stk, pos) {

        if (pos == (size_t)p) {

            struct passwd *pw = getpwuid(owner);

            if (!pw) {

                if (!errno) errno = ESRCH ;
                    log_dieu(LOG_EXIT_SYS,"get user name") ;
            }

            if (!scan_uidlist(pw->pw_name, uids))
                log_dieu(LOG_EXIT_USER,"scan account: ",pw->pw_name) ;

            continue ;
        }

        if (!scan_uidlist(stk.s + pos, uids))
            log_dieu(LOG_EXIT_USER,"scan account: ",stk.s + pos) ;
    }
}

static void tree_parse_options_depends(tree_graph_t *g, ssexec_t *info, char const *str, uint8_t requiredby, tree_what_t *what)
{
    log_flow() ;

    if (!*str)
        return ;

    int r ;
    size_t pos = 0 ;
    char *name = 0 ;
    _alloc_stk_(stk, strlen(str) + 1) ;

    if (!lexer_trim_with_delim(&stk, str, TREE_COMMA_DELIM))
        log_dieu(LOG_EXIT_SYS,"clean sub options") ;

    if (stack_retrieve_element(&stk, "none") >= 0) {
        if (!requiredby)
            what->ndepends = 0 ;
        else
            what->nrequiredby = 0 ;

        return ;
    }

    FOREACH_STK(&stk, pos) {

        name = stk.s + pos ;

        r = tree_isvalid(info->base.s, name) ;
        if (r < 0)
            log_diesys(LOG_EXIT_SYS, "invalid treename") ;
        /** We only creates trees declared as dependency.
         * TreeA depends on TreeB, we create TreeB
         * if it doesn't exist yet */
        if (!r && !requiredby) {

            ssexec_t newinfo = SSEXEC_ZERO ;
            if (!auto_stra(&newinfo.base, info->base.s) ||
                !auto_stra(&newinfo.treename, name))
                    log_die_nomem("stralloc") ;
            newinfo.owner = info->owner ;
            newinfo.prog = info->prog ;
            newinfo.help = info->help ;
            newinfo.usage = info->usage ;
            newinfo.opt_color = info->opt_color ;
            newinfo.opt_tree = info->opt_tree ;


            int nwhat = what->noseed ? 2 : 0 ;
            int nargc = 3 + nwhat ;
            char const *prog = PROG ;
            char const *newargv[nargc] ;
            uint8_t m = 0 ;
            newargv[m++] = "tree" ;

            if (nwhat) {
                newargv[m++] = "-o" ;
                newargv[m++] = "noseed" ;
            }

            newargv[m++] = name ;
            newargv[m++] = 0 ;

            log_trace("launch 66 tree sub-process for tree: ", name) ;

            PROG = "tree (child)" ;
            if (ssexec_tree_admin(nargc, newargv, &newinfo))
                log_dieusys(LOG_EXIT_SYS, "create tree: ", name) ;
            PROG = prog ;

            ssexec_free(&newinfo) ;

            r = 1 ;

        }

        if (!requiredby) {

            if (!graph_add_edge(&g->g, info->treename.s, name, true))
                log_die(LOG_EXIT_SYS,"add edge: ", name, " to vertex: ", info->treename.s) ;

        } else if (r) {
            /** if TreeA is requiredby TreeB, we don't want to create TreeB.
             * We only manages it if it exist yet */
            if (!graph_add_edge(&g->g, info->treename.s, name, true))
                log_die(LOG_EXIT_SYS,"add requiredby: ", name, " to: ", info->treename.s) ;
        }
    }
}

static void tree_parse_options(tree_graph_t *g, char const *str, ssexec_t *info, tree_what_t *what)
{
    log_flow() ;

    if (!*str)
        return ;

    size_t pos = 0, len = 0 ;
    ssize_t r ;
    char *line = 0, *key = 0, *val = 0 ;
    _alloc_stk_(stk, strlen(str) + 1) ;
    tree_opts_map_t const *t ;

    if (!lexer_trim_with_delim(&stk, str, TREE_COLON_DELIM))
        log_dieu(LOG_EXIT_SYS,"clean options") ;

    unsigned int nopts = 0 , old ;

    tree_checkopts(stk.count) ;

    FOREACH_STK(&stk, pos) {

        line = stk.s + pos ;
        t = tree_opts_table ;
        old = nopts ;

        for (; t->str ; t++) {

            len = strlen(line) ;
            char tmp[len + 1] ;

            r = tree_get_key(tmp,line) ;
            if (r == -1 && (strcmp(line, "noseed") && strcmp(line, "enable")))
                log_die(LOG_EXIT_USER,"invalid key: ", line) ;

            if (!strcmp(line, "noseed") || !strcmp(line, "enable")) {
                key = line ;
            } else {
                key = tmp ;
                val = line + r ;
            }

            if (!strcmp(key, t->str)) {

                switch(t->id) {

                    case TREE_OPTS_DEPENDS :
                        tree_parse_options_depends(g, info, val, 0, what) ;
                        what->depends = 1 ;
                        break ;

                    case TREE_OPTS_REQUIREDBY :

                        tree_parse_options_depends(g, info, val, 1, what) ;
                        what->requiredby = 1 ;
                        break ;

                    case TREE_OPTS_ENABLE :
                        what->enable = 1 ;
                        break ;

                    case TREE_OPTS_RENAME:

                        what->rename = 1 ;
                        break ;

                    case TREE_OPTS_GROUPS:

                        tree_parse_options_groups(what->gr, val) ;
                        what->groups = 1 ;
                        break ;

                    case TREE_OPTS_NOSEED:

                        what->noseed = 1 ;
                        break ;

                    case TREE_OPTS_ALLOW:

                        tree_parse_uid_list(what->auids, val) ;
                        what->allow = 1 ;
                        break ;

                    case TREE_OPTS_DENY:

                        tree_parse_uid_list(what->duids, val) ;
                        what->deny = 1 ;
                        break ;

                   case TREE_OPTS_CLONE:

                        if (strlen(val) > 99)
                            log_die(LOG_EXIT_USER, "clone name cannot exceed 100 characters") ;

                        auto_strings(what->sclone, val) ;
                        what->clone = 1 ;
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
}

void tree_parse_seed(char const *treename, tree_seed_t *seed, tree_what_t *what)
{
    log_flow() ;

    log_trace("checking seed file: ", treename, "..." ) ;

    if (tree_seed_isvalid(treename)) {

        if (!tree_seed_setseed(seed, treename))
            log_dieu(LOG_EXIT_SYS, "parse seed file: ", treename) ;

        if (seed->depends > 0)
            what->depends = 1 ;

        if (seed->requiredby > 0)
            what->requiredby = 1 ;

        if (seed->disen > 0)
            what->enable = 1 ;

        if (seed->allow > 0) {

            tree_parse_uid_list(what->auids, seed->sa.s + seed->allow) ;
            what->allow = 1 ;
        }

        if (seed->deny > 0) {

            tree_parse_uid_list(what->duids, seed->sa.s + seed->deny) ;
            what->deny = 1 ;
        }

        if (seed->current > 0)
            what->current = 1 ;

        if (seed->groups > 0) {

            tree_parse_options_groups(what->gr, seed->sa.s + seed->groups) ;
            what->groups = 1 ;
        }
    }
}

void tree_groups(tree_graph_t *graph, char const *base, char const *treename, char const *value)
{
    log_flow() ;

    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    resolve_enum_table_t table = E_TABLE_TREE_ZERO ;
    size_t nb = 0 ;
    char pack[U32_FMT] ;
    char const *val ;

    log_trace("set: ", treename," to group ..." ) ;

    if (!strcmp(value, "none")) {
        val = "" ;
        goto write ;

    } else if (!strcmp(value, "boot")) {
        /** a tree on groups boot cannot be enabled */
        tree_enable_disable(graph, base, treename, 0) ;
    }
    nb = 1 ;
    val = value ;

    write:

    uint_pack(pack, nb) ;
    pack[u32_fmt(pack, nb)] = 0 ;

    if (resolve_read_g(wres, base, treename) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", treename) ;

    table.u.tree.id = E_RESOLVE_TREE_GROUPS ;

    if (!resolve_modify_field(wres, table, val))
        log_dieusys(LOG_EXIT_SYS, "modify resolve file of: ", treename) ;

    table.u.tree.id = E_RESOLVE_TREE_NGROUPS ;

    if (!resolve_modify_field(wres, table, pack))
            log_dieusys(LOG_EXIT_SYS, "modify resolve file of: ", treename) ;

    if (!resolve_write_g(wres, base, treename))
        log_dieusys(LOG_EXIT_SYS, "write resolve file of: ", treename) ;

    resolve_free(wres) ;

    log_info("Set successfully: ", treename, " to group: ", value) ;
}

void tree_master_modify_contents(char const *base)
{
    log_flow() ;

    stralloc sa = STRALLOC_ZERO ;
    resolve_tree_master_t mres = RESOLVE_TREE_MASTER_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE_MASTER, &mres) ;
    size_t baselen = strlen(base) ;
    char solve[baselen + SS_SYSTEM_LEN + SS_RESOLVE_LEN + 1] ;

    char const *exclude[2] = { SS_MASTER + 1, 0 } ;

    log_trace("modify field contents of resolve Master file") ;

    auto_strings(solve, base, SS_SYSTEM, SS_RESOLVE) ;

    if (!sastr_dir_get(&sa, solve, exclude, S_IFREG))
        log_dieu(LOG_EXIT_SYS, "get trees resolve files") ;

    size_t ncontents = sa.len ? sastr_nelement(&sa) : 0 ;

    if (ncontents)
        if (!sastr_rebuild_in_oneline(&sa))
            log_dieu(LOG_EXIT_SYS, "rebuild stralloc") ;

    if (resolve_read_g(wres, base, SS_MASTER + 1) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve Master file") ;

    mres.ncontents = (uint32_t)ncontents ;

    if (ncontents)
        mres.contents = resolve_add_string(wres, sa.s) ;
    else
        mres.contents = resolve_add_string(wres, "") ;

    if (!resolve_write_g(wres, base, SS_MASTER + 1))
        log_dieusys(LOG_EXIT_SYS, "write resolve Master file") ;

    stralloc_free(&sa) ;
    resolve_free(wres) ;
}

void tree_create(tree_graph_t *g, ssexec_t *info, tree_what_t *what)
{
    log_flow() ;

    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    tree_seed_t seed = TREE_SEED_ZERO ;

    resolve_init(wres) ;

    /** check seed file */
    if (!what->noseed)
        tree_parse_seed(info->treename.s, &seed, what) ;

    log_trace("creating: ", info->treename.s, "..." ) ;

    // set permissions
    what->allow = 1 ;

    tres.name = resolve_add_string(wres, info->treename.s) ;
    tres.groups = resolve_add_string(wres, info->owner ? TREE_GROUPS_USER : TREE_GROUPS_ADM) ;
    tres.ngroups = 1 ;

    log_trace("write resolve file of: ", info->treename.s) ;
    if (!resolve_write_g(wres, info->base.s, info->treename.s))
        log_dieu(LOG_EXIT_SYS, "write resolve file of: ", info->treename.s) ;

    /** Check the length of seed.sa.len: If the seed file is not parsed at this point,
     * seed.sa.s + seed.depends is empty, which can lead to a segmentation fault
     * when the -o option is passed at the command line. However, we have already gone
     * through the tree_parse_options_depends in such cases. */
    if (what->depends && seed.sa.len)
        tree_parse_options_depends(g, info, seed.sa.s + seed.depends, 0, what) ;

    if (what->requiredby && seed.sa.len)
        tree_parse_options_depends(g, info, seed.sa.s + seed.requiredby, 1, what) ;

    tree_master_modify_contents(info->base.s) ;

    resolve_free(wres) ;
    tree_seed_free(&seed) ;

    log_info("Created successfully tree: ", info->treename.s) ;
}

void tree_enable_disable_deps(tree_graph_t *g,char const *base, char const *treename, uint8_t action)
{
    log_flow() ;

    size_t pos = 0, element = 0 ;
    vertex_t *v = NULL ;

    HASH_FIND_STR(g->g.vertexes, treename, v) ;
    if (v == NULL)
        return ;

    uint32_t nvertex = action ? v->ndepends : v->nrequiredby ;
    _alloc_stk_(stk, nvertex * SS_MAX_TREENAME) ;
    if (!graph_get_stkedge(&stk, &g->g, v, action ? false : true))
        return ;

    unsigned int visit[nvertex + 1] ;

    memset(visit, 0, (nvertex + 1) * sizeof(unsigned int)) ;

    if (stk.len) {

        FOREACH_STK(&stk, pos) {

            if (!visit[element]) {

                char *name = stk.s + pos ;

                tree_enable_disable(g, base, name, action) ;

                visit[element] = 1 ;
            }
            element++ ;
        }
    }
}

/** @action -> 0 disable
 * @action -> 1 enable */
void tree_enable_disable(tree_graph_t *g, char const *base, char const *treename, uint8_t action)
{
    log_flow() ;

    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;

    if (resolve_read_g(wres, base, treename) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", treename) ;

    uint8_t disen = tres.enabled ;

    if ((disen && !action) || (!disen && action)){

        log_trace(!action ? "disable " : "enable ", treename, "...") ;

        if (tree_ongroups(base, treename, TREE_GROUPS_BOOT) && action) {
            log_1_warn(treename," is a part of group ", TREE_GROUPS_BOOT," -- ignoring enable request") ;
            return ;
        }

        tres.enabled = action ;
        if (!resolve_write_g(wres, base, treename))
            log_dieusys(LOG_EXIT_SYS, "write resolve file of: ", treename) ;

        tree_enable_disable_deps(g, base, treename, action) ;

        log_info(!action ? "Disabled" : "Enabled"," successfully tree: ", treename) ;

    } else {

        log_info("Already ",!action ? "disabled" : "enabled"," tree: ",treename) ;
    }

    resolve_free(wres) ;
}

/* !deps -> add
 * deps -> remove */
void tree_depends_requiredby(tree_graph_t *g, char const *base, char const *treename, uint8_t requiredby, uint8_t none, char const *deps)
{
    log_flow() ;

    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    resolve_enum_table_t table = E_TABLE_TREE_ZERO ;
    size_t pos = 0, nb = 0, element = 0 ;
    uint8_t ewhat = !requiredby ? E_RESOLVE_TREE_DEPENDS : E_RESOLVE_TREE_REQUIREDBY ;
    uint8_t nwhat = !requiredby ? E_RESOLVE_TREE_NDEPENDS : E_RESOLVE_TREE_NREQUIREDBY ;
    stralloc sa = STRALLOC_ZERO ;
    char pack[U32_FMT] ;

    log_trace("manage ", !requiredby ? "dependencies" : "required by", " for tree: ", treename, "..." ) ;

    vertex_t *v = NULL ;
    HASH_FIND_STR(g->g.vertexes, treename, v) ;
    if (v == NULL)
        log_dieu(LOG_EXIT_SYS, "get information of treename: ", treename, " -- please make a bug report") ;

    uint32_t nvertex = requiredby ? v->nrequiredby : v->ndepends ;
    _alloc_stk_(stk, nvertex * SS_MAX_TREENAME) ;

    if (!graph_get_stkedge(&stk, &g->g, v, requiredby ? true : false))
        log_dieu(LOG_EXIT_SYS,"get sorted ", requiredby ? "required by" : "dependency", " list of tree: ", treename) ;

    size_t vlen = stk.count ;
    unsigned int visit[vlen + 1] ;

    memset(visit, 0, (vlen + 1) * sizeof(unsigned int)) ;

    {
        FOREACH_STK(&stk, pos) {

            element++ ;

            if (!visit[element]) {

                char *name = stk.s + pos ;

                if (!none) {

                    if (!graph_remove_edge(&g->g, treename, name, false))
                        log_dieu(LOG_EXIT_SYS,"remove edge: ", name, " from vertex: ", treename);

                } else {

                    if (deps) {
                        if (!strcmp(name, deps)) {
                            visit[element] = 1 ;
                            continue ;
                        }
                    }

                    if (!auto_stra(&sa, name, " "))
                        log_die_nomem("stralloc") ;

                    nb++ ;
                }

                visit[element] = 1 ;
            }
        }
    }

    if (sa.len)
        sa.len-- ; //remove last " "

    if (!stralloc_0(&sa))
        log_die_nomem("stralloc") ;

    uint_pack(pack, nb) ;
    pack[u32_fmt(pack, nb)] = 0 ;

    if (resolve_read_g(wres, base, treename) <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", treename) ;

    table.u.tree.id = ewhat ;

    if (!resolve_modify_field(wres, table, sa.s))
        log_dieusys(LOG_EXIT_SYS, "modify resolve file of: ", treename) ;

    table.u.tree.id = nwhat ;

    if (!resolve_modify_field(wres, table, pack))
            log_dieusys(LOG_EXIT_SYS, "modify resolve file of: ", treename) ;

    if (!resolve_write_g(wres, base, treename))
        log_dieusys(LOG_EXIT_SYS, "write resolve file of: ", treename) ;

    if (!none) {

        if (!graph_sort(&g->g, requiredby ? true : false))
            log_die(LOG_EXIT_SYS, "sort the graph") ;
    }

    stralloc_free(&sa) ;
    resolve_free(wres) ;

    log_info(requiredby ? "Required by " : "Dependencies ", "successfully managed for tree: ", treename) ;
}

void tree_depends_requiredby_deps(tree_graph_t *g, char const *base, char const *treename, uint8_t requiredby, uint8_t none, char const *deps)
{
    log_flow() ;

    size_t baselen = strlen(base), pos = 0, element = 0 ;
    char solve[baselen + SS_SYSTEM_LEN + 1] ;
    vertex_t *v = NULL ;

    HASH_FIND_STR(g->g.vertexes, treename, v) ;
    if (v == NULL)
        log_dieu(LOG_EXIT_SYS, "get information of treename: ", treename, " -- please make a bug report") ;

    uint32_t nvertex = requiredby ? v->nrequiredby : v->ndepends ;
    _alloc_stk_(stk, nvertex * SS_MAX_TREENAME) ;

    if (!graph_get_stkedge(&stk, &g->g, v, requiredby ? true : false))
        log_dieusys(LOG_EXIT_SYS, "get edge of treename: ", treename) ;

    unsigned int visit[stk.count + 1] ;

    memset(visit, 0, (stk.count + 1) * sizeof(unsigned int)) ;

    auto_strings(solve, base, SS_SYSTEM) ;

    FOREACH_STK(&stk, pos) {

        element++ ;

        if (!visit[element]) {

            char *name = stk.s + pos ;

            tree_depends_requiredby(g, base, name, !requiredby, none, deps) ;

            visit[element] = 1 ;
        }
    }
}

/** @what -> 0 deny
 * @what -> 1 allow */
void tree_rules(char const *base, char const *treename, uid_t *uids, uint8_t what)
{
    log_flow() ;

    int r ;
    size_t uidn = uids[0], pos = 0 ;
    uid_t owner = MYUID ;
    char pack[256] ;
    stralloc sa = STRALLOC_ZERO ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    resolve_enum_table_t table = E_TABLE_TREE_ZERO ;

    log_trace("set ", !what ? "denied" : "allowed", " user for tree: ", treename, "..." ) ;

    if (resolve_read_g(wres, base, treename)  <= 0)
        log_dieusys(LOG_EXIT_SYS, "read resolve file of: ", treename) ;

    if (tres.nallow)
        if (!sastr_clean_string(&sa, tres.sa.s + tres.allow))
            log_dieu(LOG_EXIT_SYS, "clean string") ;

    /** fresh creation of the tree */
    if (!tres.nallow) {

        if (!uids[0]) {

            uids[0] = 1 ;
            uids[1] = owner ;

        } else {
            /** command can be 66 tree -a <account> <tree>
             * where <tree> doesn't exist yet.
             * Keep the -a option value and append the owner
             * of the process at the end of the list. */
            uids[0]++ ;
            uids[uidn + 1] = owner ;
        }

        uidn++ ;

    }

    for (; pos < uidn ; pos++) {

        uint32_pack(pack,uids[pos+1]) ;
        pack[u32_fmt(pack,uids[pos+1])] = 0 ;

        r = sastr_cmp(&sa, pack) ;

        if (r < 0 && what) {

            if (!sastr_add_string(&sa, pack))
                log_die_nomem("stralloc") ;

            tres.nallow++ ;

            log_trace("user: ", pack, " is allowed for tree: ", treename) ;

        } else if (r >= 0 && !what) {

            if (owner == uids[pos+1]) {
                log_1_warn("you cannot deny yourself -- ignoring request") ;
                continue ;
            }

            if (!sastr_remove_element(&sa, pack))
                log_dieu(LOG_EXIT_SYS, "remove: ", pack, " from list") ;

            tres.nallow-- ;

            log_trace("user: ", pack, " is denied for tree: ", treename) ;
        }
    }

    if (!sastr_rebuild_in_oneline(&sa))
        log_dieu(LOG_EXIT_SYS, "rebuild string") ;

    table.u.tree.id = E_RESOLVE_TREE_ALLOW ;

    if (!resolve_modify_field(wres, table, sa.s))
        log_dieusys(LOG_EXIT_SYS, "modify resolve file of: ", treename) ;

    if (!resolve_write_g(wres, base, treename))
        log_dieusys(LOG_EXIT_SYS, "write resolve file of: ", treename) ;

    stralloc_free(&sa) ;
    resolve_free(wres) ;

    log_info("Permissions rules set successfully for tree: ", treename) ;
}

static void tree_service_switch_contents(char const *base, char const *treesrc, char const *treedst, ssexec_t *info)
{
    log_flow() ;

    _alloc_sa_(sa) ;
    size_t pos = 0 ; ssize_t r = -1 ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    resolve_service_t res = RESOLVE_SERVICE_ZERO ;
    resolve_wrapper_t_ref swres = resolve_set_struct(DATA_SERVICE, &res) ;
    resolve_enum_table_t table = E_TABLE_TREE_ZERO ;
    table.u.tree.id = E_RESOLVE_TREE_CONTENTS ;
    resolve_enum_table_t stable = E_TABLE_SERVICE_CONFIG_ZERO ;
    stable.u.service.id = E_RESOLVE_SERVICE_CONFIG_TREENAME ;

    if (!resolve_get_field_tosa_g(&sa, base, treesrc, DATA_TREE, table))
        log_dieu(LOG_EXIT_SYS, "get contents list of tree: ", treesrc) ;

    FOREACH_SASTR(&sa, pos) {
        log_trace("switch service: ", sa.s + pos, " to tree: ", treedst) ;

        /** Tree may be corrupted, check the validity of the service
         * before switching it. It also avoid to crash the process
         * for an unexisting service which can cause a stuck situation where
         * you cannot remove a tree for a corrupted list of service.*/

        r = resolve_read_g(swres, base, sa.s + pos) ;
        if (r == -1)
            log_dieusys(LOG_EXIT_SYS, "get information of service: ", sa.s + pos, " -- please make a bug report") ;

        if (!r)
            continue ;

        tree_service_add(treedst, sa.s + pos, info) ;

        if (!resolve_modify_field_g(swres, base, sa.s + pos, stable, treedst))
            log_dieu(LOG_EXIT_SYS, "modify resolve file of: ", sa.s + pos) ;
    }

    resolve_free(wres) ;
    resolve_free(swres) ;
}

void tree_remove(tree_graph_t *g, char const *base, char const *treename, ssexec_t *info)
{
    log_flow() ;

    int r ;
    char tree[SS_MAX_TREENAME + 1] ;
    char *current = SS_DEFAULT_TREENAME ;

    log_trace("delete: ", treename, "..." ) ;

    tree_enable_disable(g, base, treename, 0) ;

    /** depends */
    tree_depends_requiredby_deps(g, base, treename, 0, 1, treename) ;

    /** requiredby */
    tree_depends_requiredby_deps(g, base, treename, 1, 1, treename) ;

    if (tree_iscurrent(base, treename)) {
        /** This symlink must be valid in any case to avoid crashing the sanitize_system process.
         * If it is not valid, at least point it to the SS_DEFAULT_TREENAME,
         * as this tree is automatically created at every 66 command invocation
         * if it does not exist yet. */
        log_warn("tree ",treename, " is marked as default -- switch default to: ", SS_DEFAULT_TREENAME) ;

        if (!tree_switch_current(base, SS_DEFAULT_TREENAME))
            log_dieusys(LOG_EXIT_SYS,"set: ", SS_DEFAULT_TREENAME, " as default") ;

        log_info("Set successfully: ", SS_DEFAULT_TREENAME," as default") ;

    } else {

        r = tree_find_current(tree, base) ;
        if (r < 0)
            log_dieu(LOG_EXIT_SYS, "find default tree") ;

        if (r)
            current = tree ;
        else
            current = SS_DEFAULT_TREENAME ;
    }

    tree_service_switch_contents(base, treename, current, info) ;

    log_trace("remove resolve file of tree: ", treename) ;
    resolve_remove_g(base, treename, DATA_TREE) ;

    tree_master_modify_contents(base) ;

    log_info("Deleted successfully: ", treename) ;
}

void tree_current(ssexec_t *info)
{
    log_trace("mark: ", info->treename.s," as default ..." ) ;

    if (!tree_switch_current(info->base.s, info->treename.s))
        log_dieusys(LOG_EXIT_SYS,"set: ", info->treename.s, " as default") ;

    log_info("Set successfully: ", info->treename.s," as default") ;

}

void tree_clone(char const *clone, ssexec_t *info)
{
    log_flow() ;

    struct stat st ;
    resolve_tree_t tres = RESOLVE_TREE_ZERO ;
    resolve_wrapper_t_ref wres = resolve_set_struct(DATA_TREE, &tres) ;
    resolve_enum_table_t table = E_TABLE_TREE_ZERO ;

    size_t syslen = info->base.len + SS_SYSTEM_LEN, clonelen = strlen(clone) ;

    if (resolve_check_g(wres, info->base.s, clone))
        log_die(LOG_EXIT_USER, clone, ": already exist") ;

    /** copy tree resolve file */
    char src[syslen + SS_RESOLVE_LEN + 1 + info->treename.len + 1] ;
    char dst[syslen + SS_RESOLVE_LEN + 1 + clonelen + 1] ;
    auto_strings(src, info->base.s, SS_SYSTEM, SS_RESOLVE, "/", info->treename.s) ;
    auto_strings(dst, info->base.s, SS_SYSTEM, SS_RESOLVE, "/", clone) ;

    if (stat(src, &st) < 0)
        log_dieusys(LOG_EXIT_SYS, "stat: ", src) ;

    if (!filecopy_unsafe(src, dst, st.st_mode))
        log_dieusys(LOG_EXIT_SYS, "copy: ", src, " to: ", dst) ;

    if (lchown(dst, st.st_uid, st.st_gid) < 0)
        log_dieusys(LOG_EXIT_SYS, "chown: ", dst) ;

    if (resolve_read_g(wres, info->base.s, clone) <= 0)
        log_dieu(LOG_EXIT_SYS, "read resolve file of tree: ", clone) ;

    table.u.tree.id = E_RESOLVE_TREE_INIT ;

    if (!resolve_modify_field(wres, table, 0))
        log_dieusys(LOG_EXIT_SYS, "modify resolve file of tree: ", clone) ;

    table.u.tree.id = E_RESOLVE_TREE_SUPERVISED ;

    if (!resolve_modify_field(wres, table, 0))
        log_dieusys(LOG_EXIT_SYS, "modify resolve file of tree: ", clone) ;

    table.u.tree.id = E_RESOLVE_TREE_CONTENTS ;

    if (!resolve_modify_field(wres, table, ""))
        log_dieusys(LOG_EXIT_SYS, "modify resolve file of tree: ", clone) ;

    table.u.tree.id = E_RESOLVE_TREE_NCONTENTS ;

    if (!resolve_modify_field(wres, table, 0))
        log_dieusys(LOG_EXIT_SYS, "modify resolve file of tree: ", clone) ;

    table.u.tree.id = E_RESOLVE_TREE_ENABLED ;

    if (!resolve_modify_field(wres, table, 0))
        log_dieusys(LOG_EXIT_SYS, "modify resolve file of tree: ", clone) ;

    table.u.tree.id = E_RESOLVE_TREE_NAME ;

    if (!resolve_modify_field(wres, table, clone))
        log_dieusys(LOG_EXIT_SYS, "modify resolve file of tree: ", clone) ;

    if (!resolve_write_g(wres, info->base.s, clone))
        log_dieusys(LOG_EXIT_SYS, "write resolve file of tree: ", clone) ;

    resolve_free(wres) ;

    /** tree Master resolve file */
    tree_master_modify_contents(info->base.s) ;

    log_info("Cloned successfully: ", info->treename.s, " to: ", clone) ;
}

int ssexec_tree_admin(int argc, char const *const *argv, ssexec_t *info)
{
    log_flow();

    int r ;
    /** We can arrive here from other ssexec_xxx functions that
     * already define the tree name. It will be overwritten,
     * correcting the info structure.
     * Therefore, retrieve the original name at the end of the process. */
    char oldtree[SS_MAX_TREENAME + 1] ;
    stralloc sa = STRALLOC_ZERO ;
    tree_graph_t graph = GRAPH_TREE_ZERO ;
    uint32_t flag = GRAPH_WANT_DEPENDS|GRAPH_WANT_REQUIREDBY, ntree = 0 ;

    tree_what_t what = what_init() ;

    {
        subgetopt l = SUBGETOPT_ZERO ;

        for (;;)
        {
            int opt = subgetopt_r(argc, argv, OPTS_TREE_ADMIN, &l) ;
            if (opt == -1) break ;

            switch (opt)
            {
                case 'c' :

                    what.current = 1 ;
                    what.nopts++ ;
                    break ;

               case 'o' :

                    if (!auto_stra(&sa, l.arg))
                        log_die_nomem("stralloc") ;
                    what.nopts++ ;
                    break ;

                case 'E' :

                    what.enable = 1 ;
                    what.nopts++ ;
                    break ;

                case 'D' :

                    what.disable = 1 ;
                    what.nopts++ ;
                    break ;

                case 'R' :

                    what.remove = 1 ;
                    what.create = 0 ;
                    what.nopts++ ;
                    break ;

                default :

                    log_usage(info->usage, "\n", info->help) ;
            }
        }
        argc -= l.ind ; argv += l.ind ;
    }

    if (argc < 1)
        log_usage(info->usage, "\n", info->help) ;

    if (!graph_new(&graph, SS_MAX_SERVICE))
        log_dieusys(LOG_EXIT_SYS, "initiate the graph") ;

    check_identifier(argv[0]) ;

    if (info->opt_tree) {

        auto_strings(oldtree, info->treename.s) ;

    } else {
        /** avoid empty string */
        auto_strings(oldtree, argv[0]) ;
    }

    info->treename.len = 0 ;
    if (!auto_stra(&info->treename, argv[0]))
        log_die_nomem("stralloc") ;

    r = tree_isvalid(info->base.s, info->treename.s) ;
    if (r < 0)
        log_diesys(LOG_EXIT_SYS, "invalid tree directory") ;

    if (sa.len)
       tree_parse_options(&graph, sa.s, info, &what) ;

    /** create is the option by default
     * mark it false if the tree already exist */
    if (r) {
        if (!what.nopts) {
            log_warn(info->treename.s, ": already exist") ;
            goto freed ;
        }
        what.create = 0 ;
    }

    if(!r && what.create)
        tree_create(&graph, info, &what) ;

    if (!r && what.remove)
        log_dieusys(LOG_EXIT_SYS,"find tree: ", info->treename.s) ;

    ntree = tree_graph_build_master(&graph, info, flag) ;

    if (!ntree)
        log_dieusys(LOG_EXIT_SYS, "build the graph") ;

    if (what.remove) {
        tree_remove(&graph, info->base.s, info->treename.s, info) ;
        goto freed ;
    }

    /** groups have influence on enable. Apply it first. */
    if (what.groups)
        tree_groups(&graph, info->base.s, info->treename.s, what.gr) ;

    if (what.depends) {

        tree_depends_requiredby(&graph, info->base.s, info->treename.s, 0, what.ndepends, 0) ;

        tree_depends_requiredby_deps(&graph, info->base.s, info->treename.s, 0, what.ndepends, 0) ;

        size_t pos = 0 ;

        vertex_t *v = NULL ;
        HASH_FIND_STR(graph.g.vertexes, info->treename.s, v) ;
        if (v == NULL)
            log_dieu(LOG_EXIT_SYS, "get information of treename: ", info->treename.s, " -- please make a bug report") ;

        uint32_t nvertex = v->ndepends ;
        _alloc_stk_(stk, nvertex * SS_MAX_TREENAME) ;

        if (!graph_get_stkedge(&stk, &graph.g, v, false))
            log_dieu(LOG_EXIT_SYS,"get dependency list of tree: ", info->treename.s) ;

        if (tree_isenabled(info->base.s, info->treename.s)) {
            FOREACH_STK(&stk, pos)
                tree_enable_disable(&graph, info->base.s, stk.s + pos, 1) ;
        }
    }

    if (what.requiredby) {

        tree_depends_requiredby(&graph, info->base.s, info->treename.s, 1, what.nrequiredby, 0) ;

        tree_depends_requiredby_deps(&graph, info->base.s, info->treename.s, 1, what.nrequiredby, 0) ;

        size_t pos = 0 ;

        vertex_t *v = NULL ;
        HASH_FIND_STR(graph.g.vertexes, info->treename.s, v) ;
        if (v == NULL)
            log_dieu(LOG_EXIT_SYS, "get information of treename: ", info->treename.s, " -- please make a bug report") ;

        uint32_t nvertex = v->nrequiredby ;
        _alloc_stk_(stk, nvertex * SS_MAX_TREENAME) ;

        if (!graph_get_stkedge(&stk, &graph.g, v, true))
            log_dieu(LOG_EXIT_SYS,"get dependency list of tree: ", info->treename.s) ;

        if (!tree_isenabled(info->base.s, info->treename.s)) {
            FOREACH_STK(&stk, pos)
                tree_enable_disable(&graph, info->base.s, stk.s + pos, 0) ;
        }
    }

    if (what.enable)
        tree_enable_disable(&graph, info->base.s, info->treename.s, 1) ;

    if (what.disable)
        tree_enable_disable(&graph, info->base.s, info->treename.s, 0) ;

    if (what.allow)
        tree_rules(info->base.s, info->treename.s, what.auids, 1) ;

    if (what.deny)
        tree_rules(info->base.s, info->treename.s, what.duids, 0) ;

    if (what.current)
        tree_current(info) ;

    if (what.clone)
        tree_clone(what.sclone, info) ;

    freed:
        info->treename.len = 0 ;
        if (!auto_stra(&info->treename, oldtree))
            log_die_nomem("stralloc") ;

        stralloc_free(&sa) ;
        tree_graph_destroy(&graph) ;


    return 0 ;
}
