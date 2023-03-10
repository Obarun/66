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

#include <oblibs/log.h>
#include <66/ssexec.h>

inline void info_help (char const *help,char const *usage)
{
    DEFAULT_MSG = 0 ;

    log_info(usage,"\n", help) ;
}


char const *usage_66 = "66 [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -T timeout ] start|stop|reload|restart|unsupervise|reconfigure|enable|disable|env|service|tree|init|parse|signal|scanctl|scandir|boot|version [<command options>] service...|tree" ;

char const *help_66 =
"\nmain tool to init a system, control and manage services\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -T: timeout\n"
"\n"
"command:\n"
"   start: bring up service\n"
"   stop: bring down service\n"
"   reload: send a SIGHUP signal to service\n"
"   restart: bring down service then bring it up\n"
"   unsupervise: bring down service and remove it from scandir\n"
"   reconfigure: bring down, unsupervise, parse it again and bring up service\n"
"   enable: activate service for the next boot\n"
"   disable: deactivate service for the next boot\n"
"   env: manage service environment variable\n"
"   service: manage/see service information\n"
"   tree: manage/see tree information\n"
"   init: initiate to scandir all services marked enabled at tree\n"
"   parse: parse the service frontend file\n"
"   signal: send signal to service\n"
"   scanctl: send signal to scandir\n"
"   scandir: manage scandir\n"
"   boot: boot the system with 66\n"
"   version: display 66 version\n"
"\n"
"Use '66 <command> -h' to see command options\n"
;

char const *usage_boot = "66 boot [ -h ] [ -m ] [ -s skel ] [ -l log_user ] [ -e environment ] [ -d dev ] [ -b banner ]" ;

char const *help_boot =
"\nboot a system with 66. Generally used at /bin/init script\n"
"\n"
"options :\n"
"   -h: print this help\n"
"   -m: mount parent live directory\n"
"   -s: skeleton directory\n"
"   -l: run catch-all logger as log_user user\n"
"   -e: environment directory or file\n"
"   -d: dev directory\n"
"   -b: banner to display\n"
;

char const *usage_enable = "66 enable [ -h ] [ -f|F ] [ -I ] [ -S ] service..." ;

char const *help_enable =
"\nactivate services at the next boot\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -f: force to overwrite the service(s)\n"
"   -F: force to overwrite the service(s) and its dependencies\n"
"   -I: do not import modified configuration files from previous version\n"
"   -S: enable and start the service\n"
;

char const *usage_disable = "66 disable [ -h ] [ -S ] service..." ;

char const *help_disable =
"\ndeactivate services at the next boot\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -S: disable and stop/unsupervice service if needed\n"
;

char const *usage_start = "66 start [ -h ] [ -P ] service..." ;

char const *help_start =
"\nbring up services\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -P: do not propagate signal to its dependencies\n"
;

char const *usage_stop = "66 stop [ -h ] [ -P ] service..." ;

char const *help_stop =
"\nbring down services\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -P: do not propagate signal to its requiredby\n"
;

char const *usage_env = "66 env [ -h ] [ -c version ] [ -s version ] [ -V|L ] [ -r key=value ] [ -i src,dst ] [ -e editor ] service" ;

char const *help_env =
"\nmanage environment service files and its contents\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -c: set version as default\n"
"   -s: specifies the version to handle\n"
"   -V: lists available versioned configuration directories of the service\n"
"   -L: lists the environment variables of the service\n"
"   -r: replace the value of the key\n"
"   -i: import configuration files from src version to dst version\n"
"   -e: edit the file with editor\n"
;

char const *usage_init = "66 init [ -h ] tree" ;

char const *help_init =
"\ninitiate all services present in tree to a scandir already running\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_parse = "66 parse [ -h ] [ -f ] [ -I ] service..." ;

char const *help_parse =
"\nparse a frontend service file and install its result to resolve files\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -f: force to overwrite existing destination\n"
"   -I: do not import modified configuration files from previous version\n"
;

char const *usage_reconfigure = "66 reconfigure [ -h ] [ -P ] service..." ;

char const *help_reconfigure =
"\nconvenient tool to bring down, unsupervise, parse again and bring up services in one pass\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -P: do not propagate signal to its dependencies\n"
;

