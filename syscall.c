//#define SYS_DEBUG
#define ALPHA_LIB
#define SLASH_FIX
#define CATCH_TTY

//Show warnings about poorly implemented syscalls. Disabled by default since users are easy to scare
//#define WARN_ALL

/* syscall.c - proxy system call handler routines */

/* SimpleScalar(TM) Tool Suite
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 * All Rights Reserved. 
 * 
 * THIS IS A LEGAL DOCUMENT, BY USING SIMPLESCALAR,
 * YOU ARE AGREEING TO THESE TERMS AND CONDITIONS.
 * 
 * No portion of this work may be used by any commercial entity, or for any
 * commercial purpose, without the prior, written permission of SimpleScalar,
 * LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted
 * as described below.
 * 
 * 1. SimpleScalar is provided AS IS, with no warranty of any kind, express
 * or implied. The user of the program accepts full responsibility for the
 * application of the program and the use of any results.
 * 
 * 2. Nonprofit and noncommercial use is encouraged. SimpleScalar may be
 * downloaded, compiled, executed, copied, and modified solely for nonprofit,
 * educational, noncommercial research, and noncommercial scholarship
 * purposes provided that this notice in its entirety accompanies all copies.
 * Copies of the modified software can be delivered to persons who use it
 * solely for nonprofit, educational, noncommercial research, and
 * noncommercial scholarship purposes provided that this notice in its
 * entirety accompanies all copies.
 * 
 * 3. ALL COMMERCIAL USE, AND ALL USE BY FOR PROFIT ENTITIES, IS EXPRESSLY
 * PROHIBITED WITHOUT A LICENSE FROM SIMPLESCALAR, LLC (info@simplescalar.com).
 * 
 * 4. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 5. Noncommercial and nonprofit users may distribute copies of SimpleScalar
 * in compiled or executable form as set forth in Section 2, provided that
 * either: (A) it is accompanied by the corresponding machine-readable source
 * code, or (B) it is accompanied by a written offer, with no time limit, to
 * give anyone a machine-readable copy of the corresponding source code in
 * return for reimbursement of the cost of distribution. This written offer
 * must permit verbatim duplication by anyone, or (C) it is distributed by
 * someone who received only the executable form, and is accompanied by a
 * copy of the written offer of source code.
 * 
 * 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
 * currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
 * 2395 Timbercrest Court, Ann Arbor, MI 48105.
 * 
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 */


#include "smt.h"
#include "eio.h"

#ifdef SYS_DEBUG
#include<iostream>
#include<iomanip>
#include<string>
#endif

#include<cstdio>
#include<cstdlib>

#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/times.h>
#include <sys/ioctl.h>

#include<net/if.h>
#include<arpa/inet.h>
#include<termios.h>

#if defined(linux)
#include <utime.h>
#include <dirent.h>
#include <sys/vfs.h>
#endif

#include <sys/mount.h>
#include <rpcsvc/rex.h>

#if !defined(linux) && !defined(sparc) && !defined(hpux) && !defined(__hpux) && !defined(ultrix)
#include <sys/select.h>
#endif

#ifdef linux
#include <sgtty.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#endif /* linux */

#if defined(__svr4__)
#include <sys/dirent.h>
#include <sys/filio.h>
#elif defined(__osf__)
#include <dirent.h>
/* -- For some weird reason, getdirentries() is not declared in any
 * -- header file under /usr/include on the Alpha boxes that I tried
 * -- SS-Alpha on. But the function exists in the libraries.
 */
int getdirentries(int fd, char *buf, int nbytes, long *basep);
#endif

#if defined(__svr4__) || defined(__osf__)
#include <sys/statvfs.h>
#define statfs statvfs
#include <sys/time.h>
#include <utime.h>
#include <sgtty.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#if defined(sparc) && defined(__unix__)
#if defined(__svr4__) || defined(__USLC__)
#include <dirent.h>
#else
#include <sys/dir.h>
#endif

/* dorks */
#undef NL0
#undef NL1
#undef CR0
#undef CR1
#undef CR2
#undef CR3
#undef TAB0
#undef TAB1
#undef TAB2
#undef XTABS
#undef BS0
#undef BS1
#undef FF0
#undef FF1
#undef ECHO
#undef NOFLSH
#undef TOSTOP
#undef FLUSHO
#undef PENDIN
#endif

#if defined(hpux) || defined(__hpux)
#undef CR0
#endif

#ifdef __FreeBSD__
#include <sys/ioctl_compat.h>
#else
#ifndef _MSC_VER
#include <termio.h>
#endif
#endif

#if defined(hpux) || defined(__hpux)
/* et tu, dorks! */
#undef HUPCL
#undef ECHO
#undef B50
#undef B75
#undef B110
#undef B134
#undef B150
#undef B200
#undef B300
#undef B600
#undef B1200
#undef B1800
#undef B2400
#undef B4800
#undef B9600
#undef B19200
#undef B38400
#undef NL0
#undef NL1
#undef CR0
#undef CR1
#undef CR2
#undef CR3
#undef TAB0
#undef TAB1
#undef BS0
#undef BS1
#undef FF0
#undef FF1
#undef EXTA
#undef EXTB
#undef B900
#undef B3600
#undef B7200
#undef XTABS
#include <sgtty.h>
#include <utime.h>
#endif

#include <sys/socket.h>
#include <sys/poll.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "sim.h"
#include "endian.h"
#include "syscall.h"
#include "file_table.h"

#define OSF_SYS_syscall			0
//#define OSF_SYS_exit			1 	is defined in machine.h
#define OSF_SYS_fork			2
#define OSF_SYS_read			3
#define OSF_SYS_write			4
#define OSF_SYS_old_open		5	/* 5 is old open */
#define OSF_SYS_close			6
#define OSF_SYS_wait4			7
#define OSF_SYS_old_creat		8	/* 8 is old creat */
#define OSF_SYS_link			9
#define OSF_SYS_unlink			10
#define OSF_SYS_execv			11
#define OSF_SYS_chdir			12
#define OSF_SYS_fchdir			13
#define OSF_SYS_mknod			14
#define OSF_SYS_chmod			15
#define OSF_SYS_chown			16
#define OSF_SYS_obreak			17
#define OSF_SYS_getfsstat		18
#define OSF_SYS_lseek			19
#define OSF_SYS_getpid			20
#define OSF_SYS_mount			21
#define OSF_SYS_unmount			22
#define OSF_SYS_setuid			23
#define OSF_SYS_getuid			24
#define OSF_SYS_exec_with_loader	25
#define OSF_SYS_ptrace			26
#ifdef COMPAT_43
	#define OSF_SYS_nrecvmsg	27
	#define OSF_SYS_nsendmsg	28
	#define OSF_SYS_nrecvfrom	29
	#define OSF_SYS_naccept		30
	#define OSF_SYS_ngetpeername	31
	#define OSF_SYS_ngetsockname	32
#else
	#define OSF_SYS_recvmsg		27
	#define OSF_SYS_sendmsg		28
	#define OSF_SYS_recvfrom	29
	#define OSF_SYS_accept		30
	#define OSF_SYS_getpeername	31
//	#define OSF_SYS_getsockname	32
#endif
#define OSF_SYS_access			33
#define OSF_SYS_chflags			34
#define OSF_SYS_fchflags		35
#define OSF_SYS_sync			36
#define OSF_SYS_kill			37
#define OSF_SYS_old_stat		38	/* 38 is old stat */
#define OSF_SYS_setpgid			39
#define OSF_SYS_old_lstat		40	/* 40 is old lstat */
#define OSF_SYS_dup			41
#define OSF_SYS_pipe			42
#define OSF_SYS_set_program_attributes	43
#define OSF_SYS_profil			44
#define OSF_SYS_open			45
//#define OSF_SYS_osigaction		46	obsolete
#define OSF_SYS_getgid			47
#define OSF_SYS_sigprocmask		48
#define OSF_SYS_getlogin		49
#define OSF_SYS_setlogin		50
#define OSF_SYS_acct			51
#define OSF_SYS_sigpending		52
#define OSF_SYS_ioctl			54
#define OSF_SYS_reboot			55
#define OSF_SYS_revoke			56
#define OSF_SYS_symlink			57
#define OSF_SYS_readlink		58
#define OSF_SYS_execve			59
#define OSF_SYS_umask			60
#define OSF_SYS_chroot			61
#define OSF_SYS_old_fstat		62	/* 62 is old fstat */
#define OSF_SYS_getpgrp			63
#define OSF_SYS_getpagesize		64
#define OSF_SYS_mremap			65
#define OSF_SYS_vfork			66
#define OSF_SYS_stat			67
#define OSF_SYS_lstat			68
#define OSF_SYS_sbrk			69
#define OSF_SYS_sstk			70
#define OSF_SYS_mmap			71
#define OSF_SYS_ovadvise		72
#define OSF_SYS_munmap			73
#define OSF_SYS_mprotect		74
#define OSF_SYS_madvise			75
#define OSF_SYS_old_vhangup		76	/* 76 is old vhangup */
#define OSF_SYS_kmodcall		77
#define OSF_SYS_mincore			78
#define OSF_SYS_getgroups		79
#define OSF_SYS_setgroups		80
#define OSF_SYS_old_getpgrp		81	/* 81 is old getpgrp */
#define OSF_SYS_setpgrp			82
#define OSF_SYS_setitimer		83
#define OSF_SYS_old_wait		84	/* 84 is old wait */
#define OSF_SYS_table			85
#define OSF_SYS_getitimer		86
#define OSF_SYS_gethostname		87
#define OSF_SYS_sethostname		88
#define OSF_SYS_getdtablesize		89
#define OSF_SYS_dup2			90
#define OSF_SYS_fstat			91
#define OSF_SYS_fcntl			92
#define OSF_SYS_select			93
#define OSF_SYS_poll			94
#define OSF_SYS_fsync			95
#define OSF_SYS_setpriority		96
#define OSF_SYS_socket			97
#define OSF_SYS_connect			98
#ifdef COMPAT_43
	#define OSF_SYS_accept		99
#else
	#define OSF_SYS_old_accept	99	/* 99 is old accept */
#endif
#define OSF_SYS_getpriority		100

//COMPAT_43 only matters if we handle the call differently.
#define OSF_SYS_send			101
//#ifdef COMPAT_43
//	#define OSF_SYS_send		101
//	#define OSF_SYS_recv		102
//	#define OSF_SYS_getsockname	150
//#else
//	#define OSF_SYS_old_send	101	/* 101 is old send */
//	#define OSF_SYS_old_recv	102	/* 102 is old recv */
//	#define OSF_SYS_old_getsockname	150	/* 150 is old getsockname */
//#endif
#define OSF_SYS_sigreturn		103
#define OSF_SYS_bind			104
#define OSF_SYS_setsockopt		105
#define OSF_SYS_listen			106
#define OSF_SYS_plock			107
#define OSF_SYS_old_sigvec		108	/* 108 is old sigvec */
#define OSF_SYS_old_sigblock		109	/* 109 is old sigblock */
#define OSF_SYS_old_sigsetmask		110	/* 110 is old sigsetmask */
#define OSF_SYS_sigsuspend		111
#define OSF_SYS_sigstack		112
#ifdef COMPAT_43
	#define OSF_SYS_recvmsg		113
	#define OSF_SYS_sendmsg		114
#else
	#define OSF_SYS_old_recvmsg	113	/* 113 is old recvmsg */
	#define OSF_SYS_old_sendmsg	114	/* 114 is old sendmsg */
#endif
//#define OSF_SYS_vtrace		115	obsolete
#define OSF_SYS_gettimeofday		116
#define OSF_SYS_getrusage		117
#define OSF_SYS_getsockopt		118
#define OSF_SYS_readv			120
#define OSF_SYS_writev			121
#define OSF_SYS_settimeofday		122
#define OSF_SYS_fchown			123
#define OSF_SYS_fchmod			124
#ifdef COMPAT_43
	#define OSF_SYS_recvfrom	125
#else
	#define OSF_SYS_old_recvfrom	125	/* 125 is old recvfrom */
#endif
#define OSF_SYS_setreuid		126
#define OSF_SYS_setregid		127
#define OSF_SYS_rename			128
#define OSF_SYS_truncate		129
#define OSF_SYS_ftruncate		130
#define OSF_SYS_flock			131
#define OSF_SYS_setgid			132
#define OSF_SYS_sendto			133
#define OSF_SYS_shutdown		134
#define OSF_SYS_socketpair		135
#define OSF_SYS_mkdir			136
#define OSF_SYS_rmdir			137
#define OSF_SYS_utimes			138
//#define OSF_SYS_sigreturn		139	obsolete
#define OSF_SYS_adjtime			140
#ifdef COMPAT_43
	#define OSF_SYS_getpeername	141
#else
	#define OSF_SYS_old_getpeername	141	/* 141 is old getpeername */
#endif
#define OSF_SYS_gethostid		142
#define OSF_SYS_sethostid		143
#define OSF_SYS_getrlimit		144
#define OSF_SYS_setrlimit		145
#define OSF_SYS_old_killpg		146	/* 146 is old killpg */
#define OSF_SYS_setsid			147
#define OSF_SYS_quotactl		148
#define OSF_SYS_oldquota		149
#define OSF_SYS_getsockname		150
#define OSF_SYS_pid_block		153
#define OSF_SYS_pid_unblock		154
#define OSF_SYS_sigaction		156
#define OSF_SYS_sigwaitprim		157
#define OSF_SYS_nfssvc			158
#define OSF_SYS_getdirentries		159
#define OSF_SYS_statfs			160
#define OSF_SYS_fstatfs			161
#define OSF_SYS_async_daemon		163
#define OSF_SYS_getfh			164
#define OSF_SYS_getdomainname		165
#define OSF_SYS_setdomainname		166
#define OSF_SYS_exportfs		169
#define OSF_SYS_alt_plock		181	/* 181 is alternate plock */
#define OSF_SYS_getmnt			184
#define OSF_SYS_alt_sigpending		187	/* 187 is alternate sigpending */
#define OSF_SYS_alt_setsid		188	/* 188 is alternate setsid */
#define OSF_SYS_swapon			199
#define OSF_SYS_msgctl			200
#define OSF_SYS_msgget			201
#define OSF_SYS_msgrcv			202
#define OSF_SYS_msgsnd			203
#define OSF_SYS_semctl			204
#define OSF_SYS_semget			205
#define OSF_SYS_semop			206
#define OSF_SYS_utsname			207
#define OSF_SYS_lchown			208
#define OSF_SYS_shmat			209
#define OSF_SYS_shmctl			210
#define OSF_SYS_shmdt			211
#define OSF_SYS_shmget			212
#define OSF_SYS_mvalid			213
#define OSF_SYS_getaddressconf		214
#define OSF_SYS_msleep			215
#define OSF_SYS_mwakeup			216
#define OSF_SYS_msync			217
#define OSF_SYS_signal			218
#define OSF_SYS_utc_gettime		219
#define OSF_SYS_utc_adjtime		220
#define OSF_SYS_security		222
#define OSF_SYS_kloadcall		223
#define OSF_SYS_stat64			224
#define OSF_SYS_lstat64			225
#define OSF_SYS_fstat64			226
#define OSF_SYS_statfs64		227
#define OSF_SYS_fstatfs64		228
#define OSF_SYS_getpgid			233
#define OSF_SYS_getsid			234
#define OSF_SYS_sigaltstack		235
#define OSF_SYS_waitid			236
#define OSF_SYS_priocntlset		237
#define OSF_SYS_sigsendset		238
#define OSF_SYS_set_speculative		239
#define OSF_SYS_msfs_syscall		240
#define OSF_SYS_sysinfo			241
#define OSF_SYS_uadmin			242
#define OSF_SYS_fuser			243
#define OSF_SYS_proplist_syscall	244
#define OSF_SYS_ntp_adjtime		245
#define OSF_SYS_ntp_gettime		246
#define OSF_SYS_pathconf		247
#define OSF_SYS_fpathconf		248
#define OSF_SYS_uswitch			250
#define OSF_SYS_usleep_thread		251
#define OSF_SYS_audcntl			252
#define OSF_SYS_audgen			253
#define OSF_SYS_sysfs			254
#define OSF_SYS_subsys_info		255
#define OSF_SYS_getsysinfo		256
#define OSF_SYS_setsysinfo		257
#define OSF_SYS_afs_syscall		258
#define OSF_SYS_swapctl			259
#define OSF_SYS_memcntl			260
#define OSF_SYS_fdatasync		261
#define OSF_SYS_uname			339
#define OSF_SYS_rt_sigaction		352
#define OSF_SYS_gettimeofday2		359
#define OSF_SYS_getrusage2		364
#define OSF_SYS_exit_group		405
#define OSF_SYS_linux_stat64		425
#define OSF_SYS_linux_fstat64		427

//The following (Mach syscalls?) are defined in /usr/include/alpha/nxm.h
//The following are defined /usr/include/mach/syscall_sw.h or in machine/nxm.h (stack_create)
//These are called as system calls, but should check for execution rights?
//extern kern_return_t stack_create(struct vm_stack *); (kern/syscall_subr.h), vm_Stack in sys/mman.h
//Defined in kern/syscall_sw.c, kern/syscall_subr.c (NXM syscalls)
//http://www.m5sim.org/docs/tru64__syscalls_8cc-source.html
#define OSF_MACH_task_self		-10
#define OSF_MACH_thread_reply		-11
#define OSF_MACH_task_notify		-12
#define OSF_MACH_thread_self		-13
#define OSF_MACH_msg_send_trap		-20
#define OSF_MACH_msg_receive_trap	-21
#define OSF_MACH_msg_rpc_trap		-22
#define OSF_MACH_block			-24
#define OSF_MACH_unblock		-25
#define OSF_MACH_thread_destroy		-29	//destroy a thread
#define OSF_MACH_thread_create		-32	//create a thread for use of thread library
#define OSF_MACH_task_init		-33	//initialize process for two-level scheduling
#define OSF_MACH_idle			-35	//declare this thread to be idle
#define OSF_MACH_wakeup_idle		-36	//release idle threads
#define OSF_MACH_set_pthid		-37
#define OSF_MACH_thread_kill		-38	//send signal to a user thread
#define OSF_MACH_thread_block		-39	//block a bound thread
#define OSF_MACH_thread_wakeup		-40	//unblock a bound thread
#define OSF_MACH_get_binding		-42
#define OSF_MACH_resched		-44	//make thread perform rescheduled upcall
#define OSF_MACH_set_cancel		-45	//set cancel cancellation state
#define OSF_MACH_set_binding		-46
#define OSF_MACH_stack_create		-47
#define OSF_MACH_get_state		-48	//returns registers (debugging)
#define OSF_MACH_thread_suspend		-49	//suspend a bound thread
#define OSF_MACH_thread_resume		-50	//suspend a thread?
#define OSF_MACH_signal_check		-51	//check for signals (for library)
#define OSF_MACH_pshared_init		-63
#define OSF_MACH_pshared_block		-64
#define OSF_MACH_pshared_unblock	-65
#define OSF_MACH_pshared_destroy	-66
#define OSF_MACH_switch_pri		-67

class vm_stack
{
	public:
		md_addr_t		address;		//address hint
		md_gpr_t		rsize;			//red zone size
		md_gpr_t		ysize;			//yellow zone size
		md_gpr_t		gsize;			//green zone size
		md_gpr_t		swap;			//amount of swap to reserve
		md_gpr_t		incr;			//growth increment
		unsigned long long	align;			//address alignment
		md_gpr_t		flags;			//MAP_FIXED etc.
		md_addr_t		attr;			//allocation policy (address to type malloc_attr)
		md_gpr_t		reserved;
};

//This defines the subset of the OSF/1 getsysinfo/setsysinfo calls that we support (read: have to support)
//getsysinfo
#define GSI_MAX_UPROCS			2
#define GSI_UACPROC			8
#define GSI_LMF				9
#define GSI_PHYSMEM			19
#define GSI_MAX_CPU			30
#define GSI_CLK_TCK			42
#define GSI_IEEE_FP_CONTROL		45
#define GSI_IEEE_STATE_AT_SIGNAL	46
#define GSI_CPUS_IN_BOX			55
#define GSI_CPU_INFO			59
#define GSI_PROC_TYPE			60
#define GSI_VERSION_STRING		62
#define GSI_TIMER_MAX			67
#define GSI_GET_HWRPB			101
#define GSI_PLATFORM_NAME		103
//setsysinfo
#define SSI_NVPAIRS			1
#define SSI_LMF				7
#define SSI_IEEE_FP_CONTROL		14
#define SSI_IEEE_STATE_AT_SIGNAL	15
#define SSI_IEEE_IGNORE_STATE_AT_SIGNAL	16
#define SSI_IEEE_RAISE_EXCEPTION	1001	/* linux specific */
//other
#define SSIN_UACPROC			6
#define UAC_BITMASK			7
#define UAC_NOPRINT			1
#define UAC_NOFIX			2
#define UAC_SIGBUS			4

class osf_cpu_info
{
	public:
		int current_cpu, cpus_in_box, cpu_type, ncpus;
		unsigned long long cpus_present, cpus_running, cpu_binding, cpu_ex_binding;
		int mhz, unused[3];
};


class osf_flock
{
	public:
		short l_type;			//Type of lock: F_RDLCK, F_WRLCK, or F_UNLCK.
		short l_whence;			//Where `l_start' is relative to (like `lseek').
		unsigned long long l_start;	//Offset where the lock begins.
		unsigned long long l_len;	//Size of the locked area; zero means until EOF.
		pid_t l_pid;			//Process holding the lock.
		struct flock copy_out()
		{
			struct flock retval;
			retval.l_type = l_type;
			retval.l_whence = l_whence;

			//These may be 64 or 32 bit targets
			retval.l_start = l_start;
			retval.l_len = l_len;

			retval.l_pid = l_pid;
			return retval;
		}
};

//translate system call arguments
class xlate_table_t
{
	public:
//		xlate_table_t()
//		: target_val(0), host_val(0)
//		{}
		md_gpr_t target_val;
		int host_val;
};

int xlate_arg(md_gpr_t target_val, xlate_table_t *map, int map_sz, const char *name)
{
	for(int i=0; i < map_sz; i++)
	{
		if(target_val == map[i].target_val)
		{
			return map[i].host_val;
		}
	}

	//not found, issue warning and return target_val
	warn("Could not translate argument for `%s': %d", name, target_val);
	return target_val;
}

//internal system call buffer size, used primarily for file name arguments,
//argument larger than this will be truncated
#define MAXBUFSIZE 		1024

//total bytes to copy from a valid pointer argument for ioctl() calls,
//syscall.c does not decode ioctl() calls to determine the size of the
//arguments that reside in memory, instead, the ioctl() proxy simply copies
//NUM_IOCTL_BYTES bytes from the pointer argument to host memory
#define NUM_IOCTL_BYTES		128

//class winsize for ioctl(TIOCGWINSZ,...)
class osf_winsize
{
	public:
		osf_winsize()
		: ws_row(0), ws_col(0), ws_xpixel(0), ws_ypixel(0)
		{}
		unsigned short	ws_row;			//rows, in characters
		unsigned short	ws_col;			//columns, in characters
		unsigned short	ws_xpixel;		//horizontal size, pixels
		unsigned short	ws_ypixel;		//vertical size, pixels
};

//OSF ioctl() requests
#define OSF_IOCPARM_MASK		0x1fff
#define OSF_IOC_VOID			0x20000000
#define OSF_IOC_OUT			0x40000000
#define OSF_IOC_IN			0x80000000
#define OSF_IOC_INOUT			(OSF_IOC_IN|OSF_IOC_OUT)
#define OSF_IOC(inout,group,num,len)	(inout | ((len & OSF_IOCPARM_MASK) << 16) | ((group) << 8) | (num))
#define OSF_IO(g,n)			OSF_IOC(OSF_IOC_VOID,	(g),	(n), 0)
#define OSF_IOR(g,n,t)			OSF_IOC(OSF_IOC_OUT,	(g),	(n), sizeof(t))
#define OSF_IOW(g,n,t)			OSF_IOC(OSF_IOC_IN,	(g),	(n), sizeof(t))
#define OSF_IOWR(g,n,t)			OSF_IOC(OSF_IOC_INOUT,	(g),	(n), sizeof(t))
#define OSF_TIOCGETP			OSF_IOR('t', 8, struct osf_sgttyb)		//get parameters -- gtty  	(0x40067408)
#define OSF_FIONREAD			OSF_IOR('f', 127, int)				//get number of bytes to read	(0x4004667f)
#define OSF_TIOCISATTY			OSF_IO('t', 94)					//is this a tty?		(0x2000745e)
#define OSF_TIOCGWINSZ			OSF_IOR('t' , 104, struct osf_winsize)		//get window size		(0x40087468)
#define OSF_TIOCSWINSZ			OSF_IOW('t', 103, struct osf_winsize)		//set window size		(0x80087467)
#define OSF_TIOCGETA			OSF_IOR('t', 0x13, struct osf_termios)		//		(0x402C7413)
#define OSF_TIOCSETA			OSF_IOW('t', 0x14, struct osf_termios)		//		(0x802c7414)
#define OSF_TIOCSETAW			OSF_IOW('t', 0x15, struct osf_termios)		//drain output, set		(0x802c7415)
#define OSF_TIOCSETAF			OSF_IOW('t', 0x16, struct osf_termios)		//		(0x802C7416)
#define OSF_SIOCGIFCONF			OSF_IOWR('i', 0x24, ifconf)			//get ifnet list		(0xc0106924)
#define OSF_FIONBIO			OSF_IOW('f', 0x7e, int)				//set/clear non-blocking i/o	(0x8004667e)
//Pending
#define OSF_TIOCSPGRP			OSF_IOW('t', 0x76, pid_t)			//should be struct pid_t	(0x80047476)
#define OSF_TIOCGPGRP			OSF_IOR('t', 0x77, pid_t)			//should be struct pid_t	(0x40047477)
#define OSF_TIOCLGET			OSF_IOR('t', 0x7c, int)				//get local modes		(0x4004747c)
#define TIOCGETP TCGETA

