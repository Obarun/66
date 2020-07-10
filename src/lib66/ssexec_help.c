/* 
 * ssexec_help.c
 * 
 * Copyright (c) 2018-2020 Eric Vidal <eric@obarun.org>
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

char const *usage_enable = "66-enable [ -h ] [ -z ] [ -v verbosity ] [ - l live ] [ -t tree ] [ -f|F ] [ -c|m|C ] [ -S ] service(s)" ;

char const *help_enable =
"66-enable <options> service(s)\n"
"\n"
"options :\n"
"	-h: print this help\n"
"	-z: use color\n"
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-t: name of the tree to use\n"
"	-f: force to overwrite the service(s)\n"
"	-F: force to overwrite the service(s) and it dependencies\n"
"	-c: only appends new key=value pairs at environment configuration file from frontend file\n"
"	-m: appends new key=value and merge existing one at environment configuration file from frontend file\n"
"	-C: overwrite it environment configuration file from frontend file\n"
"	-S: enable and start the service\n"
;

char const *usage_dbctl = "66-dbctl [ -h ] [ -z ] [ -v verbosity ] [ -T timeout ] [ -l live ] [ -t tree ] [ -u | d | r ] service(s)" ;

char const *help_dbctl =
"66-dbctl <options> tree\n"
"\n"
"options :\n"
"	-h: print this help\n" 
"	-z: use color\n"
"	-v: increase/decrease verbosity\n"
"	-T: timeout\n"
"	-l: live directory\n"
"	-t: tree to use\n"
"	-u: bring up service in database of tree\n"
"	-d: bring down service in database of tree\n"
"	-r: reload service\n"
;

char const *usage_disable = "66-disable [ -h ] [ -z ] [ -v verbosity ] [ - l live ] [ -t tree ] [ -S ] service(s)" ;

char const *help_disable =
"66-disable <options> service(s)\n"
"\n"
"options :\n"
"	-h: print this help\n"
"	-z: use color\n"
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-t: name of the tree to use\n"
"	-S: disable and stop the service\n"
;

char const *usage_init = "66-init [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] classic|database|both" ;

char const *help_init =
"66-init <options> classic|database|both\n"
"\n"
"options :\n"
"	-h: print this help\n"
"	-z: use color\n"
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n" 
"	-t: name of the tree to use\n"
;

char const *usage_start = "66-start [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] [ -r | R ] service(s)" ;

char const *help_start =
"66-start <options> service(s)\n"
"\n"
"options :\n"
"	-h: print this help\n"
"	-z: use color\n"
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-t: tree to use\n"
"	-T: timeout\n"
"	-r: reload the service(s)\n"
"	-R: reload service(s) file(s) and the service(s) itself\n"
;

char const *usage_stop = "66-stop [ -h ] [ -z ] [ -v verbosity ] [ -T timeout ] [ -l live ] [ -t tree ] [ -u ] [ -X ] [ -K ] service(s)" ;

char const *help_stop =
"66-stop <options> service(s)\n"
"\n"
"options :\n"
"	-h: print this help\n"
"	-z: use color\n"
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-T: timeout\n"
"	-t: tree to use\n"
"	-u: unsupervise service(s)\n"
"	-X: bring down the service(s) and kill his supervisor\n"
"	-K: kill the service(s) and keep it down\n"
;

char const *usage_svctl = "66-svctl [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] [ -n death ] [ -u | d | r | K | X ] service(s)" ;

char const *help_svctl =
"66-svctl <options> tree\n"
"\n"
"options :\n"
"	-h: print this help\n"
"	-z: use color\n"
"	-v: increase/decrease verbosity\n"
"	-l: live directory\n"
"	-t: tree to use\n"
"	-T: service timeout\n"
"	-n: number of death\n"
"	-u: bring up the service(s)\n"
"	-d: bring down the service(s)\n"
"	-r: reload\n"
"	-X: bring down the service(s) and the kill his supervisor\n"
"	-K: kill the service(s) and keep it down\n"
;

char const *usage_env = "66-env [ -h ] [ -z ] [ -v verbosity ] [ -t tree ] [ -d dir ] [ -L ] [ -e ] [ -r key=value ] [ -s version ] service" ;

char const *help_env =
"66-env <options> service\n"
"\n"
"options :\n"
"	-h: print this help\n"
"	-z: use color\n"
"	-v: increase/decrease verbosity\n"
"	-t: tree to use\n"
"	-L: list environment variable of service\n"
"	-d: absolute path to the configuration service file\n"
"	-r: replace the value of the key\n"
"	-e: edit the file with EDITOR\n"
"	-s: switch configuration symlink to version\n"
;
