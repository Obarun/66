/* 
 * constants.h
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
 
#ifndef CONSTANTS_H
#define CONSTANTS_H


#include <66/config.h>

/**main dir*/
#define SS_SYSTEM "system"
#define SS_SYSTEM_LEN (sizeof SS_SYSTEM - 1)
#define SS_TREE_CURRENT "current"
#define SS_TREE_CURRENT_LEN (sizeof SS_TREE_CURRENT - 1)
#define SS_SERVICE "service"
#define SS_SERVICE_LEN (sizeof SS_SERVICE - 1)

/**tree dir*/
#define SS_RULES "/rules"
#define SS_RULES_LEN (sizeof SS_RULES - 1)
#define SS_SVDIRS "/servicedirs"
#define SS_SVDIRS_LEN (sizeof SS_SVDIRS - 1)
#define SS_BACKUP "/backup"
#define SS_BACKUP_LEN (sizeof SS_BACKUP - 1)

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

#define SS_SERVICE_DIR SS_SERVICEDIR SS_SERVICE
#define SS_SERVICE_DIR_LEN ((sizeof SS_SERVICEDIR - 1) + SS_SERVICE_LEN)

/** logger */
#define SS_LOG_RCSUFFIX "-log"
#define SS_LOG_RCSUFFIX_LEN (sizeof SS_LOG_RCSUFFIX - 1)
#define SS_LOG_SVSUFFIX "/log"
#define SS_LOG_SVSUFFIX_LEN (sizeof SS_LOG_SVSUFFIX - 1)
/** pipe */
#define SS_PIPE_NAME "bundle-"
#define SS_PIPE_NAME_LEN (sizeof SS_PIPE_NAME - 1)

/** backup and resolve */
#define SS_RESOLVE "/.resolve"
#define SS_RESOLVE_LEN (sizeof SS_RESOLVE - 1)
#define SS_RESOLVE_LIVE 0
#define SS_RESOLVE_SRC 1
#define SS_RESOLVE_BACK 2

#define SS_SYM_DB "bdb"
#define SS_SYM_DB_LEN (sizeof SS_SYM_DB - 1)
#define SS_SYM_SVC "bsvc"
#define SS_SYM_SVC_LEN (sizeof SS_SYM_SVC - 1)

#define SS_SWSRC 0 //switch to source
#define SS_SWBACK 1 //switch to backup

/** environment and data */
#define SS_ENVDIR "/env"
#define SS_ENVDIR_LEN (sizeof SS_ENVDIR - 1)
#define SS_DATADIR "/data"
#define SS_DATADIR_LEN (sizeof SS_DATADIR - 1)
#define SS_RUNUSER ".user"
#define SS_RUNUSER_LEN (sizeof SS_RUNUSER - 1)

#define SS_STATE "/state"
#define SS_STATE_LEN (sizeof SS_STATE - 1)
#endif
