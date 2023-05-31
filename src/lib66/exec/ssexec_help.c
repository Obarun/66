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


char const *usage_66 = "66 [ -h ] [ -z ] [ -v verbosity ] [ -l live ] [ -T timeout ] [ -t tree ] start|stop|reload|restart|unsupervise|reconfigure|enable|disable|environ|status|resolve|state|remove|signal|tree|init|parse|scandir|boot|version [<command options>] service...|tree" ;

char const *help_66 =
"\nmain tool to init a system, control and manage services\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -z: use color\n"
"   -v: increase/decrease verbosity\n"
"   -l: live directory\n"
"   -T: timeout\n"
"   -t: name of the tree to use\n"
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
"   environ: manage service environment variable\n"
"   status: display services informations\n"
"   resolve: display the resolve files contents of services\n"
"   state: display state files contents of services\n"
"   remove: remove services and cleanup all files belong to it from the system\n"
"   signal: send signal to service\n"
"   tree: manage/see tree information\n"
"   init: initiate to scandir all services marked enabled at tree\n"
"   parse: parse the service frontend file\n"
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

char const *usage_enable = "66 enable [ -h ] [ -f ] [ -I ] [ -S ] service..." ;

char const *help_enable =
"\nactivate services at the next boot\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -f: force to overwrite the service(s)\n"
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

char const *usage_env = "66 environ [ -h ] [ -c version ] [ -s version ] [ -V|L ] [ -r key=value ] [ -i src,dst ] [ -e editor ] service" ;

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

char const *usage_status = "66 status [ -h ] [ -n ] [ -o name,intree,status,... ] [ -g ] [ -d depth ] [ -r ] [ -p nline ] service..." ;

char const *help_status =
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

char const *usage_resolve = "66 resolve [ -h ] service" ;

char const *help_resolve =
"\ndisplay the resolve files contents of services\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_state = "66 state [ -h ] service" ;

char const *help_state =
"\ndisplay state files contents of services\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_remove = "66 remove [ -h ] [ -P ] service" ;

char const *help_remove =
"\nremove services and cleanup all files belong to it from the system\n"
"\n"
"options :\n"
"   -h: print this help\n"
"   -P: do not propagate signal to its dependencies at stop process\n"
;

char const *usage_signal = "66 signal [ -h ] [ -wu | -wU | -wd | -wD | -wr | -wR ] [ -abqHkti12pcyodDuUxOr ] service..." ;

char const *help_signal =
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

char const *usage_tree_wrapper = "66 tree [ -h ] create|admin|remove|enable|disable|current|status|resolve|start|stop|unsupervise [<command options>] tree" ;

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
"   start: bring up all services from tree\n"
"   stop: bring down all services from tree\n"
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

char const *usage_tree_start = "66 tree start [ -h ] [ -f ] tree" ;

char const *help_tree_start =
"\nbring up all enabled services of tree\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -f: fork the process\n"
"\n"
"If no tree name are provided, it bring up all enabled trees from the system\n"
;

char const *usage_tree_stop = "66 tree stop [ -h ] [ -f ] tree" ;

char const *help_tree_stop =
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

char const *usage_scandir_wrapper = "66 scandir [ -h ] [ -o owner ] create|remove|start|stop|reconfigure|rescan|quit|halt|abort|nuke|annihilate|zombies [<command options>]" ;

char const *help_scandir_wrapper =
"\nmain sub tools to manage scandir\n"
"\n"
"options :\n"
"   -h: print this help\n"
"   -o: handles scandir of owner\n"
"\n"
"command:\n"
"   create: create a scandir\n"
"   remove: remove a scandir\n"
"   start: start a scandir\n"
"   stop: stop a running scandir\n"
"   reconfigure: reconfigure a running scandir\n"
"   rescan: rescan a running scandir\n"
"   quit: quit a running scandir\n"
"   halt: halt a running scandir\n"
"   abort: abort a running scandir\n"
"   nuke: nuke a running scandir\n"
"   annihilate: annihilate a running scandir\n"
"   zombies: destroy zombies from a running scandir\n"
"\n"
"Use '66 scandir <command> -h' to see command options\n"
;

