/*	$OpenBSD: ktrace.h,v 1.19 2014/03/26 05:23:42 guenther Exp $	*/
/*	$NetBSD: ktrace.h,v 1.12 1996/02/04 02:12:29 christos Exp $	*/

/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ktrace.h	8.1 (Berkeley) 6/2/93
 */

/*
 * operations to ktrace system call  (KTROP(op))
 */
#define KTROP_SET		0	/* set trace points */
#define KTROP_CLEAR		1	/* clear trace points */
#define KTROP_CLEARFILE		2	/* stop all tracing to file */
#define	KTROP(o)		((o)&3)	/* macro to extract operation */
/*
 * flags (ORed in with operation)
 */
#define KTRFLAG_DESCEND		4	/* perform op on all children too */

/*
 * ktrace record header
 */
struct ktr_header {
	uint	ktr_type;		/* trace record type */
	pid_t	ktr_pid;		/* process id */
	pid_t	ktr_tid;		/* thread id */
	struct	timespec ktr_time;	/* timestamp */
	char	ktr_comm[MAXCOMLEN+1];	/* command name */
	size_t	ktr_len;		/* length of buf */
};

/*
 * Test for kernel trace point
 */
#define KTRPOINT(p, type)	\
	((p)->p_p->ps_traceflag & (1<<(type)) && ((p)->p_flag & P_INKTR) == 0)

/*
 * ktrace record types
 */

 /*
 * KTR_START - start of trace record, one per ktrace(KTROP_SET) syscall
 */
#define KTR_START	0x4b545200	/* "KTR" */

/*
 * KTR_SYSCALL - system call record
 */
#define KTR_SYSCALL	1
struct ktr_syscall {
	int	ktr_code;		/* syscall number */
	int	ktr_argsize;		/* size of arguments */
	/*
	 * followed by ktr_argsize/sizeof(register_t) "register_t"s
	 */
};

/*
 * KTR_SYSRET - return from system call record
 */
#define KTR_SYSRET	2
struct ktr_sysret {
	short	ktr_code;
	short	ktr_eosys;
	int	ktr_error;
	register_t ktr_retval;
};

/*
 * KTR_NAMEI - namei record
 */
#define KTR_NAMEI	3
	/* record contains pathname */

/*
 * KTR_GENIO - trace generic process i/o
 */
#define KTR_GENIO	4
struct ktr_genio {
	int	ktr_fd;
	enum	uio_rw ktr_rw;
	/*
	 * followed by data successfully read/written
	 */
};

/*
 * KTR_PSIG - trace processed signal
 */
#define	KTR_PSIG	5
struct ktr_psig {
	int	signo;
	sig_t	action;
	int	mask;
	int	code;
	siginfo_t si;
};

/*
 * KTR_CSW - trace context switches
 */
#define KTR_CSW		6
struct ktr_csw {
	int	out;	/* 1 if switch out, 0 if switch in */
	int	user;	/* 1 if usermode (ivcsw), 0 if kernel (vcsw) */
};

/*
 * KTR_EMUL - emulation change
 */
#define KTR_EMUL	7
	/* record contains emulation name */

/*
 * KTR_STRUCT - misc. structs
 */
#define KTR_STRUCT	8
	/*
	 * record contains null-terminated struct name followed by
	 * struct contents
	 */
struct sockaddr;
struct stat;

/*
 * KTR_USER - user record
 */
#define KTR_USER	9
#define KTR_USER_MAXIDLEN	20
#define KTR_USER_MAXLEN		2048	/* maximum length of passed data */
struct ktr_user {
	char    ktr_id[KTR_USER_MAXIDLEN];      /* string id of caller */
	/*
	 * Followed by ktr_len - sizeof(struct ktr_user) of user data.
	 */
};

/*
 * kernel trace points (in p_traceflag)
 */
#define KTRFAC_MASK	0x00ffffff
#define KTRFAC_SYSCALL	(1<<KTR_SYSCALL)
#define KTRFAC_SYSRET	(1<<KTR_SYSRET)
#define KTRFAC_NAMEI	(1<<KTR_NAMEI)
#define KTRFAC_GENIO	(1<<KTR_GENIO)
#define	KTRFAC_PSIG	(1<<KTR_PSIG)
#define KTRFAC_CSW	(1<<KTR_CSW)
#define KTRFAC_EMUL	(1<<KTR_EMUL)
#define KTRFAC_STRUCT   (1<<KTR_STRUCT)
#define KTRFAC_USER	(1<<KTR_USER)

/*
 * trace flags (also in p_traceflags)
 */
#define KTRFAC_ROOT	0x80000000	/* root set this trace */
#define KTRFAC_INHERIT	0x40000000	/* pass trace flags to children */

#ifndef	_KERNEL

#include <sys/cdefs.h>

__BEGIN_DECLS
int	ktrace(const char *, int, int, pid_t);
int	utrace(const char *, const void *, size_t);
__END_DECLS

#else

void ktrcsw(struct proc *, int, int);
void ktremul(struct proc *);
void ktrgenio(struct proc *, int, enum uio_rw, struct iovec *, ssize_t);
void ktrnamei(struct proc *, char *);
void ktrpsig(struct proc *, int, sig_t, int, int, siginfo_t *);
void ktrsyscall(struct proc *, register_t, size_t, register_t []);
void ktrsysret(struct proc *, register_t, int, register_t);
void ktr_kuser(const char *, void *, size_t);
int ktruser(struct proc *, const char *, const void *, size_t);

void ktrcleartrace(struct process *);
void ktrsettrace(struct process *, int, struct vnode *, struct ucred *);

void    ktrstruct(struct proc *, const char *, const void *, size_t);
#define ktrsockaddr(p, s, l) \
	ktrstruct((p), "sockaddr", (s), (l))
#define ktrstat(p, s) \
	ktrstruct((p), "stat", (s), sizeof(struct stat))
#define ktrabstimespec(p, s) \
	ktrstruct((p), "abstimespec", (s), sizeof(struct timespec))
#define ktrreltimespec(p, s) \
	ktrstruct((p), "reltimespec", (s), sizeof(struct timespec))
#define ktrabstimeval(p, s) \
	ktrstruct((p), "abstimeval", (s), sizeof(struct timeval))
#define ktrreltimeval(p, s) \
	ktrstruct((p), "reltimeval", (s), sizeof(struct timeval))
#define ktrsigaction(p, s) \
	ktrstruct((p), "sigaction", (s), sizeof(struct sigaction))
#define ktrrlimit(p, s) \
	ktrstruct((p), "rlimit", (s), sizeof(struct rlimit))
#define ktrrusage(p, s) \
	ktrstruct((p), "rusage", (s), sizeof(struct rusage))
#define ktrfdset(p, s, l) \
	ktrstruct((p), "fdset", (s), l)

#endif	/* !_KERNEL */
