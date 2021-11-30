/*
 * parser.h
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

#ifndef SS_PARSER_H
#define SS_PARSER_H

#include <66/enum.h>

#include <sys/types.h>
#include <stdint.h>

#include <oblibs/mill.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/types.h>
#include <skalibs/avltree.h>

#include <66/ssexec.h>

extern stralloc keep ;
extern stralloc deps ;
extern genalloc gasv ;

/**struct for run and finish file*/
typedef struct sv_exec_s sv_exec,*sv_exec_ref ;
struct sv_exec_s
{
    int build ;
    int runas ;
    int shebang ;
    int exec ;
    int real_exec ;
} ;

typedef struct sv_execlog_s sv_execlog,*sv_execlog_ref ;
struct sv_execlog_s
{
    sv_exec run ;
    /**timeout[0]->kill,timeout[1]->finish
     * kill[0][X] enabled,kill[0][0] not enabled*/
    uint32_t timeout[2][UINT_FMT] ;
    int destination ;
    uint32_t backup ;
    uint32_t maxsize ;
    int timestamp ;
    int idga ; //pos in stralloc deps
    unsigned int nga ; //number of deps in stralloc deps
} ;

typedef struct sv_classic_longrun_s sv_classic_longrun,*sv_classic_ref ;
struct sv_classic_longrun_s
{
    sv_exec run ;
    sv_exec finish ;
    sv_execlog log ;
} ;

typedef struct sv_oneshot_s sv_oneshot,*sv_oneshot_ref ;
struct sv_oneshot_s
{
    sv_exec up ;
    sv_exec down ;
    sv_execlog log ;
} ;

typedef struct sv_module_s sv_module,*sv_module_ref ;
struct sv_module_s
{
    int configure ;
    int iddir ; // pos in stralloc keep -> @directories
    unsigned int ndir ; //number of regex directories in stralloc keep -> @directories
    int idfiles ; // pos in stralloc keep -> @files
    unsigned int nfiles ; //number of regex files in stralloc keep -> @files
    int start_infiles ; // pos in stralloc keep of the start of the string -> @infiles
    int end_infiles ; // pos in stralloc keep of the end of the string -> @infiles
    int idaddservices ; // pos in stralloc keep -> @addservices
    unsigned int naddservices ; // number of addon services in stralloc keep > @addservices
} ;

typedef struct sv_type_s sv_type_t,*sv_type_t_ref ;
struct sv_type_s
{
    sv_classic_longrun classic_longrun ;
    sv_oneshot oneshot ;
    sv_module module ;
} ;

typedef struct sv_name_s sv_name_t, *sv_name_t_ref ;
struct sv_name_s
{
    int itype ; //servcie type: classic->bundle->longrun->oneshot->modules
    int name ; //pos in keep
    int description ; //pos in keep
    int version ; // pos in keep
    int intree ; // pos in keep
    int idga ; //pos in stralloc deps -> @depends
    unsigned int nga ; //number of deps in stralloc deps -> @depends
    int idopts ; // pos in stralloc deps -> @optsdepends
    unsigned int nopts ; // number of optional depends in stralloc deps-> @optsdepends
    int idext ; // pos in stralloc deps -> @extdepends
    unsigned int next ; // number of optinal depends in stralloc deps -> @extdepends
    int logname ; //pos in keep
    int dstlog ; //pos in keep
    int idcontents ; // pos in stralloc deps -> @contents
    unsigned int ncontents ; // number of service inside a bundle
} ;


