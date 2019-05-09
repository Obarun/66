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
 
#ifndef PARSER_H
#define PARSER_H

#include <66/enum.h>

#include <sys/types.h>
#include <stdint.h>

#include <oblibs/oblist.h>
#include <oblibs/stralist.h>

#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/types.h>
#include <skalibs/avltree.h>

#include <66/ssexec.h>

extern stralloc keep ;
extern stralloc deps ;
extern stralloc saenv ;
extern genalloc ganame ;
extern genalloc gadeps ;
extern genalloc gasv ;
extern avltree deps_map ;

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
	/**timestamp=49->tai,timestamp=50->iso*/
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
	unsigned int name ;//pos in keep
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
	/**opts[0]->logger,opts[1]->pipeline,[2]->env,opts[3]->data
	 * logger[1] enabled,logger[0] not enabled*/
	unsigned int opts[4] ;
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
	int signal ;//down-signal file
	unsigned int pipeline ; //pos in deps
	genalloc env ; //type diuint32, pos in saenv
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
	0 ,\
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
	0 , \
	0 ,\
	GENALLOC_ZERO \
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


typedef struct parse_mill_inner_s parse_mill_inner_t, *parse_mill_inner_t_ref ;
struct parse_mill_inner_s
{
	char curr ;
	uint32_t nline ;
	uint8_t nopen ; //number of open found
	uint8_t nclose ; //number of close found
	uint8_t jumped ;//jump was made or not 1->no,0->yes
	uint8_t flushed ;//flush was made or not 1->no,0->yes
	
} ;
#define PARSE_MILL_INNER_ZERO { .curr = 0, \
							.nline = 1, \
							.nopen = 0, \
							.nclose = 0, \
							.jumped = 0, \
							.flushed = 0 }
							
typedef struct parse_mill_s parse_mill_t,*parse_mill_t_ref ;
struct parse_mill_s
{
	char const open ;
	char const close ;
	uint8_t force ; //1 -> only one open and close
	char const *skip ;
	size_t skiplen ;
	char const *end ;
	size_t endlen ;
	char const *jump ;//skip the complete line
	size_t jumplen ;
	uint8_t check ;//check if nopen == openclose, 0 -> no,1->yes
	uint8_t flush ;//set nopen,nclose,sa.len to 0 at every new line
	uint8_t forceskip ;//force to skip even if nopen is positive
	parse_mill_inner_t inner ;
} ;

typedef enum parse_enum_e parse_enum_t,*parse_enum_t_ref ;
enum parse_enum_e
{
	IGN = 0 ,
	KEEP ,
	JUMP ,
	EXIT ,
	END
} ;

/** Main */
extern int parser(sv_alltype *service,stralloc *src,char const *file) ;
extern int parse_config(parse_mill_t *p,char const *file, stralloc *src, stralloc *kp,size_t *pos) ;
extern char next(stralloc *s,size_t *pos) ;
extern uint8_t cclass (parse_mill_t *p) ;
/** freed */
extern void sv_alltype_free(sv_alltype *sv) ;
extern void keynocheck_free(keynocheck *nocheck) ;
extern void section_free(section_t *sec) ;
extern void freed_parser(void) ;
/** utilities */
extern int parse_line(stralloc *src) ;
extern int parse_quote(stralloc *src) ;
extern int parse_bracket(stralloc *src) ;
extern int parse_env(stralloc *src) ;
/** split */
extern int get_section_range(section_t *sasection,stralloc *src) ;
extern int get_key_range(genalloc *ga, section_t *sasection,char const *file,int *svtype) ;
extern int get_mandatory(genalloc *nocheck,int idsec,int idkey) ;
extern int nocheck_toservice(keynocheck *nocheck,int svtype, sv_alltype *service) ;
/** store */
extern int keep_common(sv_alltype *service,keynocheck *nocheck,int svtype) ;
extern int keep_runfinish(sv_exec *exec,keynocheck *nocheck) ;
extern int keep_logger(sv_execlog *log,keynocheck *nocheck) ;
/** helper */
extern int read_svfile(stralloc *sasv,char const *name,char const *src) ;
extern ssize_t get_sep_before (char const *line, char const sepstart, char const sepend) ;
extern void section_setsa(int id, stralloc_ref *p,section_t *sa) ;
extern int section_skip(char const *s,size_t pos,int nline) ;
extern int section_valid(int id, uint32_t nline, size_t pos,stralloc *src, char const *file) ;
extern int clean_value(stralloc *sa) ;
extern void parse_err(int ierr,int idsec,int idkey) ;
extern int add_pipe(sv_alltype *sv, stralloc *sa) ;

/** enable phase */
extern int add_cname(genalloc *ga,avltree *tree,char const *name, sv_alltype *sv_before) ;
extern int resolve_srcdeps(sv_alltype *sv_before,char const *svmain,char const *src, char const *tree,unsigned int *nbsv,stralloc *sasv,unsigned int force) ;
extern int parse_service_before(char const *src,char const *sv,char const *tree, unsigned int *nbsv, stralloc *sasv,unsigned int force) ;

/** write */
extern int write_services(ssexec_t *info,sv_alltype *sv, char const *workdir, unsigned int force) ;
extern int write_classic(sv_alltype *sv, char const *dst, unsigned int force) ;
extern int write_longrun(sv_alltype *sv,char const *dst, unsigned int force) ;
extern int write_oneshot(sv_alltype *sv,char const *dst, unsigned int force) ;
extern int write_bundle(sv_alltype *sv, char const *dst, unsigned int force) ;
extern int write_common(sv_alltype *sv, char const *dst) ;
extern int write_exec(sv_alltype *sv, sv_exec *exec,char const *name,char const *dst,int mode) ;
extern int write_uint(char const *dst, char const *name, uint32_t ui) ;
extern int write_logger(sv_alltype *sv, sv_execlog *log,char const *name, char const *dst, int mode, unsigned int force) ;
extern int write_consprod(sv_alltype *sv,char const *prodname,char const *consname,char const *proddst,char const *consdst) ;
extern int write_dependencies(unsigned int nga,unsigned int idga,char const *dst,char const *filename, genalloc *ga, unsigned int force) ;
extern int write_env(char const *name,stralloc *sa,char const *dst) ;

#endif
