/*	$OpenBSD: pool.h,v 1.53 2014/09/22 01:04:58 dlg Exp $	*/
/*	$NetBSD: pool.h,v 1.27 2001/06/06 22:00:17 rafal Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg; by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYS_POOL_H_
#define _SYS_POOL_H_

/*
 * sysctls.
 * kern.pool.npools
 * kern.pool.name.<number>
 * kern.pool.pool.<number>
 */
#define KERN_POOL_NPOOLS	1
#define KERN_POOL_NAME		2
#define KERN_POOL_POOL		3

struct kinfo_pool {
	unsigned int	pr_size;	/* size of a pool item */
	unsigned int	pr_pgsize;	/* size of a "page" */
	unsigned int	pr_itemsperpage; /* number of items per "page" */
	unsigned int	pr_minpages;	/* same in page units */
	unsigned int	pr_maxpages;	/* maximum # of idle pages to keep */
	unsigned int	pr_hardlimit;	/* hard limit to number of allocated
					   items */

	unsigned int	pr_npages;	/* # of pages allocated */
	unsigned int	pr_nout;	/* # items currently allocated */
	unsigned int	pr_nitems;	/* # items in the pool */

	unsigned long	pr_nget;	/* # of successful requests */
	unsigned long	pr_nput;	/* # of releases */
	unsigned long	pr_nfail;	/* # of unsuccessful requests */
	unsigned long	pr_npagealloc;	/* # of pages allocated */
	unsigned long	pr_npagefree;	/* # of pages released */
	unsigned int	pr_hiwat;	/* max # of pages in pool */
	unsigned long	pr_nidle;	/* # of idle pages */
};

#if defined(_KERNEL) || defined(_LIBKVM)

#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/mutex.h>

struct pool;
struct pool_request;
TAILQ_HEAD(pool_requests, pool_request);

struct pool_allocator {
	void *(*pa_alloc)(struct pool *, int, int *);
	void (*pa_free)(struct pool *, void *);
	int pa_pagesz;
};

LIST_HEAD(pool_pagelist, pool_item_header);

struct pool {
	struct mutex	pr_mtx;
	SIMPLEQ_ENTRY(pool)
			pr_poollist;
	struct pool_pagelist
			pr_emptypages;	/* Empty pages */
	struct pool_pagelist
			pr_fullpages;	/* Full pages */
	struct pool_pagelist
			pr_partpages;	/* Partially-allocated pages */
	struct pool_item_header	*
			pr_curpage;
	unsigned int	pr_size;	/* Size of item */
	unsigned int	pr_align;	/* Requested alignment, must be 2^n */
	unsigned int	pr_minitems;	/* minimum # of items to keep */
	unsigned int	pr_minpages;	/* same in page units */
	unsigned int	pr_maxpages;	/* maximum # of idle pages to keep */
	unsigned int	pr_npages;	/* # of pages allocated */
	unsigned int	pr_itemsperpage;/* # items that fit in a page */
	unsigned int	pr_slack;	/* unused space in a page */
	unsigned int	pr_nitems;	/* number of available items in pool */
	unsigned int	pr_nout;	/* # items currently allocated */
	unsigned int	pr_hardlimit;	/* hard limit to number of allocated
					   items */
	unsigned int	pr_serial;	/* unique serial number of the pool */
	unsigned int	pr_pgsize;	/* Size of a "page" */
	vaddr_t		pr_pgmask;	/* Mask with an item to get a page */
	struct pool_allocator *
			pr_alloc;	/* backend allocator */
	const char *	pr_wchan;	/* tsleep(9) identifier */
	unsigned int	pr_flags;	/* r/w flags */
	unsigned int	pr_roflags;	/* r/o flags */
#define PR_WAITOK	0x0001 /* M_WAITOK */
#define PR_NOWAIT	0x0002 /* M_NOWAIT */
#define PR_LIMITFAIL	0x0004 /* M_CANFAIL */
#define PR_ZERO		0x0008 /* M_ZERO */
#define PR_WANTED	0x0100
#define PR_DEBUGCHK	0x1000

	int		pr_ipl;

	RB_HEAD(phtree, pool_item_header)
			pr_phtree;

	int		pr_maxcolor;	/* Cache colouring */
	int		pr_curcolor;
	int		pr_phoffset;	/* Offset in page of page header */

	/*
	 * Warning message to be issued, and a per-time-delta rate cap,
	 * if the hard limit is reached.
	 */
	const char	*pr_hardlimit_warning;
	struct timeval	pr_hardlimit_ratecap;
	struct timeval	pr_hardlimit_warning_last;

	/*
	 * pool item requests queue
	 */
	struct mutex	pr_requests_mtx;
	struct pool_requests
			pr_requests;
	unsigned int	pr_requesting;

	/*
	 * Instrumentation
	 */
	unsigned long	pr_nget;	/* # of successful requests */
	unsigned long	pr_nfail;	/* # of unsuccessful requests */
	unsigned long	pr_nput;	/* # of releases */
	unsigned long	pr_npagealloc;	/* # of pages allocated */
	unsigned long	pr_npagefree;	/* # of pages released */
	unsigned int	pr_hiwat;	/* max # of pages in pool */
	unsigned long	pr_nidle;	/* # of idle pages */

	/* Physical memory configuration. */
	const struct kmem_pa_mode *
			pr_crange;
};

#endif /* _KERNEL || _LIBKVM */

#ifdef _KERNEL

extern struct pool_allocator pool_allocator_nointr;

struct pool_request {
	TAILQ_ENTRY(pool_request) pr_entry;
	void (*pr_handler)(void *, void *);
	void *pr_cookie;
	void *pr_item;
};

void		pool_init(struct pool *, size_t, u_int, u_int, int,
		    const char *, struct pool_allocator *);
void		pool_destroy(struct pool *);
void		pool_setipl(struct pool *, int);
void		pool_setlowat(struct pool *, int);
void		pool_sethiwat(struct pool *, int);
int		pool_sethardlimit(struct pool *, u_int, const char *, int);
struct uvm_constraint_range; /* XXX */
void		pool_set_constraints(struct pool *,
		    const struct kmem_pa_mode *mode);

void		*pool_get(struct pool *, int) __malloc;
void		pool_request_init(struct pool_request *,
		    void (*)(void *, void *), void *);
void		pool_request(struct pool *, struct pool_request *);
void		pool_put(struct pool *, void *);
int		pool_reclaim(struct pool *);
void		pool_reclaim_all(void);
int		pool_prime(struct pool *, int);

#ifdef DDB
/*
 * Debugging and diagnostic aides.
 */
void		pool_printit(struct pool *, const char *,
		    int (*)(const char *, ...));
void		pool_walk(struct pool *, int, int (*)(const char *, ...),
		    void (*)(void *, int, int (*)(const char *, ...)));
#endif

/* the allocator for dma-able memory is a thin layer on top of pool  */
void		 dma_alloc_init(void);
void		*dma_alloc(size_t size, int flags);
void		 dma_free(void *m, size_t size);
#endif /* _KERNEL */

#endif /* _SYS_POOL_H_ */
