/*
 * constants.h
 *
 * Copyright (c) 2018-2024 Eric Vidal <eric@obarun.org>
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
#define SS_MODULE "module"
#define SS_MODULE_LEN (sizeof SS_MODULE - 1)
#define SS_SCANDIR "scandir"
#define SS_SCANDIR_LEN (sizeof SS_SCANDIR - 1)
#define SS_NOTIFICATION "notification-fd"
#define SS_NOTIFICATION_LEN (sizeof SS_NOTIFICATION - 1)
#define SS_MAXDEATHTALLY "max-death-tally"
#define SS_MAXDEATHTALLY_LEN (sizeof SS_MAXDEATHTALLY - 1)

/**tree dir*/
#define SS_RULES "/rules"
#define SS_RULES_LEN (sizeof SS_RULES - 1)

/**service dir*/
#define SS_SVC "/svc"
#define SS_SVC_LEN (sizeof SS_SVC - 1)
#define SS_SERVICE "/service"
#define SS_SERVICE_LEN (sizeof SS_SERVICE - 1)
#define SS_MASTER "/Master"
#define SS_MASTER_LEN (sizeof SS_MASTER - 1)
#define SS_CONTENTS "contents"
#define SS_CONTENTS_LEN (sizeof SS_CONTENTS - 1)

/** logger */
#define SS_LOG "log"
#define SS_LOG_LEN (sizeof SS_LOG - 1)

#define SS_LOG_SUFFIX "-log"
#define SS_LOG_SUFFIX_LEN (sizeof SS_LOG_SUFFIX - 1)

#define SS_SVSCAN_LOG "/.s6-svscan"
#define SS_SVSCAN_LOG_LEN (sizeof SS_SVSCAN_LOG - 1)

/** environment and data */
#define SS_ENVDIR "/conf"
#define SS_ENVDIR_LEN (sizeof SS_ENVDIR - 1)
#define SS_DATADIR "/data"
#define SS_DATADIR_LEN (sizeof SS_DATADIR - 1)
#define SS_RUNUSER ".user"
#define SS_RUNUSER_LEN (sizeof SS_RUNUSER - 1)

#define SS_STATE "/state"
#define SS_STATE_LEN (sizeof SS_STATE - 1)
#define SS_STATUS "status"
#define SS_STATUS_LEN (sizeof SS_STATUS - 1)

#define SS_RESOLVE "/.resolve"
#define SS_RESOLVE_LEN (sizeof SS_RESOLVE - 1)

#define SS_FDHOLDER "fdholder"
#define SS_FDHOLDER_LEN (sizeof SS_FDHOLDER - 1)
#define SS_FDHOLDER_PIPENAME "pipe:66-"
#define SS_FDHOLDER_PIPENAME_LEN (sizeof SS_FDHOLDER_PIPENAME - 1)
#define SS_ONESHOTD "oneshotd"
#define SS_ONESHOTD_LEN (sizeof SS_ONESHOTD - 1)

#define SS_VAR_UNEXPORT '!'

#define SS_SYM_VERSION "/version"
#define SS_SYM_VERSION_LEN (sizeof SS_SYM_VERSION - 1)

#define SS_SERVICE_VERSION_NDOT 2
#define SS_SYSTEM_VERSION_NDOT 3

#define SS_EVENTDIR "/event"
#define SS_EVENTDIR_LEN (sizeof SS_EVENTDIR - 1)

#define SS_SUPERVISEDIR "/supervise"
#define SS_SUPERVISEDIR_LEN (sizeof SS_SUPERVISEDIR - 1)

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
#define SS_INSTANCE_REGEX "@I"
#define SS_INSTANCE_TEMPLATE 0
#define SS_INSTANCE_NAME 1

/** Snapshot */
#define SS_SNAPSHOT "/.snapshot"
#define SS_SNAPSHOT_LEN (sizeof SS_SNAPSHOT - 1)

#define SS_MAX_PATH_LEN (SS_MAX_PATH + SS_SYSTEM_LEN + SS_RESOLVE_LEN + SS_SERVICE_LEN + SS_MAX_TREENAME + SS_MAX_SERVICE_NAME + 63 + 1)

#endif
