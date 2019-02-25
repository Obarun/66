/* 
 * ssexec_help.c
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

#include <66/ssexec.h>

char const *usage_enable = "66-enable [ -h help ] [ -v verbosity ] [ - l live ] [ -t tree ] [ -f ] [ -S ] service(s)" ;

char const *help_enable =
"66-enable <options> service(s)\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-t: name of the tree to use\n"
"	-f: overwrite service(s)\n"
"	-S: enable and start the service\n"
;

char const *usage_dbctl = "66-dbctl [ -h ] [ -v verbosity ] [ -T timeout ] [ -l live ] [ -t tree ] [ -u | d | r ] service(s)" ;

char const *help_dbctl =
"66-dbctl <options> tree\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-T: timeout\n"
"	-l: live directory\n"
"	-t: tree to use\n"
"	-u: bring up service in database of tree\n"
"	-d: bring down service in database of tree\n"
"	-r: reload service\n"
;

char const *usage_disable = "66-disable [ -h help ] [ -v verbosity ] [ - l live ] [ -t tree ] [ -S ] service(s)" ;

char const *help_disable =
"66-disable <options> service(s)\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-t: name of the tree to use\n"
"	-S: disable and stop the service\n"
;

char const *usage_init = "66-init [ -h ] [ -v verbosity ] [ -l live ] [ -c | d | B ] tree" ;

char const *help_init =
"66-init <options> tree\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n" 
"	-c: init classic service\n"
"	-d: init database service\n"
"	-B: init classic and database service\n"
;

char const *usage_start = "66-start [ -h ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] [ -r | R ] service(s)" ;

char const *help_start =
"66-start <options> service(s)\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-t: tree to use\n"
"	-T: timeout\n"
"	-r: reload the service(s)\n"
"	-R: reload service(s) file(s) and the service(s) itself\n"
;

char const *usage_stop = "66-stop [ -h ] [ -v verbosity ] [ -T timeout ] [ -l live ] [ -t tree ] [ -u ] [ -X ] [ -K ] service(s)" ;

char const *help_stop =
"66-stop <options> service(s)\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-T: timeout\n"
"	-t: tree to use\n"
"	-u: unsupervise service(s)\n"
"	-X: bring down the service(s) and kill his supervisor\n"
"	-K: kill the service(s) and keep it down\n"
;

char const *usage_svctl = "66-svctl [ -h ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] [ -n death ] [ -u | U | d | D | r | R | K | X ] service(s)" ;

char const *help_svctl =
"66-svctl <options> tree\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-t: tree to use\n"
"	-T: service timeout\n"
"	-n: number of death\n"
"	-u: bring up the service(s)\n"
"	-U: really up\n"
"	-d: bring down the service(s)\n"
"	-D: really down\n"
"	-r: reload\n"
"	-R: reload and really up\n"
"	-X: bring down the service(s) and the kill his supervisor\n"
"	-K: kill the service(s) and keep it down\n"
;

char const *usage_all = "66-all [ -h ] [ -v verbosity ] [ -f ] [ -T timeout ] [ -l live ] [ -o tree ] up/down" ;

char const *help_all =
"66-all <options> up/down\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-v: increase/decrease verbosity\n"
"	-T: timeout\n"
"	-l: live directory\n"
"	-o: tree to use\n"
"	-f: fork the process\n"
;