char const *usage_reload = "66 reload [ -h ] [ -P ] service..." ;

char const *help_reload =
"\nconvenient tool to send a SIGHUP signal to services\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -P: do not propagate signal to its dependencies\n"
;

char const *usage_restart = "66 restart [ -h ] [ -P ] service..." ;

char const *help_restart =
"\nconvenient tool to bring down then bring up services in one pass\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -P: do not propagate signal to its dependencies\n"
;

char const *usage_unsupervise = "66 unsupervise [ -h ] [ -P ] service..." ;

char const *help_unsupervise =
"\nbring down services and remove it from the scandir\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -P: do not propagate signal to its dependencies\n"
;

char const *usage_service_signal = "66 signal [ -h ] [ -wu | -wU | -wd | -wD | -wr | -wR ] [ -abqHkti12pcyodDuUxOr ] service..." ;

char const *help_service_signal =
"\nsend a signal to services\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -wu: do not exit until the service is up\n"
"   -wU: do not exit until the service is up and ready and has notified readiness\n"
"   -wd: do not exit until the service is down\n"
"   -wD: do not exit until the service is down and ready to be brought up and has notified readiness\n"
"   -wr: do not exit until the service has been started or restarted\n"
"   -wR: do not exit until the service has been started or restarted and has notified readiness\n"
"   -a: send a SIGALRM signal\n"
"   -b: send a SIGABRT signal\n"
"   -q: send a SIGQUIT signal\n"
"   -H: send a SIGHUP signal\n"
"   -k: send a SIGKILL signal\n"
"   -t: send a SIGTERM signal\n"
"   -i: send a SIGINT signal\n"
"   -1: send a SIGUSR1 signal\n"
"   -2: send a SIGUSR2 signal\n"
"   -p: send a SIGSTOP signal\n"
"   -c: send a SIGCONT signal\n"
"   -y: send a SIGWINCH signal\n"
"   -o: once. Equivalent to '-uO'\n"
"   -d: send a SIGTERM signal then a SIGCONT signal\n"
"   -D: bring down service and avoid to be bring it up automatically\n"
"   -u: bring up service\n"
"   -U: bring up service and ensure that service can be restarted automatically\n"
"   -x: bring down the service and propagate to its supervisor\n"
"   -O : mark the service to run once at most\n"
"   -r : restart service by sending it a signal(default SIGTERM)\n"
;

char const *usage_tree_wrapper = "66 tree [ -h ] create|admin|remove|enable|disable|current|status|resolve|up|down|unsupervise [<command options>] tree" ;

char const *help_tree_wrapper =
"\nmain sub tools to manages trees\n"
"\n"
"options:\n"
"   -h: print this help\n"
"\n"
"command:\n"
"   create: create tree\n"
"   admin: manage tree\n"
"   remove: remove tree\n"
"   enable: activate tree for the next boot\n"
"   disable: deactivate tree for the next boot\n"
"   current: mark tree as current\n"
"   status: display information about tree\n"
"   resolve: display resolve file of tree\n"
"   up: bring up all services from tree\n"
"   down: bring down all services from tree\n"
"   unsupervise: bring down and unsupervise all services from tree\n"
"\n"
"Use '66 tree <command> -h' to see command options\n"
;

char const *usage_tree_create = "66 tree create [ -h ] [ -o depends=:requiredby=:... ] tree" ;

char const *help_tree_create =
"\ncreate a tree\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -o: colon separated list of options\n"
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

char const *usage_tree_admin = "66 tree admin [ -h ] [ -o depends=:requiredby=:... ] tree" ;

char const *help_tree_admin =
"\nadministrate an existing tree\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -o: colon separated list of options\n"
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

char const *usage_tree_remove = "66 tree remove [ -h ] tree" ;

char const *help_tree_remove =
"\nremove a tree\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_tree_enable = "66 tree enable [ -h ] tree" ;

char const *help_tree_enable =
"\nactivate tree at next boot\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_tree_disable = "66 tree disable [ -h ] tree" ;

char const *help_tree_disable =
"\ndeactivate tree at next boot\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_tree_current = "66 tree current [ -h ] tree" ;

char const *help_tree_current =
"\nmark tree as current\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_tree_resolve = "66 tree resolve [ -h ] tree" ;