class osf_termios
{
	public:
		unsigned int	c_iflag;	//input flags
		unsigned int	c_oflag;	//output flags
		unsigned int	c_cflag;	//control flags
		unsigned int	c_lflag;	//local flags
		unsigned char	c_cc[20];	//control chars
		int		c_ispeed;	//input speed
		int		c_ospeed;	//output speed

		termios copy_out()
		{
			termios retval;
			retval.c_cflag = (c_cflag >> 4);

			retval.c_iflag = c_iflag & 0x29ff;
			if(c_iflag & 0x200)
			{
				retval.c_iflag |= 0x400;
			}
			if(c_iflag & 0x400)
			{
				retval.c_iflag |= 0x1000;
			}
			if(c_iflag & 0x1000)
			{
				retval.c_iflag |= 0x200;
			}

			retval.c_oflag = c_oflag & 0x1f9;
			if(c_oflag & 0x2)
			{
				retval.c_oflag |= 0x4;
			}
			if(c_oflag & 0x4)
			{
				retval.c_oflag |= 0x2;
			}
			if(c_oflag & 0x300)	//This may be wrong
			{
				retval.c_oflag |= 0x100;
			}
			if(c_oflag & 0x400)
			{
				retval.c_oflag |= 0x800;
			}
			if(c_oflag & 0x800)
			{
				retval.c_oflag |= 0x1000;
			}
			if(c_oflag & 0x1000)
			{
				retval.c_oflag |= 0x200;
			}
			if(c_oflag & 0x2000)
			{
				retval.c_oflag |= 0x400;
			}
			if(c_oflag & 0x4000)
			{
				retval.c_oflag |= 0x8000;
			}
			if(c_oflag & 0x8000)
			{
				retval.c_oflag |= 0x2000;
			}
			if(c_oflag & 0x10000)
			{
				retval.c_oflag |= 0x4000;
			}
//			if(c_oflag & 0x40000)		//Uncomment if XTABS is equivalent to OXTABS
//			{
//				retval.c_oflag |= 0x1800;
//			}

			retval.c_lflag = c_lflag & 0x8;
			if(c_lflag & 0x1)
			{
				retval.c_lflag |= 0x800;
			}
			if(c_lflag & 0x2)
			{
				retval.c_lflag |= 0x10;
			}
			if(c_lflag & 0x4)
			{
				retval.c_lflag |= 0x20;
			}
			if(c_lflag & 0x10)
			{
				retval.c_lflag |= 0x40;
			}
			if(c_lflag & 0x20)
			{
				retval.c_lflag |= 0x400;
			}
			if(c_lflag & 0x40)
			{
				retval.c_lflag |= 0x200;
			}
			if(c_lflag & 0x80)
			{
				retval.c_lflag |= 0x1;
			}
			if(c_lflag & 0x100)
			{
				retval.c_lflag |= 0x2;
			}
			if(c_lflag & 0x400)
			{
				retval.c_lflag |= 0x8000;
			}
			if(c_lflag & 0x4000)
			{
				retval.c_lflag |= 0x4;
			}
			if(c_lflag & 0x400000)
			{
				retval.c_lflag |= 0x100;
			}
			if(c_lflag & 0x800000)
			{
				retval.c_lflag |= 0x1000;
			}
			if(c_lflag & 0x20000000)
			{
				retval.c_lflag |= 0x4000;
			}
			if(c_lflag & 0x80000000)
			{
				retval.c_lflag |= 0x80;
			}

			memset(retval.c_cc,255,32);
			retval.c_cc[VINTR] = c_cc[8];
			retval.c_cc[VQUIT] = c_cc[9];
			retval.c_cc[VERASE] = c_cc[3];
			retval.c_cc[VKILL] = c_cc[5];
			retval.c_cc[VEOF] = c_cc[0];
			retval.c_cc[VTIME] = c_cc[17];
			retval.c_cc[VMIN] = c_cc[16];
			retval.c_cc[VSWTC] = 255;
			retval.c_cc[VSTART] = c_cc[12];
			retval.c_cc[VSTOP] = c_cc[13];
			retval.c_cc[VSUSP] = c_cc[10];
			retval.c_cc[VEOL] = c_cc[1];
			retval.c_cc[VREPRINT] = c_cc[6];
			retval.c_cc[VDISCARD] = c_cc[15];
			retval.c_cc[VWERASE] = c_cc[4];
			retval.c_cc[VLNEXT] = c_cc[14];
			retval.c_cc[VEOL2] = c_cc[2];
			retval.c_ispeed = c_ispeed;
			retval.c_ospeed = c_ospeed;
			return retval;
		}

		void copy_in(termios & in)
		{
			c_cflag = (in.c_cflag << 4) & ~0xff;

			c_iflag = in.c_iflag & 0x29ff;
			if(in.c_iflag & 0x200)
			{
				c_iflag |= 0x1000;
			}
			if(in.c_iflag & 0x400)
			{
				c_iflag |= 0x200;
			}
			if(in.c_iflag & 0x1000)
			{
				c_iflag |= 0x400;
			}
			//IUTF8 is lost

			c_oflag = in.c_oflag & 0x1f9;
			if(in.c_oflag & 0x2)
			{
				c_oflag |= 0x4;
			}
			if(in.c_oflag & 0x4)
			{
				c_oflag |= 0x2;
			}
			if(in.c_oflag & 0x100)	//This may be wrong
			{
				c_oflag |= 0x300;
			}
			if(in.c_oflag & 0x200)
			{
				c_oflag |= 0x1000;
			}
			if(in.c_oflag & 0x400)
			{
				c_oflag |= 0x2000;
			}
			if(in.c_oflag & 0x800)
			{
				c_oflag |= 0x400;
			}
			if(in.c_oflag & 0x1000)
			{
				c_oflag |= 0x800;
			}
			if(in.c_oflag & 0x2000)
			{
				c_oflag |= 0x8000;
			}
			if(in.c_oflag & 0x4000)
			{
				c_oflag |= 0x10000;
			}
			if(in.c_oflag & 0x8000)
			{
				c_oflag |= 0x4000;
			}
//			if(in.c_oflag & 0x1800)		//Uncomment if XTABS is equivalent to OXTABS
//			{
//				c_oflag |= 0x40000;
//			}

			c_lflag = in.c_lflag & 0x8;
			if(in.c_lflag & 0x1)
			{
				c_lflag |= 0x80;
			}
			if(in.c_lflag & 0x2)
			{
				c_lflag |= 0x100;
			}
			if(in.c_lflag & 0x4)
			{
				c_lflag |= 0x4000;
			}
			if(in.c_lflag & 0x10)
			{
				c_lflag |= 0x2;
			}
			if(in.c_lflag & 0x20)
			{
				c_lflag |= 0x4;
			}
			if(in.c_lflag & 0x40)
			{
				c_lflag |= 0x10;
			}
			if(in.c_lflag & 0x80)
			{
				c_lflag |= 0x80000000;
			}
			if(in.c_lflag & 0x100)
			{
				c_lflag |= 0x400000;
			}
			if(in.c_lflag & 0x200)
			{
				c_lflag |= 0x40;
			}
			if(in.c_lflag & 0x400)
			{
				c_lflag |= 0x20;
			}
			if(in.c_lflag & 0x800)
			{
				c_lflag |= 0x1;
			}
			if(in.c_lflag & 0x1000)
			{
				c_lflag |= 0x800000;
			}
			if(in.c_lflag & 0x4000)
			{
				c_lflag |= 0x20000000;
			}
			if(in.c_lflag & 0x8000)
			{
				c_lflag |= 0x400;
			}

			memset(c_cc,255,20);
			c_cc[0] = in.c_cc[VEOF];
			c_cc[1] = in.c_cc[VEOL];
			c_cc[2] = in.c_cc[VEOL2];
			c_cc[3] = in.c_cc[VERASE];
			c_cc[4] = in.c_cc[VWERASE];
			c_cc[5] = in.c_cc[VKILL];
			c_cc[6] = in.c_cc[VREPRINT];
			c_cc[7] = 255;	//spare
			c_cc[8] = in.c_cc[VINTR];
			c_cc[9] = in.c_cc[VQUIT];
			c_cc[11] = 255;	//VDSUSP
			c_cc[12] = in.c_cc[VSTART];
			c_cc[10] = in.c_cc[VSUSP];
			c_cc[13] = in.c_cc[VSTOP];
			c_cc[14] = in.c_cc[VLNEXT];
			c_cc[15] = in.c_cc[VDISCARD];
			c_cc[16] = in.c_cc[VMIN];
			c_cc[17] = in.c_cc[VTIME];
			c_cc[18] = 255;	//VSTATUS
			c_cc[19] = 255;	//spare
			c_ispeed = in.c_ispeed;
			c_ospeed = in.c_ospeed;
		}
};

//ifconf structure for alpha
class osf_ifconf
{
	public:
		int ifc_len;
		int padding;
		union
		{
			md_gpr_t ifcu_buf;
			md_gpr_t ifcu_req;

		} ifc_ifcu;
};

//target stat() buffer definition, the host stat buffer format is automatically mapped to/from this format in syscall.c
class osf_statbuf
{
	public:
		osf_statbuf()
		: osf_st_dev(0), osf_st_ino(0), osf_st_mode(0), osf_st_nlink(0), pad0(0), osf_st_uid(0), osf_st_gid(0),
			osf_st_rdev(0), pad1(0), osf_st_size(0), osf_st_atime(0), osf_st_spare1(0), osf_st_mtime(0),
			osf_st_spare2(0), osf_st_ctime(0), osf_st_spare3(0), osf_st_blksize(0), osf_st_flags(0), osf_st_gen(0)
		{}
		word_t osf_st_dev;
		word_t osf_st_ino;
		word_t osf_st_mode;
		half_t osf_st_nlink;
		half_t pad0;			//to match Alpha/AXP padding...
		word_t osf_st_uid;
		word_t osf_st_gid;
		word_t osf_st_rdev;
		word_t pad1;			//to match Alpha/AXP padding...
		qword_t osf_st_size;
		word_t osf_st_atime;
		word_t osf_st_spare1;
		word_t osf_st_mtime;
		word_t osf_st_spare2;
		word_t osf_st_ctime;
		word_t osf_st_spare3;
		word_t osf_st_blksize;
		word_t osf_st_blocks;
		word_t osf_st_flags;
		word_t osf_st_gen;
};

class osf_statbuf64
{
	public:
		osf_statbuf64()
		: osf_st_dev(0), osf_st_retired1(0), osf_st_mode(0), osf_st_nlink(0), osf_st_nlink_reserved(0), osf_st_uid(0), osf_st_gid(0),
			osf_st_rdev(0), osf_st_ldev(0), osf_st_size(0), osf_st_retired2(0), osf_st_uatime(0), osf_st_retired3(0),
			osf_st_umtime(0), osf_st_retired4(0), osf_st_uctime(0), osf_st_retired5(0), osf_st_retired6(0), osf_st_flags(0),
			osf_st_gen(0), osf_st_ino(0), osf_st_ino_reserved(0), osf_st_atime(0), osf_st_atime_reserved(0), osf_st_mtime(0),
			osf_st_mtime_reserved(9), osf_st_ctime(0), osf_st_ctime_reserved(0), osf_st_blksize(0), osf_st_blocks(0)
		{
			for(size_t i=0;i<4;i++)
			{
				osf_st_spare[i] = 0;
			}
		}
		word_t			osf_st_dev;
		word_t			osf_st_retired1;
		word_t			osf_st_mode;
		half_t			osf_st_nlink;
		half_t			osf_st_nlink_reserved;
		unsigned int		osf_st_uid;
		unsigned int		osf_st_gid;
		word_t			osf_st_rdev;
		word_t			osf_st_ldev;
		qword_t			osf_st_size;

		word_t			osf_st_retired2;
		word_t			osf_st_uatime;
		word_t			osf_st_retired3;
		word_t			osf_st_umtime;
		word_t			osf_st_retired4;
		word_t			osf_st_uctime;

		word_t			osf_st_retired5;
		word_t			osf_st_retired6;

		unsigned int		osf_st_flags;
		unsigned int		osf_st_gen;

		long long		osf_st_spare[4];

		word_t			osf_st_ino;
		word_t			osf_st_ino_reserved;

		word_t			osf_st_atime;
		word_t			osf_st_atime_reserved;
		word_t			osf_st_mtime;
		word_t			osf_st_mtime_reserved;
		word_t			osf_st_ctime;
		word_t			osf_st_ctime_reserved;

		qword_t			osf_st_blksize;
		qword_t			osf_st_blocks;
};

//FIXME: This needs to be verified, it used to be byte_t for the first four and that
//didn't do assignment correctly
//short, short, char, char seems not to crash but byte_t.... is what the patch used
class osf_sgttyb
{
	public:
		osf_sgttyb()
		: sg_ispeed(0), sg_ospeed(0), sg_erase(0), sg_kill(0), sg_flags()
		{}
		byte_t sg_ispeed;	//input speed
		byte_t sg_ospeed;	//output speed
		byte_t sg_erase;	//erase character
		byte_t sg_kill;		//kill character
		shalf_t sg_flags;	//mode flags

		sgttyb copy_out()
		{
			sgttyb retval;
			retval.four = 4;
			retval.chars[0] = sg_ispeed;
			retval.chars[1] = sg_ospeed;
			retval.chars[2] = sg_erase;
			retval.chars[3] = sg_kill;
			retval.flags = sg_flags;
			return retval;
		}

		void copy_in(sgttyb & in)
		{
			sg_ispeed = in.chars[0];
			sg_ospeed = in.chars[1];
			sg_erase = in.chars[2];
			sg_kill = in.chars[3];
			sg_flags = in.flags;
		}
};

#define OSF_NSIG		32

#define OSF_SIG_BLOCK		1
#define OSF_SIG_UNBLOCK		2
#define OSF_SIG_SETMASK		3

class osf_sigstack
{
	public:
		osf_sigstack()
		: ss_sp(NULL), ss_onstack(0)
		{}
		void * ss_sp;
		int ss_onstack;
};

class osf_sigcontext
{
	public:
		osf_sigcontext()
		: sc_onstack(0), sc_mask(0), sc_pc(0), sc_ps(0),
			sc_ownedfp(0),sc_fpcr(0), sc_fp_control(0)
		{
			for(size_t i=0;i<32;i++)
			{
				sc_regs[i] = sc_fpregs[i] = 0;
			}
		}
		qword_t sc_onstack;		//sigstack state to restore
		qword_t sc_mask;		//signal mask to restore
		qword_t sc_pc;			//pc at time of signal
		qword_t sc_ps;			//psl to retore
		qword_t sc_regs[32];		//processor regs 0 to 31
		qword_t sc_ownedfp;		//fp has been used
		qword_t sc_fpregs[32];		//fp regs 0 to 31
		qword_t sc_fpcr;		//floating point control register
		qword_t sc_fp_control;		//software fpcr
};

class osf_statfs
{
	public:
		osf_statfs()
		: f_type(0), f_flags(0), f_fsize(0), f_bsize(0), f_blocks(0), f_bfree(0), f_bavail(0), f_files(0), f_ffree(0), f_fsid(0),
			f_namemax(0), f_reserved1(0)
		{
			size_t i = 0;
			for(;i<8;i++)
			{
				f_spare[i] = f_mntonname[i] = f_mntfromname[i] = mount_info[i] = 0;
			}
			for(;i<80;i++)
			{
				f_mntonname[i] = f_mntfromname[i] = mount_info[i] = 0;
			}
			for(;i<90;i++)
			{
				f_mntonname[i] = f_mntfromname[i] = 0;
			}
		}
		shalf_t		f_type;			//type of filesystem (see below)
		shalf_t		f_flags;		//copy of mount flags
		word_t		f_fsize;		//fundamental filesystem block size
		word_t		f_bsize;		//optimal transfer block size
		word_t		f_blocks;		//total data blocks in file system, may not represent fs size.

		word_t		f_bfree;		//free blocks in fs
		word_t		f_bavail;		//free blocks avail to non-root
		word_t		f_files;		//total file nodes in file system
		word_t		f_ffree;		//free file nodes in fs
		qword_t		f_fsid;			//file system id
		half_t		f_namemax;		//Maximum filename length (statvfs)
		shalf_t		f_reserved1;		//spare or grow f_namemax later
		word_t		f_spare[8];
		char		f_mntonname[90];	//Directory on which mounted
		char		f_mntfromname[90];	//Mounted filesystem
		char		mount_info[80];		//Mount options (this should be union mount_info mount_info, see sys/mount.h - this is a stub for now)
};

class osf_statfs64
{
	public:
		osf_statfs64()
		: f_type(0), f_flags(0), f_fsid(0), f_namemax(0), f_reserved1(0), f_flags2(0), f_fsize(0), f_bsize(0), f_blocks(0), f_bfree(0), f_bavail(0),
			f_files(0), f_ffree(0)
		{
			size_t i = 0;
			for(;i<7;i++)
			{
				f_mntonname[i] = f_mntfromname[i] = f_retired[i] = f_spare2[i] = f_spare[i] = f_retired8[i] = f_retired9[i] = mount_info[i] = 0;
			}
			for(;i<8;i++)
			{
				f_mntonname[i] = f_mntfromname[i] = f_spare2[i] = f_spare[i] = f_retired8[i] = f_retired9[i] = mount_info[i] = 0;
			}
			for(;i<14;i++)
			{
				f_mntonname[i] = f_mntfromname[i] = f_spare2[i] = f_retired8[i] = f_retired9[i] = mount_info[i] = 0;
			}
			for(;i<80;i++)
			{
				f_mntonname[i] = f_mntfromname[i] = f_retired8[i] = f_retired9[i] = mount_info[i] = 0;
			}
			for(;i<90;i++)
			{
				f_mntonname[i] = f_mntfromname[i] = f_retired8[i] = f_retired9[i] = 0;
			}
			for(;i<1024;i++)
			{
				f_mntonname[i] = f_mntfromname[i] = 0;
			}
		}
		shalf_t		f_type;			//type of filesystem (see below)
		shalf_t		f_flags;		//copy of mount flags
		word_t		f_retired[7];		//These are f_retired1, f_retired2 .. f_retired7 in the source
		qword_t		f_fsid;			//file system id
		half_t		f_namemax;		//Maximum filename length (statvfs)
		shalf_t		f_reserved1;		//spare or grow f_namemax later
		word_t		f_spare[8];
		char		f_retired8[90];
		char		f_retired9[90];
		char		mount_info[80];		//Mount options (this should be union mount_info mount_info, see sys/mount.h - this is a stub for now)
		qword_t		f_flags2;		//Extended f_flags field
		long long	f_spare2[14];
		qword_t		f_fsize;		//fundamental filesystem block size
		qword_t		f_bsize;		//optimal transfer block size
		qword_t		f_blocks;		//total data blocks in file system, may not represent fs size.
		qword_t		f_bfree;		//free blocks in fs
		qword_t		f_bavail;		//free blocks avail to non-root
		qword_t		f_files;		//total file nodes in file system
		qword_t		f_ffree;		//free file nodes in fs
		char		f_mntonname[1024];
		char		f_mntfromname[1024];
};

class osf_timeval
{
	public:
		osf_timeval()
		: osf_tv_sec(0), osf_tv_usec(0)
		{}
		sword_t osf_tv_sec;	//seconds
		sword_t osf_tv_usec;	//microseconds
};

class osf_timezone
{
	public:
		osf_timezone()
		: osf_tz_minuteswest(0), osf_tz_dsttime(0)
		{}
		sword_t osf_tz_minuteswest;	//minutes west of Greenwich
		sword_t osf_tz_dsttime;		//type of dst correction
};

//target getrusage() buffer definition, the host stat buffer format is
//automagically mapped to/from this format in syscall.c
class osf_rusage
{
	public:
		osf_rusage()
		: osf_ru_maxrss(0), osf_ru_ixrss(0), osf_ru_idrss(0), osf_ru_isrss(0), osf_ru_minflt(0), osf_ru_majflt(0), osf_ru_nswap(0),
			osf_ru_inblock(0), osf_ru_oublock(0), osf_ru_msgsnd(0), osf_ru_msgrcv(0), osf_ru_nsignals(0), osf_ru_nvcsw(0), osf_ru_nivcsw(0)
		{}
		osf_timeval osf_ru_utime;
		osf_timeval osf_ru_stime;
		sword_t osf_ru_maxrss;
		sword_t osf_ru_ixrss;
		sword_t osf_ru_idrss;
		sword_t osf_ru_isrss;
		sword_t osf_ru_minflt;
		sword_t osf_ru_majflt;
		sword_t osf_ru_nswap;
		sword_t osf_ru_inblock;
		sword_t osf_ru_oublock;
		sword_t osf_ru_msgsnd;
		sword_t osf_ru_msgrcv;
		sword_t osf_ru_nsignals;
		sword_t osf_ru_nvcsw;
		sword_t osf_ru_nivcsw;
};

class osf_rlimit
{
	public:
		osf_rlimit()
		: osf_rlim_cur(0), osf_rlim_max(0)
		{}
		qword_t osf_rlim_cur;		//current (soft) limit
		qword_t osf_rlim_max;		//maximum value for rlim_cur
};

class osf_sockaddr
{
	public:
		osf_sockaddr()
		: sa_family(0)
		{
			for(size_t i=0;i<14;i++)
			{
				sa_data[i] = 0;
			}
		}
		half_t sa_family;		//address family, AF_xxx
		byte_t sa_data[14];		//14 bytes of protocol address
};

class osf_iovec
{
	public:
		osf_iovec()
		: iov_base(0), iov_len(0), pad(0)
		{}
		md_addr_t iov_base;		//starting address
		word_t iov_len;			//length in bytes
		word_t pad;
};

//returns size of DIRENT structure - was:	(sizeof(word_t) + 2*sizeof(half_t) + strlen(STR) + 1)
#define OSF_DIRENT_SZ(STR)			(sizeof(word_t) + 2*sizeof(half_t) + (((strlen(STR) + 1) + 3)/4)*4)

class osf_dirent
{
	public:
		osf_dirent()
		: d_ino(0), d_reclen(0), d_namlen(0)
		{
			for(size_t i=0;i<256;i++)
			{
				d_name[i] = 0;
			}
		}
		word_t d_ino;			//file number of entry
		half_t d_reclen;		//length of this record
		half_t d_namlen;		//length of string in d_name
		char d_name[256];		//DUMMY NAME LENGTH, the real maximum length is
						//returned by pathconf(). At this time, this MUST
						//be 256 -- the kernel requires it
};

//open(2) flags for Alpha/AXP OSF target, syscall.c automatically maps
//between these codes to/from host open(2) flags
#define OSF_O_RDONLY		0x0000
#define OSF_O_WRONLY		0x0001
#define OSF_O_RDWR		0x0002
#define OSF_O_NONBLOCK		0x0004
#define OSF_O_APPEND		0x0008
#define OSF_O_CREAT		0x0200
#define OSF_O_TRUNC		0x0400
#define OSF_O_EXCL		0x0800
#define OSF_O_NOCTTY		0x1000
#define OSF_O_SYNC		0x4000

