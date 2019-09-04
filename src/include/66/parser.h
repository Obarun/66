/* 
 * parser.h
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
 
#ifndef SS_PARSER_H
#define SS_PARSER_H

#include <66/enum.h>

#include <sys/types.h>
#include <stdint.h>

#include <oblibs/oblist.h>
#include <oblibs/stralist.h>
#include <oblibs/mill.h>


#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/types.h>
#include <skalibs/avltree.h>

#include <66/ssexec.h>

extern stralloc keep ;
extern stralloc deps ;
//extern genalloc gadeps ;
extern genalloc gasv ;

typedef enum actions_e actions_t, *actions_t_ref ;
enum actions_e
{
	COMMON = 0 ,
	EXECRUN ,
	EXECFINISH ,
	EXECLOG ,
	EXECUP ,
	EXECDOWN,
	ENVIRON,
	SKIP
} ;

/**struct for run and finish file*/
typedef struct sv_exec_s sv_exec,*sv_exec_ref ;
struct sv_exec_s
{
	/**build=45->auto,build=46->custom*/
	int build ;
	uid_t runas ;
	unsigned int shebang ;
	unsigned int exec ;
} ;

typedef struct sv_execlog_s sv_execlog,*sv_execlog_ref ;
struct sv_execlog_s
{
	sv_exec run ;
	/**timeout[0]->kill,timeout[1]->finish
	 * kill[0][X] enabled,kill[0][0] not enabled*/
	uint32_t timeout[2][UINT_FMT] ;
	unsigned int destination ;
	uint32_t backup ;
	uint32_t maxsize ;
	/**timestamp=50->tai,timestamp=51->iso,52->none*/
	int timestamp ;
	unsigned int idga ; //pos in genalloc gadeps
	unsigned int nga ; //len of idga in genalloc gadeps
} ;

typedef struct sv_classic_longrun_s sv_classic_longrun,*sv_classic_ref ;
struct sv_classic_longrun_s
{
	sv_exec	run ;
	sv_exec finish ;
	sv_execlog log ;
} ;

typedef struct sv_oneshot_s sv_oneshot,*sv_oneshot_ref ;
struct sv_oneshot_s
{
	sv_exec up ;
	sv_exec down ;
} ;

typedef union sv_type_u sv_type_t,*sv_type_t_ref ;
union sv_type_u
{
	sv_classic_longrun classic_longrun ;
	sv_oneshot oneshot ;
} ;

typedef struct sv_name_s sv_name_t, *sv_name_t_ref ;
struct sv_name_s
{
	int itype ;/**int type =30->classic,31->bundle,32->longrun,33->oneshot*/
	int name ;//pos in keep
	unsigned int description ;//pos in keep
	unsigned int idga ; //pos in genalloc gadeps
	unsigned int nga ; //len of idga in genalloc gadeps
	unsigned int logname ; //pos in keep
	unsigned int dstlog ; //pos in keep
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
	/**flags[0]->down,flags[1]->nosetsid,
	 * down[1] enabled,down[0] not enabled*/
	int flags[2] ;
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
	unsigned int pipeline ; //pos in deps
	stralloc saenv ;
	/* path of the environment file, this is only concern the write 
	 * process, the read process could be different if conf/sysadmin/service
	 * exist */
	uint32_t srconf ; 
						
} ;

#define SV_EXEC_ZERO \
{ \
	0 ,\
	0 ,\
	0 ,\
	0 \
}

#define SV_EXECLOG_ZERO \
{ \
	SV_EXEC_ZERO,\
	{ { 0 } } ,\
	0 ,\
	0 ,\
	0 ,\
	0,\
	0,\
	0 \
}

#define SV_CLASSIC_LONGRUN_ZERO \
{ \
	SV_EXEC_ZERO,\
	SV_EXEC_ZERO, \
	SV_EXECLOG_ZERO \
}

#define SV_ONESHOT_ZERO \
{ \
	SV_EXEC_ZERO,\
	SV_EXEC_ZERO \
}

#define SV_NAME_ZERO \
{ \
	-1 ,\
	-1 ,\
	0 ,\
	0 ,\
	0 ,\
	0 ,\
	0 \
}
						
#define SV_ALLTYPE_ZERO \
{ \
	{ SV_CLASSIC_LONGRUN_ZERO } ,\
	SV_NAME_ZERO ,\
	{ 0 } ,\
	{ 0 } ,\
	0 , \
	{ 0 } , \
	{ { 0 } } ,\
	0 , \
	0 , \
	{ 0 } , \
	0 , \
	0 , \
	STRALLOC_ZERO , \
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
	int mandatory ;
	stralloc val ;
	
} ;
#define KEYNOCHECK_ZERO { .idsec = -1, .idkey = -1, .expected = -1, .mandatory = -1, .val = STRALLOC_ZERO }
extern keynocheck const keynocheck_zero ;//set in sv_alltype_zero.c