typedef struct sv_alltype_s sv_alltype,*sv_alltype_ref ;
struct sv_alltype_s
{
    sv_type_t type ;
    sv_name_t cname ;//cname, Container Name
    /**opts[0]->logger,opts[1]->pipeline,[2]->env
     * logger[1] enabled,logger[0] not enabled*/
    unsigned int opts[3] ;
    /**0->no notification*/
    /**flags[0]->down
     * down[1] enabled,down[0] not enabled*/
    unsigned int flags[1] ;
    uint32_t notification ;
    /** array of uid_t
     * the first element of the table
     * is reserved to know the number of
     * user set e.g user[0]=3->3 user*/
    uid_t user[256] ;
    /**timeout[0]->kill,timeout[1]->finish
    * timeout[2]->up,timeout[3]->down
    * timeout-kill[0][X] enabled,timeout-kill[0][0] not enabled*/
    uint32_t timeout[4][UINT_FMT] ;
    uint32_t src ;// original source of the service
    uint32_t death ;//max-death-tally file
    /** array of uint32_t
     * the first element of the table
     * is reserved to know the number of
     * dir/file to copy e.g hiercopy[0]=3->3 dir/file to copy */
    uint32_t hiercopy[24] ; //dir/file to copy
    int signal ;//down-signal file
    int pipeline ; //pos in deps
    stralloc saenv ;
    /* path of the environment file, this is only concern the write
     * process, the read process could be different if conf/sysadmin/service
     * exist */
    uint32_t srconf ;
    /** set the CONF variable for each service.
     * useful in case of module. We do not overwrite
     * the configuration of the module but we overwrite
     * the configuration of each service inside a module */
    uint8_t overwrite_conf ;

} ;

#define SV_EXEC_ZERO \
{ \
    -1 , \
    -1 , \
    -1 , \
    -1 , \
    -1 \
}

#define SV_EXECLOG_ZERO \
{ \
    SV_EXEC_ZERO, \
    { { 0 } } , \
    -1 , \
    0 , \
    0 , \
    -1 , \
    -1 , \
    0 \
}

#define SV_CLASSIC_LONGRUN_ZERO \
{ \
    SV_EXEC_ZERO, \
    SV_EXEC_ZERO, \
    SV_EXECLOG_ZERO \
}

#define SV_ONESHOT_ZERO \
{ \
    SV_EXEC_ZERO, \
    SV_EXEC_ZERO, \
    SV_EXECLOG_ZERO \
}

#define SV_TYPE_ZERO \
{ \
    SV_CLASSIC_LONGRUN_ZERO , \
    SV_ONESHOT_ZERO , \
    SV_MODULE_ZERO \
}

#define SV_MODULE_ZERO \
{ \
    -1 , \
    -1 , \
    0 , \
    -1 , \
    0 , \
    -1 , \
    -1 , \
    -1 , \
    0 \
}

#define SV_NAME_ZERO \
{ \
    -1 , \
    -1 , \
    -1 , \
    -1 , \
    -1 , \
    -1 , \
    0 , \
    -1 , \
    0 , \
    -1 , \
    0 , \
    -1 , \
    -1 , \
    -1 , \
    0 \
}

#define SV_ALLTYPE_ZERO \
{ \
    SV_TYPE_ZERO , \
    SV_NAME_ZERO , \
    { 1,0,0 } , \
    { 0 } , \
    0 , \
    { 0 } , \
    { { 0 } } , \
    0 , \
    0 , \
    { 0 } , \
    -1 , \
    -1 , \
    STRALLOC_ZERO , \
    0 , \
    0 \
}

extern sv_alltype const sv_alltype_zero ;
extern sv_name_t const sv_name_zero ;//set in sv_alltype_zero.c

typedef struct keynocheck_s keynocheck, *keynocheck_t_ref;
struct keynocheck_s
{
    int idsec ;
    int idkey ;
    int expected ;
    stralloc val ;

} ;
#define KEYNOCHECK_ZERO { .idsec = -1, .idkey = -1, .expected = -1, .val = STRALLOC_ZERO }
extern keynocheck const keynocheck_zero ;//set in sv_alltype_zero.c

typedef struct section_s section_t,*section_t_ref ;
struct section_s
{
    stralloc main ;
    stralloc start ;
    stralloc stop ;
    stralloc logger ;
    stralloc environment ;
    stralloc regex ;
    uint32_t idx[6] ; //[0] == 0 -> no, [0] == 1-> yes
    char const *file ;
} ;
#define SECTION_ZERO { .main = STRALLOC_ZERO , \
                        .start = STRALLOC_ZERO , \
                        .stop = STRALLOC_ZERO , \
                        .logger = STRALLOC_ZERO , \
                        .environment = STRALLOC_ZERO , \
                        .regex = STRALLOC_ZERO , \
                        .idx = { 0 } , \
                        .file = 0 }