//open(2) flags translation table for SimpleScalar target
class osf_flag_table_object
{
	public:
//		osf_flag_table_object()
//		: osf_flag(0), local_flag(0)
//		{}
		int osf_flag;
		int local_flag;
} osf_flag_table[] =
{
//{target flag,	host flag}
	{ OSF_O_RDONLY,		O_RDONLY },
	{ OSF_O_WRONLY,		O_WRONLY },
	{ OSF_O_RDWR,		O_RDWR },
	{ OSF_O_APPEND,		O_APPEND },
	{ OSF_O_CREAT,		O_CREAT },
	{ OSF_O_TRUNC,		O_TRUNC },
	{ OSF_O_EXCL,		O_EXCL },
	{ OSF_O_NONBLOCK,	O_NONBLOCK },
	{ OSF_O_NOCTTY,		O_NOCTTY },
#ifdef O_SYNC
	{ OSF_O_SYNC,		O_SYNC },
#endif
};
#define OSF_NFLAGS	(sizeof(osf_flag_table)/sizeof(osf_flag_table[0]))

unsigned long sigmask = 0;

qword_t sigaction_array[OSF_NSIG] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//setsockopt option names
#define OSF_SO_DEBUG		0x0001
#define OSF_SO_ACCEPTCONN	0x0002
#define OSF_SO_REUSEADDR	0x0004
#define OSF_SO_KEEPALIVE	0x0008
#define OSF_SO_DONTROUTE	0x0010
#define OSF_SO_BROADCAST	0x0020
#define OSF_SO_USELOOPBACK	0x0040
#define OSF_SO_LINGER		0x0080
#define OSF_SO_OOBINLINE	0x0100
#define OSF_SO_REUSEPORT	0x0200

//This is probably handled incorrectly with SO_BINDTODEVICE
#define OSF_SO_RESVPORT		0x100000

xlate_table_t sockopt_map[] =
{
	{ OSF_SO_DEBUG,		SO_DEBUG },
#ifdef SO_ACCEPTCONN
	{ OSF_SO_ACCEPTCONN,	SO_ACCEPTCONN },
#endif
	{ OSF_SO_REUSEADDR,	SO_REUSEADDR },
	{ OSF_SO_KEEPALIVE,	SO_KEEPALIVE },
	{ OSF_SO_DONTROUTE,	SO_DONTROUTE },
	{ OSF_SO_BROADCAST,	SO_BROADCAST },
#ifdef SO_USELOOPBACK
	{ OSF_SO_USELOOPBACK,	SO_USELOOPBACK },
#endif
	{ OSF_SO_LINGER,	SO_LINGER },
	{ OSF_SO_OOBINLINE,	SO_OOBINLINE },
	{ OSF_SO_RESVPORT,	SO_BINDTODEVICE  },
#ifdef SO_REUSEPORT
	{ OSF_SO_REUSEPORT,	SO_REUSEPORT }
#endif
};

//setsockopt TCP options
#define OSF_TCP_NODELAY		0x01	//don't delay send to coalesce packets
#define OSF_TCP_MAXSEG		0x02	//maximum segment size
#define OSF_TCP_RPTR2RXT	0x03	//set repeat count for R2 RXT timer
#define OSF_TCP_KEEPIDLE	0x04	//secs before initial keepalive probe
#define OSF_TCP_KEEPINTVL	0x05	//seconds between keepalive probes
#define OSF_TCP_KEEPCNT		0x06	//num of keepalive probes before drop
#define OSF_TCP_KEEPINIT	0x07	//initial connect timeout (seconds)
#define OSF_TCP_PUSH		0x08	//set push bit in outbnd data packets
#define OSF_TCP_NODELACK	0x09	//don't delay send to coalesce packets

xlate_table_t tcpopt_map[] =
{
	{ OSF_TCP_NODELAY,	TCP_NODELAY },
	{ OSF_TCP_MAXSEG,	TCP_MAXSEG },
#ifdef TCP_RPTR2RXT
	{ OSF_TCP_RPTR2RXT,	TCP_RPTR2RXT },
#endif
#ifdef TCP_KEEPIDLE
	{ OSF_TCP_KEEPIDLE,	TCP_KEEPIDLE },
#endif
#ifdef TCP_KEEPINTVL
	{ OSF_TCP_KEEPINTVL,	TCP_KEEPINTVL },
#endif
#ifdef TCP_KEEPCNT
	{ OSF_TCP_KEEPCNT,	TCP_KEEPCNT },
#endif
#ifdef TCP_KEEPINIT
	{ OSF_TCP_KEEPINIT,	TCP_KEEPINIT },
#endif
#ifdef TCP_PUSH
	{ OSF_TCP_PUSH,		TCP_PUSH },
#endif
#ifdef TCP_NODELACK
	{ OSF_TCP_NODELACK,	TCP_NODELACK }
#endif
};

//setsockopt IP options (netinet/in.h) -> (bits/in.h)
#define OSF_IP_TOS		0x03	//int; IP type of service and precedence
xlate_table_t ipopt_map[] =
{
	{ OSF_IP_TOS,	IP_TOS }
};

//setsockopt level names
#define OSF_SOL_SOCKET		0xffff		//options for socket level
#define OSF_SOL_IP		0		//dummy for IP
#define OSF_SOL_TCP		6		//tcp
#define OSF_SOL_UDP		17		//user datagram protocol

xlate_table_t socklevel_map[] =
{
	{ OSF_SOL_SOCKET,	SOL_SOCKET },
	{ OSF_SOL_IP,		SOL_IP },
	{ OSF_SOL_TCP,		SOL_TCP },
	{ OSF_SOL_UDP,		SOL_UDP }
};

//socket() address families
#define OSF_AF_UNSPEC		0
#define OSF_AF_UNIX		1		//Unix domain sockets
#define OSF_AF_INET		2		//internet IP protocol
#define OSF_AF_IMPLINK		3		//arpanet imp addresses
#define OSF_AF_PUP		4		//pup protocols: e.g. BSP
#define OSF_AF_CHAOS		5		//mit CHAOS protocols
#define OSF_AF_NS		6		//XEROX NS protocols
#define OSF_AF_ISO		7		//ISO protocols

#define OSF_AF_ROUTE		17		//Internal Routing Protocol
#define OSF_AF_INET6		26		//IPv6: UDP, TCP, etc.

xlate_table_t family_map[] =
{
	{ OSF_AF_UNSPEC,	AF_UNSPEC },
	{ OSF_AF_UNIX,		AF_UNIX },
	{ OSF_AF_INET,		AF_INET },
#ifdef AF_IMPLINK
	{ OSF_AF_IMPLINK,	AF_IMPLINK },
#endif
#ifdef AF_PUP
	{ OSF_AF_PUP,		AF_PUP },
#endif
#ifdef AF_CHAOS
	{ OSF_AF_CHAOS,		AF_CHAOS },
#endif
#ifdef AF_NS
	{ OSF_AF_NS,		AF_NS },
#endif
#ifdef AF_ISO
	{ OSF_AF_ISO,		AF_ISO },
#endif
	{ OSF_AF_ROUTE,		AF_ROUTE },
	{ OSF_AF_INET6,		AF_INET6 }
};

//socket() socket types
#define OSF_SOCK_STREAM		1		//stream (connection) socket
#define OSF_SOCK_DGRAM		2		//datagram (conn.less) socket
#define OSF_SOCK_RAW		3		//raw socket
#define OSF_SOCK_RDM		4		//reliably-delivered message
#define OSF_SOCK_SEQPACKET	5		//sequential packet socket

xlate_table_t socktype_map[] =
{
	{ OSF_SOCK_STREAM,	SOCK_STREAM },
	{ OSF_SOCK_DGRAM,	SOCK_DGRAM },
	{ OSF_SOCK_RAW,		SOCK_RAW },
	{ OSF_SOCK_RDM,		SOCK_RDM },
	{ OSF_SOCK_SEQPACKET,	SOCK_SEQPACKET }
};

//OSF table() calls (most are unsupported)
#define OSF_TBL_PROCINFO	10
#define OSF_TBL_SYSINFO		12
class osf_tbl_sysinfo
{
	public:
		osf_tbl_sysinfo()
		: si_user(0), si_nice(0), si_sys(0), si_idle(0), si_hz(0), si_phz(0), si_boottime(0), wait(0)
		{}
		long si_user;		//user time
		long si_nice;		//nice time
		long si_sys;		//system time
		long si_idle;		//idle time
		long si_hz;
		long si_phz;
		long si_boottime;	//boot time in seconds
		long wait;		//wait time
};
#define OSF_PI_COMLEN		19	//length of command string
class osf_tbl_procinfo
{
	public:
		osf_tbl_procinfo()
		: pi_uid(0), pi_pid(0), pi_ppid(0), pi_pgrp(0), pi_ttyd(0), pi_status(0), pi_flag(0), pi_ruid(0), pi_svuid(0), pi_rgid(0),
			pi_svgid(0), pi_session(0), pi_tpgrp(0), pi_tsession(0), pi_jobc(0), pi_cursig(0), pi_sig(0), pi_sigmask(0),
			pi_sigignore(0), pi_sigcatch(0)
		{
			for(size_t i=0;i<=OSF_PI_COMLEN;i++)
			{
				pi_comm[i] = 0;
			}
		}
		uid_t		pi_uid;		//(effective) user ID
		pid_t		pi_pid;		//proc ID
		pid_t		pi_ppid;	//parent proc ID
		pid_t		pi_pgrp;	//proc group ID
		uid_t		pi_ttyd;	//controlling terminal number (dev_t is too large natively, use uid_t instead)
		int		pi_status;	//process status:
#define OSF_PI_EMPTY		0			//no process
#define OSF_PI_ACTIVE		1			//active process
#define OSF_PI_EXITING		2			//exiting
#define OSF_PI_ZOMBIE		3			//zombie
		int		pi_flag;	//other random flags
		char		pi_comm[OSF_PI_COMLEN+1];
						//short command name
		uid_t		pi_ruid;	//(real) user ID
		uid_t		pi_svuid;	//saved (effective) user ID
		gid_t		pi_rgid;	//(real) group ID
		gid_t		pi_svgid;	//saved (effective) group ID
		pid_t		pi_session;	//session ID
		pid_t		pi_tpgrp;	//tty pgrp
		pid_t		pi_tsession;	//tty session id
		u_int		pi_jobc;	//# procs qualifying pgrp for job control
		int		pi_cursig;
		//the following are sigset_t (which is "quad" on alpha). Doesn't match well at all on linux
		qword_t		pi_sig;		//signals pending
		qword_t		pi_sigmask;	//current signal mask
		qword_t		pi_sigignore;	//signals being ignored
		qword_t		pi_sigcatch;	//signals being caught by user
};

class osf_utsname
{
	public:
//		osf_utsname()
//		{
//			for(size_t i=0;i<32;i++)
//			{
//				sysname[i] = nodename[i] = release[i] = version[i] = machine[i] = 0;
//			}
//		}
		char sysname[32];
		char nodename[32];
		char release[32];
		char version[32];
		char machine[32];
};

osf_utsname utsname_data = {"OSF1", "tru64", "V5.1", "2650", "alpha"};

#ifdef SYS_DEBUG
void sys_output(const char * format, ...)
{
	va_list v;
	va_start(v, format);

	char buf[MAXBUFSIZE];
	int size = vsprintf(buf,format,v);
	if(size>=MAXBUFSIZE)
	{
		std::cerr << "sys_output may have buffer overflowed (size: " << size << " avail: " << MAXBUFSIZE << ")" << std::endl;
		assert(size<MAXBUFSIZE);
	}
	std::cerr << buf;
	va_end(v);
}
#else
inline void sys_output(const char * ignore, ...)
{}
#endif

//Since we have to acquire and fix the filename in various places, we should functionalize it
std::string get_filename(mem_t* mem, md_addr_t addr)
{
	std::string filename;
	mem->mem_strcpy(Read, addr, filename);

	//Currently, mem_strcpy copies the NULL character into the string. This ends up inserted into filename.
	//Remove a trailing NULL, if it exists
	if(*filename.rbegin() == 0)
	{
		filename.erase(filename.end()-1);
	}

#ifdef SLASH_FIX
	if((filename[0]=='/') && (filename[1]=='/'))
	{
		int start = 2;
		while(filename[start]=='/')
		{
			start++;
		}
		filename = filename.substr(start);
		sys_output("(path fixed) ");
	}
	while(filename.find("//")!=std::string::npos)
	{
		//sys_output("Trying to fix bad path: %s \t",filename.c_str());
		filename.replace(filename.find("//"),2,"/");
		sys_output("(path fixed) ");
	}
#endif

#ifdef ALPHA_LIB
	static std::string replace = "../sysfiles/alpha-sys-root";

	//This is meant to redirect Tru64 OS files
	//	These have been required by Spec2K and Spec2K6 (non_shared versions)
	//	/usr/lib/nls/msg/C/libc.cat
	//	/usr/share/.msg_conv-C
	//	/etc/zoneinfo/localtime
	//	/etc/zoneinfo/GMT flags
	//	/etc/zoneinfo/posixrules
	//	/usr/lib/nls/msg/en_US.UTF-8/for_msg.cat
	//	/usr/share/.msg_conv-en_US.UTF-8
	//	/usr/lib/nls/msg/en_US.ISO8859-1/for_msg.cat
	//This now allows us to find shared object files and other relevant files

	if((filename.substr(0,5)=="/usr/") || (filename.substr(0,5)=="/etc/") || (filename.substr(0,6)=="/sbin/") || (filename.substr(0,5)=="/lib/") || (filename.substr(0,5)=="/var/") || (filename.substr(0,5)=="/bin/"))
	{
		filename = replace + filename;
	}

	//This handles the problem with povray: SPEC-benchmark.tga
	if(filename.find("SPEC-benchmark.tga")!=std::string::npos)
	{
		filename = "./SPEC-benchmark.tga";
	}
#endif
	return filename;
}

#define PAL_clrfen	174	//0xae
#define PAL_gentrap	170	//0xaa
#define PAL_rduniq	158	//0x9e
#define PAL_wruniq	159	//0x9f
#define PAL_urti	146	//0x92 == VMS PAL_rei
#define PAL_bpt		128	//0x80
#define PAL_bugchk	129	//0x81
#define PAL_chmk	131	//0x83	syscall	- already implemented
#define PAL_callsys	131	//0x83	syscall - already implemented
#define PAL_imb		134	//0x86	instruction memory barrier
#define PAL_halt	0	//0x00
#define PAL_draina	2	//0x02
#define PAL_nphalt	190	//0xbe
#define PAL_cobratt	9	//0x9
#define PAL_cserve	9	//0x9 same as cobratt
#define PAL_ipir	13	//0xd
#define PAL_cflush	1	//0x01
#define PAL_rti		63	//0x3f	Return from trap, fault or instruction
#define PAL_wtint	62	//0x3e
#define PAL_rtsys	61	//0x3d	This is probably return from system.
#define PAL_whami	60	//0x3c
#define PAL_rdusp	58	//0x3a	returns user stack pointer in v0
#define PAL_wrperfmon	57	//0x39
#define PAL_wrusp	56	//0x38	writes user stack pointer.
#define PAL_wrkgp	55	//0x37	write kernel global pointer
#define PAL_rdps	54	//0x36
#define PAL_swpipl	53	//0x35
#define PAL_wrent	52	//0x34	write system entry
#define PAL_tbi		51	//0x33
#define PAL_rdval	50	//0x32
#define PAL_wrval	49	//0x31
#define PAL_swpctx	48	//0x30	Swap context - process registers are stored in control block. Another process is loaded as indicated by a0. Old control block is returned in v0. control block base register is set to new register.
#define PAL_jtopal	46	//0x2e
#define PAL_wrvptptr	45	//0x2d
#define PAL_wrfen	43	//0x2b
#define PAL_mtpr_mces	17	//0x11
#define PAL_rdmces	16	//0x10 -read machine check err summary reg
#define PAL_swppal	10	//0xa -swap PALcode image
//palcall proxy handler, architectural registers and memory are assumed to be
//precise when this function is called, register and memory are updated with
//the results of the palcall
void sys_palcall(regs_t *regs,		//registers to access
	mem_t *mem,			//memory space to access
	md_inst_t inst)			//palcall inst
{
#define arg(X)	regs->regs_R[MD_REG_##X]
	qword_t palcode = inst;

	int context_id = regs->context_id;
	sys_output("%llx(#%lld): ",regs->regs_PC,contexts[context_id].fastfwd_left);
	sys_output("PALcall(%d): %lld\t",context_id,palcode);

	switch(palcode)
	{
	case PAL_rduniq:
		arg(V0) = regs->regs_C.uniq;
		sys_output("OSF_PAL_rduniq: Read(%llx) ", arg(V0));
		break;
	case PAL_wruniq:
		regs->regs_C.uniq = arg(A0);
		sys_output("OSF_PAL_wruniq: SetUniq_with(%llx) ", arg(A0));
		break;
	case PAL_wrkgp:
		sys_output("OSF_PAL_wrkgp (unsupported): Setting kgp to(%llx) ", arg(A0));
		//regs->regs_R[MD_REG_GP] = arg(A0);
		//Set kgp here
		break;
	case PAL_wrent:
		sys_output("OSF_PAL_wrent (unsupported): entry_point(%llx) entry_val(%llx)", arg(A0), arg(A1));
		break;
	case PAL_imb:
		sys_output("OSF_PAL_imb: Does nothing?");
		break;
	default:
		warn("invalid/unimplemented palcall %ld, PC=0x%08p, winging it",palcode, regs->regs_PC);
		sys_output("Args: 0(%lld) 1(%lld) 2(%lld) 3(%lld) 4(%lld) 5(%lld)\n",arg(A0),arg(A1),arg(A2),arg(A3),arg(A4),arg(A5));
		sys_output("Args: 0(0x%llx) 1(0x%llx) 2(0x%llx) 3(0x%llx) 4(0x%llx) 5(0x%llx)\n",arg(A0),arg(A1),arg(A2),arg(A3),arg(A4),arg(A5));
		break;
	}
	sys_output("\n");
}

//OSF SYSCALL -- standard system call sequence
//the kernel expects arguments to be passed with the normal C calling
//sequence; v0 should contain the system call number; on return from the
//kernel mode, a3 will be 0 to indicate no error and non-zero to indicate an
//error; if an error occurred v0 will contain an errno; if the kernel returns
//an error, setup a valid gp and jump to _cerror

