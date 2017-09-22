#ifndef _PROC_FILE_SYS_H_
#define _PROC_FILE_SYS_H_

/*
 ***************************************************************************
 * System files containing system statistics
 ***************************************************************************
 */

#define PROC		"/proc"
#define SERIAL		"/proc/tty/driver/serial"
#define FDENTRY_STATE	"/proc/sys/fs/dentry-state"
#define FFILE_NR	"/proc/sys/fs/file-nr"
#define FINODE_STATE	"/proc/sys/fs/inode-state"
#define PTY_NR		"/proc/sys/kernel/pty/nr"
#define NET_DEV		"/proc/net/dev"
#define NET_SOCKSTAT	"/proc/net/sockstat"
#define NET_SOCKSTAT6	"/proc/net/sockstat6"
#define NET_RPC_NFS	"/proc/net/rpc/nfs"
#define NET_RPC_NFSD	"/proc/net/rpc/nfsd"
#define LOADAVG		"/proc/loadavg"
#define VMSTAT		"/proc/vmstat"
#define NET_SNMP	"/proc/net/snmp"
#define NET_SNMP6	"/proc/net/snmp6"
#define CPUINFO		"/proc/cpuinfo"
#define MTAB		"/etc/mtab"
#define IF_DUPLEX	"/sys/class/net/%s/duplex"
#define IF_SPEED	"/sys/class/net/%s/speed"
#define FC_RX_FRAMES	"%s/%s/statistics/rx_frames"
#define FC_TX_FRAMES	"%s/%s/statistics/tx_frames"
#define FC_RX_WORDS	"%s/%s/statistics/rx_words"
#define FC_TX_WORDS	"%s/%s/statistics/tx_words"

/*
 ***************************************************************************
 * System files containing process statistics
 ***************************************************************************
 */

#define PID_STAT	"/proc/%u/stat"
#define PID_STATUS	"/proc/%u/status"
#define PID_IO		"/proc/%u/io"
#define PID_CMDLINE	"/proc/%u/cmdline"
#define PID_SMAP	"/proc/%u/smaps"
#define PID_FD		"/proc/%u/fd"

#define PROC_TASK	"/proc/%u/task"
#define TASK_STAT	"/proc/%u/task/%u/stat"
#define TASK_STATUS	"/proc/%u/task/%u/status"
#define TASK_IO		"/proc/%u/task/%u/io"
#define TASK_CMDLINE	"/proc/%u/task/%u/cmdline"
#define TASK_SMAP	"/proc/%u/task/%u/smaps"
#define TASK_FD		"/proc/%u/task/%u/fd"

#endif
