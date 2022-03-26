/*
 * constants.h
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

#ifndef SS_CONSTANTS_H
#define SS_CONSTANTS_H


#include <66/config.h>

/**main dir*/
#define SS_SYSTEM "system"
#define SS_SYSTEM_LEN (sizeof SS_SYSTEM - 1)
#define SS_TREE_CURRENT "current"
#define SS_TREE_CURRENT_LEN (sizeof SS_TREE_CURRENT - 1)
#define SS_SERVICE "service"
#define SS_SERVICE_LEN (sizeof SS_SERVICE - 1)
#define SS_MODULE "module"
#define SS_MODULE_LEN (sizeof SS_MODULE - 1)
#define SS_SCANDIR "scandir"
#define SS_SCANDIR_LEN (sizeof SS_SCANDIR - 1)
#define SS_TREE "tree"
#define SS_TREE_LEN (sizeof SS_TREE - 1)
#define SS_NOTIFICATION "notification-fd"
#define SS_NOTIFICATION_LEN (sizeof SS_NOTIFICATION - 1)
#define SS_MAXDEATHTALLY "max-death-tally"
#define SS_MAXDEATHTALLY_LEN (sizeof SS_MAXDEATHTALLY - 1)

/**tree dir*/
#define SS_RULES "/rules"
#define SS_RULES_LEN (sizeof SS_RULES - 1)
#define SS_SVDIRS "/servicedirs"
#define SS_SVDIRS_LEN (sizeof SS_SVDIRS - 1)
#define SS_BACKUP "/backup"
#define SS_BACKUP_LEN (sizeof SS_BACKUP - 1)
#define SS_LIVETREE_INIT "init"
#define SS_LIVETREE_INIT_LEN (sizeof SS_LIVETREE_INIT - 1)

/**service dir*/
#define SS_SVC "/svc"
#define SS_SVC_LEN (sizeof SS_SVC - 1)
#define SS_DB "/db"
#define SS_DB_LEN (sizeof SS_DB - 1)
#define SS_SRC "/source"
#define SS_SRC_LEN (sizeof SS_SRC - 1)
#define SS_MASTER "/Master"
#define SS_MASTER_LEN (sizeof SS_MASTER - 1)
#define SS_CONTENTS "contents"
#define SS_CONTENTS_LEN (sizeof SS_CONTENTS - 1)

/** logger */
#define SS_LOG "log"
#define SS_LOG_LEN (sizeof SS_LOG - 1)
#define SS_LOG_SUFFIX "-" SS_LOG
#define SS_LOG_SUFFIX_LEN (sizeof SS_LOG_SUFFIX - 1)
#define SS_SVSCAN_LOG "/.s6-svscan"
#define SS_SVSCAN_LOG_LEN (sizeof SS_SVSCAN_LOG - 1)

/** pipe */
#define SS_PIPE_NAME "bundle-"
#define SS_PIPE_NAME_LEN (sizeof SS_PIPE_NAME - 1)

/** backup */
#define SS_SYM_DB "bdb"
#define SS_SYM_DB_LEN (sizeof SS_SYM_DB - 1)
#define SS_SYM_SVC "bsvc"
#define SS_SYM_SVC_LEN (sizeof SS_SYM_SVC - 1)

#define SS_SWSRC 0 //switch to source
#define SS_SWBACK 1 //switch to backup

/** environment and data */
#define SS_ENVDIR "/conf"
#define SS_ENVDIR_LEN (sizeof SS_ENVDIR - 1)
#define SS_DATADIR "/data"
#define SS_DATADIR_LEN (sizeof SS_DATADIR - 1)
#define SS_RUNUSER ".user"
#define SS_RUNUSER_LEN (sizeof SS_RUNUSER - 1)

#define SS_STATE "/state"
#define SS_STATE_LEN (sizeof SS_STATE - 1)

#define SS_VAR_UNEXPORT '!'

#define SS_SYM_VERSION "/version"
#define SS_SYM_VERSION_LEN (sizeof SS_SYM_VERSION - 1)

#define SS_CONFIG_VERSION_NDOT 2

#define SS_EVENTDIR "/event"
#define SS_EVENTDIR_LEN (sizeof SS_EVENTDIR - 1)
/** boot */
#define SS_BOOT_CONF "init.conf"
#define SS_BOOT_CONF_LEN (sizeof SS_BOOT_CONF - 1)
#define SS_BOOT_PATH "/usr/bin:/usr/sbin:/bin:/sbin:/usr/local/bin"
#define SS_BOOT_PATH_LEN (sizeof SS_BOOT_PATH - 1)
#define SS_BOOT_TREE "boot"
#define SS_BOOT_TREE_LEN (sizeof SS_BOOT_TREE - 1)
#define SS_BOOT_RCINIT "rc.init"
#define SS_BOOT_RCINIT_LEN (sizeof SS_BOOT_RCINIT - 1)
#define SS_BOOT_RCSHUTDOWN "rc.shutdown"
#define SS_BOOT_RCSHUTDOWN_LEN (sizeof SS_BOOT_RCSHUTDOWN - 1)
#define SS_BOOT_RCSHUTDOWNFINAL "rc.shutdown.final"
#define SS_BOOT_RCSHUTDOWNFINAL_LEN (sizeof SS_BOOT_RCSHUTDOWNFINAL -1)
#define SS_BOOT_UMASK 0022
#define SS_BOOT_RESCAN 0
#define SS_BOOT_CATCH_LOG 1
#define SS_BOOT_LOG "scandir/0/scandir-log"
#define SS_BOOT_LOG_LEN (sizeof SS_BOOT_LOG - 1)
#define SS_BOOT_LOGFIFO "scandir/0/scandir-log/fifo"
#define SS_BOOT_LOGFIFO_LEN (sizeof SS_BOOT_LOGFIFO - 1)
#define SS_BOOT_SHUTDOWND "66-shutdownd"
#define SS_BOOT_SHUTDOWND_LEN (sizeof SS_BOOT_SHUTDOWND - 1)
/** container */
#define SS_BOOT_CONTAINER 0
#define SS_BOOT_RCINIT_CONTAINER "rc.init.container"
#define SS_BOOT_RCINIT_CONTAINER_LEN (sizeof SS_BOOT_RCINIT_CONTAINER - 1)
#define SS_BOOT_CONTAINER_DIR "scandir/container"
#define SS_BOOT_CONTAINER_DIR_LEN (sizeof SS_BOOT_CONTAINER_DIR - 1)
#define SS_BOOT_CONTAINER_HALTCMD "halt"
#define SS_BOOT_CONTAINER_HALTCMD_LEN (sizeof SS_BOOT_CONTAINER_HALTCMD - 1)

/** Instance */
#define SS_INSTANCE_DIR "/instance"
#define SS_INSTANCE_DIR_LEN (sizeof SS_INSTANCE_DIR - 1)
#define SS_INSTANCE_REGEX "@I"
#define SS_INSTANCE_TEMPLATE 0
#define SS_INSTANCE_NAME 1

#endif
