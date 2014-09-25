/*	$OpenBSD: linux_types.h,v 1.13 2013/10/25 04:51:39 guenther Exp $	*/
/*	$NetBSD: linux_types.h,v 1.5 1996/05/20 01:59:28 fvdl Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUX_TYPES_H_
#define _LINUX_TYPES_H_

typedef struct {
	long	val[2];
} linux_fsid_t;

typedef unsigned short linux_uid_t;
typedef unsigned short linux_gid_t;
typedef unsigned short linux_dev_t;
typedef unsigned long long linux_ino64_t;
typedef unsigned long linux_ino_t;
typedef unsigned short linux_mode_t;
typedef unsigned short linux_nlink_t;
typedef long linux_time_t;
typedef long linux_clock_t;
typedef long long linux_off64_t;
typedef long linux_off_t;
typedef u_int64_t linux_loff_t;
typedef int linux_pid_t;

#define LINUX_TIME_MAX	LONG_MAX
#define LINUX_INO_MAX	ULONG_MAX
#define LINUX_INO64_MAX	ULLONG_MAX

#define LINUX_FSTYPE_FFS	0x11954
#define LINUX_FSTYPE_NFS	0x6969
#define LINUX_FSTYPE_MSDOS	0x4d44
#define LINUX_FSTYPE_PROCFS	0x9fa0
#define LINUX_FSTYPE_EXT2FS	0xef53
#define LINUX_FSTYPE_CD9660	0x9660
#define LINUX_FSTYPE_NCPFS	0x6969
#define LINUX_FSTYPE_NTFS	0x5346544e	/* "NTFS" */
#define LINUX_FSTYPE_UDF	0x15013346
#define LINUX_FSTYPE_AFS	0x5346414f

/*
 * Linux version of time-based structures, passed to many system calls
 */
struct linux_timespec {
	linux_time_t	tv_sec;
	long		tv_nsec;
};

struct linux_timeval {
	linux_time_t	tv_sec;
	long		tv_usec;
};

struct linux_itimerval {
	struct linux_timeval	it_interval;
	struct linux_timeval	it_value;
};

/*
 * Passed to the statfs(2) system call family
 */
struct linux_statfs {
	long		l_ftype;
	long		l_fbsize;
	long		l_fblocks;
	long		l_fbfree;
	long		l_fbavail;
	long		l_ffiles;
	long		l_fffree;
	linux_fsid_t	l_ffsid;
	long		l_fnamelen;
	long		l_fspare[6];
};

struct linux_statfs64 {
        int		l_ftype;
        int		l_fbsize;
        uint64_t	l_fblocks;
        uint64_t	l_fbfree;
        uint64_t	l_fbavail;
        uint64_t	l_ffiles;
        uint64_t	l_fffree;
        linux_fsid_t	l_ffsid;
        int		l_fnamelen;
        int		l_fspare[6];
};

/*
 * Passed to the uname(2) system call
 */
struct linux_utsname {
	char l_sysname[65];
	char l_nodename[65];
	char l_release[65];
	char l_version[65];
	char l_machine[65];
	char l_domainname[65];
};

struct linux_oldutsname {
	char l_sysname[65];
	char l_nodename[65];
	char l_release[65];
	char l_version[65];
	char l_machine[65];
};

struct linux_oldoldutsname {
	char l_sysname[9];
	char l_nodename[9];
	char l_release[9];
	char l_version[9];
	char l_machine[9];
};

/*
 * Passed to the mmap() system call
 */
struct linux_mmap {
	caddr_t lm_addr;
	int lm_len;
	int lm_prot;
	int lm_flags;
	int lm_fd;
	int lm_pos;
};

/*
 * Passed to the select() system call
 */
struct linux_select {
	int nfds;
	fd_set *readfds;
	fd_set *writefds;
	fd_set *exceptfds;
	struct linux_timeval *timeout;
};

struct linux_stat {
	linux_dev_t		lst_dev;
	unsigned short		pad1;
	linux_ino_t		lst_ino;
	linux_mode_t		lst_mode;
	linux_nlink_t		lst_nlink;
	linux_uid_t		lst_uid;
	linux_gid_t		lst_gid;
	linux_dev_t		lst_rdev;
	unsigned short		pad2;
	linux_off_t		lst_size;
	unsigned long		lst_blksize;
	unsigned long		lst_blocks;
	linux_time_t		lst_atime;
	unsigned long		unused1;
	linux_time_t		lst_mtime;
	unsigned long		unused2;
	linux_time_t		lst_ctime;
	unsigned long		unused3;
	unsigned long		unused4;
	unsigned long		unused5;
};

struct linux_tms {
	linux_clock_t ltms_utime;	
	linux_clock_t ltms_stime;
	linux_clock_t ltms_cutime;
	linux_clock_t ltms_cstime;
};

struct linux_utimbuf {
	linux_time_t l_actime;
	linux_time_t l_modtime;
};

struct linux___sysctl {
	int          *name;
	int           namelen;
	void         *old;
	size_t       *oldlenp;
	void         *new;
	size_t        newlen;
	unsigned long __unused0[4];
};

/* This matches struct stat64 in glibc2.1, hence the absolutely
 * insane amounts of padding around dev_t's.
 */
struct linux_stat64 {
	unsigned long long lst_dev;
	unsigned int	__pad1;

#define LINUX_STAT64_HAS_BROKEN_ST_INO	1
	linux_ino_t	__lst_ino;
	unsigned int	lst_mode;
	unsigned int	lst_nlink;

	unsigned int	lst_uid;
	unsigned int	lst_gid;

	unsigned long long lst_rdev;
	unsigned int	__pad2;

	long long	lst_size;
	unsigned int	lst_blksize;

	unsigned long long lst_blocks;	/* Number 512-byte blocks allocated. */

	unsigned int	lst_atime;
	unsigned int	__unused1;

	unsigned int	lst_mtime;
	unsigned int	__unused2;

	unsigned int	lst_ctime;
	unsigned int	__unused3;	/* will be high 32 bits of ctime someday */

	linux_ino64_t	lst_ino;
};

#endif /* !_LINUX_TYPES_H_ */