char const *help_tree_resolve =
"\ndisplay the resolve files contents of tree\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_tree_status = "66 tree status [ -h ] [ -n ] [ -o name,init,enabled,... ] [ -g ] [ -d depth ] [ -r ] tree" ;

char const *help_tree_status =
"\ndisplay information about tree\n"
"\n"
"options :\n"
"   -h: print this help\n"
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
"   contents: displays the contents of the tree\n"
"\n"
"If no tree name are provided, it display all trees from the system\n"
;

char const *usage_tree_up = "66 tree up [ -h ] [ -f ] tree" ;

char const *help_tree_up =
"\nbring up all enabled services of tree\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -f: fork the process\n"
"\n"
"If no tree name are provided, it bring up all enabled trees from the system\n"
;

char const *usage_tree_down = "66 tree down [ -h ] [ -f ] tree" ;

char const *help_tree_down =
"\nbring down all services of tree\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -f: fork the process\n"
"\n"
"If no tree name are provided, it bring down all enabled trees from the system\n"
;

char const *usage_tree_unsupervise = "66 tree unsupervise [ -h ] [ -f ] tree" ;

char const *help_tree_unsupervise =
"\nbring down and unsupervise all services of tree\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -f: fork the process\n"
"\n"
"If no tree name are provided, it unsupervise all enabled trees from the system\n"
;

char const *usage_service_wrapper = "66 service [ -h ] status|resolve|state|remove [<command options>] service..." ;

char const *help_service_wrapper =
"\nmain sub tools to view and manages services\n"
"\n"
"options :\n"
"   -h: print this help\n"
"\n"
"command:\n"
"   status: displays information of the service\n"
"   resolve: displays the resolve file of the service\n"
"   state: displays the state file of the service\n"
"   remove: remove and cleanup the service\n"
"\n"
"Use '66 service <command> -h' to see command options\n"
;

char const *usage_service_status = "66 service status [ -h ] [ -n ] [ -o name,intree,status,... ] [ -g ] [ -d depth ] [ -r ] [ -p nline ] service..." ;

char const *help_service_status =
"\ndisplay services informations\n"
"\n"
"options :\n"
"   -h: print this help\n"
"   -n: do not display the field name\n"
"   -o: comma separated list of options\n"
"   -g: displays the contents field as graph\n"
"   -d: limit the depth of the contents field recursion\n"
"   -r: reverse the contents field\n"
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
"   optsdepends: displays the service's optional dependencies\n"
"   start: displays the service's start script\n"
"   stop: displays the service's stop script\n"
"   envat: displays the source of the environment file\n"
"   envfile: displays the contents of the environment file\n"
"   logname: displays the logger's name\n"
"   logdst: displays the logger's destination\n"
"   logfile: displays the contents of the log file\n"
;

char const *usage_service_resolve = "66 service resolve [ -h ] service" ;

char const *help_service_resolve =
"\ndisplay the resolve files contents of services\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_service_state = "66 service state [ -h ] service" ;

char const *help_service_state =
"\ndisplay state files contents of services\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_service_remove = "66 service remove [ -h ] service" ;

char const *help_service_remove =
"\nremove services and cleanup all files belong to it from the system\n"
"\n"
"options :\n"
"   -h: print this help\n"
;

char const *usage_scanctl = "66 scanctl [ -d notif ] [ -t rescan ] [ -e environment ] [ -o owner ] start|stop|reload|quit|nuke|zombies or any s6-svscanctl options" ;

char const *help_scanctl =
"\nsend signal to scandir\n"
"\n"
"options :\n"
"   -h: print this help\n"
"   -d: notify readiness on file descriptor\n"
"   -t: rescan scandir every milliseconds\n"
"   -e: environment directory\n"
"   -o: handles scandir of owner\n"
;

char const *usage_scandir = "66 scandir [ -h ] [ -b|B ] [ -c ] [ -L log_user ] [ -s skel ] [ -o owner ] create|remove" ;

char const *help_scandir =
"\nmanage a scandir\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -b: create scandir for a boot process\n"
"   -B: create scandir for a boot process inside a container\n"
"   -c: do not catch log\n"
"   -L: run catch-all logger as log_user user\n"
"   -s: skeleton directory\n"
"   -o: handles owner scandir\n"
;