typedef struct section_s section_t,*section_t_ref ;
struct section_s
{
	stralloc main ;
	stralloc start ;
	stralloc stop ;
	stralloc logger ;
	stralloc environment ;
	uint32_t idx[5] ; //[0] == 0 -> no, [0] == 1-> yes
	char const *file ;
} ;
#define SECTION_ZERO { .main = STRALLOC_ZERO , \
						.start = STRALLOC_ZERO , \
						.stop = STRALLOC_ZERO , \
						.logger = STRALLOC_ZERO , \
						.environment = STRALLOC_ZERO , \
						.idx = { 0 } , \
						.file = 0 }

/** freed */
extern void sv_alltype_free(sv_alltype *sv) ;
extern void keynocheck_free(keynocheck *nocheck) ;
extern void section_free(section_t *sec) ;
extern void freed_parser(void) ;
/** enable phase */
extern int parser(sv_alltype *service,stralloc *src,char const *svname) ;
extern int parse_service_check_enabled(ssexec_t *info, char const *svname,uint8_t force,uint8_t *exist) ;
extern int parse_service_before(ssexec_t *info, stralloc *parsed_list, char const *sv,unsigned int *nbsv, stralloc *sasv,uint8_t force,uint8_t *exist) ;
extern int parse_service_deps(ssexec_t *info,stralloc *parsed_list, sv_alltype *sv_before, char const *sv,unsigned int *nbsv,stralloc *sasv,uint8_t force) ;
extern int parse_add_service(stralloc *parsed_list,sv_alltype *sv_before,char const *service,unsigned int *nbsv,uid_t owner) ;
/** mill utilities 
extern parse_mill_t MILL_FIRST_BRACKET ;
extern parse_mill_t MILL_GET_AROBASE_KEY ;
extern parse_mill_t MILL_GET_COMMENTED_KEY ;
extern parse_mill_t MILL_GET_SECTION_NAME ; */
/** utilities 
extern int parse_line(stralloc *src,size_t *pos) ;
extern int parse_bracket(stralloc *src,size_t *pos) ; */
/** split */
extern int section_get_range(section_t *sasection,stralloc *src) ;
extern int key_get_range(genalloc *ga, section_t *sasection,int *svtype) ;
extern int get_mandatory(genalloc *nocheck,int idsec,int idkey) ;
extern int nocheck_toservice(keynocheck *nocheck,int svtype, sv_alltype *service) ;
/** store */
extern int keep_common(sv_alltype *service,keynocheck *nocheck,int svtype) ;
extern int keep_runfinish(sv_exec *exec,keynocheck *nocheck) ;
extern int keep_logger(sv_execlog *log,keynocheck *nocheck) ;
/** helper 
extern void section_setsa(int id, stralloc_ref *p,section_t *sa) ;
extern int section_get_skip(char const *s,size_t pos,int nline) ;
extern int section_get_id(stralloc *secname, char const *string,size_t *pos,int *id) ;
extern int key_get_next_id(stralloc *sa, char const *string,size_t *pos) ;
extern void parse_err(int ierr,int idsec,int idkey) ; */
extern int read_svfile(stralloc *sasv,char const *name,char const *src) ;
extern int add_pipe(sv_alltype *sv, stralloc *sa) ;
/** write */
extern int write_services(ssexec_t *info,sv_alltype *sv, char const *workdir, uint8_t force,uint8_t conf) ;
extern int write_classic(sv_alltype *sv, char const *dst, uint8_t force, uint8_t conf) ;
extern int write_longrun(sv_alltype *sv,char const *dst, uint8_t force, uint8_t conf) ;
extern int write_oneshot(sv_alltype *sv,char const *dst, uint8_t conf) ;
extern int write_bundle(sv_alltype *sv, char const *dst) ;
extern int write_common(sv_alltype *sv, char const *dst,uint8_t conf) ;
extern int write_exec(sv_alltype *sv, sv_exec *exec,char const *name,char const *dst,int mode) ;
extern int write_uint(char const *dst, char const *name, uint32_t ui) ;
extern int write_logger(sv_alltype *sv, sv_execlog *log,char const *name, char const *dst, int mode, uint8_t force) ;
extern int write_consprod(sv_alltype *sv,char const *prodname,char const *consname,char const *proddst,char const *consdst) ;
extern int write_dependencies(unsigned int nga,unsigned int idga,char const *dst,char const *filename) ;
extern int write_env(char const *name,stralloc *sa,char const *dst) ;

#endif