/** freed and cleanup*/
extern void sv_alltype_free(sv_alltype *sv) ;
extern void keynocheck_free(keynocheck *nocheck) ;
extern void section_free(section_t *sec) ;
extern void freed_parser(void) ;
extern void ssexec_enable_cleanup(void) ;

/** enable phase */
extern void start_parser(char const *sv, ssexec_t *info, uint8_t disable_module, char const *directory_forced) ;
extern int parse_service(char const *sv, stralloc *parsed_list, ssexec_t *info, uint8_t force, uint8_t conf) ;
int parse_service_alldeps(sv_alltype *alltype, ssexec_t *info, stralloc *parsed_list, uint8_t force, char const *directory_forced) ;
extern int parse_service_deps(sv_alltype *alltype, ssexec_t *info, stralloc *parsed_list, uint8_t force, char const *directory_forced) ;
extern int parse_service_optsdeps(stralloc *rebuild, sv_alltype *alltype, ssexec_t *info, stralloc *parsed_list, uint8_t force, uint8_t field, char const *directory_forced) ;
extern int parser(sv_alltype *service,stralloc *src,char const *svname,int svtype) ;

/** split */
extern int section_get_range(section_t *sasection,stralloc *src) ;
extern int key_get_range(genalloc *ga, section_t *sasection) ;
extern int check_mandatory(sv_alltype *service, section_t *sasection) ;
extern int nocheck_toservice(keynocheck *nocheck, int svtype, sv_alltype *service) ;

/** store */
extern int keep_common(sv_alltype *service,keynocheck *nocheck,int svtype) ;
extern int keep_runfinish(sv_exec *exec,keynocheck *nocheck) ;
extern int keep_logger(sv_execlog *log,keynocheck *nocheck) ;
extern int keep_environ(sv_alltype *service,keynocheck *nocheck) ;
extern int keep_regex(sv_module *module,keynocheck *nocheck) ;

/** helper */
extern int get_svtype(sv_alltype *sv_before, char const *contents) ;
extern int get_svtype_from_file(char const *file) ;
extern int get_svintree(sv_alltype *sv_before, char const *contents) ;
extern int add_pipe(sv_alltype *sv, stralloc *sa) ;

/** write */
extern void start_write(stralloc *tostart,unsigned int *nclassic,unsigned int *nlongrun,char const *workdir, genalloc *gasv,ssexec_t *info) ;
extern int write_services(sv_alltype *sv, char const *workdir, uint8_t force,uint8_t conf) ;
extern int write_classic(sv_alltype *sv, char const *dst, uint8_t force, uint8_t conf) ;
extern int write_longrun(sv_alltype *sv,char const *dst, uint8_t force, uint8_t conf) ;
extern int write_oneshot(sv_alltype *sv,char const *dst, uint8_t conf) ;
extern int write_bundle(sv_alltype *sv, char const *dst) ;
extern int write_common(sv_alltype *sv, char const *dst,uint8_t conf) ;
extern int write_exec(sv_alltype *sv, sv_exec *exec,char const *name,char const *dst,mode_t mode) ;
extern int write_uint(char const *dst, char const *name, uint32_t ui) ;
extern int write_logger(sv_alltype *sv, sv_execlog *log,char const *name, char const *dst, mode_t mode, uint8_t force) ;
extern int write_consprod(sv_alltype *sv,char const *prodname,char const *consname,char const *proddst,char const *consdst) ;
extern int write_dependencies(unsigned int nga,unsigned int idga,char const *dst,char const *filename) ;
extern int write_env(char const *name,char const *contents,char const *dst) ;
extern int write_oneshot_logger(stralloc *destlog, sv_alltype *sv) ;

/** module */
extern int parse_module(sv_alltype *alltype, ssexec_t *info, stralloc *parsed_list, uint8_t force) ;
extern int regex_get_file_name(char *filename,char const *str) ;
extern int regex_get_replace(char *replace, char const *str) ;
extern int regex_get_regex(char *regex, char const *str) ;
extern int regex_replace(stralloc *list,sv_alltype *sv_before,char const *svname) ;
extern int regex_rename(stralloc *list, int id, unsigned int nid, char const *sdir) ;
extern int regex_configure(sv_alltype *sv_before,ssexec_t *info, char const *module_dir,char const *module_name, uint8_t conf) ;
#endif
