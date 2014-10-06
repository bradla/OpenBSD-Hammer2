/*	$OpenBSD: ufs_ihash.c,v 1.18 2014/04/14 22:25:40 beck Exp $	*/
/*	$NetBSD: ufs_ihash.c,v 1.3 1996/02/09 22:36:04 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
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
 *	@(#)ufs_ihash.c	8.4 (Berkeley) 12/30/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>

/*
 * Structures associated with inode cacheing.
 */
LIST_HEAD(ihashhead, inode) *ihashtbl;
u_long	ihash;		/* size of hash table - 1 */
#define	INOHASH(device, inum)	(&ihashtbl[((device) + (inum)) & ihash])

/*
 * Initialize inode hash table.
 */
void
ufs_ihashinit(void)
{
	ihashtbl = hashinit(desiredvnodes, M_UFSMNT, M_WAITOK, &ihash);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, return it, even if it is locked.
 */
struct vnode *
ufs_ihashlookup(dev_t dev, ufsino_t inum)
{
        struct inode *ip;

	/* XXXLOCKING lock hash list */
	LIST_FOREACH(ip, INOHASH(dev, inum), i_hash)
		if (inum == ip->i_number && dev == ip->i_dev)
			break;
	/* XXXLOCKING unlock hash list? */

	if (ip)
		return (ITOV(ip));

	return (NULLVP);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, but locked, wait for it.
 */
struct vnode *
ufs_ihashget(dev_t dev, ufsino_t inum)
{
	struct proc *p = curproc;
	struct inode *ip;
	struct vnode *vp;
loop:
	/* XXXLOCKING lock hash list */
	LIST_FOREACH(ip, INOHASH(dev, inum), i_hash) {
		if (inum == ip->i_number && dev == ip->i_dev) {
			vp = ITOV(ip);
			/* XXXLOCKING unlock hash list? */
			if (vget(vp, LK_EXCLUSIVE, p))
				goto loop;
			return (vp);
 		}
	}
	/* XXXLOCKING unlock hash list? */
	return (NULL);
}

/*
 * Insert the inode into the hash table, and return it locked.
 */
int
ufs_ihashins(struct inode *ip)
{
	struct   inode *curip;
	struct   ihashhead *ipp;
	dev_t    dev = ip->i_dev;
	ufsino_t inum = ip->i_number;

	/* lock the inode, then put it on the appropriate hash list */
	lockmgr(&ip->i_lock, LK_EXCLUSIVE, NULL);

	/* XXXLOCKING lock hash list */

	LIST_FOREACH(curip, INOHASH(dev, inum), i_hash) {
		if (inum == curip->i_number && dev == curip->i_dev) {
			/* XXXLOCKING unlock hash list? */
			lockmgr(&ip->i_lock, LK_RELEASE, NULL);
			return (EEXIST);
		}
	}

	ipp = INOHASH(dev, inum);
	SET(ip->i_flag, IN_HASHED);
	LIST_INSERT_HEAD(ipp, ip, i_hash);
	/* XXXLOCKING unlock hash list? */

	return (0);
}

/*
 * Remove the inode from the hash table.
 */
void
ufs_ihashrem(struct inode *ip)
{
	/* XXXLOCKING lock hash list */

	if (ip->i_hash.le_prev == NULL)
		return;
	if (ISSET(ip->i_flag, IN_HASHED)) {
		LIST_REMOVE(ip, i_hash);
		CLR(ip->i_flag, IN_HASHED);
	}
#ifdef DIAGNOSTIC
	ip->i_hash.le_next = NULL;
	ip->i_hash.le_prev = NULL;
#endif
	/* XXXLOCKING unlock hash list? */
}