//syscall proxy handler, architectural registers and memory are assumed to be
//precise when this function is called, register and memory are updated with
//the results of the system call
void sys_syscall(regs_t *regs,		//registers to access
	mem_t *mem,			//memory space to access
	md_inst_t inst)			//system call inst
{
//This macro allows us to use shorthand for accessing the registers.
//The main registers we access here are A0, A1, A2, A3, A4, A5 (the 6 argument registers) and V0 (return register)
//In general, V0 is the return register and A3 is used to store errno (when applicable), this is reserved in some syscalls
#define arg(X)	regs->regs_R[MD_REG_##X]

	qword_t syscode = arg(V0);
	int context_id = regs->context_id;

	//fix for syscall() which uses CALL_PAL CALLSYS for making system calls
	if(syscode == OSF_SYS_syscall)
	{
		sys_output("CALL_PAL CALLSYS using MD_REG_A0 used\n");
		syscode = arg(A0);
	}

	sys_output("(%lld)Syscall(%d): %lld\t",contexts[context_id].sim_num_insn,context_id,syscode);

	//Most of the "check if an error occurred code" is duplicated. We just flag if we should do the check and do it later.
	bool check_error = false;

	//Preprocess, we want to fix input/output redirection here
	bool redirected = false;
	md_gpr_t oldA0 = arg(A0);
	switch(syscode)
	{
	case OSF_SYS_read:
	case OSF_SYS_write:
	case OSF_SYS_lseek:
	case OSF_SYS_ioctl:
	case OSF_SYS_fstat:
	case OSF_SYS_fstat64:
	case OSF_SYS_linux_fstat64:
	case OSF_SYS_stat:
	case OSF_SYS_stat64:
	case OSF_SYS_linux_stat64:
	case OSF_SYS_lstat:
	case OSF_SYS_lstat64:
//	case OSF_SYS_linux_lstat64:
	case OSF_SYS_ftruncate:
//	case OSF_SYS_close:
	case OSF_SYS_fpathconf:
	case OSF_SYS_fcntl:
	case OSF_SYS_flock:
	case OSF_SYS_getdirentries:
	case OSF_SYS_setsockopt:
	case OSF_SYS_getsockopt:
	case OSF_SYS_send:
	case OSF_SYS_sendto:
	case OSF_SYS_connect:
	case OSF_SYS_getsockname:
	case OSF_SYS_shutdown:
	case OSF_SYS_fsync:
		redirected = contexts[context_id].file_table.require_redirect(arg(A0));
		if(redirected)
		{
			sys_output("(redirecting %lld to %lld) ",oldA0,arg(A0));
		}
	}


	//no, OK execute the live system call...
	switch(syscode)
	{
	case OSF_SYS_exit:
		fprintf(stderr,"(%lld) Received syscode OSF_SYS_exit with status: %lld, exiting\n",contexts[context_id].pid,arg(A0));

		pid_handler.kill_pid(contexts[context_id].pid, arg(A0));

		//This might cause the instruction count to be off by 1.
		cores[contexts[context_id].core_id].flushcontext(contexts[context_id],contexts[context_id].sim_num_insn);
		cores[contexts[context_id].core_id].ejectcontext(contexts[context_id]);
		contexts[context_id].file_table.closeall();
		contexts[context_id].pid = 0;
		ejected_contexts.push_back(contexts[context_id]);
		break;

	case OSF_SYS_exit_group:
		fprintf(stderr,"(%lld) Received syscode OSF_SYS_exit_group with status: %lld, exiting\n",contexts[context_id].pid,arg(A0));

		pid_handler.kill_pid(contexts[context_id].pid, arg(A0));

		//This might cause the instruction count to be off by 1.
		cores[contexts[context_id].core_id].flushcontext(contexts[context_id],contexts[context_id].sim_num_insn);
		cores[contexts[context_id].core_id].ejectcontext(contexts[context_id]);
		contexts[context_id].file_table.closeall();
		contexts[context_id].pid = 0;
		ejected_contexts.push_back(contexts[context_id]);
		break;

	case OSF_SYS_read:
		{
			char *buf = new char[arg(A2)];

			sys_output("OSF_SYS_read(from %lld):\t",arg(A0));
			//read data from file
			md_gpr_t so_far = 0;
			do
			{
				so_far += arg(V0) = read(arg(A0)+so_far,&buf[so_far],arg(A2)-so_far);
			} while ((arg(V0) == (md_gpr_t)-1) && (errno == EAGAIN));
			if(arg(V0) != (md_gpr_t)-1)
			{
				arg(V0) = so_far;
			}

			//copy results back into host memory (only copy what we got)
			if(arg(V0) > 0)
			{
				mem->mem_bcopy(Write, arg(A1), buf, arg(V0));
			}

			sys_output("Read: %lld\tTried to read: %lld\tinto location 0x%llx",arg(V0),arg(A2),arg(A1));

			check_error = true;
			delete [] buf;
		}
		break;

	case OSF_SYS_write:
		{
			char *buf = new char[arg(A2)];

			//copy inputs into host memory
			mem->mem_bcopy(Read, /*buf*/arg(A1), buf, arg(A2));

			//write data
			do
			{
				arg(V0) = write(arg(A0), buf, arg(A2));
			} while ((arg(V0) == (qword_t)-1) && (errno == EAGAIN));

			//check for an error condition
			if(arg(V0) == arg(A2))
			{
				arg(A3) = 0;
			}
			else	// got an error, return details
			{
				arg(A3) = -1;
				arg(V0) = errno;
			}

			sys_output("OSF_SYS_write: to %lld\tWrote: %lld\tTried to write: %lld\tinto location 0x%llx",arg(A0),arg(V0),arg(A2),arg(A1));
			delete [] buf;
		}
		break;

	//ADDED BY CALDER 10/27/99
	case OSF_SYS_getdomainname:
		{
			//No getdomainname system call on linux, instead return from uname
			arg(V0) = 6;
			char *buf = utsname_data.nodename;

			mem->mem_bcopy(Write, arg(A0), buf, MIN(arg(A1),sizeof(utsname_data.nodename)));

			check_error = true;
			sys_output("OSF_SYS_getdomainname: into(0x%llx) len(%lld) Received(%s)\t",arg(A0),arg(A1),buf);
		}
	 	break;

	//ADDED BY CALDER 10/27/99
	case OSF_SYS_flock:		//get flock() information on the file
		{
			arg(V0) = flock((int)arg(A0), (int)arg(A1));
			check_error = true;
			sys_output("OSF_SYS_flock: file(%lld) return(%lld) command(%lld)",arg(A0),arg(A3),arg(A1));
		}
		break;

	//ADDED BY CALDER 10/27/99
	case OSF_SYS_bind:
		{
			sockaddr a_sock;
			mem->mem_bcopy(Read, /* serv_addr */arg(A1), (void *)&a_sock, /* addrlen */(int)arg(A2));
			arg(V0) = bind((int) arg(A0),&a_sock,(int) arg(A2));

			check_error = true;
			sys_output("OSF_SYS_bind: Socket(%lld) return(%lld) socket_data_len(%lld) host_socket_data_len(%lld)",arg(A0),arg(A3),arg(A2),sizeof(sockaddr));
		}
		break;

	//ADDED BY CALDER 10/27/99
	case OSF_SYS_sendto:
		{
			sys_output("OSF_SYS_sendto: fd(%lld) message_addr(0x%llx) len(%lld) flags(%lld) sockaddr_dest*(0x%llx) sock_len(%lld) ",arg(A0),arg(A1),arg(A2),arg(A3),arg(A4),arg(A5));

			char *buf = NULL;
			sockaddr d_sock;

			int buf_len = arg(A2);
			if(buf_len > 0)
			{
				buf = new char[buf_len*sizeof(char)];
			}

			mem->mem_bcopy(Read, arg(A1), buf, buf_len);

			if(arg(A5) > 0)
			{
//				mem->mem_bcopy(Read, arg(A4), &d_sock, arg(A5));
				mem->mem_bcopy(Read, arg(A4), &d_sock, sizeof(sockaddr));
			}

			sys_output("sa_family(%d) sa_data(%s)\t",d_sock.sa_family,d_sock.sa_data);

//			arg(V0) = sendto(arg(A0), buf, buf_len, (int)arg(A3), &d_sock, arg(A5));
			arg(V0) = sendto(arg(A0), buf, buf_len, (int)arg(A3), &d_sock, sizeof(sockaddr));
			mem->mem_bcopy(Write, /* serv_addr */arg(A1), buf, buf_len);

			//maybe copy back whole size of sockaddr
			if(arg(A5) > 0)
			{
//				mem->mem_bcopy(Write, arg(A4), &d_sock, (int)arg(A5));
				mem->mem_bcopy(Write, arg(A4), &d_sock, sizeof(sockaddr));
			}

			check_error = true;
			delete [] buf;
		}
		break;

	case OSF_SYS_send:
		{
			sys_output("OSF_SYS_send: fd(%lld) message_addr(0x%llx) len(%lld) flags(%lld) ",arg(A0),arg(A1),arg(A2),arg(A3));

			char *buf = NULL;
			int buf_len = arg(A2);
			if(buf_len > 0)
			{
				buf = new char[buf_len*sizeof(char)];
				mem->mem_bcopy(Read, arg(A1), buf, buf_len);
			}

			arg(V0) = send(arg(A0), buf, buf_len, (int)arg(A3));
			mem->mem_bcopy(Write, /* serv_addr */arg(A1), buf, buf_len);

			check_error = true;
			delete [] buf;
		}
		break;


	//ADDED BY CALDER 10/27/99
	case OSF_SYS_old_recvfrom:
	case OSF_SYS_recvfrom:
		{
			int addr_len;
			sockaddr *a_sock;
			char *buf = new char[sizeof(char)*arg(A2)];

			mem->mem_bcopy(Read, /* serv_addr */arg(A1), buf, /* addrlen */(int)arg(A2));
			mem->mem_bcopy(Read, /* serv_addr */arg(A5), &addr_len, sizeof(int));

			a_sock = new sockaddr[addr_len/sizeof(sockaddr)];
			mem->mem_bcopy(Read, arg(A4), a_sock, addr_len);

			arg(V0) = recvfrom((int)arg(A0), buf, (int)arg(A2), (int)arg(A3), a_sock,(socklen_t *)&addr_len);

			mem->mem_bcopy(Write, arg(A1), buf, (int) arg(V0));
			mem->mem_bcopy(Write, /* serv_addr */arg(A5), &addr_len, sizeof(int));
			mem->mem_bcopy(Write, arg(A4), a_sock, addr_len);
			check_error = true;
			delete [] buf;
		}
		break;

	case OSF_SYS_open:
		{
			int osf_flags = arg(A1), local_flags = 0;

			//translate open(2) flags
			for(unsigned int i=0; i < OSF_NFLAGS; i++)
			{
				if(osf_flags & osf_flag_table[i].osf_flag)
				{
					//We probably don't need to clear the bit from osf_flags. There shouldn't be any overlap.
					osf_flags &= ~osf_flag_table[i].osf_flag;
					local_flags |= osf_flag_table[i].local_flag;
				}
			}
			//any target flags left?
			if(osf_flags != 0)
			{
				fatal("syscall: open: cannot decode flags: 0x%08x", osf_flags);
			}

			std::string filename = get_filename(mem,arg(A0));
			sys_output("OSF_SYS_open: %s flags: %d(orig: %lld) mode: %lld",filename.c_str(),local_flags,arg(A1),arg(A2));

#ifdef CATCH_TTY
			if(filename == "/dev/tty")
			{
				arg(V0) = contexts[context_id].file_table.duper(1);
				check_error = true;
				break;
			}
#endif
			//FORRT1 Check
			//When trying to generate working directory information, getwd (legacy, is contained in SPEC2K)
			//traverses backwards to generate the pathname. However, if permissions are blocked, it will generate an error.
			//For some reason, the error message is used to try to open the file.
			if(filename.find("getwd: can't open ../")!=std::string::npos)
			{
				fprintf(stderr,"Warning: getwd failed, make sure you can read all directories that are part of the path\n");
			}

			//open the file
			arg(V0) = contexts[context_id].file_table.opener(filename, local_flags, arg(A2));
			check_error = true;
		}
		break;

	case OSF_SYS_close:
		sys_output("OSF_SYS_close: Closing file %lld",arg(A0));
		arg(V0) = contexts[context_id].file_table.closer(arg(A0));
		check_error = true;
		break;

#if 0
	//FIXME: This may be legacy but isn't supported anywhere else (this should be translated into an open)
	//Equivalent to open(arg(A0), O_WRONLY | O_CREAT | O_TRUNC, arg(A1)
	case OSF_SYS_creat:
		{
			std::string filename = get_filename(mem,arg(A0));
			arg(V0) = creat(filename.c_str(), /*mode*/arg(A1));
			check_error = true;
		}
		break;
#endif

	case OSF_SYS_link:
		{
			std::string filename = get_filename(mem,arg(A0));
			std::string filename2 = get_filename(mem,arg(A1));
			arg(V0) = link(filename.c_str(),filename2.c_str());
			check_error = true;
			sys_output("OSF_SYS_link: %s -> %s\t",filename.c_str(),filename2.c_str());
		}
		break;

	case OSF_SYS_unlink:
		{
			std::string filename = get_filename(mem,arg(A0));
			arg(V0) = unlink(filename.c_str());
			check_error = true;
			sys_output("OSF_SYS_unlink: %s\t",filename.c_str());
		}
		break;

	case OSF_SYS_chdir:
		{
			std::string filename = get_filename(mem,arg(A0));
			arg(V0) = chdir(filename.c_str());
			check_error = true;
			sys_output("OSF_SYS_chdir: %s\t",filename.c_str());
		}
		break;

	case OSF_SYS_chmod:
		{
			std::string filename = get_filename(mem,arg(A0));
			arg(V0) = chmod(filename.c_str(), arg(A1));
			check_error = true;
			sys_output("OSF_SYS_chmod: (%s) mod: %o\t",filename.c_str(),arg(A1));
		}
		break;

    case OSF_SYS_chown:
		{
			std::string filename = get_filename(mem,arg(A0));
			arg(V0) = chown(filename.c_str(), /*owner*/arg(A1), /*group*/arg(A2));
			check_error = true;
		}
		break;

	case OSF_SYS_sbrk:
		{
			sqword_t delta = arg(A0);
			md_addr_t addr = mem->ld_brk_point + delta;

			if(verbose)
			{
				myfprintf(stderr, "SYS_sbrk: delta: 0x%012p (%ld)\n", delta, delta);
			}

			mem->ld_brk_point = addr;
			arg(V0) = mem->ld_brk_point;
			check_error = true;
			sys_output("OSF_SYS_obreak (or deprecated OSF_SYS_sbrk): 0x%llx",addr);

			if(verbose)
			{
				myfprintf(stderr, "mem->ld_brk_point: 0x%012p\n", mem->ld_brk_point);
			}
#if 0
			//check whether heap area has merged with stack area
			if(/* addr >= mem->ld_brk_point && */ addr < regs->regs_R[MD_REG_SP])
			{
				arg(A3) = 0;
				mem->ld_brk_point = addr;
			}
			else
			{
				 //out of address space, indicate error
				arg(A3) = -1;
			}
#endif
		}
		break;

	case OSF_SYS_obreak:
		{
			//http://kerneltrap.org/man/linux/man2/brk.2
			if(arg(A0)==0)
			{
				arg(V0) = mem->ld_brk_point;
				arg(A3) = 0;
			}
			else
			{
				mem->ld_brk_point = arg(A0);
//				if(arg(A0) < mem->ld_brk_point)
//				{
//					mem->ld_brk_point += arg(A0);
//				}
//				else
//				{
//					mem->ld_brk_point = arg(A0);
//				}

				arg(V0) = 0;
//				arg(V0) = mem->ld_brk_point;
				arg(A3) = 0;
			}
			check_error = true;
			sys_output("OSF_SYS_obreak: 0x%llx",arg(A0));
		}
		break;

	case OSF_SYS_lseek:
		arg(V0) = lseek(/*fd*/arg(A0), /*off*/arg(A1), /*dir*/arg(A2));
//		arg(V0) = contexts[context_id].file_table.lseeker(oldA0,arg(A1),arg(A2));
		check_error = true;
		sys_output("OSF_SYS_lseek: fd(%lld) off(%lld) dir(%lld)\t",arg(A0),arg(A1),arg(A2));
		break;

	case OSF_SYS_getpid:
		//Now, return the context's pid
		arg(V0) = contexts[context_id].pid;
		check_error = true;
		sys_output("OSF_SYS_getpid: %lld",arg(V0));
		break;

	case OSF_SYS_setpgid:
		arg(V0) = arg(A3) = 0;
		if(arg(A0) == 0)
		{
			//set group id to arg(A1)
			contexts[context_id].gpid = arg(A1);
		}
		else if(arg(A1) == 0)
		{
			//set group id of arg(A0) to the group id of caller
			bool found = false;
			size_t index = 0;
			for(size_t i=0;(!found) && (i<contexts.size());i++)
			{
				found = (contexts[i].pid == arg(A0));
				index = i;
			}
			if(!found)
			{
				arg(A3) = EPERM;
				arg(V0) = -1;
			}
			else
			{
				contexts[index].gpid = contexts[context_id].gpid;
			}
		}
		check_error = true;
		sys_output("OSF_SYS_setpgid: pid(0x%llx), pgid(0x%llx)",arg(A0),arg(A1));
		break;

	case OSF_SYS_getuid:
		//get current user id
		arg(V0) = getuid();	//first result
		arg(A4) = geteuid();	//second result
		sys_output("OSF_SYS_getuid: %lld\t%lld",arg(V0),arg(A4));
		check_error = true;
		break;

	case OSF_SYS_access:
		{
			std::string filename = get_filename(mem,arg(A0));
			//check access on the file (only look at bottom 3 bits, alpha uses 0x8 for something unnecessary)
			arg(V0) = access(filename.c_str(), arg(A1) & 0x7);
			check_error = true;
			sys_output("OSF_SYS_access: %s\tMode(%lld)\t",filename.c_str(),arg(A1) & 0x7);
		}
		break;

	case OSF_SYS_stat:
	case OSF_SYS_lstat:
		{
			osf_statbuf osf_sbuf;
			class stat sbuf;
			std::string filename = get_filename(mem,arg(A0));
			check_error = true;

			//stat() the file
			if(syscode == OSF_SYS_stat)
			{
				sys_output("OSF_SYS_stat: Trying to stat: %s",filename.c_str());
				arg(V0) = stat(filename.c_str(), &sbuf);
			}
			else
			{
				sys_output("OSF_SYS_lstat: Trying to stat: %s",filename.c_str());
				arg(V0) = lstat(filename.c_str(), &sbuf);
			}

			//translate from host stat structure to target format
			osf_sbuf.osf_st_dev = MD_SWAPW(sbuf.st_dev);
			osf_sbuf.osf_st_ino = MD_SWAPW(sbuf.st_ino);
			osf_sbuf.osf_st_mode = MD_SWAPW(sbuf.st_mode);
			osf_sbuf.osf_st_nlink = MD_SWAPH(sbuf.st_nlink);
			osf_sbuf.osf_st_uid = MD_SWAPW(sbuf.st_uid);
			osf_sbuf.osf_st_gid = MD_SWAPW(sbuf.st_gid);
			osf_sbuf.osf_st_rdev = MD_SWAPW(sbuf.st_rdev);
			osf_sbuf.osf_st_size = MD_SWAPQ(sbuf.st_size);
			osf_sbuf.osf_st_atime = MD_SWAPW(sbuf.st_atime);
			osf_sbuf.osf_st_mtime = MD_SWAPW(sbuf.st_mtime);
			osf_sbuf.osf_st_ctime = MD_SWAPW(sbuf.st_ctime);
			osf_sbuf.osf_st_blksize = MD_SWAPW(sbuf.st_blksize);
			osf_sbuf.osf_st_blocks = MD_SWAPW(sbuf.st_blocks);

			//copy stat() results to simulator memory
			mem->mem_bcopy(Write, arg(A1), &osf_sbuf, sizeof(osf_statbuf));
		}
		break;

	case OSF_SYS_stat64:
	case OSF_SYS_linux_stat64:
	case OSF_SYS_lstat64:
//	case OSF_SYS_linux_lstat64:
		{
			std::string filename = get_filename(mem,arg(A0));
			osf_statbuf64 osf_sbuf;
			class stat64 sbuf;
			check_error = true;

			if((syscode == OSF_SYS_stat64) || (syscode == OSF_SYS_linux_stat64))
			{
				sys_output("OSF_SYS_stat64: Trying to stat: %s",filename.c_str());
				arg(V0) = stat64(filename.c_str(), &sbuf);
			}
			else
			{
				sys_output("OSF_SYS_lstat64: Trying to stat: %s",filename.c_str());
				arg(V0) = lstat64(filename.c_str(), &sbuf);
			}

			//translate from host stat structure to target format
			osf_sbuf.osf_st_dev = MD_SWAPW(sbuf.st_dev);
			osf_sbuf.osf_st_ino = MD_SWAPW(sbuf.st_ino);
			osf_sbuf.osf_st_mode = MD_SWAPW(sbuf.st_mode);
			osf_sbuf.osf_st_nlink = MD_SWAPH(sbuf.st_nlink);
			osf_sbuf.osf_st_uid = MD_SWAPW(sbuf.st_uid);
			osf_sbuf.osf_st_gid = MD_SWAPW(sbuf.st_gid);
			osf_sbuf.osf_st_rdev = MD_SWAPW(sbuf.st_rdev);
			osf_sbuf.osf_st_size = MD_SWAPQ(sbuf.st_size);
			osf_sbuf.osf_st_atime = MD_SWAPW(sbuf.st_atime);
			osf_sbuf.osf_st_mtime = MD_SWAPW(sbuf.st_mtime);
			osf_sbuf.osf_st_ctime = MD_SWAPW(sbuf.st_ctime);
			osf_sbuf.osf_st_blksize = MD_SWAPW(sbuf.st_blksize);
			osf_sbuf.osf_st_blocks = MD_SWAPW(sbuf.st_blocks);

			//copy stat() results to simulator memory
			mem->mem_bcopy(Write, arg(A1), &osf_sbuf, sizeof(osf_statbuf));
		}
		break;

	case OSF_SYS_dup:
		sys_output("OSF_SYS_dup: dup_this(%lld)\t",arg(A0));
		arg(V0) = contexts[context_id].file_table.duper(arg(A0));
		check_error = true;
		break;

	case OSF_SYS_pipe:
		{
			int fd[2];
			arg(V0) = pipe(fd);
			fd[0] = (int)contexts[context_id].file_table.insert(fd[0],"PIPE_0");
			fd[1] = (int)contexts[context_id].file_table.insert(fd[1],"PIPE_1");
			check_error = true;

			//copy pipe descriptors to host memory
			//We almost certainly don't need this. 2 and 3 insts afterwards the values from V0 and A4 are copied into "fd"
			mem->mem_bcopy(Write, arg(A0), fd, sizeof(fd));

			arg(A3) = 0;			//Return value
			arg(V0) = fd[0];		//pipe 1
			arg(A4) = fd[1];		//pipe 2
			check_error = false;
			sys_output("OSF_SYS_pipe: filedes(0x%llx)\tPipes: %lld, %lld\t",arg(A0),fd[0],fd[1]);
		}
		break;

	case OSF_SYS_getpgrp:
//		arg(V0) = getpgrp();		//first result
		arg(V0) = contexts[context_id].gpid;
		check_error = true;
		sys_output("OSF_SYS_getpgrp: %lld\t",arg(V0));
		break;

	case OSF_SYS_getgid:
		//get current group id
//		arg(V0) = getgid();		//first result
//		arg(A4) = getegid();		//second result
		arg(V0) = contexts[context_id].gid;		//first result
		arg(A4) = contexts[context_id].gid;		//second result, this is copied to V0 by the syscall wrapper but we can't guarantee it will be used.
		check_error = true;
		sys_output("OSF_SYS_getgid: grp(%lld eff(%lld))\t",arg(V0),arg(A4));
		break;

	case OSF_SYS_ioctl:
		{
			int ioctl_call = (int)arg(A1);
			sys_output("OSF_SYS_ioctl: fd(%lld) ioctl_op(0x%x)\t",arg(A0),ioctl_call);
			switch(ioctl_call)
			{
			case OSF_TIOCISATTY:
				sys_output("TIOCISATTY\t");
				//check_error = true;
				//FIXME: We should call file_table (on oldA0) to check if the fd is a standard I/O
				//if it isn't, should be call isatty? (should file_table call it?)
				arg(A3) = !contexts[context_id].file_table.istty(oldA0);
				if(arg(A3))
				{
					arg(V0) = ENOTTY;
					sys_output("is not a tty\t");
				}
				else
				{
					arg(V0) = 0;
					sys_output("is a tty\t");
				}
				break;

			case OSF_TIOCGPGRP:
				{
//					arg(V0) = getpgrp();
					arg(V0) = contexts[context_id].gpid;
//					arg(V0) = 0;
					arg(A3) = 0;
					unsigned int groupval = contexts[context_id].gpid;
					mem->mem_bcopy(Write, arg(A2), &groupval, 4);
//					sys_output("TIOCGPGRP returning group (%d)\t",arg(V0));
					sys_output("TIOCGPGRP addr(0x%llx) grp(0x%x)\t",arg(A2),groupval);
				}
				break;
			case OSF_TIOCSPGRP:
				{
					unsigned int groupval = 0;
					mem->mem_bcopy(Read, arg(A2), &groupval, 4);
					contexts[context_id].gpid = groupval;
					arg(V0) = 0;
					arg(A3) = 0;
					sys_output("TIOCSPGRP addr(0x%llx) grp(0x%x)\t",arg(A2),groupval);
				}
				break;

			case OSF_TIOCGWINSZ:
				{
					osf_winsize buf;
					arg(V0) = ioctl(/* fd */(int)arg(A0),/*req*/TIOCGWINSZ, &buf);
					mem->mem_bcopy(Write, /* buf */arg(A2), &buf, sizeof(osf_winsize));
					check_error = true;
					sys_output("TIOCGWINSZ\tRows(%d) Columns(%d), Pixels(Hsize(%d) Ysize(%d))\t",buf.ws_row,buf.ws_col,buf.ws_xpixel,buf.ws_ypixel);
				}
				break;
			case OSF_TIOCSWINSZ:
				{
					osf_winsize buf;
					mem->mem_bcopy(Read, arg(A2), &buf, sizeof(osf_winsize));
					arg(V0) = ioctl(arg(A0), TIOCSWINSZ, &buf);
					check_error = true;
					sys_output("TIOCSWINSZ (FIXME)\tRows(%d) Columns(%d), Pixels(Hsize(%d) Ysize(%d))\t",buf.ws_row,buf.ws_col,buf.ws_xpixel,buf.ws_ypixel);
				}
				break;

			case OSF_SIOCGIFCONF:
				{
					osf_ifconf osf_ifc;
					mem->mem_bcopy(Read, arg(A2), &osf_ifc, sizeof(osf_ifconf));
					ifconf ifc;
					ifc.ifc_len = osf_ifc.ifc_len;
					ifc.ifc_buf = 0;
					md_gpr_t allocated = (md_gpr_t)osf_ifc.ifc_ifcu.ifcu_buf;
					if(allocated)
					{
						ifc.ifc_req = (ifreq *)new char[ifc.ifc_len];
					}

					arg(V0) = ioctl(arg(A0), SIOCGIFCONF, &ifc);

					if(allocated)
					{
						mem->mem_bcopy(Write, allocated, ifc.ifc_req, ifc.ifc_len);
					}
					osf_ifc.ifc_len = ifc.ifc_len;
					mem->mem_bcopy(Write, arg(A2), &osf_ifc, sizeof(ifconf));
					check_error = true;
					sys_output("SIOCGIFCONF to(0x%llx) retsize(%d) ",arg(A2),ifc.ifc_len);
					if(allocated)
					{
						int bound = ifc.ifc_len / sizeof(ifreq);
						sys_output("Data(0x%llx) ",allocated);
						for(int i=0;i<bound;i++)
						{
							struct ifreq *r = &ifc.ifc_req[i];
							sockaddr_in *sin = (sockaddr_in *)&r->ifr_addr;
							sys_output("(%s - %s) ",r->ifr_name,inet_ntoa(sin->sin_addr));
						}
						sys_output("\t");
						delete [] (char *)ifc.ifc_req;
					}
				}
				break;

			case OSF_TIOCGETA:
				{
					termios tty;
					arg(V0) = tcgetattr(arg(A0), &tty);

					osf_termios dest;
					dest.copy_in(tty);

					mem->mem_bcopy(Write, arg(A2), &dest, sizeof(osf_termios));
					check_error = true;
					sys_output("TIOCGETA at(0x%llx) i(%u) o(%u) c(%u) l(%u) ",arg(A2),dest.c_iflag,dest.c_oflag,dest.c_cflag,dest.c_lflag);
				}
				break;

			case OSF_TIOCSETAW:
				{
					osf_termios tty_source;
					mem->mem_bcopy(Read, arg(A2), &tty_source, sizeof(osf_termios));
					termios tty = tty_source.copy_out();

					arg(V0) = tcsetattr(arg(A0), TCSAFLUSH, &tty);

					check_error = true;
					sys_output("TIOCSETAW at(0x%llx) i(%u) o(%u) c(%u) l(%u) ",arg(A2),tty_source.c_iflag,tty_source.c_oflag,tty_source.c_cflag,tty_source.c_lflag);
				}
				break;

			case OSF_TIOCSETA:
				{
					osf_termios tty_source;
					mem->mem_bcopy(Read, arg(A2), &tty_source, sizeof(osf_termios));
					termios tty = tty_source.copy_out();

					arg(V0) = tcsetattr(arg(A0), TCSADRAIN, &tty);

					check_error = true;
					sys_output("TIOCSETA at(0x%llx) i(%u) o(%u) c(%u) l(%u) ",arg(A2),tty_source.c_iflag,tty_source.c_oflag,tty_source.c_cflag,tty_source.c_lflag);
				}
				break;

			case OSF_TIOCLGET:
				{
					termios tty;
					arg(V0) = tcgetattr(arg(A0), &tty);

					osf_termios dest;
					dest.copy_in(tty);
					int buf = dest.c_lflag;

					mem->mem_bcopy(Write, arg(A2), &buf, sizeof(int));
					check_error = true;
					sys_output("TIOCLGET at(0x%llx) lflag(%u) ",arg(A2),buf);
				}
				break;

			case OSF_TIOCGETP:
				{
					sys_output("TIOCGETP(gtty)\t");

					sgttyb buf;
					arg(V0) = ioctl(arg(A0), TIOCGETP, &buf);
					osf_sgttyb osf_buf;
					osf_buf.copy_in(buf);

					mem->mem_bcopy(Write, arg(A2), &osf_buf, sizeof(osf_sgttyb));
					check_error = true;

					if(arg(V0)==(md_gpr_t)-1)
					{
						if(errno == 38)
						{
							errno = 25;
						}
					}
					sys_output("iSpeed(%u) oSpeed(%u) erase(%d) kill(%u) flags(%u)\t",osf_buf.sg_ispeed,osf_buf.sg_ospeed,osf_buf.sg_erase,osf_buf.sg_kill,osf_buf.sg_flags);
				}
				break;

			case OSF_FIONREAD:
				{
					int nread;
					arg(V0) = ioctl(arg(A0), FIONREAD, &nread);
					mem->mem_bcopy(Write, arg(A2), &nread, sizeof(nread));
					check_error = true;
					sys_output("FIONREAD at(0x%llx)",arg(A2));
				}
				break;

			case OSF_FIONBIO:
				{
					int flags = fcntl(arg(A0), F_GETFL, 0);
					if(arg(A2))
					{
						int temp = 0;
						mem->mem_bcopy(Read, arg(A2), &temp, sizeof(int));
						if(temp)
						{
							flags |= O_NONBLOCK;
						}
						else
						{
							flags &= ~O_NONBLOCK;
						}
						arg(V0) = fcntl(arg(A0), F_SETFL, flags);
					}

					check_error = true;
					sys_output("FIONBIO at(0x%llx) nonblock(%d)\t",arg(A2), !!(flags & O_NONBLOCK));
				}
				break;

			default:
				sys_output("unsupported ioctl call: ioctl(0x%x, ...(%llx) (%llx) (%llx) (%llx) (%llx) (%llx))",ioctl_call,arg(A0),arg(A1),arg(A2),arg(A3),arg(A4),arg(A5));
				arg(A3) = 0;
				break;
			}

#if 0
			{
				char buf[NUM_IOCTL_BYTES];
				int local_req = 0;

				//convert target ioctl() request to host ioctl() request values
				switch (/*req*/arg(A1))
				{
				case SS_IOCTL_TIOCGETP:
					local_req = TIOCGETP;
					break;
				case SS_IOCTL_TIOCSETP:
					local_req = TIOCSETP;
					break;
				case SS_IOCTL_TCGETP:
					local_req = TIOCGETP;
					break;
#ifdef TCGETA
				case SS_IOCTL_TCGETA:
					local_req = TCGETA;
					break;
#endif
#ifdef TIOCGLTC
				case SS_IOCTL_TIOCGLTC:
					local_req = TIOCGLTC;
					break;
#endif
#ifdef TIOCSLTC
				case SS_IOCTL_TIOCSLTC:
					local_req = TIOCSLTC;
					break;
#endif
				case SS_IOCTL_TIOCGWINSZ:
					local_req = TIOCGWINSZ;
					break;
#ifdef TCSETAW
				case SS_IOCTL_TCSETAW:
					local_req = TCSETAW;
					break;
#endif
#ifdef TIOCGETC
				case SS_IOCTL_TIOCGETC:
					local_req = TIOCGETC;
					break;
#endif
#ifdef TIOCSETC
				case SS_IOCTL_TIOCSETC:
					local_req = TIOCSETC;
					break;
#endif
#ifdef TIOCLBIC
				case SS_IOCTL_TIOCLBIC:
					local_req = TIOCLBIC;
					break;
#endif
#ifdef TIOCLBIS
				case SS_IOCTL_TIOCLBIS:
					local_req = TIOCLBIS;
					break;
#endif
#ifdef TIOCLGET
				case SS_IOCTL_TIOCLGET:
					local_req = TIOCLGET;
					break;
#endif
#ifdef TIOCLSET
				case SS_IOCTL_TIOCLSET:
					local_req = TIOCLSET;
					break;
#endif
				}
				if(!local_req)
				{
					//FIXME: could not translate the ioctl() request, just warn user and ignore the request
					warn("syscall: ioctl: ioctl code not supported d=%d, req=%d", arg(A0), arg(A1));
					arg(V0) = 0;
					regs->regs_R[7] = 0;
				}
				else
				{
					//ioctl() code was successfully translated to a host code

					//if arg ptr exists, copy NUM_IOCTL_BYTES bytes to host mem
					if(/*argp*/arg(A2) != 0)
					{
						mem->mem_bcopy(Read, /*argp*/arg(A2), buf, NUM_IOCTL_BYTES);
					}

					//perform the ioctl() call
					/*result*/arg(V0) = ioctl(/*fd*/arg(A0), local_req, buf);

					//if arg ptr exists, copy NUM_IOCTL_BYTES bytes from host mem
					if(/*argp*/arg(A2) != 0)
					{
						mem->mem_bcopy(Write, arg(A2), buf, NUM_IOCTL_BYTES);
					}

					//check for an error condition
					if(arg(V0) != (qword_t)-1)
					{
						regs->regs_R[7] = 0;
					}
					else
					{
						//got an error, return details
						arg(V0) = errno;
						regs->regs_R[7] = 1;
					}
				}
			}
#endif
		}
		break;

	case OSF_MACH_thread_kill:
		{
#ifdef WARN_ALL
			static int first(false);
			if(!first)
			{
				warn("OSF_MACH_thread_kill is unchecked and successful and may not be correct");
				first = !first;
			}
#endif
			arg(V0) =  0;
			check_error = true;
			//FIXME: Must handle if 0, -1, -pid propagation, priviledges.

			long long killwho;
			mem->mem_bcopy(Read, arg(A0), &killwho, sizeof(long long));
			if(killwho<0)
			{
				killwho = -killwho;
			}

			sys_output("OSF_MACH_thread_kill: from(0x%llx), %lld with signal %lld\t",arg(A0),killwho,arg(A1));
			sys_output("Args: 0(0x%llx) 1(0x%llx) 2(0x%llx) 3(0x%llx) 4(0x%llx) 5(0x%llx)\n",arg(A0),arg(A1),arg(A2),arg(A3),arg(A4),arg(A5));
			{
				for(size_t i=0;i<contexts.size();i++)
				{
					if(contexts[i].pid == (unsigned long long)killwho)
					{
						int core_num = contexts[i].core_id;
						cores[core_num].flushcontext(contexts[i],contexts[i].sim_num_insn);
						cores[core_num].ejectcontext(contexts[i]);
						pid_handler.kill_pid(arg(A0),arg(A1));
						fprintf(stderr,"(%lld) Killed by signal %lld\n",killwho,arg(A1));
						contexts[i].pid = 0;
						break;
					}
				}
			}
		}
		break;

	case OSF_SYS_kill:
		{
#ifdef WARN_ALL
			static int first(false);
			if(!first)
			{
				warn("OSF_SYS_kill is unchecked and successful");
				first = !first;
			}
#endif
			arg(V0) =  0;
			check_error = true;
			//FIXME: Must handle if 0, -1, -pid propagation, priviledges.
			long long killwho = arg(A0);
			if(killwho<0)
			{
				killwho = -killwho;
			}

			sys_output("OSF_SYS_kill: %lld with signal %lld\t",killwho,arg(A1));
			{
				for(size_t i=0;i<contexts.size();i++)
				{
					if((killwho == 0) || (contexts[i].pid == (unsigned long long)killwho))
					{
						int core_num = contexts[i].core_id;
						cores[core_num].flushcontext(contexts[i],contexts[i].sim_num_insn);
						cores[core_num].ejectcontext(contexts[i]);
						pid_handler.kill_pid(arg(A0),arg(A1));
						fprintf(stderr,"(%lld) Killed by signal %lld\n",killwho,arg(A1));
						contexts[i].pid = 0;
						break;
					}
				}
			}
		}
		break;

	case OSF_SYS_umask:
		sys_output("OSF_SYS_umask: mask(%o)\t",arg(A0));
		arg(V0) = umask(arg(A0));
		check_error = true;
		break;

	case OSF_SYS_fstat64:
	case OSF_SYS_linux_fstat64:
		{
			osf_statbuf64 osf_sbuf;
			class stat64 sbuf;
			arg(V0) = fstat64(arg(A0), &sbuf);
			check_error = true;

			sys_output("OSF_SYS_fstat64: Reading from(%lld)",arg(A0));

			//translate the stat structure to host format
			osf_sbuf.osf_st_dev = MD_SWAPQ(sbuf.st_dev);
			osf_sbuf.osf_st_ino = MD_SWAPQ(sbuf.st_ino);
			osf_sbuf.osf_st_rdev = MD_SWAPQ(sbuf.st_rdev);
			osf_sbuf.osf_st_size = MD_SWAPQ(sbuf.st_size);
			osf_sbuf.osf_st_mode = MD_SWAPW(sbuf.st_mode);
			osf_sbuf.osf_st_uid = MD_SWAPW(sbuf.st_uid);
			osf_sbuf.osf_st_gid = MD_SWAPW(sbuf.st_gid);
			osf_sbuf.osf_st_nlink = MD_SWAPW(sbuf.st_nlink);
			osf_sbuf.osf_st_atime = MD_SWAPQ(sbuf.st_atime);
			osf_sbuf.osf_st_mtime = MD_SWAPQ(sbuf.st_mtime);
			osf_sbuf.osf_st_ctime = MD_SWAPQ(sbuf.st_ctime);
			osf_sbuf.osf_st_blksize = MD_SWAPQ(sbuf.st_blksize);
			osf_sbuf.osf_st_blocks = MD_SWAPW(sbuf.st_blocks);
			osf_sbuf.osf_st_retired1 = 0;
			osf_sbuf.osf_st_retired2 = 0;
			osf_sbuf.osf_st_retired3 = 0;
			osf_sbuf.osf_st_retired4 = 0;
			osf_sbuf.osf_st_retired5 = 0;
			osf_sbuf.osf_st_retired6 = 0;
			osf_sbuf.osf_st_nlink_reserved = 0;
			osf_sbuf.osf_st_ldev = 0;
			osf_sbuf.osf_st_uatime = 0;
			osf_sbuf.osf_st_umtime = 0;
			osf_sbuf.osf_st_uctime = 0;
			osf_sbuf.osf_st_flags = 0;
			osf_sbuf.osf_st_gen = 0;
			osf_sbuf.osf_st_ino_reserved = 0;
			osf_sbuf.osf_st_atime_reserved = 0;
			osf_sbuf.osf_st_mtime_reserved = 0;
			osf_sbuf.osf_st_ctime_reserved = 0;
			osf_sbuf.osf_st_spare[0] = 0;
			osf_sbuf.osf_st_spare[1] = 0;
			osf_sbuf.osf_st_spare[2] = 0;
			osf_sbuf.osf_st_spare[3] = 0;

			//copy fstat64() results to simulator memory
			mem->mem_bcopy(Write, arg(A1), &osf_sbuf, sizeof(osf_statbuf64));
		}
		break;

	case OSF_SYS_fstat:
		{
			osf_statbuf osf_sbuf;
			class stat sbuf;
			arg(V0) = fstat(/*fd*/arg(A0), &sbuf);
			check_error = true;

			sys_output("OSF_SYS_fstat: Reading from: %lld",arg(A0));

			//translate the stat structure to host format
			osf_sbuf.osf_st_dev = MD_SWAPW(sbuf.st_dev);
			osf_sbuf.osf_st_ino = MD_SWAPW(sbuf.st_ino);
			osf_sbuf.osf_st_mode = MD_SWAPW(sbuf.st_mode);
			osf_sbuf.osf_st_nlink = MD_SWAPH(sbuf.st_nlink);
			osf_sbuf.osf_st_uid = MD_SWAPW(sbuf.st_uid);
			osf_sbuf.osf_st_gid = MD_SWAPW(sbuf.st_gid);
			osf_sbuf.osf_st_rdev = MD_SWAPW(sbuf.st_rdev);
			osf_sbuf.osf_st_size = MD_SWAPQ(sbuf.st_size);
			osf_sbuf.osf_st_atime = MD_SWAPW(sbuf.st_atime);
			osf_sbuf.osf_st_mtime = MD_SWAPW(sbuf.st_mtime);
			osf_sbuf.osf_st_ctime = MD_SWAPW(sbuf.st_ctime);

			osf_sbuf.osf_st_blksize = MD_SWAPW(sbuf.st_blksize);
			osf_sbuf.osf_st_blocks = MD_SWAPW(sbuf.st_blocks);

			osf_sbuf.pad0 = 0;
			osf_sbuf.pad1 = 0;
			osf_sbuf.osf_st_spare1 = 0;
			osf_sbuf.osf_st_spare2 = 0;
			osf_sbuf.osf_st_spare3 = 0;
			osf_sbuf.osf_st_flags = 0;
			osf_sbuf.osf_st_gen = 0;

			//copy fstat() results to simulator memory
			mem->mem_bcopy(Write, arg(A1), &osf_sbuf, sizeof(osf_statbuf));
		}
		break;

	case OSF_SYS_getpagesize:
		arg(V0) = MD_PAGE_SIZE;
		check_error = true;
		sys_output("OSF_SYS_getpagesize: %lld",arg(V0));
		break;

	case OSF_SYS_setitimer:
		{
			sys_output("OSF_SYS_setitimer: %lld new(0x%llx) old(0x%llx)",arg(A0),arg(A1),arg(A2));
			char * new_value = new char[sizeof(itimerval)];
			char * old_value = new char[sizeof(itimerval)];

			mem->mem_bcopy(Read, arg(A1), &new_value, sizeof(itimerval));

			arg(V0) = setitimer(arg(A0), (const itimerval *)new_value, (itimerval *)old_value);
			if(arg(A2) && (arg(V0) == 0))
			{
				mem->mem_bcopy(Write, arg(A2), &old_value, sizeof(itimerval));
			}

			check_error = true;
			delete [] new_value;
			delete [] old_value;
		}
		break;

	case OSF_SYS_table:
		{
			qword_t table_id, table_index, buf_addr, num_elem, size_elem, last;

			//Our version of Spec2K puts OSF_SYS_table in A0 (which shouldn't be but is allowable)
			if(arg(A0) == OSF_SYS_table)
			{
				table_id = arg(A1);
				table_index = arg(A2);
				buf_addr = arg(A3);
				num_elem = arg(A4);
				size_elem = arg(A5);
				last = arg(A0);
			}
			else
			{
				//This is the correct handling
				table_id = arg(A0);
				table_index = arg(A1);
				buf_addr = arg(A2);
				num_elem = arg(A3);
				size_elem = arg(A4);
				last = arg(A5);
			}
			sys_output("OSF_SYS_table: table_id(%lld) index(%lld) buf_addr(0x%llx) num_elem(%lld) elem_size(%lld) last(0x%llx)\t",table_id,table_index,buf_addr,num_elem,size_elem,last);

			switch(table_id)
			{
			case OSF_TBL_PROCINFO:
				{
					//Two cases, a) index == 0, num_elem == INT_MAX, size_elem == 0
					//b) index == pid, num_elem == 1, size_elem == size(osf_tbl_procinfo) == 120
					//These only vary by index, we aren't too picky
					if(table_index == 0)
					{
						//return success, this is what was returned on native alpha
						arg(A3) = 32768;
					}
					else
					{
#ifdef WARN_ALL
						warn("partially supported OSF_SYS_table(OSF_TBL_PROCINFO,10,PROC_ID,...)");
#endif
						osf_tbl_procinfo info;
						long long buffer;

						char pid[6];
						sprintf(pid,"%lld",table_index);
						std::ifstream infile(std::string(("/proc/") + std::string(pid) + "/stat").c_str());
						infile >> info.pi_pid >> info.pi_comm >> info.pi_status >> info.pi_ppid >> info.pi_pgrp >> info.pi_session >> info.pi_ttyd;
						infile >> info.pi_tpgrp >> info.pi_flag >> buffer >> buffer >> buffer >> buffer >> buffer >> buffer >> buffer >> buffer;
						infile >> buffer >> buffer >> buffer >> buffer >> buffer >> buffer >> buffer >> buffer >> buffer >> buffer;
						infile >> buffer >> buffer;
						infile >> info.pi_cursig;	//This is probably wrong
						infile >> info.pi_sig >> buffer >> info.pi_sigignore >> info.pi_sigcatch;

						//Unknown values:
						info.pi_sigmask = info.pi_jobc = info.pi_tsession = 0;

						//Elsewhere values:
						info.pi_uid = info.pi_ruid = info.pi_svuid = getuid();
						info.pi_rgid = info.pi_svgid = getgid();

						//Override values from host with internal values:
						info.pi_pid = contexts[context_id].pid;
						info.pi_pgrp = contexts[context_id].gpid;
						info.pi_rgid = contexts[context_id].gid;
						info.pi_svgid = contexts[context_id].gid;

						//Fix pi_status:
						switch(info.pi_status)
						{
						case 'Z':	info.pi_status = OSF_PI_ZOMBIE; break;
						case 'R':
						default:	//Don't have enough info for now
							info.pi_status = OSF_PI_ACTIVE;
							break;
						}

						infile.close();
						//copy structure into simulator memory
						mem->mem_bcopy(Write, buf_addr, &info, size_elem * num_elem);

						//return success
						arg(A3) = 0;
					}
					check_error = true;
				}
				break;

			case OSF_TBL_SYSINFO:
				{
					osf_tbl_sysinfo sysinfo;
					if(table_index != 0)
					{
						panic("table: table id TBL_SYSINFO requires 0 index, got %08d", table_index);
					}
					else if(num_elem != 1)
					{
						panic("table: table id TBL_SYSINFO requires 1 elts, got %08d", num_elem);
					}
					else
					{
						rusage rusage_info;

						//use getrusage() to determine user & system time
						if(getrusage(RUSAGE_SELF, &rusage_info) < 0)
						{
							//abort the system call
							arg(A3) = -1;
							//not kosher to pass off errno of getrusage() as errno of table(), but what the heck...
							arg(V0) = errno;
							break;
						}
						//use sysconf() to determine clock tick frequency
						sysinfo.si_hz = sysconf(_SC_CLK_TCK);

						//convert user and system time into clock ticks
						sysinfo.si_user = rusage_info.ru_utime.tv_sec * sysinfo.si_hz + (rusage_info.ru_utime.tv_usec * sysinfo.si_hz) / 1000000UL;
						sysinfo.si_sys = rusage_info.ru_stime.tv_sec * sysinfo.si_hz + (rusage_info.ru_stime.tv_usec * sysinfo.si_hz) / 1000000UL;

						//following can't be determined in a portable manner and are ignored
						sysinfo.si_nice = 0;
						sysinfo.si_idle = 0;
						sysinfo.si_phz = 0;
						sysinfo.si_boottime = 0;
						sysinfo.wait = 0;

						//copy structure into simulator memory
						mem->mem_bcopy(Write, buf_addr, &sysinfo, sizeof(osf_tbl_sysinfo));

						//return success
						arg(A3) = 0;
					}
					break;
				}

			default:
				warn("table: unsupported table id %d requested, ignored", table_id);
				arg(A3) = 0;
			}
		}
		break;

	case OSF_SYS_getdtablesize:
		{
			rlimit rl;
			if(getrlimit(RLIMIT_NOFILE, &rl) != -1)
			{
				arg(V0) = rl.rlim_max;
				arg(A3) = 0;
			}
			else
			{
				//Specification does not handle errors
				arg(V0) = 0x1000;
				arg(A3) = 0;
			}
			sys_output("OSF_SYS_getdtablesize: soft(%d)\t",rl.rlim_cur);
			check_error = true;
		}
		break;

	case OSF_SYS_dup2:
		sys_output("OSF_SYS_dup2: old(%lld) new(%lld)\t",arg(A0),arg(A1));
		arg(V0) = contexts[context_id].file_table.dup2(arg(A0), arg(A1));
		check_error = true;
		break;

	case OSF_SYS_fcntl:
		//get fcntl() information on the file
		arg(A3) = arg(V0) = 0;
		check_error = true;
		sys_output("OSF_SYS_fcntl: File(%lld) cmd(%lld-",arg(A0),arg(A1));
		switch(arg(A1))
		{
		case 0:
			sys_output("f_dupfd");
			arg(V0) = contexts[context_id].file_table.duper(oldA0);	//Dup does not use real fds.
			break;
		case 1:
			sys_output("f_getfd");
			arg(V0) = contexts[context_id].file_table.getfd_cloexec(oldA0);
			break;
		case 2:
			sys_output("f_setfd");
			arg(V0) = contexts[context_id].file_table.setfd_cloexec(oldA0,arg(A2));
			break;
		case 3:
			sys_output("f_getfl");
			arg(V0) = fcntl(oldA0, F_GETFL);
			break;
		case 4:
			{
				osf_flock buf;
				mem->mem_bcopy(Read, arg(A2), &buf, sizeof(osf_flock));
				sys_output("f_setfl");
				struct flock lock_param = buf.copy_out();
				arg(V0) = fcntl(oldA0, F_SETFL, lock_param);
			}
			break;
		case 9:
			{
				osf_flock buf;
				mem->mem_bcopy(Read, arg(A2), &buf, sizeof(osf_flock));
				sys_output("f_setlkw");
				struct flock lock_param = buf.copy_out();
				sys_output(" %d %d %d %d ",lock_param.l_type, lock_param.l_whence, lock_param.l_start, lock_param.l_len);
				arg(V0) = fcntl(oldA0, F_SETLKW, lock_param);
			}
			break;
		default:	//There are other cases... we'll handle them as needed
			sys_output("unsupported");
			break;
		}
		//arg(V0) = fcntl(/*fd*/arg(A0), /*cmd*/arg(A1), /*arg*/arg(A2));
		sys_output(") arg(0x%llx) ",arg(A2));
		break;

#if 0
	case OSF_SYS_sigvec:
		//FIXME: the sigvec system call is ignored
		warn("syscall: sigvec ignored");
		arg(A3) = 0;
		break;
#endif

#if 0
	case OSF_SYS_sigblock:
		//FIXME: the sigblock system call is ignored
		warn("syscall: sigblock ignored");
		arg(A3) = 0;
		break;
#endif

#if 0
	case OSF_SYS_sigsetmask:
		//FIXME: the sigsetmask system call is ignored
		warn("syscall: sigsetmask ignored");
		arg(A3) = 0;
		break;
#endif

	case OSF_SYS_gettimeofday:
	case OSF_SYS_gettimeofday2:
		{
			osf_timeval osf_tv;
			timeval tv, *tvp;
			osf_timezone osf_tz;
			class timezone tz, *tzp;

			sys_output("OSF_SYS_gettimeofday: timeval?(0x%llx) timezone?(%lld) ",arg(A0),arg(A1));
			if(/*timeval*/arg(A0) != 0)
			{
				//copy timeval into host memory
				mem->mem_bcopy(Read, /*timeval*/arg(A0),&osf_tv, sizeof(osf_timeval));

				//convert target timeval structure to host format
				tv.tv_sec = MD_SWAPW(osf_tv.osf_tv_sec);
				tv.tv_usec = MD_SWAPW(osf_tv.osf_tv_usec);
				tvp = &tv;
			}
			else
			{
				tvp = NULL;
			}

			if(/*timezone*/arg(A1) != 0)
			{
				//copy timezone into host memory
				mem->mem_bcopy(Read, /*timezone*/arg(A1), &osf_tz, sizeof(osf_timezone));

				//convert target timezone structure to host format
				tz.tz_minuteswest = MD_SWAPW(osf_tz.osf_tz_minuteswest);
				tz.tz_dsttime = MD_SWAPW(osf_tz.osf_tz_dsttime);
				tzp = &tz;
			}
			else
			{
				tzp = NULL;
			}

			//get time of day
			arg(V0) = gettimeofday(tvp, tzp);
			check_error = true;

			if(/*timeval*/arg(A0) != 0)
			{
				//convert host timeval structure to target format
				osf_tv.osf_tv_sec = MD_SWAPW(tv.tv_sec);
				osf_tv.osf_tv_usec = MD_SWAPW(tv.tv_usec);

				//copy timeval to target memory
				mem->mem_bcopy(Write, /*timeval*/arg(A0),&osf_tv, sizeof(osf_timeval));
				sys_output("(%u.%u sec.usec)\t",osf_tv.osf_tv_sec,osf_tv.osf_tv_usec);
			}

			if(/*timezone*/arg(A1) != 0)
			{
				//convert host timezone structure to target format
				osf_tz.osf_tz_minuteswest = MD_SWAPW(tz.tz_minuteswest);
				osf_tz.osf_tz_dsttime = MD_SWAPW(tz.tz_dsttime);

				//copy timezone to target memory
				mem->mem_bcopy(Write, /*timezone*/arg(A1), &osf_tz, sizeof(osf_timezone));
				sys_output("(%u minuteswest %u dsttime)\t",osf_tz.osf_tz_minuteswest,osf_tz.osf_tz_dsttime);
			}
		}
		break;

	case OSF_SYS_getrusage:
	case OSF_SYS_getrusage2:
			sys_output("OSF_SYS_getrusage[2]: who(%lld) rusage(0x%llx) ",arg(A0), arg(A1));

#if defined(__svr4__) || defined(__USLC__) || defined(hpux) || defined(__hpux) || defined(_AIX)
		{
			tms tms_buf;
			sf_rusage rusage;

			//get user and system times
			if(times(&tms_buf) != (qword_t)-1)
			{
				arg(V0) = 0;
				arg(A3) = 0;
			}
			else	//got an error, indicate result
			{
				arg(A3) = -1;
				arg(V0) = errno;
			}

			//initialize target rusage result structure
#if defined(__svr4__)
			memset(&rusage, '\0', sizeof(osf_rusage));
#else /* !defined(__svr4__) */
			bzero(&rusage, sizeof(osf_rusage));
#endif
			//convert from host rusage structure to target format
			rusage.osf_ru_utime.osf_tv_sec = MD_SWAPW(tms_buf.tms_utime/CLK_TCK);
			rusage.osf_ru_utime.osf_tv_sec = MD_SWAPW(rusage.osf_ru_utime.osf_tv_sec);
			rusage.osf_ru_utime.osf_tv_usec = 0;
			rusage.osf_ru_stime.osf_tv_sec = MD_SWAPW(tms_buf.tms_stime/CLK_TCK);
			rusage.osf_ru_stime.osf_tv_sec = MD_SWAPW(rusage.osf_ru_stime.osf_tv_sec);
			rusage.osf_ru_stime.osf_tv_usec = 0;

			//copy rusage results into target memory
			mem->mem_bcopy(Write, /*rusage*/arg(A1), &rusage, sizeof(osf_rusage));
		}
#elif defined(__unix__)
		{
			rusage local_rusage;
			osf_rusage rusage;

			//get rusage information
			/*result*/arg(V0) = getrusage(/*who*/arg(A0), &local_rusage);
			check_error = true;

			//convert from host rusage structure to target format
			rusage.osf_ru_utime.osf_tv_sec = MD_SWAPW(local_rusage.ru_utime.tv_sec);
			rusage.osf_ru_utime.osf_tv_usec = MD_SWAPW(local_rusage.ru_utime.tv_usec);
			rusage.osf_ru_utime.osf_tv_sec = MD_SWAPW(local_rusage.ru_utime.tv_sec);
			rusage.osf_ru_utime.osf_tv_usec = MD_SWAPW(local_rusage.ru_utime.tv_usec);
			rusage.osf_ru_stime.osf_tv_sec = MD_SWAPW(local_rusage.ru_stime.tv_sec);
			rusage.osf_ru_stime.osf_tv_usec = MD_SWAPW(local_rusage.ru_stime.tv_usec);
			rusage.osf_ru_stime.osf_tv_sec = MD_SWAPW(local_rusage.ru_stime.tv_sec);
			rusage.osf_ru_stime.osf_tv_usec = MD_SWAPW(local_rusage.ru_stime.tv_usec);
			rusage.osf_ru_maxrss = MD_SWAPW(local_rusage.ru_maxrss);
			rusage.osf_ru_ixrss = MD_SWAPW(local_rusage.ru_ixrss);
			rusage.osf_ru_idrss = MD_SWAPW(local_rusage.ru_idrss);
			rusage.osf_ru_isrss = MD_SWAPW(local_rusage.ru_isrss);
			rusage.osf_ru_minflt = MD_SWAPW(local_rusage.ru_minflt);
			rusage.osf_ru_majflt = MD_SWAPW(local_rusage.ru_majflt);
			rusage.osf_ru_nswap = MD_SWAPW(local_rusage.ru_nswap);
			rusage.osf_ru_inblock = MD_SWAPW(local_rusage.ru_inblock);
			rusage.osf_ru_oublock = MD_SWAPW(local_rusage.ru_oublock);
			rusage.osf_ru_msgsnd = MD_SWAPW(local_rusage.ru_msgsnd);
			rusage.osf_ru_msgrcv = MD_SWAPW(local_rusage.ru_msgrcv);
			rusage.osf_ru_nsignals = MD_SWAPW(local_rusage.ru_nsignals);
			rusage.osf_ru_nvcsw = MD_SWAPW(local_rusage.ru_nvcsw);
			rusage.osf_ru_nivcsw = MD_SWAPW(local_rusage.ru_nivcsw);

			//copy rusage results into target memory
			mem->mem_bcopy(Write, /*rusage*/arg(A1), &rusage, sizeof(osf_rusage));
		}
#elif defined(_MSC_VER)
		warn("syscall: called getrusage\n");
		regs->regs_R[7] = 0;
#else
#error No getrusage() implementation!
#endif
		break;

	case OSF_SYS_utimes:
		{
			std::string filename = get_filename(mem,arg(A0));
			if(/*timeval*/arg(A1) == 0)
			{
#if defined(hpux) || defined(__hpux) || defined(linux)
				//no utimes() in hpux, use utime() instead
				/*result*/arg(V0) = utime(filename.c_str(), NULL);
#elif defined(__svr4__) || defined(__USLC__) || defined(unix) || defined(_AIX) || defined(__alpha)
				/*result*/arg(V0) = utimes(filename.c_str(), NULL);
#else
#error No utimes() implementation!
#endif
			}
			else
			{
				osf_timeval osf_tval[2];
				timeval tval[2];

				//copy timeval structure to host memory
				mem->mem_bcopy(Read, /*timeout*/arg(A1), osf_tval, 2*sizeof(osf_timeval));

				//convert timeval structure to host format
				tval[0].tv_sec = MD_SWAPW(osf_tval[0].osf_tv_sec);
				tval[0].tv_usec = MD_SWAPW(osf_tval[0].osf_tv_usec);
				tval[1].tv_sec = MD_SWAPW(osf_tval[1].osf_tv_sec);
				tval[1].tv_usec = MD_SWAPW(osf_tval[1].osf_tv_usec);

#if defined(hpux) || defined(__hpux) || defined(__svr4__)
				//no utimes() in hpux, use utime() instead
				{
					utimbuf ubuf;
					ubuf.actime = MD_SWAPW(tval[0].tv_sec);
					ubuf.modtime = MD_SWAPW(tval[1].tv_sec);
					/* result */arg(V0) = utime(filename.c_str(), &ubuf);
				}
#elif defined(__USLC__) || defined(unix) || defined(_AIX) || defined(__alpha)
				/* result */arg(V0) = utimes(filename.c_str(), tval);
#else
#error No utimes() implementation!
#endif
			}
			check_error = true;
		}
		break;

	case OSF_SYS_getrlimit:
	case OSF_SYS_setrlimit:
		{
			sys_output("OSF_SYS_[gs]etrlimit: (%lld, 0x%llx) ",arg(A0),arg(A1));

			osf_rlimit osf_rl;
			mem->mem_bcopy(Read, arg(A1), &osf_rl, sizeof(osf_rlimit));
			sys_output("cur(%llx), max(%llx): ", osf_rl.osf_rlim_cur, osf_rl.osf_rlim_max);


			//We'll fake the results, we aren't going to actually query our machine.
			switch(arg(A0))
			{
			case 0:	//CPU time
			case 1: //file size
			case 4: //core file size
#ifdef __LP64__
				osf_rl.osf_rlim_cur = osf_rl.osf_rlim_max = 0x7fffffffffffffff;
#else
				osf_rl.osf_rlim_cur = osf_rl.osf_rlim_max = 0x7fffffff;
#endif
				break;
			case 2: //data size
			case 7: //address space
#ifdef __LP64__
				osf_rl.osf_rlim_cur = osf_rl.osf_rlim_max = 0x100000000;
#else
				osf_rl.osf_rlim_cur = osf_rl.osf_rlim_max = 0x80000000;
#endif
				break;
			case 3: //stack size
				osf_rl.osf_rlim_cur = osf_rl.osf_rlim_max = 0x200000;
				osf_rl.osf_rlim_cur = osf_rl.osf_rlim_max = 0x08000000;
				break;
			case 5: //resident set size
				osf_rl.osf_rlim_cur = osf_rl.osf_rlim_max = 0x3d60c000;
				break;
			}

			arg(V0) = 0;
			check_error = true;
			sys_output("Ret: (%lld, %lld)\t",osf_rl.osf_rlim_cur,osf_rl.osf_rlim_max);
			mem->mem_bcopy(Write, /*rlimit*/arg(A1), &osf_rl, sizeof(osf_rlimit));
			break;
#if 0
			//Old code here

			osf_rlimit osf_rl;
			rlimit rl;

			//copy rlimit structure to host memory
			mem->mem_bcopy(Read, /*rlimit*/arg(A1), &osf_rl, sizeof(osf_rlimit));

			//convert rlimit structure to host format
			rl.rlim_cur = MD_SWAPQ(osf_rl.osf_rlim_cur);
			rl.rlim_max = MD_SWAPQ(osf_rl.osf_rlim_max);

			//get rlimit information
			if(syscode == OSF_SYS_getrlimit)
			{
				arg(V0) = getrlimit(arg(A0), &rl);
				sys_output("OSF_SYS_getrlimit: (%lld, 0x%llx) ",arg(A0),arg(A1));
			}
			else	//syscode == OSF_SYS_setrlimit
			{
				arg(V0) = setrlimit(arg(A0), &rl);
				sys_output("OSF_SYS_setrlimit: (%lld, 0x%llx) ",arg(A0),arg(A1));
			}

			//convert rlimit structure to target format
			osf_rl.osf_rlim_cur = MD_SWAPQ(rl.rlim_cur);
			osf_rl.osf_rlim_max = MD_SWAPQ(rl.rlim_max);

			sys_output("Ret: (%llx, %llx)\t",osf_rl.osf_rlim_cur,osf_rl.osf_rlim_max);
			//if stack request, limit to 0x2000000
			if(arg(A0) == 3)
			{
				if(osf_rl.osf_rlim_max > 0x2000000)
				{
					osf_rl.osf_rlim_max = 0x2000000;
				}
			}

			check_error = true;

			//copy rlimit structure to target memory
			mem->mem_bcopy(Write, /*rlimit*/arg(A1), &osf_rl, sizeof(osf_rlimit));
#endif
		}
		break;

	case OSF_SYS_sigprocmask:
		{
#ifdef WARN_ALL
			static bool first = TRUE;
			if(first)
			{
				fprintf(stderr,"partially supported sigprocmask() call..., this is your only warning\t");
				first = !first;
			}
#endif
			sys_output("OSF_SYS_sigprocmask: How(%lld) New(0x%llx) Old(0x%llx) ",arg(A0),arg(A1),arg(A2));

			//from klauser@cs.colorado.edu: there are a couple bugs in the
			//sigprocmask implementation; here is a fix: the problem comes from an
			//impedance mismatch between application/libc interface and
			//libc/syscall interface, the former of which you read about in the
			//manpage, the latter of which you actually intercept here.  The
			//following is mostly correct, but does not capture some minor
			//details, which you only get right if you really let the kernel
			//handle it. (e.g. you can't really ever block sigkill etc.)

			arg(V0) = sigmask;
			arg(A3) = 0;

			switch(arg(A0))
			{
			case OSF_SIG_BLOCK:
				sigmask |= (unsigned long)arg(A1);
				break;
			case OSF_SIG_UNBLOCK:
				sigmask &= (unsigned int)(~arg(A1));
				break;
			case OSF_SIG_SETMASK:
				sigmask = (unsigned int)arg(A1);
				break;
			default:
				arg(V0) = EINVAL;
				arg(A3) = 1;
			}
		}
		break;

	//Fixme: rt_sigaction has one extra parameter.... (this may not be on Tru64)
	case OSF_SYS_sigaction:
	case OSF_SYS_rt_sigaction:
		{
			int signum = arg(A0);
			sys_output("OSF_SYS_sigaction (fixme): Signal(%lld) Action(0x%llx) Prev(0x%llx)\t",arg(A0),arg(A1),arg(A2));

			if(arg(A1) != 0)
			{
				sigaction_array[signum] = arg(A1);
			}

			if(arg(A2))
			{
				arg(A2) = sigaction_array[signum];
			}

			arg(V0) = 0;
			arg(A3) = 0;			//for some reason, __sigaction expects A3 to have a 0 return value

			//FIXME: still need to add code so that on a signal, the correct action is actually taken.
			//FIXME: still need to add support for returning the correct error messages (EFAULT, EINVAL)
		}
		break;

	case OSF_SYS_sigstack:
		sys_output("OSF_SYS_sigstack: instack(%llx) outstack(%llx)\t",arg(A0),arg(A1));
#ifdef WARN_ALL
		warn("unsupported sigstack() call... instack(0x%llx), outstack(0x%llx)",arg(A0),arg(A1));
#endif
		if(arg(A0))
		{
			osf_sigstack instack;
			mem->mem_bcopy(Read, arg(A0), &instack, sizeof(osf_sigstack));
			//instack.ss_sp is the new signal stack
			//instack.onstack is 1 if the process is currently running on the stack

			//This is the new signal stack
		}
		else
		{
			//The signal stack state is not set
		}
		if(arg(A1))
		{
			//Argument 2 is a sigstack pointer, store current sigstack state here
			osf_sigstack outstack;
			stack_t oss;
			sigaltstack(NULL,&oss);
			outstack.ss_sp = oss.ss_sp;
			outstack.ss_onstack = (oss.ss_flags == SS_ONSTACK);
			mem->mem_bcopy(Write, arg(A1), &outstack, sizeof(osf_sigstack));
		}
		else
		{
			//no previous state reported
		}
		arg(A3) = 0;

//		arg(V0) = 0;
		break;

	case OSF_SYS_sigaltstack:
		sys_output("OSF_SYS_sigaltstack: new(%llx) prior(%llx)\t",arg(A0),arg(A1));
#ifdef WARN_ALL
		warn("unsupported sigaltstack() call... new(0x%llx), prior(0x%llx)",arg(A0),arg(A1));
#endif
		if(arg(A0))
		{
			osf_sigstack instack;
			mem->mem_bcopy(Read, arg(A0), &instack, sizeof(osf_sigstack));
			//instack.ss_sp is the new signal stack
			//instack.onstack is 1 if the process is currently running on the stack
			//This is the new signal stack
		}
		else
		{
			//The signal stack state is not set
		}
		if(arg(A1))
		{
			//Argument 2 is a sigstack pointer, store current sigstack state here
			osf_sigstack outstack;
			stack_t oss;
			sigaltstack(NULL,&oss);
			outstack.ss_sp = oss.ss_sp;
			outstack.ss_onstack = (oss.ss_flags == SS_ONSTACK);
			mem->mem_bcopy(Write, arg(A1), &outstack, sizeof(osf_sigstack));
		}
		else
		{
			//no previous state reported
		}
		arg(A3) = 0;

//		arg(V0) = 0;
		break;

	case OSF_SYS_sigreturn:
		{
#ifdef WARN_ALL
			static int first = TRUE;
			if(first)
			{
				warn("partially supported sigreturn() call...");
				first = FALSE;
			}
#endif
			osf_sigcontext sc;
			sys_output("OSF_SYS_sigreturn: from(%llx) - does not return. ",arg(A0));
			mem->mem_bcopy(Read, /* sc */arg(A0), &sc, sizeof(osf_sigcontext));

			sigmask = MD_SWAPQ(sc.sc_mask); /* was: prog_sigmask */
			regs->regs_NPC = MD_SWAPQ(sc.sc_pc);

			//FIXME: should check for the branch delay bit
			//FIXME: current->nextpc = current->pc + 4; not sure about this...
			for(int i=0; i < 32; ++i)
			{
				regs->regs_R[i] = sc.sc_regs[i];
				regs->regs_F[i].q = sc.sc_fpregs[i];
			}
			regs->regs_C.fpcr = sc.sc_fpcr;
		}
		break;

	case OSF_SYS_uswitch:
		warn("unsupported uswitch() call...");
		arg(V0) = arg(A1);
		break;

	case OSF_SYS_getsysinfo:
		{
			unsigned long long retval = 0;
			switch(arg(A0))
			{
			case GSI_MAX_UPROCS:
				{	//Ranges from 0, 256, 524287
					int procs = 1024;
					//Assuming (and checking) that size of this buffer is 4 bytes.
					assert(arg(A2) == 4);
					mem->mem_bcopy(Write, arg(A1), &procs, arg(A2));
					retval = 1;
					sys_output("OSF_SYS_getsysinfo: GSI_MAX_UPROCS (FIXME): dest(0x%llx) size(%lld) value(static %d)",arg(A1),arg(A2),procs);
				}
				break;
			case GSI_LMF:
				{
					//LMF_GETSERV 1
					//LMF_GETSMM 2
					//LMF_GETLIC 3

					retval = 1;
					sys_output("OSF_SYS_getsysinfo: GSI_LMF: arg1(0x%llx) ",arg(A1));
					sys_output("Args: 0(0x%llx) 1(0x%llx) 2(0x%llx) 3(0x%llx) 4(0x%llx) 5(0x%llx)\n",arg(A0),arg(A1),arg(A2),arg(A3),arg(A4),arg(A5));
				}
				break;
			case GSI_CLK_TCK:
				{
					int speed = 1024;
					//Assuming (and checking) that size of this buffer is 4 bytes.
					assert(arg(A2) == 4);
					mem->mem_bcopy(Write, arg(A1), &speed, arg(A2));
					retval = 1;
					sys_output("OSF_SYS_getsysinfo: GSI_CLK_TCK: dest(0x%llx) size(%lld) value(static %d)",arg(A1),arg(A2),speed);
				}
				break;
			case GSI_IEEE_FP_CONTROL:
				{
					//This is taken from the FPCR_MASK define in asm/fpu.h
					unsigned long long temp(0xffff800000000000ULL);
					mem->mem_bcopy(Write, arg(A1), &temp, sizeof(unsigned long long));
					retval = 1;
					sys_output("OSF_SYS_getsysinfo: GSI_IEEE_FP_CONTROL: dest(0x%llx) value(static %llx)",arg(A1),temp);
				}
				break;
			case GSI_VERSION_STRING:
				{
					static std::string version = "Compaq Tru64 UNIX V5.1B (Rev. 2650); Friday Jun  5 17:29:33 EDT 2009";
					mem->mem_bcopy(Write, arg(A1), &version[0], version.size());
					retval = 1;
					sys_output("OSF_SYS_getsysinfo: GSI_VERSION_STRING: dest(0x%llx) size(%lld)",arg(A1),arg(A2));
				}
				break;
			case GSI_TIMER_MAX:
				{
					md_gpr_t timers = 32;
					mem->mem_bcopy(Write, arg(A1), &timers, arg(A2));
					retval = 1;
					sys_output("OSF_SYS_getsysinfo: GSI_TIMER_MAX (providing 32): dest(0x%llx) destsize(%lld)",arg(A1),arg(A2));
				}
				break;
			case GSI_CPUS_IN_BOX:
				{
					int num_cpus = (int)cores.size();
					mem->mem_bcopy(Write, arg(A1), &num_cpus, arg(A2));
					retval = 1;
					sys_output("OSF_SYS_getsysinfo: GSI_CPUS_IN_BOX: cpus(%lld) dest(0x%llx) destsize(%lld) ",num_cpus,arg(A1),arg(A2));
				}
				break;
			case GSI_MAX_CPU:
				{
					int max_cpus = 256;
					if(max_cpus<(int)cores.size())
					{
						max_cpus = (int)cores.size();
					}
					mem->mem_bcopy(Write, arg(A1), &max_cpus, arg(A2));
					retval = 1;
					sys_output("OSF_SYS_getsysinfo: GSI_MAX_CPU: max_cpu(%lld) dest(0x%llx) destsize(%lld) ",max_cpus,arg(A1),arg(A2));
				}
				break;
			case GSI_PHYSMEM:
				{
					//This is an int?
					int mem_avail_kb = 0x7fffffff;
					mem->mem_bcopy(Write, arg(A1), &mem_avail_kb, arg(A2));
					retval = 1;
					sys_output("OSF_SYS_getsysinfo: GSI_PHYSMEM: mem(%lld) dest(0x%llx) destsize(%lld) ",mem_avail_kb,arg(A1),arg(A2));
				}
				break;
			case GSI_PLATFORM_NAME:
				{
					std::string platform_name = "COMPAQ Professional Workstation XP1000";
//This needs to be handled, but it isn't as straight forward as retval and errno...
//					if(arg(A2) < platform_name.size())
//					{
//						retval = -1;
//						errno = EINVAL;
//					}
//					else
//					{
						mem->mem_bcopy(Write, arg(A1), (char *)platform_name.c_str(), arg(A2));
						retval = 1;
//					}
					sys_output("OSF_SYS_getsysinfo: GSI_PLATFORM_NAME: name(%s) dest(0x%llx) destsize(%lld) ",platform_name.c_str(),arg(A1),arg(A2));
				}
				break;
			case GSI_CPU_INFO:
				{
					osf_cpu_info cpu_info;
					cpu_info.current_cpu = contexts[context_id].core_id;
					cpu_info.cpus_in_box = cores.size();
					cpu_info.cpu_type = 57;
					cpu_info.ncpus = cores.size();
					cpu_info.cpus_present = (1<<cores.size()) - 1;
					cpu_info.cpus_running = cpu_info.cpu_binding = cpu_info.cpu_ex_binding = 0;
					for(unsigned long long i=0;i<cores.size();i++)
					{
						if(cores[i].context_ids.size() < cores[i].max_contexts)
						{
							cpu_info.cpus_running |= (1<<i);
						}
						if((!cores[i].context_ids.empty()) && cores[i].max_contexts)
						{
							cpu_info.cpu_binding |= (1<<i);
						}
					}
					cpu_info.mhz = 1000;
					cpu_info.unused[0] = cpu_info.unused[1] = cpu_info.unused[2] = 0;
					mem->mem_bcopy(Write, arg(A1), &cpu_info, arg(A2));
					retval = 1;
					sys_output("OSF_SYS_getsysinfo: GSI_CPU_INFO: cur_cpu(%d) in_box(%d) type(%d) ncpus(%d) present(0x%llx) running(0x%llx) bind(0x%llx) exbind(0x%llx) "
						"mhz(%d) dest(0x%llx) destsize(%lld) ",cpu_info.current_cpu, cpu_info.cpus_in_box, cpu_info.cpu_type, cpu_info.ncpus, cpu_info.cpus_present, cpu_info.cpus_running,
						cpu_info.cpu_binding, cpu_info.cpu_ex_binding, cpu_info.mhz, arg(A1),arg(A2));
				}
				break;
			case GSI_PROC_TYPE:
				{
					int proc_type = 8;
					mem->mem_bcopy(Write, arg(A1), &proc_type, arg(A2));
					retval = 1;
					sys_output("OSF_SYS_getsysinfo: GSI_PROC_TYPE: type(%d) dest(0x%llx) destsize(%lld) ",proc_type,arg(A1),arg(A2));
				}
				break;
			default:
				warn("unsupported getsysinfo(op,...) option");
				fprintf(stderr,"Args:\n0: %lld\n1: %lld\n2: %lld\n3: %lld\n4: %lld\n5: %lld\n",arg(A0),arg(A1),arg(A2),arg(A3),arg(A4),arg(A5));
				break;
			}
			//Returns a value equal to the number of elements placed into arg(A1);
			arg(V0) = retval;
			check_error = true;
		}
		break;

	case OSF_SYS_setsysinfo:
		{
			unsigned long long temp;
			switch(arg(A0))
			{
			case SSI_IEEE_FP_CONTROL:
				//We are just ignoring this, it "might" be ok
				mem->mem_bcopy(Read, arg(A1), &temp, sizeof(unsigned long long));
				sys_output("OSF_SYS_setsysinfo(SSI_IEEE_FP_CONTROL): 0x%llx, returning ok: ",temp);
				arg(V0) = 1;
				break;
			case SSI_LMF:
				sys_output("OSF_SYS_setsysinfo:SSI_LMF, unsupported: ");
				sys_output("Args:\n0: %lld\n1: %lld\n2: %lld\n3: %lld\n4: %lld\n5: %lld\n",arg(A0),arg(A1),arg(A2),arg(A3),arg(A4),arg(A5));
				arg(V0) = 0;
				break;
			default:
				warn("unsupported setsysinfo(0x%x,...) option",arg(A0));
				sys_output("Args:\n0: %lld\n1: %lld\n2: %lld\n3: %lld\n4: %lld\n5: %lld\n",arg(A0),arg(A1),arg(A2),arg(A3),arg(A4),arg(A5));
				arg(V0) = 0;
			}
			arg(A3) = 0;
			check_error = true;
		}
		break;

	case OSF_SYS_getdirentries:
		{
			long long basepoff = 0;
			mem->mem_bcopy(Read, arg(A3), &basepoff, sizeof(unsigned long long));

			sys_output("OSF_SYS_getdirentries: File(%lld) into(0x%llx) size(%lld) basep(0x%llx -> %lld) ",arg(A0),arg(A1),arg(A2),arg(A3),basepoff);

			int fd = arg(A0);
			md_addr_t osf_buf = arg(A1);
			size_t osf_nbytes = arg(A2);
			md_addr_t osf_pbase = arg(A3);

			//number of entries in simulated memory
			if(!osf_nbytes)
				warn("attempting to get 0 directory entries...");

			//allocate local memory, whatever fits
			char *buf = (char *)calloc(1, osf_nbytes);
			if(!buf)
			{
				fatal("out of virtual memory");
			}

			//get directory entries
			arg(V0) = getdirentries((int)fd, buf, osf_nbytes, (long *)&basepoff);

//			sys_output("Current offset is: %ld\n",lseek((int)fd,0,SEEK_CUR));
			//check for an error condition
			if(arg(V0) != (qword_t)-1)
			{
				unsigned int osf_cnt = 0;
				//anything to copy back?
				if(arg(V0) > 0)
				{
					//copy all possible results to simulated space
					unsigned int i=0, cnt=0;
					for(dirent *p=(dirent *)buf; cnt < arg(V0); i++, cnt += p->d_reclen, p=(dirent *)(buf+cnt))
					{
						osf_dirent osf_dirent;

						osf_dirent.d_ino = MD_SWAPW(p->d_ino);
						osf_dirent.d_namlen = MD_SWAPH(strlen(p->d_name));
						strcpy(osf_dirent.d_name, p->d_name);
						osf_dirent.d_reclen = MD_SWAPH(OSF_DIRENT_SZ(p->d_name));
						mem->mem_bcopy(Write, osf_buf + osf_cnt, &osf_dirent, OSF_DIRENT_SZ(p->d_name));

						osf_cnt += OSF_DIRENT_SZ(p->d_name);
					}
				}
				if(osf_pbase != 0)
				{
					sqword_t osf_base = lseek(fd, 0, SEEK_CUR);
					mem->mem_bcopy(Write, osf_pbase, &osf_base, sizeof(osf_base));
				}

				//update V0 to indicate translated read length
				arg(V0) = osf_cnt;
//				sys_output("osf_cnt is: %d\n",osf_cnt);
			}
			check_error = true;
			free(buf);
		}
		break;

	case OSF_SYS_truncate:
		{
			std::string filename = get_filename(mem,arg(A0));
			arg(V0) = truncate(filename.c_str(), /* length */(size_t)arg(A1));
			check_error = true;
			sys_output("OSF_SYS_truncate: file(%s) to size(%lld)",filename.c_str(),arg(A1));
		}
		break;

	case OSF_SYS_ftruncate:
		arg(V0) = ftruncate(/* fd */(int)arg(A0), /* length */(size_t)arg(A1));
		check_error = true;
		sys_output("OSF_SYS_ftruncate: fd(%lld) to size(%lld)",arg(A0),arg(A1));
		break;

	case OSF_SYS_statfs:
		{
			std::string filename = get_filename(mem,arg(A0));
			osf_statfs osf_sbuf;
			class statfs sbuf;
			arg(V0) = statfs(filename.c_str(), &sbuf);
			check_error = true;

			sys_output("OSF_SYS_statfs: Stating(%s)\t",filename.c_str());

			//translate from host stat structure to target format
#if defined(__svr4__) || defined(__osf__)
			osf_sbuf.f_type = MD_SWAPH(0x6969) /* NFS, whatever... */;
#else
			osf_sbuf.f_type = MD_SWAPH(sbuf.f_type);
#endif
			osf_sbuf.f_fsize = MD_SWAPW(sbuf.f_bsize);
			osf_sbuf.f_blocks = MD_SWAPW(sbuf.f_blocks);
			osf_sbuf.f_bfree = MD_SWAPW(sbuf.f_bfree);
			osf_sbuf.f_bavail = MD_SWAPW(sbuf.f_bavail);
			osf_sbuf.f_files = MD_SWAPW(sbuf.f_files);
			osf_sbuf.f_ffree = MD_SWAPW(sbuf.f_ffree);
			//osf_sbuf.f_fsid = MD_SWAPW(sbuf.f_fsid);

			//copy stat() results to simulator memory
			mem->mem_bcopy(Write, /*sbuf*/arg(A1),&osf_sbuf, sizeof(osf_statbuf));
		}
		break;

	case OSF_SYS_statfs64:
		{
			std::string filename = get_filename(mem,arg(A0));
			osf_statfs64 osf_sbuf;
			class statfs64 sbuf;
			arg(V0) = statfs64(filename.c_str(), &sbuf);
			check_error = true;

			sys_output("OSF_SYS_statfs64: Stating(%s)\t",filename.c_str());

			//translate from host stat structure to target format
#if defined(__svr4__) || defined(__osf__)
			osf_sbuf.f_type = MD_SWAPH(0x6969) /* NFS, whatever... */;
#else
			osf_sbuf.f_type = MD_SWAPH(sbuf.f_type);
#endif
			osf_sbuf.f_fsize = MD_SWAPW(sbuf.f_bsize);
			osf_sbuf.f_blocks = MD_SWAPW(sbuf.f_blocks);
			osf_sbuf.f_bfree = MD_SWAPW(sbuf.f_bfree);
			osf_sbuf.f_bavail = MD_SWAPW(sbuf.f_bavail);
			osf_sbuf.f_files = MD_SWAPW(sbuf.f_files);
			osf_sbuf.f_ffree = MD_SWAPW(sbuf.f_ffree);
			//osf_sbuf.f_fsid = MD_SWAPW(sbuf.f_fsid);

			//copy stat() results to simulator memory
			mem->mem_bcopy(Write, /*sbuf*/arg(A1),&osf_sbuf, sizeof(osf_statbuf));
		}
		break;

	case OSF_SYS_setregid:
		sys_output("OSF_SYS_setregid: rgid(%lld) egid(%lld) ",arg(A0),arg(A1));
		//set real and effective group ID
		arg(V0) = setregid(/* rgid */(gid_t)arg(A0), /* egid */(gid_t)arg(A1));
		check_error = true;
		break;

	case OSF_SYS_setreuid:
		//-1 means no change
		sys_output("OSF_SYS_setreuid: ruid(%lld) euid(%lld) ",arg(A0),arg(A1));
		//set real and effective user ID
		arg(V0) = setreuid(/* ruid */(uid_t)arg(A0), /* euid */(uid_t)arg(A1));
		check_error = true;
		break;

	case OSF_SYS_socket:
		{
			sys_output("OSF_SYS_socket: Domain(%lld) type(%lld) protocol(%lld) ",arg(A0),arg(A1),arg(A2));
			//Domain 1 is AF_UNIX (local to host, pipes, portals)
			//Type 2 is SOCK_DGRAM (datagram socket)
			//Protocol 0 is the default behavior for that type

			//create an endpoint for communication
			int domain = xlate_arg(arg(A0), family_map, N_ELT(family_map),"socket(family)");
			int type = xlate_arg(arg(A1), socktype_map, N_ELT(socktype_map), "socket(type)");
			int protocol = xlate_arg(arg(A2), family_map, N_ELT(family_map), "socket(proto)");
			arg(V0) = socket(domain, type, protocol);

			//Doesn't allow checkpoint recovery, but, at least it handles the problem with close failing.
			//FIXME: insert marks this file descriptor as a PIPE (it really shouldn't default to that without any choice).
			if(arg(V0)!=(md_gpr_t)-1)
			{
				arg(V0) = contexts[context_id].file_table.insert(arg(V0),"SOCKET");
			}
			check_error = true;
		}
		break;

	case OSF_SYS_connect:
		{
			//FIXME: It is possible for sockaddr to reflect some other type (such as INET6)?

			//initiate a connection on a socket
			osf_sockaddr osf_sa;
			sys_output("OSF_SYS_connect: socket(%lld) sockaddr(0x%llx) socklen(%lld) ",arg(A0),arg(A1),arg(A2));
			check_error = true;

			//copy sockaddr structure to host memory
			mem->mem_bcopy(Read, arg(A1), &osf_sa, (int)arg(A2));
			sys_output("fam(%u) ",osf_sa.sa_family);
			sys_output("2(%u) ",*(&osf_sa.sa_family + 1));
			for(int i=0;i<14;i++)
			{
				sys_output("%d(%u) ",i,osf_sa.sa_data[i]);
			}

			//get the socket address
			if(arg(A2) > sizeof(osf_sockaddr))
			{
				errno = EINVAL;
				arg(V0) = -1;
				break;
				fatal("sockaddr size overflow: addrlen = %lld: limit %lld", arg(A2), sizeof(osf_sockaddr));
			}

			arg(V0) = connect(arg(A0), (const sockaddr *)&osf_sa, (int)arg(A2));
		}
		break;

	case OSF_SYS_utsname:
	case OSF_SYS_uname:
		//Entry size is defined by the use of sys/utsname.h. We will hope/assume the pointer can handle it without overflow.
		//Gets name and information about the current kernel
		arg(V0) = 0;

		mem->mem_bcopy(Write, arg(A0), &utsname_data, sizeof(osf_utsname));
		check_error = true;
		sys_output("OSF_SYS_u[ts]name: 0x%llx\t",arg(A0));
		break;

	case OSF_SYS_writev:
		{
			char *buf;
			iovec *iov;

			//allocate host side I/O vectors
			iov = new iovec[/* iovcnt */arg(A2)];

			//copy target side I/O vector buffers to host memory
			for(unsigned int i=0; i < /* iovcnt */arg(A2); i++)
			{
				osf_iovec osf_iov;

				//copy target side pointer data into host side vector
				mem->mem_bcopy(Read,(/*iov*/arg(A1) + i*sizeof(osf_iovec)),&osf_iov, sizeof(osf_iovec));

				iov[i].iov_len = MD_SWAPW(osf_iov.iov_len);
				if(osf_iov.iov_base != 0 && osf_iov.iov_len != 0)
				{
					buf = (char *)calloc(MD_SWAPW(osf_iov.iov_len), sizeof(char));
					if(!buf)
					{
						fatal("out of virtual memory in SYS_writev");
					}
					mem->mem_bcopy(Read, MD_SWAPQ(osf_iov.iov_base), buf, MD_SWAPW(osf_iov.iov_len));
					iov[i].iov_base = buf;
				}
				else
				{
					iov[i].iov_base = NULL;
				}
			}

			//perform the vector'ed write
			do
			{
				/*result*/arg(V0) = writev(/* fd */(int)arg(A0), iov, /* iovcnt */(size_t)arg(A2));
			} while(/*result*/arg(V0) == (qword_t)-1 && errno == EAGAIN);
			check_error = true;

			//free all the allocated memory
			for(unsigned int i=0; i < /* iovcnt */arg(A2); i++)
			{
				if(iov[i].iov_base)
				{
					free(iov[i].iov_base);
					iov[i].iov_base = NULL;
				}
			}
			free(iov);
		}
		break;

	case OSF_SYS_readv:
		{
			char *buf = NULL;
			osf_iovec *osf_iov;
			iovec *iov;

			//allocate host side I/O vectors
			osf_iov = (osf_iovec *)calloc(/* iovcnt */arg(A2),sizeof(osf_iovec));
			if(!osf_iov)
			{
				fatal("out of virtual memory in SYS_readv");
			}

			iov = (iovec *)calloc(/* iovcnt */arg(A2), sizeof(iovec));
			if(!iov)
			{
				fatal("out of virtual memory in SYS_readv");
			}

			//copy host side I/O vector buffers
			for(unsigned int i=0; i < /* iovcnt */arg(A2); i++)
			{
				//copy target side pointer data into host side vector
				mem->mem_bcopy(Read,(/*iov*/arg(A1) + i*sizeof(osf_iovec)),&osf_iov[i], sizeof(osf_iovec));

				iov[i].iov_len = MD_SWAPW(osf_iov[i].iov_len);
				if(osf_iov[i].iov_base != 0 && osf_iov[i].iov_len != 0)
				{
					buf = (char *)calloc(MD_SWAPW(osf_iov[i].iov_len), sizeof(char));
					if(!buf)
					{
						fatal("out of virtual memory in SYS_readv");
					}
					iov[i].iov_base = buf;
				}
				else
				{
					iov[i].iov_base = NULL;
				}
			}

			//perform the vector'ed read
			do
			{
				/*result*/arg(V0) = readv(/* fd */(int)arg(A0), iov, /* iovcnt */(size_t)arg(A2));
			} while(/*result*/arg(V0) == (qword_t)-1 && errno == EAGAIN);

			//copy target side I/O vector buffers to host memory
			for(unsigned int i=0; i < /* iovcnt */arg(A2); i++)
			{
				if(osf_iov[i].iov_base != 0)
				{
					mem->mem_bcopy(Write, MD_SWAPQ(osf_iov[i].iov_base),iov[i].iov_base, MD_SWAPW(osf_iov[i].iov_len));
				}
			}
			check_error = true;

			//free all the allocated memory
			for(unsigned int i=0; i < /* iovcnt */arg(A2); i++)
			{
				if(iov[i].iov_base)
				{
					free(iov[i].iov_base);
					iov[i].iov_base = NULL;
				}
			}

			if(osf_iov)
			{
				free(osf_iov);
			}
			if(iov)
			{
				free(iov);
			}
		}
		break;

	case OSF_SYS_setsockopt:
		{
			//set options on sockets
			char *buf = NULL;
			xlate_table_t *map = NULL;
			int nmap;

			//copy optval to host memory
			if(arg(A3) && arg(A4))
			{
				//copy target side pointer data into host side vector
				buf = new char[arg(A4)];
				mem->mem_bcopy(Read, /* optval */arg(A3), buf, /* optlen */(int)arg(A4));
			}
			sys_output("OSF_SYS_setsockopt: socket(%lld) level(%lld) opt_name(%lld) &opt_val(0x%llx - %d) opt_len(%lld) ",arg(A0),arg(A1),arg(A2),arg(A3),*(int *)buf,arg(A4));

			//pick the correct translation table
			switch(arg(A1))
			{
			case OSF_SOL_SOCKET:
				map = sockopt_map;
				nmap = N_ELT(sockopt_map);
				break;
			case OSF_SOL_TCP:
				map = tcpopt_map;
				nmap = N_ELT(tcpopt_map);
				break;
			case OSF_SOL_IP:
				map = ipopt_map;
				nmap = N_ELT(ipopt_map);
				break;
			default:
				warn("no translation map available for `setsockopt()': %d",(int)arg(A1));
				map = sockopt_map;
				nmap = N_ELT(sockopt_map);
			}
			int level = xlate_arg(arg(A1),socklevel_map, N_ELT(socklevel_map), "setsockopt(level)");
			int optname = xlate_arg(arg(A2), map, nmap, "setsockopt(opt)");

			/* result */arg(V0) = setsockopt(/* sock */(int)arg(A0), level, optname, /* optval */buf, /* optlen */arg(A4));
			check_error = true;
			delete [] buf;
		}
		break;

	case OSF_SYS_getsockopt:
		{
			//set options on sockets
			char *buf = NULL;
			xlate_table_t *map = NULL;
			int nmap;
			md_gpr_t optlen = 0;

			//copy optval to host memory: if optval is not NULL and optlen (a ptr) is not NULL
			if(arg(A3) && arg(A4))
			{
//				socklen_t * points to a 64-bit value on alpha, however, don't use as a 64-bit.
				mem->mem_bcopy(Read, arg(A4), &optlen, 4);
//				fprintf(stderr, "ptr %llx, value %llx\n",arg(A4), optlen);
				if(optlen)
				{
					buf = new char[optlen];
					//copy target side pointer data into host side vector
					mem->mem_bcopy(Read, /* optval */arg(A3), buf, optlen);
				}

			}
			sys_output("OSF_SYS_getsockopt: socket(%lld) level(%lld) opt_name(%lld) &opt_val(0x%llx - %d) opt_len(%lld) ",arg(A0),arg(A1),arg(A2),arg(A3),*(int *)buf,optlen);

			//pick the correct translation table
			if((int)arg(A1) == OSF_SOL_SOCKET)
			{
				map = sockopt_map;
				nmap = N_ELT(sockopt_map);
			}
			else if((int)arg(A1) == OSF_SOL_TCP)
			{
				map = tcpopt_map;
				nmap = N_ELT(tcpopt_map);
			}
			else
			{
				warn("no translation map available for `setsockopt()': %d",(int)arg(A1));
				map = sockopt_map;
				nmap = N_ELT(sockopt_map);
			}
			int level = xlate_arg(arg(A1),socklevel_map, N_ELT(socklevel_map), "setsockopt(level)");
			int optname = xlate_arg(arg(A2), map, nmap, "setsockopt(opt)");

			int optlen_pass = (int)optlen;
			arg(V0) = getsockopt(/* sock */(int)arg(A0), level, optname, /* optval */buf, (socklen_t *)&optlen_pass);
			check_error = true;
			delete [] buf;
		}
		break;

	case OSF_SYS_getsockname:
		{
			//get socket name
			char *buf = NULL;
			md_gpr_t osf_addrlen;

			//get simulator memory parameters to host memory
			mem->mem_bcopy(Read, arg(A2), &osf_addrlen, sizeof(osf_addrlen));
			int addrlen = (int)osf_addrlen;
			sys_output("OSF_SYS_getsockname: socket(%lld) sockaddr(0x%llx) socklen(0x%llx - %d) ",arg(A0),arg(A1),arg(A2),addrlen);
			if(addrlen)
			{
				buf = new char[addrlen];
				memset(buf,0,addrlen);

				arg(V0) = getsockname(arg(A0), (sockaddr *)buf, (socklen_t *)&addrlen);

				sys_output("(%s) ",buf);
				if(addrlen)
				{
					//addrlen can be greater than the allowed space
					mem->mem_bcopy(Write, arg(A1), buf, (int)osf_addrlen);

					//Write back addrlen, not what we read
					mem->mem_bcopy(Write, arg(A2), &addrlen, sizeof(addrlen));
				}
			}
			else
			{
				arg(V0) = (md_gpr_t)-1;
				errno = ENOBUFS;
			}

			check_error = true;
			delete [] buf;
		}
		break;

	case OSF_SYS_old_getpeername:
		{
			//get socket name
			char *buf;
			word_t osf_addrlen;
			int addrlen;

			//get simulator memory parameters to host memory
			mem->mem_bcopy(Read, /* paddrlen */arg(A2), &osf_addrlen, sizeof(osf_addrlen));
			addrlen = (int)osf_addrlen;
			if(addrlen != 0)
			{
				buf = (char *)calloc(1, addrlen);
				if(!buf)
				{
					fatal("cannot allocate memory in OSF_SYS_old_getsockname");
				}
			}
			else
			{
				buf = NULL;
			}
			/* result */arg(V0) = getpeername(/* sock */(int)arg(A0),/* name */(sockaddr *)buf, /* namelen */(socklen_t *)&addrlen);
			check_error = true;

			//copy results to simulator memory
			if(addrlen != 0)
			{
				mem->mem_bcopy(Write, /* addr */arg(A1), buf, addrlen);
			}
			osf_addrlen = (qword_t)addrlen;
			mem->mem_bcopy(Write, /* paddrlen */arg(A2), &osf_addrlen, sizeof(osf_addrlen));

			if(buf != NULL)
			{
				free(buf);
			}
		}
		break;

	case OSF_SYS_setgid:
		arg(V0) = 0;
		contexts[context_id].gid = arg(A0);
		check_error = true;
		sys_output("OSF_SYS_setgid: (0x%llx)\t",arg(A0));
		break;

	case OSF_SYS_setuid:
		arg(V0) = setuid(/* uid */(uid_t)arg(A0));
		check_error = true;
		sys_output("OSF_SYS_setuid: (0x%llx)\t",arg(A0));
		break;

	case OSF_SYS_getpriority:
		//get program scheduling priority
		arg(V0) = getpriority(/* which */(int)arg(A0), /* who */(int)arg(A1));
		check_error = true;
		break;

	case OSF_SYS_setpriority:
		//set program scheduling priority
		arg(V0) = setpriority(/* which */(int)arg(A0), /* who */(int)arg(A1), /* prio */(int)arg(A2));
		check_error = true;
		break;

	case OSF_SYS_select:
		{
			//FIXME: Flags must be redirected
			//FIXME: timeout must be handled (it must be set to 0 (polling) for simulation purposes, but we must handle it somehow)

			fd_set readfd, writefd, exceptfd;
			fd_set *readfdp(NULL), *writefdp(NULL), *exceptfdp(NULL);
			timeval timeout, *timeoutp(NULL);

			//copy read file descriptor set into host memory
			if(arg(A1) != 0)
			{
				mem->mem_bcopy(Read, arg(A1), &readfd, sizeof(fd_set));
				readfdp = &readfd;
			}

			//copy write file descriptor set into host memory
			if(arg(A2) != 0)
			{
				mem->mem_bcopy(Read, arg(A2), &writefd, sizeof(fd_set));
				writefdp = &writefd;
			}

			//copy exception file descriptor set into host memory
			if(arg(A3) != 0)
			{
				mem->mem_bcopy(Read, arg(A3), &exceptfd, sizeof(fd_set));
				exceptfdp = &exceptfd;
			}

			//copy timeout value into host memory
			if(arg(A4) != 0)
			{
				mem->mem_bcopy(Read, arg(A4), &timeout, sizeof(timeval));
				timeoutp = &timeout;
			}

#ifdef SYS_DEBUG
			static unsigned long long counter = 0;
			static md_addr_t source = regs->regs_PC;
			if(regs->regs_PC == source)
			{
				counter++;
			}
			else
			{
				source = regs->regs_PC;
				counter = 1;
			}
			if(counter==1)
			{
				std::cerr << "OSF_SYS_select: nfds(" << arg(A0) << ") ";
				std::cerr << "readfile " << arg(A1) << "(";// << readfd << ")\t";
				for(size_t i=0;readfdp && i<(__FD_SETSIZE / __NFDBITS);i++)
				{
					std::cerr << readfd.fds_bits[i] << " ";
				}
				std::cerr << ")\t";
				std::cerr << "writefile " << arg(A2) << "(";// << writefd << ")\t";
				for(size_t i=0;writefdp && i<(__FD_SETSIZE / __NFDBITS);i++)
				{
					std::cerr << writefd.fds_bits[i] << " ";
				}
				std::cerr << ")\t";
				std::cerr << "exceptfile " << arg(A3) << "(";// << exceptfd << ")\t";
				for(size_t i=0;exceptfdp && i<(__FD_SETSIZE / __NFDBITS);i++)
				{
					std::cerr << exceptfd.fds_bits[i] << " ";
				}
				std::cerr << ")\t";
				std::cerr << "timeout " << arg(A4);
				if(timeoutp)
				{
					std::cerr << "(" << timeout.tv_sec << ", " << timeout.tv_usec << ")\t";
				}
			}

#endif
#if 1
			static int ignores = 0;
			ignores++;
			if(ignores<200000)
			{
				arg(V0) = 0;
				contexts[context_id].regs.regs_PC = regs->regs_PC-8;
				contexts[context_id].regs.regs_NPC = regs->regs_PC+4;
				check_error = false;
				break;
			}
			ignores = 0;
#endif
			if(contexts[context_id].fastfwd_left == -1)
			{
				sys_output("%llx(+%lld): ",regs->regs_PC,contexts[context_id].sim_num_insn);
			}
			else
			{
				sys_output("%llx(#%lld): ",regs->regs_PC,contexts[context_id].fastfwd_left);
			}
			sys_output("Syscall(%d): %lld\t",context_id,syscode);

			sys_output("Calling select: (");
			for(size_t i=0;readfdp && i<(__FD_SETSIZE / __NFDBITS);i++)
			{
				sys_output("%d ",readfd.fds_bits[i]);
			}
			sys_output(")\t");

			if(!timeoutp)
			{
				timeout.tv_sec = 0;
				timeout.tv_usec = 0;
				timeoutp = &timeout;
			}

#if defined(hpux) || defined(__hpux)
			//select() on the specified file descriptors
			arg(V0) = select(/* nfds */arg(A0), (int *)readfdp, (int *)writefdp, (int *)exceptfdp, timeoutp);
#else
			//select() on the specified file descriptors
			arg(V0) = select(/* nfds */arg(A0), readfdp, writefdp, exceptfdp, timeoutp);
#endif
			check_error = true;

			if(readfd.fds_bits[0]!=0)
			{
				sys_output("Select has a valid value of: %d\n", readfd.fds_bits[0]);
			}

			if((arg(V0)==0) || (arg(V0)==(md_gpr_t)-1))
			{
				sys_output("Select failed with value: %lld\n");
				for(size_t i=0;readfdp && i<(__FD_SETSIZE / __NFDBITS);i++)
				{
					sys_output("%d ",readfd.fds_bits[i]);
				}
				sys_output(")\n");
				contexts[context_id].regs.regs_PC = regs->regs_PC-8;
				contexts[context_id].regs.regs_NPC = regs->regs_PC+4;
				check_error = false;

				contexts[context_id].interrupts |= 0x20000;
				contexts[context_id].nfds = arg(A0);
				contexts[context_id].readfd = readfd;
				contexts[context_id].writefd = writefd;
				contexts[context_id].exceptfd = exceptfd;
				contexts[context_id].timeout = timeout;
				contexts[context_id].next_check = 200000;
			}
			else
			{
				sys_output("Select has passed! With value: %lld\n",arg(V0));
				//copy read file descriptor set to target memory
				if(arg(A1) != 0)
				{
					mem->mem_bcopy(Write, /* readfds */arg(A1), &readfd, sizeof(fd_set));
				}

				//copy write file descriptor set to target memory
				if(arg(A2) != 0)
				{
					mem->mem_bcopy(Write, /* writefds */arg(A2), &writefd, sizeof(fd_set));
				}

				//copy exception file descriptor set to target memory
				if(arg(A3) != 0)
				{
					mem->mem_bcopy(Write, /* exceptfds */arg(A3), &exceptfd, sizeof(fd_set));
				}

				//copy timeout value result to target memory
				if(arg(A4) != 0)
				{
					mem->mem_bcopy(Write, /* timeout */arg(A4), &timeout, sizeof(timeval));
				}
			}
			break;
		}
		break;

	case OSF_SYS_shutdown:
		//shuts down socket send and receive operations
		sys_output("OSF_SYS_shutdown: socket(%lld) how(%lld) ",arg(A0),arg(A1));
		arg(V0) = shutdown(arg(A0), arg(A1));
		check_error = true;
		break;

	case OSF_SYS_poll:
		{
			//allocate host side I/O vectors
			pollfd *fds = (pollfd *)calloc(/* nfds */arg(A1), sizeof(pollfd));
			if(!fds)
			{
				fatal("out of virtual memory in SYS_poll");
			}

			//copy target side I/O vector buffers to host memory
			for(unsigned int i=0; i < /* nfds */arg(A1); i++)
			{
				//copy target side pointer data into host side vector
				mem->mem_bcopy(Read,(/* fds */arg(A0) + i*sizeof(pollfd)), &fds[i], sizeof(pollfd));
			}

			//perform the vector'ed write
			arg(V0) = poll(fds,/* nfds */(unsigned long)arg(A1),/* timeout */(int)arg(A2));
			check_error = true;

			//copy target side I/O vector buffers to host memory
			for(unsigned int i=0; i < /* nfds */arg(A1); i++)
			{
				//copy target side pointer data into host side vector
				mem->mem_bcopy(Write,(/* fds */arg(A0)+ i*sizeof(pollfd)),&fds[i], sizeof(pollfd));
			}

			//free all the allocated memory
			free(fds);
		}
		break;

	case OSF_SYS_usleep_thread:
		{
			static int var = 200;
			var = (var + 1)%1000;
			unsigned int useconds = 0;
			mem->mem_bcopy(Read, arg(A0), &useconds, sizeof(unsigned int));
			sys_output("OSF_SYS_usleep_thread (%d) ", useconds);
			contexts[context_id].sleep = useconds*1000 + var;

			arg(V0) = 0;
			check_error = true;
		}
		break;

	case OSF_SYS_gethostname:
		{
			char *buf = new char[(size_t)arg(A1)];
			arg(V0) = gethostname(buf, (size_t)arg(A1));
			check_error = true;

			//copy string back to simulated memory
			mem->mem_bcopy(Write, /* name */arg(A0), buf, /* len */arg(A1));
			sys_output("OSF_SYS_gethostname (%s) ", buf);
			delete [] buf;
		}
		break;

	case OSF_SYS_madvise:
		sys_output("OSF_SYS_madvise: addr 0x%llx size: 0x%llx behavior(%lld)\t",arg(A0),arg(A1),arg(A2));

#ifdef WARN_ALL
		warn("partially unsupported madvise() call ignored...");
#endif
		check_error = true;
		if(arg(A2) >= 7)
		{
			arg(V0) = EINVAL;
			arg(A3) = -1;
		}
		else
		{
			//#define MADV_NORMAL		0		no further special treatment
			//#define MADV_RANDOM		1		expect random page references
			//#define MADV_SEQUENTIAL	2		expect sequential page references
			//#define MADV_WILLNEED		3		will need these pages
			//#define MADV_DONTNEED_COMPAT	4		for backward compatibility
			//#define MADV_SPACEAVAIL	5		ensure resources are available
			//#define MADV_DONTNEED		6		dont need these pages
			//FIXME: We only need to handle MADV_SPACEAVAIL since it can generate errno ENOSPC (resources can't be reserved)
		}
		arg(V0) = 0;
		break;

	case OSF_SYS_getgroups:
		{
			sys_output("OSF_SYS_getgroups: num_groups(%lld) into_mem(0x%llx) ",arg(A0),arg(A1));
			check_error = true;

			arg(V0) = 1;
			gid_t group_id = contexts[context_id].gid;
			mem->mem_bcopy(Write, arg(A1), &group_id, sizeof(gid_t));
			break;

			int num_groups = arg(A0);
			long long max_groups(num_groups);
			if(max_groups==0)
			{
				max_groups = sysconf(_SC_NGROUPS_MAX) + 1;
			}
			gid_t *groups = new gid_t[max_groups];

			for(unsigned int i=0;i<arg(A0);i++)
			{
				sys_output("%d ",groups[i]);
			}
			sys_output(")");

			arg(V0) = getgroups(max_groups,groups);
			mem->mem_bcopy(Write, arg(A1), groups, max_groups*sizeof(gid_t));
			delete [] groups;
		}
		break;

	case OSF_SYS_mkdir:
		{
			std::string dirname = get_filename(mem,arg(A0));
			int mode = arg(A1);
			check_error = true;
			arg(V0) = mkdir(dirname.c_str(),mode);
			sys_output("OSF_SYS_mkdir: dirname(%s) mode(%o)",dirname.c_str(),arg(A1));
		}
		break;

	case OSF_SYS_rmdir:
		{
			std::string dirname = get_filename(mem,arg(A0));
			check_error = true;
			arg(V0) = rmdir(dirname.c_str());
			sys_output("OSF_SYS_rmdir: dirname(%s) ",dirname.c_str());
		}
		break;

	case OSF_SYS_rename:
		{
			std::string oldpath = get_filename(mem,arg(A0));
			std::string newpath = get_filename(mem,arg(A1));
			sys_output("OSF_SYS_rename: renaming(%s) to (%s)\n",oldpath.c_str(),newpath.c_str());
			check_error = true;
			arg(V0) = rename(oldpath.c_str(),newpath.c_str());
		}
		break;

	case OSF_SYS_profil:
		{
			warn("Poorly supported profil");
			//No return value
		}
		break;

	case OSF_SYS_mprotect:
		{
			warn("partially supported mprotect (no protections changed)");
			check_error = true;
			arg(V0) = 0;
		}
		break;

	case OSF_SYS_msync:
			//We probably only have to handle real mmaps. Everything else is synched immediately.
			sys_output("OSF_SYS_msync: addr(0x%llx) len(%lld) flags(%lld)\t",arg(A0),arg(A1),arg(A2));
			check_error = true;
			arg(V0) = 0;
		break;

	case OSF_SYS_mmap:
		{
			unsigned long long addr(arg(A0));
			long long len(arg(A1));
			int prot(arg(A2)), flags(arg(A3));
			md_gpr_t fd(arg(A4));
			long long offset(arg(A5));

			bool A4redirected = contexts[context_id].file_table.require_redirect(fd);
			if(A4redirected)
			{
				sys_output("(redirecting %lld to %lld) ",arg(A4),fd);
			}
			sys_output("OSF_SYS_mmap: addr(0x%llx) len(%lld) prot(%lld) flags(%lld) fd(%lld) offset(%lld)\t",arg(A0),arg(A1),arg(A2),arg(A3),fd,arg(A5));

			arg(V0) = mem->mem_map(addr,fd,offset,len,prot,flags);
			arg(A3) = 0;

			sys_output("Returning address: 0x%llx",arg(V0));
			break;
		}
		break;

	//As long as mmap is supported, this should be ok. Ideally, some memory manager would handle this (at least replicate bounds, etc).
	case OSF_SYS_munmap:
		{
			sys_output("OSF_SYS_munmap: 0x%llx\t",arg(A0));
			mem->mem_unmap(arg(A0));
			arg(A3) = 0;
			arg(V0) = 0;
			break;
		}
		break;


	case OSF_SYS_set_program_attributes:
		{
			sys_output("OSF_SYS_set_program_attributes: Text_Base(0x%llx) Text_Size(0x%llx) Data_Base(0x%llx) Data_Size(0x%llx)\t",arg(A0),arg(A1),arg(A2),arg(A3));
			mem->ld_text_base = arg(A0);
			mem->ld_text_size = arg(A1);
			mem->ld_data_base = arg(A2);
			mem->ld_data_size = arg(A3);

			check_error = true;
			arg(V0) = 0;
			arg(A3) = 0;
		}
		break;

	case OSF_SYS_readlink:
		{
			std::string filename = get_filename(mem,arg(A0));
			int buf_size = arg(A2);
			char read_file[buf_size];

			arg(V0) = readlink(filename.c_str(),read_file,buf_size);
			check_error = true;
			sys_output("OSF_SYS_readlink: trying to readlink (%s) with buf_size(%lld) -> Result(%s) ",filename.c_str(),arg(A2),read_file);
		}
		break;

	case OSF_SYS_execve:
		{
			std::string filename = get_filename(mem,arg(A0));
			sys_output("OSF_SYS_execve: inst_pc(0x%llx) path(0x%llx %s) argv(0x%llx) envp(0x%llx)\n",regs->regs_PC,arg(A0),filename.c_str(),arg(A1),arg(A2));

			std::vector<std::string> argv;
			md_addr_t addr;
			md_addr_t argvaddr = arg(A1);
			mem->mem_bcopy(Read, argvaddr, &addr, sizeof(md_addr_t));
			while(addr)
			{
				argv.push_back(std::string());
				mem->mem_strcpy(Read, addr, argv.back());
				argvaddr+=sizeof(md_addr_t);
				mem->mem_bcopy(Read, argvaddr, &addr, sizeof(md_addr_t));
			}

			//FIXME: This may not be a good idea, it is possible for argv[0] not to be the filename
			argv[0] = filename;

			std::vector<std::string> envp;
			md_addr_t envpaddr = arg(A2);
			mem->mem_bcopy(Read, envpaddr, &addr, sizeof(md_addr_t));
			while(addr)
			{
				envp.push_back(std::string());
				mem->mem_strcpy(Read, addr, envp.back());

				envpaddr+=sizeof(md_addr_t);
				mem->mem_bcopy(Read, envpaddr, &addr, sizeof(md_addr_t));
			}

			if(access(filename.c_str(), F_OK)==-1)
			{
				arg(A3) = errno;
				arg(V0) = -1;
				sys_output("\texecve failed!");
				break;
			}

			//exec_flush must occur first, otherwise it will purse pages that may have been loaded dynamically
			//However, this causes exec to have no possible return path on failure (if the target can be loaded into memory at all).
			mem->exec_flush();
			int load_ret = loader.ld_load_prog(filename, argv, envp, &contexts[context_id].regs, contexts[context_id].mem, 1);
			if(load_ret)
			{
				arg(A3) = load_ret;
				arg(V0) = -1;
				sys_output("\texecve failed at load time, this is probably fatal to the thread");
				//FIXME, need to recover from mem->exec_flush
				break;
			}
			contexts[context_id].regs.regs_NPC = contexts[context_id].mem->ld_prog_entry+4;

			//The -=0x10 is probably because fastfwd would increment the PC by 4 after this, causing the first instruction to be skipped (SP -= 16).
			//In non-fastfwd, we didn't skip the first inst, so, we had to skip the instruction in the fetching handler.
			//We should figure out a way to handle this without the -=0x10... although, it may not be clean (efficient) to do so.
			//For now, let's make sure to skip that instruction.....
			mem->ld_environ_base -= 0x10;
			regs->regs_R[MD_REG_SP] = mem->ld_environ_base;
			regs->regs_C.uniq = 0;

			contexts[context_id].filename = filename;
			contexts[context_id].pred->retstack.clear();

			//Handle FD_CLOEXEC for all open handles:
			contexts[context_id].file_table.handle_cloexec();

			//If we are not fast forwarding, abort this instruction
			if(contexts[context_id].icount)
			{
				contexts[context_id].interrupts |= 0x10000000;
			}
			//No return values, the old execution is gone.
		}
		break;

	case OSF_SYS_wait4:
		{
			sys_output("OSF_SYS_wait4: pid(%lld) statloc(0x%llx) options(%lld) deprecated(0x%llx)",arg(A0),arg(A1),arg(A2),arg(A3));

			arg(A3) = 0;
			arg(V0) = arg(A0);

			//get_retval sets arg(V0) and arg(A3) with the return value (A3) and the source child's pid (V0) on success. Otherwise, no change.
			if(!pid_handler.get_retval(contexts[context_id].pid,arg(V0),arg(A3)))
			{
				//FIXME: Can we just cancel this instruction somehow?
				arg(V0) = 0;
				//FIXME: This is causing the prior instruction (and the syscall) to be refetched over and over (we just want the syscall... and not even that).
//				contexts[context_id].regs.regs_PC = regs->regs_PC-8;
//				contexts[context_id].regs.regs_NPC = regs->regs_PC+4;
				contexts[context_id].regs.regs_PC = regs->regs_PC-8;
				contexts[context_id].regs.regs_NPC = regs->regs_PC+4;
//				contexts[context_id].fetch_regs_PC = regs->regs_PC+4;

				contexts[context_id].interrupts |= 0x10000;
				contexts[context_id].waiting_for = arg(A0);
				break;
			}
			if(arg(V0) != (md_gpr_t)-1)
			{
				mem->mem_bcopy(Write, arg(A1), &arg(A3), 4);
				arg(A3) = 0;
			}
			else
			{
				arg(A3) = ECHILD;
			}
			//sys_output("Wait passed %llx, %llx statloc(%llx)\n", arg(V0), arg(A3), arg(A1));
			//If this fails, arg(V0) is already -1 and arg(A3) is ECHILD
		}
		break;


	case OSF_SYS_fork:
		{
			//FIXME: Orignally, we wanted fork to create the thread on a separate core. This was harder than anticipated.
			//Adding the thread to the same core requires that we know the number of threads in advance (for architectural registers)
			//Perhaps we can add an auto resizer... adds a set of registers if it is needed (if allowed to do so).
			sys_output("OSF_SYS_fork: inst_pc(0x%llx)\t",regs->regs_PC);

			int core_id = contexts[context_id].core_id;
			sys_output("Forking from context %d on core %d\n",context_id,core_id);

//			//This code was part of the attempt to move to a separate core, it remains since we still have that goal in mind.
//			//Add the new core
//			cores.push_back(cores[core_id]);
//			//Fix the new core's id
//			cores.back().id = cores.size()-1;
//			//Create the copy
//			contexts.push_back(contexts[context_id]);
//			int new_context_id = contexts.size()-1;
//			contexts[new_context_id].regs->context_id = new_context_id;
//			contexts[new_context_id].mem->context_id = new_context_id;
//			//Fix core_id
//			contexts[new_context_id].core_id = core_id;
//			//Fix context resources that belong to the core...

			context * new_context = new context(contexts[context_id]);
			new_context->id = num_contexts;
			new_context->regs.context_id = num_contexts;

			//Are any instructions in flight? (We are not fast-forwarding if they are)
			bool adjust_NPC = true;
			if(new_context->icount)
			{
				assert(!new_context->ROB_num);
				assert(!new_context->LSQ_num);
				new_context->fetch_num = new_context->icount = 0;
				new_context->fetch_head = new_context->fetch_tail = 0;

				//Do not adjust next PC if icount is not 0
				adjust_NPC = false;
				new_context->fetch_pred_PC = new_context->fetch_regs_PC = regs->regs_PC+4;
			}
			new_context->core_id = -1;
			new_context->sim_num_insn = 0;

			for(unsigned int i=0;(i<cores.size() && (new_context->core_id==-1));i++)
			{
				if(!cores[i].addcontext(*new_context))
				{
					sys_output("Could not add forked context(%d) to core %d\n",num_contexts,i);
				}
			}

			if(new_context->core_id == -1)
			{
				//FIXME: When fork fails, the parent never appears to know (verify, the tester code may have been too dumb to realize this)

				//delete new_context;	Make sure this is not a problem before uncommenting.
				arg(A3) = EAGAIN;
				arg(V0) = -1;
			}
			else
			{
				contexts.push_back(*new_context);

				//Remember, pointers may be broken at this point.
				regs = &contexts[context_id].regs;

				//FIXME: Next statement can probably be removed.
				contexts.back().id = num_contexts;

				num_contexts++;

			        contexts.back().mem->name = std::string("Thread_Forked");
			        contexts.back().mem->context_id = contexts.back().id;

				//The forked context is not in the pipeline so it won't advance to the next instruction. This fixes that behavior.
				if(adjust_NPC)
				{
					contexts.back().regs.regs_PC = regs->regs_PC+4;
					contexts.back().regs.regs_NPC = regs->regs_PC+8;
				}

				//arg(A4) determines which is the child
				unsigned long long child_pid = pid_handler.get_new_pid();
				contexts.back().regs.regs_R[MD_REG_A3] = arg(A3) = 0;
				contexts.back().regs.regs_R[MD_REG_A4] = 1;
				contexts.back().regs.regs_R[MD_REG_V0] = arg(V0) = contexts.back().pid = child_pid;
				contexts.back().gpid = contexts[context_id].gpid;
				contexts.back().gid = contexts[context_id].gid;
				arg(A4) = 0;
				contexts.back().file_table.copy_from(contexts[context_id].file_table);
				pid_handler.add_child(contexts[context_id].pid,child_pid);
				contexts.back().fastfwd_cnt = contexts[context_id].fastfwd_left;
				contexts.back().fastfwd_left = contexts[context_id].fastfwd_left;

				//Fix dlite_evaluator pointers.
				contexts.back().dlite_evaluator = contexts[context_id].dlite_evaluator->repair_ptrs(&contexts[context_id].regs, &contexts.back().regs, contexts[context_id].mem, contexts.back().mem);
			}
			delete new_context;
			//check_error = true;
		}
		break;

	case OSF_SYS_mvalid:
			sys_output("OSF_SYS_mvalid: addr(0x%llx) len(%lld) prot(%lld) (assuming valid)\t",arg(A0),arg(A1),arg(A2));
			//Need to check if mem[addr,addr+len) can be accessed with the access options specified by prot (PROT_READ,write,exec)
			arg(V0) = 0;
			check_error = true;
		break;

	case OSF_SYS_fsync:
			sys_output("OSF_SYS_fsync: fd(%lld)\t",arg(A0));
			arg(V0) = fsync(arg(A0));
			check_error = true;
		break;

	case OSF_SYS_audcntl:
		{
			static md_gpr_t audcntl = 0x002;	//Default value is AUDIT_OFF
			sys_output("OSF_SYS_audcntl: request(%lld) argp(0x%llx) len(%lld) flag(%lld) audit_id(%lld) pid(%lld) (FIXME)\t",arg(A0),arg(A1),arg(A2),arg(A3),arg(A4),arg(A5));
			//sys/audit.h
			arg(V0) = 0;
			char *buf = NULL;
			switch(arg(A0))
			{
			case 4:		//GET_PROC_AMASK
				//No auditmask exists, reporting 0 (assuming length is valid)
				if(arg(A1) && arg(A2))
				{
					buf = new char[arg(A2)];
					memset(buf, 0, arg(A2));
					mem->mem_bcopy(Write, arg(A1), buf, arg(A2));
					arg(V0) = arg(A2);
				}
				else
				{
					arg(V0) = (md_gpr_t)-1;
				}
				break;
			case 5:		//SET_PROC_AMASK
				//Can't set anything. Just assuming we can (we could make a static variable for this purpose)...
				if(arg(A1) && arg(A2))
				{
					arg(V0) = arg(A2);
				}
				else
				{
					arg(V0) = (md_gpr_t)-1;
				}
				break;
			case 6:		//GET_PROC_ACNTL
				arg(V0) = audcntl;
				break;
			case 7:		//SET_PROC_ACNTL
				arg(V0) = audcntl;
				audcntl = arg(A4);
				break;

			default:
				sys_output("Unsupported audcntl request\t");
			}
			delete [] buf;
			check_error = true;
		}
		break;

	case OSF_SYS_getlogin:
		{
			std::string filename = get_filename(mem,arg(A0));
			sys_output("OSF_SYS_getlogin: into(%llx) size(%lld)\tAssumes \"msim_user\"\t",arg(A0),arg(A1));
			std::string buf = "msim_user";
			mem->mem_bcopy(Write, arg(A0), &buf[0], buf.size() );
			arg(A3) = 0;
			arg(V0) = arg(A0);
			check_error = true;
		}
		break;

	case OSF_SYS_pathconf:
		{
			std::string filename = get_filename(mem,arg(A0));
			sys_output("OSF_SYS_pathconf: file(%s) val(%lld)\tUnimplemented\t",filename.c_str(),arg(A1));
			arg(A3) = -1;
			arg(V0) = 0;
		}
		break;

	case OSF_SYS_fpathconf:
		sys_output("OSF_SYS_fpathconf: fd(%lld) val(%lld)\tUnimplemented\t",arg(A0),arg(A1));
		arg(A3) = -1;
		arg(V0) = 0;
		break;

	case OSF_MACH_task_init:
		//2 params, verified by forcing an error.
		sys_output("OSF_MACH_task_init:0(0x%llx) 1(0x%llx) ",arg(A0),arg(A1));
		long long a0, a1;
		mem->mem_bcopy(Read, arg(A0), &a0, sizeof(long long));
		mem->mem_bcopy(Read, arg(A1), &a1, sizeof(long long));
		sys_output(" read 0(0x%llx) 1(0x%llx) ", a0, a1);
		a1 = 13500;
		mem->mem_bcopy(Write, arg(A1), &a1, sizeof(long long));
		a1 = 14000;
		mem->mem_bcopy(Write, arg(A0), &a1, sizeof(long long));
		mem->mem_bcopy(Read, arg(A0), &a0, sizeof(long long));
		mem->mem_bcopy(Read, arg(A1), &a1, sizeof(long long));
		sys_output(" read 0(0x%llx) 1(0x%llx) ", a0, a1);

		fprintf(stderr,"Unsupported MACH\n");
//		check_error = true;
//		arg(A3) = 0;
		arg(V0) = 0;		//Error code
		break;


	case OSF_MACH_stack_create:
		{
			sys_output("OSF_MACH_stack_create (experimental): vm_stack(0x%llx) ",arg(A0));
			vm_stack request;
			mem->mem_bcopy(Read, arg(A0), &request, sizeof(vm_stack));
			sys_output("addr_hint(0x%llx) redsize(%lld) yellowsize(%lld) greensize(%lld) swap(%lld) increment(%lld) align(%lld) flags(%lld) alloc_policy_addr(0x%llx)", request.address, request.rsize, request.ysize, 
				request.gsize, request.swap, request.incr, request.align, request.flags, request.attr);
			fprintf(stderr, "stack_create support is not available, return value will likely fail.\n");

			//Copy mmap semantics
			unsigned long long addr(request.address);
			long long len(request.rsize + request.ysize + request.gsize);
			int flags(request.flags);
			md_gpr_t fd((md_gpr_t)-1);
			long long offset(0);

			//Assuming full prot configuration
			int prot(0777);

			arg(V0) = -1;
			request.address = mem->mem_map(addr,fd,offset,len,prot,flags | OSF_MAP_ANON);
			if(request.address != (md_addr_t)-1)
			{
				arg(V0) = 0;
				mem->mem_bcopy(Write, arg(A0), &request, sizeof(vm_stack));
			}
			//arg(A3) = 0;

			sys_output("Returning address: 0x%llx",arg(V0));

		}
		break;

	default:
		warn("invalid/unimplemented syscall %ld, PC=0x%08p, RA=0x%08p, winging it",syscode, regs->regs_PC, regs->regs_R[MD_REG_RA]);
		sys_output("Args: 0(%lld) 1(%lld) 2(%lld) 3(%lld) 4(%lld) 5(%lld)\n",arg(A0),arg(A1),arg(A2),arg(A3),arg(A4),arg(A5));
		sys_output("Args: 0(0x%llx) 1(0x%llx) 2(0x%llx) 3(0x%llx) 4(0x%llx) 5(0x%llx)\n",arg(A0),arg(A1),arg(A2),arg(A3),arg(A4),arg(A5));
		arg(A3) = -1;
		arg(V0) = 0;
		break;
	}

	//Generic check for an error condition
	if(check_error)
	{
		if(arg(V0) != (qword_t)-1)
		{
			arg(A3) = 0;
			sys_output("\tSuccess: %lld",arg(V0));
		}
		else	//got an error, return details
		{
			arg(A3) = -1;
			arg(V0) = errno;
			sys_output("\tFailed: Errno(%lld)",arg(V0));
		}
	}

	//Restore MD_REG_A0 value where appropriate
	if(redirected)
	{
		arg(A0) = oldA0;
	}

	if(verbose)
	{
		fprintf(stderr, "syscall(%d): returned %d:%d...\n",(int)syscode, (int)arg(A3), (int)arg(V0));
	}
#undef arg
}
