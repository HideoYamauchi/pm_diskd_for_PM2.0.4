/* $Id: xmlvalues.h,v 1.6 2004/03/16 10:46:30 andrew Exp $ */
/* 
 * Copyright (C) 2004 Andrew Beekhof <andrew@beekhof.net>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#define CIB_VAL_CLEARON_NEVER       "never"
#define CIB_VAL_CLEARON_STONITH     "stonith"
#define CIB_VAL_CLEARON_ACTIVE      "active"
#define CIB_VAL_CLEARON_RESTART     "local_restart"
#define CIB_VAL_CLEARON_DEFAULT     CIB_VAL_CLEARON_NEVER

#define CIB_VAL_RESSTATUS_STOPED     "stopped"
#define CIB_VAL_RESSTATUS_STARTING   "starting"
#define CIB_VAL_RESSTATUS_RUNNING    "running"
#define CIB_VAL_RESSTATUS_STOPPING   "stopping"
#define CIB_VAL_RESSTATUS_FAILED     "failed"
#define CIB_VAL_RESSTATUS_DEFAULT    CIB_VAL_RESSTATUS_STOPED 

#define CIB_VAL_RESTYPE_NONE        "none"
#define CIB_VAL_RESTYPE_IP          "IPAddr"
#define CIB_VAL_RESTYPE_NFS         "Nfs"
#define CIB_VAL_RESTYPE_APACHE      "Apache"
#define CIB_VAL_RESTYPE_DRBD        "Drbd"
#define CIB_VAL_RESTYPE_STONITH     "Stonith"
#define CIB_VAL_RESTYPE_DEFAULT     CIB_VAL_RESTYPE_NONE 

#define CIB_VAL_NODETYPE_PING       "ping"
#define CIB_VAL_NODETYPE_NODE       "node"
#define CIB_VAL_NODETYPE_DEFAULT    CIB_VAL_NODETYPE_PING

#define CIB_VAL_HEALTH_0             "0"
#define CIB_VAL_HEALTH_10            "10"
#define CIB_VAL_HEALTH_20            "20"
#define CIB_VAL_HEALTH_30            "30"
#define CIB_VAL_HEALTH_40            "40"
#define CIB_VAL_HEALTH_50            "50"
#define CIB_VAL_HEALTH_60            "60"
#define CIB_VAL_HEALTH_70            "70"
#define CIB_VAL_HEALTH_80            "80"
#define CIB_VAL_HEALTH_90            "90"
#define CIB_VAL_HEALTH_100           "100"
#define CIB_VAL_HEALTH_DEFAULT       CIB_VAL_HEALTH_0

#define CIB_VAL_NODESTATUS_DOWN     "down"
#define CIB_VAL_NODESTATUS_UP       "up"
#define CIB_VAL_NODESTATUS_ACTOVE   "active"
#define CIB_VAL_NODESTATUS_STONITH  "stonith"
#define CIB_VAL_NODESTATUS_FAILED   "stonith_failed"
#define CIB_VAL_NODESTATUS_DEFAULT  CIB_VAL_NODESTATUS_DOWN

#define CIB_VAL_CONTYPE_NONE     "none"
#define CIB_VAL_CONTYPE_AFTER    "after"
#define CIB_VAL_CONTYPE_SAME     "same"
#define CIB_VAL_CONTYPE_BLOCK    "block"
#define CIB_VAL_CONTYPE_VAR      "sysvar"
#define CIB_VAL_CONTYPE_DEFAULT  CIB_VAL_CONTYPE_NONE

#define CIB_VAL_SOURCE_DEFAULT  "unknown"

#define CRM_SYSTEM_DC       "dc"
#define CRM_SYSTEM_DCIB     "dcib" // The master CIB
#define CRM_SYSTEM_CIB      "cib"
#define CRM_SYSTEM_CRMD     "crmd"
#define CRM_SYSTEM_LRMD     "lrmd"
#define CRM_SYSTEM_PENGINE  "pengine"
#define CRM_SYSTEM_TENGINE  "tengine"

#define CRM_OPERATION_BUMP	"bump"
#define CRM_OPERATION_CREATE	"create"
#define CRM_OPERATION_UPDATE	"update"
#define CRM_OPERATION_DELETE	"delete"
#define CRM_OPERATION_ERASE	"erase"
#define CRM_OPERATION_STORE	"store"
#define CRM_OPERATION_FORWARD	"forward"
#define CRM_OPERATION_JOINACK	"join_ack"
#define CRM_OPERATION_WELCOME	"welcome"
#define CRM_OPERATION_PING	"ping"
#define CRM_OPERATION_VOTE	"vote"
#define CRM_OPERATION_ANNOUNCE	"announce"
#define CRM_OPERATION_HBEAT	"dc_beat"
