/*
 * caps.h
 *
 * Copyright (c) 2025 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file./
 */

#ifndef SS_CAPS_H
#define SS_CAPS_H

#include <stdint.h>
#include <linux/capability.h> // CAP_LAST_CAP

#include <oblibs/stack.h>

#include <66/service.h>

#define CAPS_MYLAST_CAP CAP_LAST_CAP + 1
#define CAPSET_ARRAY_SIZE ((CAPS_MYLAST_CAP + UINT_NBITS - 1) / UINT_NBITS)

#ifndef SYS_capset
#define SYS_capset __NR_capset
#endif
#ifndef SYS_capget
#define SYS_capget __NR_capget
#endif

extern void parse_store_caps(stack *result, stack *store, uint32_t *ncaps) ;
extern void execute_caps(resolve_service_t *res) ;

/** Complete list of Linux capabilities (up to CAP_LAST_CAP = CAP_PERFMON, kernel 6.11)
 * #define CAP_CHOWN            0   // Allow changing file ownership
 * #define CAP_DAC_OVERRIDE     1   // Bypass file read/write/execute permission checks
 * #define CAP_DAC_READ_SEARCH  2   // Bypass file read and directory search permission checks
 * #define CAP_FOWNER           3   // Bypass permission checks on file ownership
 * #define CAP_FSETID           4   // Allow setting setuid/setgid bits
 * #define CAP_KILL             5   // Allow sending signals to processes
 * #define CAP_SETGID           6   // Allow changing group IDs
 * #define CAP_SETUID           7   // Allow changing user IDs
 * #define CAP_SETPCAP          8   // Allow transferring/setting capabilities
 * #define CAP_LINUX_IMMUTABLE  9   // Allow setting immutable and append-only file attributes
 * #define CAP_NET_BIND_SERVICE 10  // Allow binding to privileged ports (< 1024)
 * #define CAP_NET_BROADCAST    11  // Allow network broadcasting
 * #define CAP_NET_ADMIN        12  // Allow network administration tasks
 * #define CAP_NET_RAW          13  // Allow raw sockets and packet forging
 * #define CAP_IPC_LOCK         14  // Allow locking memory (mlock)
 * #define CAP_IPC_OWNER        15  // Bypass IPC ownership checks
 * #define CAP_SYS_MODULE       16  // Allow loading/unloading kernel modules
 * #define CAP_SYS_RAWIO        17  // Allow raw I/O operations (e.g., iopl, ioperm)
 * #define CAP_SYS_CHROOT       18  // Allow chroot()
 * #define CAP_SYS_PTRACE       19  // Allow ptrace() on other processes
 * #define CAP_SYS_PACCT        20  // Allow process accounting
 * #define CAP_SYS_ADMIN        21  // Allow various system administration tasks
 * #define CAP_SYS_BOOT         22  // Allow rebooting the system
 * #define CAP_SYS_NICE         23  // Allow setting process priorities (nice, sched)
 * #define CAP_SYS_RESOURCE     24  // Allow overriding resource limits
 * #define CAP_SYS_TIME         25  // Allow setting system clock
 * #define CAP_SYS_TTY_CONFIG   26  // Allow configuring TTY devices
 * #define CAP_MKNOD            27  // Allow creating special files (mknod)
 * #define CAP_LEASE            28  // Allow taking leases on files
 * #define CAP_AUDIT_WRITE      29  // Allow writing to audit log
 * #define CAP_AUDIT_CONTROL    30  // Allow configuring audit subsystem
 * #define CAP_SETFCAP          31  // Allow setting file capabilities
 * #define CAP_MAC_OVERRIDE     32  // Allow overriding Mandatory Access Control
 * #define CAP_MAC_ADMIN        33  // Allow configuring Mandatory Access Control
 * #define CAP_SYSLOG           34  // Allow accessing kernel syslog
 * #define CAP_WAKE_ALARM       35  // Allow triggering wake alarms
 * #define CAP_BLOCK_SUSPEND    36  // Allow preventing system suspend
 * #define CAP_AUDIT_READ       37  // Allow reading audit log
 * #define CAP_PERFMON          38  // Allow performance monitoring (introduced in kernel 5.8)
 * #define CAP_BPF              39  // Allow BPF operations (introduced in kernel 5.8)
 * #define CAP_CHECKPOINT_RESTORE 40 // Allow checkpoint/restore operations (introduced in kernel 5.9)
 * #define CAP_PIDFD_OPEN       41  // Allow pidfd_open() (introduced in kernel 5.6)
 * #define CAP_PIDFD_GETFD      42  // Allow pidfd_getfd() (introduced in kernel 5.6)
 * #define CAP_PIDFD_SPAWN      43  // Allow pidfd_spawn() (proposed, not yet standard)
 * #define CAP_SETPROCTITLE     44  // Allow setting process title (proposed, not yet standard)
 * #define CAP_MEMLOCK          45  // Allow locking memory (mlockall, proposed)
 * #define CAP_PERFMON          46  // Duplicate for CAP_PERFMON (kernel 5.8, listed for completeness)
 * #define CAP_PERFMON          38
 * #define CAP_BPF              39
 * #define CAP_CHECKPOINT_RESTORE 40


 * #define CAP_PIDFD_OPEN       41
 * #define CAP_PIDFD_GETFD      42
 * #define CAP_PIDFD_SPAWN      43
 * #define CAP_SETPROCTITLE     44
 * #define CAP_MEMLOCK          45
 */
#endif