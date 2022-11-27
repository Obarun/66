/*
 * ssexec_help.c
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

#include <66/ssexec.h>

char const *usage_parse = "66 parse [ -h ] [ -z ] [ -v verbosity ] [ -t tree ] [ -f|F ] [ -I ] service" ;

char const *help_parse =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -f: force to overwrite existing destination\n"
"   -F: also force to overwrite its dependencies\n"
"   -I: do not import modified configuration files from previous version\n"
;

char const *usage_enable = "66 enable [ -h ] [ -z ] [ -v verbosity ] [ - l live ] [ -t tree ] [ -f|F ] [ -I ] [ -S ] service(s)" ;

char const *help_enable =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -t: name of the tree to use\n"
"   -f: force to overwrite the service(s)\n"
"   -F: force to overwrite the service(s) and its dependencies\n"
"   -I: do not import modified configuration files from previous version\n"
"   -S: enable and start the service\n"
;

char const *usage_disable = "66 disable [ -h ] [ -z ] [ -v verbosity ] [ - l live ] [ -t tree ] [ -S ] [ -F ] [ -R ] service(s)" ;

char const *help_disable =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -t: name of the tree to use\n"
"   -S: disable and stop the service\n"
"   -F: forces to disable the service\n"
"   -R: disable the service and remove its configuration and logger files\n"
;

char const *usage_init = "66 init [ -h ] [ -z ] [ -v verbosity ] [ -l live ] tree" ;

char const *help_init =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
;

char const *usage_start = "66 start [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] service(s)" ;

char const *help_start =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -t: tree to use\n"
"   -T: timeout\n"
;

char const *usage_stop = "66 stop [ -h ] [ -z ] [ -v verbosity ] [ -T timeout ] [ -l live ] [ -t tree ] [ -u ] service(s)" ;

char const *help_stop =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -T: timeout\n"
"   -t: tree to use\n"
"   -u: unsupervise service(s)\n"
;

char const *usage_svctl = "66 svctl [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] [ -wu | -wU | -wd | -wD | -wr | -wR ] [ -abqhkti12pcyoduxOr ] service(s)" ;

char const *help_svctl =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -t: tree to use\n"
"   -T: service timeout\n"
;

char const *usage_env = "66 env [ -h ] [ -z ] [ -v verbosity ] [ -t tree ] [ -c version ] [ -s version ] [ -V|L ] [ -r key=value ] [ -i src,dst ] [ -e editor ] service" ;

char const *help_env =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -t: tree to use\n"
"   -c: set version as default\n"
"   -s: specifies the version to handle\n"
"   -V: lists available versioned configuration directories of the service\n"
"   -L: lists the environment variables of the service\n"
"   -r: replace the value of the key\n"
"   -i: import configuration files from src version to dst version\n"
"   -e: edit the file with editor\n"
;

char const *usage_all = "66 all [ -h ] [ -z ] [ -v verbosity ] [ -T timeout ] [ -l live ] [ -t tree ] [ -f ] up|down|unsupervise" ;

char const *help_all =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -T: timeout\n"
"   -l: live directory\n"
"   -t: tree to use\n"
"   -f: fork the process\n"
;

char const *usage_tree = "66 tree [ -h ] [ -z ] [ -v verbosity ] [ -c ] [ -o depends=:... ] [ -E|D ] [ -R ] tree" ;

char const *help_tree =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -c: set tree as default\n"
"   -o: colon separated list of options\n"
"   -E: enable the tree\n"
"   -D: disable the tree\n"
"   -R: remove the tree\n"

"\n"
"valid fields for -o options are:\n"
"\n"
"   depends=: comma separated list of dependencies for tree or none\n"
"   requiredby=: comma separated list of trees required by tree or none\n"
"   groups=: add tree to the specified groups\n"
"   allow=: comma separated list of account to allow at tree\n"
"   deny=: comma separated list of account to deny at tree\n"
"   clone=: make a clone of tree\n"
"   noseed: do not use seed file to build the tree\n"
;

char const *usage_tree = "66 treectl [ -h ] [ -z ] [ -v verbosity ] tree" ;

char const *help_treectl =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
;

char const *usage_reconfigure = "66 reconfigure [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] [ -P ] service(s)" ;

char const *help_reconfigure =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -t: tree to use\n"
"   -T: timeout\n"
"   -P: do not propagate signal to its dependencies\n"
;

char const *usage_reload = "66 reload [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] [ -P ] service(s)" ;

char const *help_reload =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -t: tree to use\n"
"   -T: timeout\n"
"   -P: do not propagate signal to its dependencies\n"
;

char const *usage_restart = "66 restart [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] [ -P ] service(s)" ;

char const *help_restart =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -t: tree to use\n"
"   -T: timeout\n"
"   -P: do not propagate signal to its dependencies\n"
;

char const *usage_unsupervise = "66 unsupervise [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -t tree ] [ -T timeout ] [ -P ] service(s)" ;

char const *help_unsupervise =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -t: tree to use\n"
"   -T: timeout\n"
"   -P: do not propagate signal to its dependencies\n"
;

char const *usage_inresolve = "66 inresolve [ -h ] [ -z ] [ -v verbosity ] [ -t tree ] tree|service name" ;

char const *help_inresolve =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -t: only search at the specified tree\n"
"   tree: search for tree name\n"
"   service: search for service name\n"
;

char const *usage_instate = "66 instate [ -h ] [ -z ] [ -v verbosity ] service" ;

char const *help_instate =
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
;

char const *usage_intree = "66 intree [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -n ] [ -o name,init,enabled,... ] [ -g ] [ -d depth ] [ -r ] tree" ;

char const *help_intree =
"\n"
"options :\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -n: do not display the names of fields\n"
"   -o: comma separated list of field to display\n"
"   -g: displays the contents field as graph\n"
"   -d: limit the depth of the contents field recursion\n"
"   -r: reverse the contents field\n"
"\n"
"valid fields for -o options are:\n"
"\n"
"   name: displays the name of the tree\n"
"   current: displays a boolean value of the current state\n"
"   enabled: displays a boolean value of the enable state\n"
"   init: displays a boolean value of the initialization state\n"
"   depends: displays the list of tree(s) started before\n"
"   requiredby: displays the list of tree(s) started after\n"
"   allowed: displays a list of allowed user to use the tree\n"
"   symlinks: displays the target of tree's symlinks\n"
"   contents: displays the contents of the tree\n"
;

char const *usage_inservice = "66 inservice [ -h ] [ -z ] [ -v verbosity ] [ -n ] [ -o name,intree,status,... ] [ -g ] [ -d depth ] [ -r ] [ -t tree ] [ -p nline ] service" ;

char const *help_inservice =
"\n"
"options :\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -n: do not display the field name\n"
"   -o: comma separated list of field to display\n"
"   -g: displays the contents field as graph\n"
"   -d: limit the depth of the contents field recursion\n"
"   -r: reverse the contents field\n"
"   -t: only search service at the specified tree\n"
"   -p: print n last lines of the log file\n"
"\n"
"valid fields for -o options are:\n"
"\n"
"   name: displays the name\n"
"   version: displays the version of the service\n"
"   intree: displays the service's tree name\n"
"   status: displays the status\n"
"   type: displays the service type\n"
"   description: displays the description\n"
"   source: displays the source of the service's frontend file\n"
"   live: displays the service's live directory\n"
"   depends: displays the service's dependencies\n"
"   requiredby: displays the service(s) which depends on service\n"
"   extdepends: displays the service's external dependencies\n"
"   optsdepends: displays the service's optional dependencies\n"
"   start: displays the service's start script\n"
"   stop: displays the service's stop script\n"
"   envat: displays the source of the environment file\n"
"   envfile: displays the contents of the environment file\n"
"   logname: displays the logger's name\n"
"   logdst: displays the logger's destination\n"
"   logfile: displays the contents of the log file\n"
;

char const *usage_boot = "66 boot [ -h ] [ -z ] [ -m ] [ -s skel ] [ -l log_user ] [ -e environment ] [ -d dev ] [ -b banner ]" ;

char const *help_boot =
"\n"
"options :\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -m: mount parent live directory\n"
"   -s: skeleton directory\n"
"   -l: run catch-all logger as log_user user\n"
"   -e: environment directory or file\n"
"   -d: dev directory\n"
"   -b: banner to display\n"
;

char const *usage_scanctl = "66 scanctl [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -d notif ] [ -t rescan ] [ -e environment ] [ -o owner ] start|stop|reload|quit|nuke|zombies or any s6-svscanctl options" ;

char const *help_scanctl =
"\n"
"options :\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -d: notify readiness on file descriptor\n"
"   -t: rescan scandir every milliseconds\n"
"   -e: environment directory\n"
"   -o: handles scandir of owner\n"
;

char const *usage_scandir = "66 scandir [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -b|B ] [ -c ] [ -L log_user ] [ -s skel ] [ -o owner ] create|remove" ;

char const *help_scandir =
"\n"
"options :\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -b: create scandir for a boot process\n"
"   -B: create scandir for a boot process inside a container\n"
"   -c: do not catch log\n"
"   -L: run catch-all logger as log_user user\n"
"   -s: skeleton directory\n"
"   -o: handles owner scandir\n"
;

char const *usage_66 = "66 start|stop|unsupervise|enable|disable|all|init|env|parse|svctl|tree|reconfigure|reload|restart|scanctl|scandir|boot service(s)|tree" ;