char const *usage_scandir_create = "66 scandir create [ -h ] [ -b|B ] [ -c ] [ -L log_user ] [ -s skel ]" ;

char const *help_scandir_create =
"\ncreate a scandir\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -b: create scandir for a boot process\n"
"   -B: create scandir for a boot process inside a container\n"
"   -c: do not catch logs\n"
"   -L: run catch-all logger as log_user user\n"
"   -s: use skel as skeleton directory\n"
;

char const *usage_scandir_remove = "66 scandir remove [ -h ]" ;

char const *help_scandir_remove =
"\nremove a existing scandir\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_scandir_start = "66 scandir start [ -h ] [ -d notif ] [ -s rescan ] [ -e environment ] [ -b|B ]" ;

char const *help_scandir_start =
"\nstart a scandir\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -d: notify readiness on file descriptor\n"
"   -s: scan scandir every milliseconds\n"
"   -e: use environment as environment directory\n"
"   -b: create scandir (if it not exist yet) for a boot process\n"
"   -B: create scandir (if it not exist yet) for a boot process inside a container\n"
;

char const *usage_scandir_stop = "66 scandir stop [ -h ]" ;

char const *help_scandir_stop =
"\nstop a running scandir\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_scandir_reconfigure = "66 scandir reconfigure [ -h ]" ;

char const *help_scandir_reconfigure =
"\nperform a scan and destroy inactive service of scandir\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_scandir_rescan = "66 scandir rescan [ -h ]" ;

char const *help_scandir_rescan =
"\nrescan a running scandir to integrate new services\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -o: handles scandir of owner\n"
;

char const *usage_scandir_quit = "66 scandir quit [ -h ]" ;

char const *help_scandir_quit =
"\nquit a running scandir\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_scandir_halt = "66 scandir halt [ -h ]" ;

char const *help_scandir_halt =
"\nhalt a running scandir\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_scandir_abort = "66 scandir abort [ -h ]" ;

char const *help_scandir_abort =
"\nabort a running scandir\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_scandir_nuke = "66 scandir nuke [ -h ]" ;

char const *help_scandir_nuke =
"\nnuke a running scandir\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_scandir_annihilate = "66 scandir annihilate [ -h ]" ;

char const *help_scandir_annihilate =
"\nannihilate a running scandir\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_scandir_zombies = "66 scandir zombies [ -h ]" ;

char const *help_scandir_zombies =
"\ndestroy zombies from a running scandir\n"
"\n"
"options:\n"
"   -h: print this help\n"
;

char const *usage_poweroff = "66 poweroff [ -h ] [ -a ] [ -f|F ] [ -m message ] [ -t time ] [ -W ] when" ;

char const *help_poweroff =
"\npoweroff the system\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -a: use access control\n"
"   -f: sync filesytem and immediately poweroff the system\n"
"   -F: do not sync filesytem and immediately poweroff the system\n"
"   -m: replace the default message by message\n"
"   -t: grace time period between SIGTERM and SIGKILL\n"
"   -W: do not send a wall message to users\n"
;

char const *usage_reboot = "66 reboot [ -h ] [ -a ] [ -f|F ] [ -m message ] [ -t time ] [ -W ] when" ;

char const *help_reboot =
"\nreboot the system\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -a: use access control\n"
"   -f: sync filesytem and immediately reboot the system\n"
"   -F: do not sync filesytem and immediately reboot the system\n"
"   -m: replace the default message by message\n"
"   -t: grace time period between SIGTERM and SIGKILL\n"
"   -W: do not send a wall message to users\n"
;

char const *usage_halt = "66 halt [ -h ] [ -a ] [ -f|F ] [ -m message ] [ -t time ] [ -W ] when" ;

char const *help_halt =
"\nhalt the system\n"
"\n"
"options:\n"
"   -h: print this help\n"
"   -a: use access control\n"
"   -f: sync filesytem and immediately halt the system\n"
"   -F: do not sync filesytem and immediately halt the system\n"
"   -m: replace the default message by message\n"
"   -t: grace time period between SIGTERM and SIGKILL\n"
"   -W: do not send a wall message to users\n"
;
