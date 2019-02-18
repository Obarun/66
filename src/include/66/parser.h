/* 
 * parser.h
 * 
 * Copyright (c) 2018 Eric Vidal <eric@obarun.org>
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
	int nkey ;
	int idsec ;
	int idkey ;
	int expected ;
	int mandatory ;
	int idsa ;
	stralloc tbsa[115] ;
	
} ;
#define KEYNOCHECK_ZERO { .nkey = 0, .idsec = -1, .idkey = -1, .expected = -1, .mandatory = -1, .idsa = 0, .tbsa = { STRALLOC_ZERO } }
extern keynocheck const keynocheck_zero ;//set in sv_alltype_zero.c

typedef struct sv_db_s sv_db, *sv_db_t_ref ;
struct sv_db_s
{
	sv_alltype *services ; //pos in gasv
	unsigned int nsv ; //number of service
	char *string ;
	unsigned int stringlen ;
	unsigned int ndeps ;//deps genalloc_len	
} ;

/** Main functions */
extern int parser(char const *str,sv_alltype *service) ;

extern int get_section_range(char const *s, int idsec, genalloc *gasection) ;

extern int get_key_range(char const *s, int idsec, genalloc *ganocheck) ;

extern int get_keystyle(keynocheck *nocheck) ;

extern int nocheck_toservice(keynocheck *nocheck,int svtype, sv_alltype *service) ;

/** Sub functions set in parser_utils.c*/

/** Check if @septart exist before @sepend into @line
 * @Return the lenght on success
 * @Return -1 if @sepstart was not found or
 * if @sepstart is found after @sepend
 * @Return 0 if @sepend is not found*/
extern ssize_t get_sep_before (char const *line, char const sepstart, char const sepend) ;

/** Search in @s the '=' character
 * @Return n bytes to '='
 * @Return -1 if '=' is not found 
 * @Return -2 if '=' is the first character of the line*/
extern int scan_key(char const *s) ;

/** Search for a valid key at @s
 * remove all spaces around @s
 * @Return 1 on success
 *@Return 0 on fail*/
extern int get_cleankey(char *s) ;

extern int get_cleanval(char *s) ;

extern int get_keyline(char const *s,char const *key,stralloc *sa) ;

extern int get_key(char const *s,char const *key,stralloc *keep) ;

extern int get_nextkey(char const *val, int idsec, const key_description_t *list) ;

/** Scan @s to find '[' and ']' character
 * @Return n bytes to ']'
 * @Return 0 if '[' is not found
 * @Return -1 if ']' is not found*/ 
extern ssize_t scan_section(char const *s) ;

/** Search for a valid section on @s
 * and store the result in @sa
 * @Return 1 on success
 * @Return -4 for invalid section syntax
 * @Return -1 if it's not a section
 * @Return -2 if it's not a good section*/
extern int get_cleansection(char const *s,stralloc *sa) ;

extern int parse_line(keynocheck *nocheck) ;

extern int parse_quote(keynocheck *nocheck) ;

extern int parse_bracket(keynocheck *nocheck) ;

extern int parse_env(keynocheck *nocheck) ;

void parse_err(int ierr,int idsec,int idkey) ;

/** Search if the number of @sepstart and @sepend are equal
 * into @line over @lensearch of lenght
 * @Return 1 on success
 * @Return -1 if @sepstart>@sepend
 * @Return -2 if @sepstart<@sepend
 * @Return 0 if @sepstart && @sepend is zero*/
extern int scan_nbsep(char *line,int lensearch, char const sepstart, char const sepend) ;

extern int add_pipe(sv_alltype *sv, stralloc *sa) ;

/** Display the corresponding warning when @sepend and @sepstart are not equal */ 
extern void sep_err(int r,char const sepstart,char const sepend,char const *keyname) ;

extern int add_cname(genalloc *ga,avltree *tree,char const *name, sv_alltype *sv_before) ;

extern int add_env(char *line,genalloc *ga,stralloc *sa) ;

extern int parse_env(keynocheck *nocheck) ;

extern int resolve_srcdeps(sv_alltype *sv_before,char const *svmain,char const *src, char const *tree,unsigned int *nbsv,stralloc *sasv,unsigned int force) ;

extern int parse_service_before(char const *src,char const *sv,char const *tree, unsigned int *nbsv, stralloc *sasv,unsigned int force) ;

extern int keep_common(sv_alltype *service,keynocheck *nocheck) ;

extern int keep_runfinish(sv_exec *exec,keynocheck *nocheck) ;

extern int keep_logger(sv_execlog *log,keynocheck *nocheck) ;

extern int write_services(sv_alltype *sv, char const *workdir, unsigned int force) ;

extern int write_classic(sv_alltype *sv, char const *dst, unsigned int force) ;

extern int write_longrun(sv_alltype *sv,char const *dst, unsigned int force) ;

extern int write_oneshot(sv_alltype *sv,char const *dst, unsigned int force) ;

extern int write_bundle(sv_alltype *sv, char const *dst, unsigned int force) ;

extern int write_common(sv_alltype *sv, char const *dst) ;

extern int write_exec(sv_alltype *sv, sv_exec *exec,char const *name,char const *dst,int mode) ;

extern int write_uint(char const *dst, char const *name, uint32_t ui) ;

extern int write_logger(sv_alltype *sv, sv_execlog *log,char const *name, char const *dst, int mode, unsigned int force) ;

extern int write_consprod(sv_alltype *sv,char const *prodname,char const *consname,char const *proddst,char const *consdst) ;

extern int write_dependencies(sv_name_t *cname,char const *dst,char const *filename, genalloc *ga, unsigned int force) ;

extern int write_env(char const *name, genalloc *env,stralloc *sa,char const *dst) ;

extern void freed_parser(void) ;

#endif
