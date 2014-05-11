/*
 * Copyright (c) 2011-2014 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 * by Venkatesh Srinivas <vsrinivas@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/lock.h>
#include <sys/stdint.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

#include "hammer2.h"

#define M_HAMMER2 1001
#define VA_UID_UUID_VALID    0x0004  /* uuid fields also populated */
#define VA_GID_UUID_VALID    0x0008  /* uuid fields also populated */

//u_int8_t   uuid_t[16];
//uuid_t          va_uid_uuid;    /* native uuids if available */

#define INODE_DEBUG	0

static void hammer2_inode_move_to_hidden(hammer2_trans_t *trans,
					 hammer2_cluster_t **clusterp,
					 hammer2_tid_t inum);

#define RB_GENERATE2(name, type, field, cmp, datatype, indexfield)
RB_GENERATE2(hammer2_inode_tree, hammer2_inode, rbnode, hammer2_inode_cmp,
	     hammer2_tid_t, inum);

int
hammer2_inode_cmp(hammer2_inode_t *ip1, hammer2_inode_t *ip2)
{
	if (ip1->inum < ip2->inum)
		return(-1);
	if (ip1->inum > ip2->inum)
		return(1);
	return(0);
}

/*
 * HAMMER2 inode locks
 *
 * HAMMER2 offers shared locks and exclusive locks on inodes.
 *
 * The inode locking function locks the inode itself, resolves any stale
 * chains in the inode's cluster, and allocates a fresh copy of the
 * cluster with 1 ref and all the underlying chains locked.  Duplication
 * races are handled by this function.
 *
 * ip->cluster will be stable while the inode is locked.
 *
 * NOTE: We don't combine the inode/chain lock because putting away an
 *       inode would otherwise confuse multiple lock holders of the inode.
 *
 * NOTE: Hardlinks are followed in the returned cluster but not in the
 *	 inode's internal cluster (ip->cluster).
 */
hammer2_cluster_t *
hammer2_inode_lock_ex(hammer2_inode_t *ip)
{
	hammer2_inode_data_t *ipdata;
	hammer2_cluster_t *cluster;
	hammer2_chain_t *chain;
	hammer2_chain_t *ochain;
	hammer2_chain_core_t *core;
	int error;
	int i;

	hammer2_inode_ref(ip);
	ccms_thread_lock(&ip->topo_cst, CCMS_STATE_EXCLUSIVE);
	cluster = hammer2_cluster_copy(&ip->cluster, 0);

	ip->cluster.focus = NULL;
	cluster->focus = NULL;

	for (i = 0; i < cluster->nchains; ++i) {
		chain = ip->cluster.array[i];
		core = chain->core;
		for (;;) {
			if (chain->flags & HAMMER2_CHAIN_DUPLICATED) {
				(&core->cst.spin);
				while (chain->flags & HAMMER2_CHAIN_DUPLICATED)
					chain = TAILQ_NEXT(chain, core_entry);
				hammer2_chain_ref(chain);
				ochain = ip->cluster.array[i];
				ip->cluster.array[i] = chain;
				if (ip->cluster.focus == NULL)
					ip->cluster.focus = chain;
				hammer2_chain_drop(ochain);
			}
			hammer2_chain_lock(chain, HAMMER2_RESOLVE_ALWAYS);
			if ((chain->flags & HAMMER2_CHAIN_DUPLICATED) == 0)
				break;
			hammer2_chain_unlock(chain);
		}
		cluster->array[i] = chain;
		if (cluster->focus == NULL)
			cluster->focus = chain;
	}

	/*
	 * Returned cluster must resolve hardlink pointers
	 */
	ipdata = &hammer2_cluster_data(cluster)->ipdata;
	if (ipdata->type == HAMMER2_OBJTYPE_HARDLINK &&
	    (cluster->focus->flags & HAMMER2_CHAIN_DELETED) == 0) {
		error = hammer2_hardlink_find(ip->pip, cluster);
		KKASSERT((cluster->focus->flags &
			  HAMMER2_CHAIN_DUPLICATED) == 0);
		KKASSERT(error == 0);
	}

	return (cluster);
}

void
hammer2_inode_unlock_ex(hammer2_inode_t *ip, hammer2_cluster_t *cluster)
{
	if (cluster)
		hammer2_cluster_unlock(cluster);
	ccms_thread_unlock(&ip->topo_cst);
	hammer2_inode_drop(ip);
}

/*
 * NOTE: We don't combine the inode/chain lock because putting away an
 *       inode would otherwise confuse multiple lock holders of the inode.
 *
 *	 Shared locks are especially sensitive to having too many shared
 *	 lock counts (from the same thread) on certain paths which might
 *	 need to upgrade them.  Only one count of a shared lock can be
 *	 upgraded.
 */
hammer2_cluster_t *
hammer2_inode_lock_sh(hammer2_inode_t *ip)
{
	hammer2_inode_data_t *ipdata;
	hammer2_cluster_t *cluster;
	hammer2_chain_core_t *core;
	hammer2_chain_t *chain;
	int error = 0;
	int i;

	hammer2_inode_ref(ip);
	cluster = hammer2_cluster_copy(&ip->cluster, 0);
	ccms_thread_lock(&ip->topo_cst, CCMS_STATE_SHARED);

	cluster->focus = NULL;

	for (i = 0; i < cluster->nchains; ++i) {
		chain = ip->cluster.array[i];
		core = chain->core;

		if (chain->flags & HAMMER2_CHAIN_DUPLICATED)
			goto cycle_excl;
		hammer2_chain_lock(chain, HAMMER2_RESOLVE_ALWAYS |
					  HAMMER2_RESOLVE_SHARED);
		if (chain->flags & HAMMER2_CHAIN_DUPLICATED) {
			hammer2_chain_unlock(chain);

			/*
			 * Cycle exclusive inode lock and start the loop
			 * over again.
			 */
cycle_excl:
			while (--i >= 0) {
				chain = cluster->array[i];
				cluster->array[i] = NULL;
				hammer2_chain_unlock(chain);
			}
			ccms_thread_unlock(&ip->topo_cst);
			hammer2_inode_unlock_ex(ip, hammer2_inode_lock_ex(ip));
			ccms_thread_lock(&ip->topo_cst, CCMS_STATE_SHARED);
			continue;	/* restart at i=-1 -> i=0 on loop */
		}
		cluster->array[i] = chain;
		if (cluster->focus == NULL)
			cluster->focus = chain;
	}

	/*
	 * Returned cluster must resolve hardlink pointers
	 */
	ipdata = &hammer2_cluster_data(cluster)->ipdata;
	if (ipdata->type == HAMMER2_OBJTYPE_HARDLINK &&
	    (cluster->focus->flags & HAMMER2_CHAIN_DELETED) == 0) {
		error = hammer2_hardlink_find(ip->pip, cluster);
		KKASSERT((cluster->focus->flags &
			  HAMMER2_CHAIN_DUPLICATED) == 0);
		KKASSERT(error == 0);
	}

	return (cluster);
}

void
hammer2_inode_unlock_sh(hammer2_inode_t *ip, hammer2_cluster_t *cluster)
{
	if (cluster)
		hammer2_cluster_unlock(cluster);
	ccms_thread_unlock(&ip->topo_cst);
	hammer2_inode_drop(ip);
}

ccms_state_t
hammer2_inode_lock_temp_release(hammer2_inode_t *ip)
{
	return(ccms_thread_lock_temp_release(&ip->topo_cst));
}

void
hammer2_inode_lock_temp_restore(hammer2_inode_t *ip, ccms_state_t ostate)
{
	ccms_thread_lock_temp_restore(&ip->topo_cst, ostate);
}

ccms_state_t
hammer2_inode_lock_upgrade(hammer2_inode_t *ip)
{
	return(ccms_thread_lock_upgrade(&ip->topo_cst));
}

void
hammer2_inode_lock_downgrade(hammer2_inode_t *ip, ccms_state_t ostate)
{
	ccms_thread_lock_downgrade(&ip->topo_cst, ostate);
}

/*
 * Lookup an inode by inode number
 */
hammer2_inode_t *
hammer2_inode_lookup(hammer2_pfsmount_t *pmp, hammer2_tid_t inum)
{
	hammer2_inode_t *ip;

	if (pmp) {
		(&pmp->inum_spin);
		ip = RB_LOOKUP(hammer2_inode_tree, &pmp->inum_tree, inum);
		if (ip)
			hammer2_inode_ref(ip);
	} else {
		ip = NULL;
	}
	return(ip);
}

/*
 * Adding a ref to an inode is only legal if the inode already has at least
 * one ref.
 */
void
hammer2_inode_ref(hammer2_inode_t *ip)
{
	atomic_add_int(&ip->refs, 1);
}

/*
 * Drop an inode reference, freeing the inode when the last reference goes
 * away.
 */
void
hammer2_inode_drop(hammer2_inode_t *ip)
{
	hammer2_pfsmount_t *pmp;
	hammer2_inode_t *pip;
	u_int refs;

	while (ip) {
		refs = ip->refs;
		//cpu_ccfence();
		if (refs == 1) {
			/*
			 * Transition to zero, must interlock with
			 * the inode inumber lookup tree (if applicable).
			 *
			 * NOTE: The super-root inode has no pmp.
			 */
			pmp = ip->pmp;
			if (pmp)
				(&pmp->inum_spin);

			if (atomic_cmpset_int(&ip->refs, 1, 0)) {
				KKASSERT(ip->topo_cst.count == 0);
				if (ip->flags & HAMMER2_INODE_ONRBTREE) {
					atomic_clear_int(&ip->flags,
						     HAMMER2_INODE_ONRBTREE);
					RB_REMOVE(hammer2_inode_tree,
						  &pmp->inum_tree, ip);
				}
				if (pmp)

				pip = ip->pip;
				ip->pip = NULL;
				ip->pmp = NULL;

				/*
				 * Cleaning out ip->cluster isn't entirely
				 * trivial.
				 */
				hammer2_inode_repoint(ip, NULL, NULL);

				/*
				 * We have to drop pip (if non-NULL) to
				 * dispose of our implied reference from
				 * ip->pip.  We can simply loop on it.
				 */
				if (pmp) {
					KKASSERT((ip->flags &
						  HAMMER2_INODE_SROOT) == 0);
					free(ip, pmp->minode);
					atomic_add_long(&pmp->inmem_inodes, -1);
				} else {
					KKASSERT(ip->flags &
						 HAMMER2_INODE_SROOT);
					free(ip, M_HAMMER2);
				}
				ip = pip;
				/* continue with pip (can be NULL) */
			} 
		} else {
			/*
			 * Non zero transition
			 */
			if (atomic_cmpset_int(&ip->refs, refs, refs - 1))
				break;
		}
	}
}

/*
 * Get the vnode associated with the given inode, allocating the vnode if
 * necessary.  The vnode will be returned exclusively locked.
 *
 * The caller must lock the inode (shared or exclusive).
 *
 * Great care must be taken to avoid deadlocks and vnode acquisition/reclaim
 * races.
 */
struct vnode *
hammer2_igetv(hammer2_inode_t *ip, hammer2_cluster_t *cparent, int *errorp)
{
	hammer2_inode_data_t *ipdata;
	hammer2_pfsmount_t *pmp;
	struct vnode *vp;
	ccms_state_t ostate;

	pmp = ip->pmp;
	KKASSERT(pmp != NULL);
	*errorp = 0;

	ipdata = &hammer2_cluster_data(cparent)->ipdata;

	for (;;) {
		/*
		 * Attempt to reuse an existing vnode assignment.  It is
		 * possible to race a reclaim so the vget() may fail.  The
		 * inode must be unlocked during the vget() to avoid a
		 * deadlock against a reclaim.
		 */
		vp = ip->vp;
		if (vp) {
			/*
			 * Inode must be unlocked during the vget() to avoid
			 * possible deadlocks, but leave the ip ref intact.
			 *
			 * vnode is held to prevent destruction during the
			 * vget().  The vget() can still fail if we lost
			 * a reclaim race on the vnode.
			 */
			vhold(vp);
			ostate = hammer2_inode_lock_temp_release(ip);
			if (vget(vp, LK_EXCLUSIVE, NULL)) {
				vdrop(vp);
				hammer2_inode_lock_temp_restore(ip, ostate);
				continue;
			}
			hammer2_inode_lock_temp_restore(ip, ostate);
			vdrop(vp);
			/* vp still locked and ref from vget */
			if (ip->vp != vp) {
				kprintf("hammer2: igetv race %p/%p\n",
					ip->vp, vp);
				vput(vp);
				continue;
			}
			*errorp = 0;
			break;
		}

		/*
		 * No vnode exists, allocate a new vnode.  Beware of
		 * allocation races.  This function will return an
		 * exclusively locked and referenced vnode.
		 */
		*errorp = getnewvnode(VT_HAMMER2, pmp->mp, &vp, 0);
		if (*errorp) {
			kprintf("hammer2: igetv getnewvnode failed %d\n",
				*errorp);
			vp = NULL;
			break;
		}

		/*
		 * Lock the inode and check for an allocation race.
		 */
		ostate = hammer2_inode_lock_upgrade(ip);
		if (ip->vp != NULL) {
			vp->v_type = VBAD;
			vx_put(vp);
			hammer2_inode_lock_downgrade(ip, ostate);
			continue;
		}

		switch (ipdata->type) {
		case HAMMER2_OBJTYPE_DIRECTORY:
			vp->v_type = VDIR;
			break;
		case HAMMER2_OBJTYPE_REGFILE:
			vp->v_type = VREG;
			vinitvmio(vp, ipdata->size,
				  HAMMER2_LBUFSIZE,
				  (int)ipdata->size & HAMMER2_LBUFMASK);
			break;
		case HAMMER2_OBJTYPE_SOFTLINK:
			/*
			 * XXX for now we are using the generic file_read
			 * and file_write code so we need a buffer cache
			 * association.
			 */
			vp->v_type = VLNK;
			vinitvmio(vp, ipdata->size,
				  HAMMER2_LBUFSIZE,
				  (int)ipdata->size & HAMMER2_LBUFMASK);
			break;
		case HAMMER2_OBJTYPE_CDEV:
			vp->v_type = VCHR;
			/* fall through */
		case HAMMER2_OBJTYPE_BDEV:
			/* XXX vp->v_ops = &pmp->mp->mnt_vn_spec_ops; */
			if (ipdata->type != HAMMER2_OBJTYPE_CDEV)
				vp->v_type = VBLK;
			addaliasu(vp, ipdata->rmajor, ipdata->rminor);
			break;
		case HAMMER2_OBJTYPE_FIFO:
			vp->v_type = VFIFO;
			/* XX no type vp->v_ops = &pmp->mp->mnt_vn_fifo_ops; */
			break;
		default:
			panic("hammer2: unhandled objtype %d", ipdata->type);
			break;
		}

		if (ip == pmp->iroot)
			vsetflags(vp, VROOT);

		vp->v_data = ip;
		ip->vp = vp;
		hammer2_inode_ref(ip);		/* vp association */
		hammer2_inode_lock_downgrade(ip, ostate);
		break;
	}

	/*
	 * Return non-NULL vp and *errorp == 0, or NULL vp and *errorp != 0.
	 */
	if (hammer2_debug & 0x0002) {
		kprintf("igetv vp %p refs /*0x%08x aux 0x%08x*/\n",
			vp/*, vp->v_refcnt, vp->v_auxrefs*/);
	}
	return (vp);
}

/*
 * Returns the inode associated with the passed-in cluster, creating the
 * inode if necessary and synchronizing it to the passed-in cluster otherwise.
 *
 * The passed-in chain must be locked and will remain locked on return.
 * The returned inode will be locked and the caller may dispose of both
 * via hammer2_inode_unlock_ex().  However, if the caller needs to resolve
 * a hardlink it must ref/unlock/relock/drop the inode.
 *
 * The hammer2_inode structure regulates the interface between the high level
 * kernel VNOPS API and the filesystem backend (the chains).
 *
 * WARNING!  The mount code is allowed to pass dip == NULL for iroot and
 *	     is allowed to pass pmp == NULL and dip == NULL for sroot.
 */
hammer2_inode_t *
hammer2_inode_get(hammer2_pfsmount_t *pmp, hammer2_inode_t *dip,
		  hammer2_cluster_t *cluster)
{
	hammer2_inode_t *nip;
	hammer2_inode_data_t *iptmp;
	hammer2_inode_data_t *nipdata;

	KKASSERT(hammer2_cluster_type(cluster) == HAMMER2_BREF_TYPE_INODE);

	/*
	 * Interlocked lookup/ref of the inode.  This code is only needed
	 * when looking up inodes with nlinks != 0 (TODO: optimize out
	 * otherwise and test for duplicates).
	 */
again:
	for (;;) {
		iptmp = &hammer2_cluster_data(cluster)->ipdata;
		nip = hammer2_inode_lookup(pmp, iptmp->inum);
		if (nip == NULL)
			break;

		ccms_thread_lock(&nip->topo_cst, CCMS_STATE_EXCLUSIVE);
		if ((nip->flags & HAMMER2_INODE_ONRBTREE) == 0) { /* race */
			ccms_thread_unlock(&nip->topo_cst);
			hammer2_inode_drop(nip);
			continue;
		}
		hammer2_inode_repoint(nip, NULL, cluster);
		return nip;
	}

	/*
	 * We couldn't find the inode number, create a new inode.
	 */
	if (pmp) {
		nip = malloc(sizeof(*nip), pmp->minode, M_WAITOK | M_ZERO);
		atomic_add_long(&pmp->inmem_inodes, 1);
		hammer2_pfs_memory_inc(pmp);
		hammer2_pfs_memory_wakeup(pmp);
	} else {
		nip = malloc(sizeof(*nip), M_HAMMER2, M_WAITOK | M_ZERO);
		nip->flags = HAMMER2_INODE_SROOT;
	}

	/*
	 * Initialize nip's cluster
	 */
	nip->cluster.refs = 1;
	nip->cluster.pmp = pmp;
	nip->cluster.flags |= HAMMER2_CLUSTER_INODE;
	hammer2_cluster_replace(&nip->cluster, cluster);

	nipdata = &hammer2_cluster_data(cluster)->ipdata;
	nip->inum = nipdata->inum;
	nip->size = nipdata->size;
	nip->mtime = nipdata->mtime;
	hammer2_inode_repoint(nip, NULL, cluster);
	nip->pip = dip;				/* can be NULL */
	if (dip)
		hammer2_inode_ref(dip);	/* ref dip for nip->pip */

	nip->pmp = pmp;

	/*
	 * ref and lock on nip gives it state compatible to after a
	 * hammer2_inode_lock_ex() call.
	 */
	nip->refs = 1;
	ccms_cst_init(&nip->topo_cst, &nip->cluster);
	ccms_thread_lock(&nip->topo_cst, CCMS_STATE_EXCLUSIVE);
	/* combination of thread lock and chain lock == inode lock */

	/*
	 * Attempt to add the inode.  If it fails we raced another inode
	 * get.  Undo all the work and try again.
	 */
	if (pmp) {
		(&pmp->inum_spin);
		if (RB_INSERT(hammer2_inode_tree, &pmp->inum_tree, nip)) {
			ccms_thread_unlock(&nip->topo_cst);
			hammer2_inode_drop(nip);
			goto again;
		}
		atomic_set_int(&nip->flags, HAMMER2_INODE_ONRBTREE);
	}

	return (nip);
}

/*
 * Create a new inode in the specified directory using the vattr to
 * figure out the type of inode.
 *
 * If no error occurs the new inode with its cluster locked is returned in
 * *nipp, otherwise an error is returned and *nipp is set to NULL.
 *
 * If vap and/or cred are NULL the related fields are not set and the
 * inode type defaults to a directory.  This is used when creating PFSs
 * under the super-root, so the inode number is set to 1 in this case.
 *
 * dip is not locked on entry.
 */
hammer2_inode_t *
hammer2_inode_create(hammer2_trans_t *trans, hammer2_inode_t *dip,
		     struct vattr *vap, struct ucred *cred,
		     const uint8_t *name, size_t name_len,
		     hammer2_cluster_t **clusterp, int *errorp)
{
	hammer2_inode_data_t *dipdata;
	hammer2_inode_data_t *nipdata;
	hammer2_cluster_t *cluster;
	hammer2_cluster_t *cparent;
	hammer2_inode_t *nip;
	hammer2_key_t key_dummy;
	hammer2_key_t lhc;
	int error;
	uid_t xuid;
	uuid_t dip_uid;
	uuid_t dip_gid;
	uint32_t dip_mode;
	uint8_t dip_algo;
	int ddflag;

	lhc = hammer2_dirhash(name, name_len);
	*errorp = 0;

	/*
	 * Locate the inode or indirect block to create the new
	 * entry in.  At the same time check for key collisions
	 * and iterate until we don't get one.
	 *
	 * NOTE: hidden inodes do not have iterators.
	 */
retry:
	cparent = hammer2_inode_lock_ex(dip);
	dipdata = &hammer2_cluster_data(cparent)->ipdata;
	dip_uid = dipdata->uid;
	dip_gid = dipdata->gid;
	dip_mode = dipdata->mode;
	dip_algo = dipdata->comp_algo;

	error = 0;
	while (error == 0) {
		cluster = hammer2_cluster_lookup(cparent, &key_dummy,
						 lhc, lhc, 0, &ddflag);
		if (cluster == NULL)
			break;
		if ((lhc & HAMMER2_DIRHASH_VISIBLE) == 0)
			error = ENOSPC;
		if ((lhc & HAMMER2_DIRHASH_LOMASK) == HAMMER2_DIRHASH_LOMASK)
			error = ENOSPC;
		hammer2_cluster_unlock(cluster);
		cluster = NULL;
		++lhc;
	}

	if (error == 0) {
		error = hammer2_cluster_create(trans, cparent, &cluster,
					     lhc, 0,
					     HAMMER2_BREF_TYPE_INODE,
					     HAMMER2_INODE_BYTES);
	}
#if INODE_DEBUG
	kprintf("CREATE INODE %*.*s chain=%p\n",
		(int)name_len, (int)name_len, name,
		(cluster ? cluster->focus : NULL));
#endif

	/*
	 * Cleanup and handle retries.
	 */
	if (error == EAGAIN) {
		hammer2_cluster_ref(cparent);
		hammer2_inode_unlock_ex(dip, cparent);
		hammer2_cluster_wait(cparent);
		hammer2_cluster_drop(cparent);
		goto retry;
	}
	hammer2_inode_unlock_ex(dip, cparent);
	cparent = NULL;

	if (error) {
		KKASSERT(cluster == NULL);
		*errorp = error;
		return (NULL);
	}

	/*
	 * Set up the new inode.
	 *
	 * NOTE: *_get() integrates chain's lock into the inode lock.
	 *
	 * NOTE: Only one new inode can currently be created per
	 *	 transaction.  If the need arises we can adjust
	 *	 hammer2_trans_init() to allow more.
	 *
	 * NOTE: nipdata will have chain's blockset data.
	 */
	nipdata = &hammer2_cluster_data(cluster)->ipdata;
	nipdata->inum = trans->inode_tid;
	nip = hammer2_inode_get(dip->pmp, dip, cluster);
	nipdata = &hammer2_cluster_data(cluster)->ipdata;

	if (vap) {
		KKASSERT(trans->inodes_created == 0);
		nipdata->type = hammer2_get_obj_type(vap->va_type);
		nipdata->inum = trans->inode_tid;
		++trans->inodes_created;

		switch (nipdata->type) {
		case HAMMER2_OBJTYPE_CDEV:
		case HAMMER2_OBJTYPE_BDEV:
			/*nipdata->rmajor = vap->va_rmajor;
			nipdata->rminor = vap->va_rminor;
			*/
			nipdata->rmajor = 0;
			nipdata->rminor = 0;
			break;
		default:
			break;
		}
	} else {
		nipdata->type = HAMMER2_OBJTYPE_DIRECTORY;
		nipdata->inum = 1;
	}
	
	/* Inherit parent's inode compression mode. */
	nip->comp_heuristic = 0;
	nipdata->comp_algo = dip_algo;
	nipdata->version = HAMMER2_INODE_VERSION_ONE;
	hammer2_update_time(&nipdata->ctime);
	nipdata->mtime = nipdata->ctime;
	if (vap)
		nipdata->mode = vap->va_mode;
	nipdata->nlinks = 1;
	if (vap) {
		if (dip && dip->pmp) {
			xuid = hammer2_to_unix_xid(&dip_uid);
			xuid = vop_helper_create_uid(dip->pmp->mp,
						     dip_mode,
						     xuid,
						     cred,
						     &vap->va_mode);
		} else {
			/* super-root has no dip and/or pmp */
			xuid = 0;
		}
		if (vap->va_vaflags & VA_UID_UUID_VALID)
			nipdata->uid = vap->va_uid_uuid;
		else if (vap->va_uid != (uid_t)VNOVAL)
			hammer2_guid_to_uuid(&nipdata->uid, vap->va_uid);
		else
			hammer2_guid_to_uuid(&nipdata->uid, xuid);

		if (vap->va_vaflags & VA_GID_UUID_VALID)
			nipdata->gid = vap->va_gid_uuid;
		else if (vap->va_gid != (gid_t)VNOVAL)
			hammer2_guid_to_uuid(&nipdata->gid, vap->va_gid);
		else if (dip)
			nipdata->gid = dip_gid;
	}

	/*
	 * Regular files and softlinks allow a small amount of data to be
	 * directly embedded in the inode.  This flag will be cleared if
	 * the size is extended past the embedded limit.
	 */
	if (nipdata->type == HAMMER2_OBJTYPE_REGFILE ||
	    nipdata->type == HAMMER2_OBJTYPE_SOFTLINK) {
		nipdata->op_flags |= HAMMER2_OPFLAG_DIRECTDATA;
	}

	KKASSERT(name_len < HAMMER2_INODE_MAXNAME);
	bcopy(name, nipdata->filename, name_len);
	nipdata->name_key = lhc;
	nipdata->name_len = name_len;
	*clusterp = cluster;

	return (nip);
}

/*
 * Shift *chainp up to the specified directory, change the filename
 * to "0xINODENUMBER", and adjust the key.  The chain becomes the
 * invisible hardlink target.
 *
 * The original *chainp has already been marked deleted.
 */
static
void
hammer2_hardlink_shiftup(hammer2_trans_t *trans, hammer2_cluster_t *cluster,
			hammer2_inode_t *dip, hammer2_cluster_t *dcluster,
			int nlinks, int *errorp)
{
	hammer2_inode_data_t *iptmp;
	hammer2_inode_data_t *nipdata;
	hammer2_cluster_t *xcluster;
	hammer2_key_t key_dummy;
	hammer2_key_t lhc;
	hammer2_blockref_t bref;
	int ddflag;

	iptmp = &hammer2_cluster_data(cluster)->ipdata;
	lhc = iptmp->inum;
	KKASSERT((lhc & HAMMER2_DIRHASH_VISIBLE) == 0);

	/*
	 * Locate the inode or indirect block to create the new
	 * entry in.  lhc represents the inode number so there is
	 * no collision iteration.
	 *
	 * There should be no key collisions with invisible inode keys.
	 *
	 * WARNING! Must use inode_lock_ex() on dip to handle a stale
	 *	    dip->cluster cache.
	 */
retry:
	*errorp = 0;
	xcluster = hammer2_cluster_lookup(dcluster, &key_dummy,
				      lhc, lhc, 0, &ddflag);
	if (xcluster) {
		kprintf("X3 chain %p dip %p dchain %p dip->chain %p\n",
			xcluster->focus, dip, dcluster->focus,
			dip->cluster.focus);
		hammer2_cluster_unlock(xcluster);
		xcluster = NULL;
		*errorp = ENOSPC;
#if 0
		Debugger("X3");
#endif
	}

	/*
	 * Create entry in common parent directory using the seek position
	 * calculated above.
	 *
	 * We must refactor cluster because it might have been shifted into
	 * an indirect cluster by the create.
	 */
	if (*errorp == 0) {
		KKASSERT(xcluster == NULL);
#if 0
		*errorp = hammer2_cluster_create(trans, dcluster, &xcluster,
					       lhc, 0,
					       HAMMER2_BREF_TYPE_INODE,/* n/a */
					       HAMMER2_INODE_BYTES);   /* n/a */
#endif
		/*XXX this somehow isn't working on cluster XXX*/
		/*KKASSERT(xxx)*/
	}

	/*
	 * Cleanup and handle retries.
	 */
	if (*errorp == EAGAIN) {
		kprintf("R");
		hammer2_cluster_wait(dcluster);
		hammer2_cluster_drop(dcluster);
		goto retry;
	}

	/*
	 * Handle the error case
	 */
	if (*errorp) {
		panic("error2");
		KKASSERT(xcluster == NULL);
		return;
	}

	/*
	 * Use xcluster as a placeholder for (lhc).  Duplicate cluster to the
	 * same target bref as xcluster and then delete xcluster.  The
	 * duplication occurs after xcluster in flush order even though
	 * xcluster is deleted after the duplication. XXX
	 *
	 * WARNING! Duplications (to a different parent) can cause indirect
	 *	    blocks to be inserted, refactor xcluster.
	 *
	 * WARNING! Only key and keybits is extracted from a passed-in bref.
	 */
	hammer2_cluster_bref(cluster, &bref);
	bref.key = lhc;			/* invisible dir entry key */
	bref.keybits = 0;
	hammer2_cluster_duplicate(trans, dcluster, cluster, &bref, 0, 2);

	/*
	 * cluster is now 'live' again.. adjust the filename.
	 *
	 * Directory entries are inodes but this is a hidden hardlink
	 * target.  The name isn't used but to ease debugging give it
	 * a name after its inode number.
	 */
	hammer2_cluster_modify(trans, cluster, 0);
	nipdata = &hammer2_cluster_data(cluster)->ipdata;
	ksnprintf(nipdata->filename, sizeof(nipdata->filename),
		  "0x%016jx", (intmax_t)nipdata->inum);
	nipdata->name_len = strlen(nipdata->filename);
	nipdata->name_key = lhc;
	nipdata->nlinks += nlinks;
}

/*
 * Connect the target inode represented by (*chainp) to the media topology
 * at (dip, name, len).  The caller can pass a rough *chainp, this function
 * will issue lookup()s to position the parent chain properly for the
 * chain insertion.
 *
 * If hlink is TRUE this function creates an OBJTYPE_HARDLINK directory
 * entry instead of connecting (*chainp).
 *
 * If hlink is FALSE this function uses chain_duplicate() to make a copy
 * if (*chainp) in the directory entry.  (*chainp) is likely to be deleted
 * by the caller in this case (e.g. rename).
 */
int
hammer2_inode_connect(hammer2_trans_t *trans,
		      hammer2_cluster_t **clusterp, int hlink,
		      hammer2_inode_t *dip, hammer2_cluster_t *dcluster,
		      const uint8_t *name, size_t name_len,
		      hammer2_key_t lhc)
{
	hammer2_inode_data_t *ipdata;
	hammer2_cluster_t *ocluster;
	hammer2_cluster_t *ncluster;
	hammer2_key_t key_dummy;
	int ddflag;
	int error;

	/*
	 * Since ocluster is either disconnected from the topology or
	 * represents a hardlink terminus which is always a parent of or
	 * equal to dip, we should be able to safely lock dip->chain for
	 * our setup.
	 *
	 * WARNING! Must use inode_lock_ex() on dip to handle a stale
	 *	    dip->cluster.
	 */
	ocluster = *clusterp;

	/*
	 * If name is non-NULL we calculate lhc, else we use the passed-in
	 * lhc.
	 */
	if (name) {
		lhc = hammer2_dirhash(name, name_len);

		/*
		 * Locate the inode or indirect block to create the new
		 * entry in.  At the same time check for key collisions
		 * and iterate until we don't get one.
		 */
		error = 0;
		while (error == 0) {
			ncluster = hammer2_cluster_lookup(dcluster, &key_dummy,
						      lhc, lhc,
						      0, &ddflag);
			if (ncluster == NULL)
				break;
			if ((lhc & HAMMER2_DIRHASH_LOMASK) ==
			    HAMMER2_DIRHASH_LOMASK) {
				error = ENOSPC;
			}
			hammer2_cluster_unlock(ncluster);
			ncluster = NULL;
			++lhc;
		}
	} else {
		/*
		 * Reconnect to specific key (used when moving
		 * unlinked-but-open files into the hidden directory).
		 */
		ncluster = hammer2_cluster_lookup(dcluster, &key_dummy,
						  lhc, lhc,
						  0, &ddflag);
		KKASSERT(ncluster == NULL);
	}

	if (error == 0) {
		if (hlink) {
			/*
			 * Hardlink pointer needed, create totally fresh
			 * directory entry.
			 *
			 * We must refactor ocluster because it might have
			 * been shifted into an indirect cluster by the
			 * create.
			 */
			KKASSERT(ncluster == NULL);
			error = hammer2_cluster_create(trans,
						       dcluster, &ncluster,
						       lhc, 0,
						       HAMMER2_BREF_TYPE_INODE,
						       HAMMER2_INODE_BYTES);
			hammer2_cluster_refactor(ocluster);
		} else {
			/*
			 * Reconnect the original cluster and rename.  Use
			 * cluster_duplicate().  The caller will likely delete
			 * or has already deleted the original chain in
			 * this case.
			 *
			 * NOTE: cluster_duplicate() generates a new cluster
			 *	 with CHAIN_DELETED cleared (ocluster typically
			 *	 has it set from the file unlink).
			 *
			 * WARNING! Can cause held-over clusters to require a
			 *	    refactor.  Fortunately we have none (our
			 *	    locked clusters are passed into and
			 *	    modified by the call).
			 */
			ncluster = ocluster;
			ocluster = NULL;
			hammer2_cluster_duplicate(trans, NULL, ncluster, NULL,
						  0, 3);
			error = hammer2_cluster_create(trans,
						       dcluster, &ncluster,
						       lhc, 0,
						       HAMMER2_BREF_TYPE_INODE,
						       HAMMER2_INODE_BYTES);
		}
	}

	/*
	 * Unlock stuff.
	 */
	KKASSERT(error != EAGAIN);

	/*
	 * ncluster should be NULL on error, leave ocluster
	 * (ocluster == *clusterp) alone.
	 */
	if (error) {
		KKASSERT(ncluster == NULL);
		return (error);
	}

	/*
	 * Directory entries are inodes so if the name has changed we have
	 * to update the inode.
	 *
	 * When creating an OBJTYPE_HARDLINK entry remember to unlock the
	 * cluster, the caller will access the hardlink via the actual hardlink
	 * target file and not the hardlink pointer entry, so we must still
	 * return ocluster.
	 */
	if (hlink && hammer2_hardlink_enable >= 0) {
		/*
		 * Create the HARDLINK pointer.  oip represents the hardlink
		 * target in this situation.
		 *
		 * We will return ocluster (the hardlink target).
		 */
		hammer2_cluster_modify(trans, ncluster, 0);
		KKASSERT(name_len < HAMMER2_INODE_MAXNAME);
		ipdata = &hammer2_cluster_data(ncluster)->ipdata;
		bcopy(name, ipdata->filename, name_len);
		ipdata->name_key = lhc;
		ipdata->name_len = name_len;
		ipdata->target_type =
				hammer2_cluster_data(ocluster)->ipdata.type;
		ipdata->type = HAMMER2_OBJTYPE_HARDLINK;
		ipdata->inum = hammer2_cluster_data(ocluster)->ipdata.inum;
		ipdata->nlinks = 1;
		hammer2_cluster_unlock(ncluster);
		ncluster = ocluster;
		ocluster = NULL;
	} else {
		/*
		 * ncluster is a duplicate of ocluster at the new location.
		 * We must fixup the name stored in oip.  The bref key
		 * has already been set up.
		 */
		hammer2_cluster_modify(trans, ncluster, 0);
		ipdata = &hammer2_cluster_data(ncluster)->ipdata;

		KKASSERT(name_len < HAMMER2_INODE_MAXNAME);
		bcopy(name, ipdata->filename, name_len);
		ipdata->name_key = lhc;
		ipdata->name_len = name_len;
		ipdata->nlinks = 1;
	}

	/*
	 * We are replacing ocluster with ncluster, unlock ocluster.  In the
	 * case where ocluster is left unchanged the code above sets
	 * ncluster to ocluster and ocluster to NULL, resulting in a NOP here.
	 */
	if (ocluster)
		hammer2_cluster_unlock(ocluster);
	*clusterp = ncluster;

	return (0);
}

/*
 * Repoint ip->cluster's chains to cluster's chains.  Caller must hold
 * the inode exclusively locked.  cluster may be NULL to clean out any
 * chains in ip->cluster.
 */
void
hammer2_inode_repoint(hammer2_inode_t *ip, hammer2_inode_t *pip,
		      hammer2_cluster_t *cluster)
{
	hammer2_chain_t *ochain;
	hammer2_chain_t *nchain;
	hammer2_inode_t *opip;
	int i;

	/*
	 * Replace chains in ip->cluster with chains from cluster and
	 * adjust the focus if necessary.
	 *
	 * NOTE: nchain and/or ochain can be NULL due to gaps
	 *	 in the cluster arrays.
	 */
	ip->cluster.focus = NULL;
	for (i = 0; cluster && i < cluster->nchains; ++i) {
		nchain = cluster->array[i];
		if (i < ip->cluster.nchains) {
			ochain = ip->cluster.array[i];
			if (ochain == nchain)
				continue;
		} else {
			ochain = NULL;
		}

		/*
		 * Make adjustments
		 */
		ip->cluster.array[i] = nchain;
		if (ip->cluster.focus == NULL)
			ip->cluster.focus = nchain;
		if (nchain)
			hammer2_chain_ref(nchain);
		if (ochain)
			hammer2_chain_drop(ochain);
	}

	/*
	 * Release any left-over chains in ip->cluster.
	 */
	while (i < ip->cluster.nchains) {
		nchain = ip->cluster.array[i];
		if (nchain) {
			ip->cluster.array[i] = NULL;
			hammer2_chain_drop(nchain);
		}
		++i;
	}
	ip->cluster.nchains = cluster ? cluster->nchains : 0;

	/*
	 * Repoint ip->pip if requested (non-NULL pip).
	 */
	if (pip && ip->pip != pip) {
		opip = ip->pip;
		hammer2_inode_ref(pip);
		ip->pip = pip;
		if (opip)
			hammer2_inode_drop(opip);
	}
}

/*
 * Unlink the file from the specified directory inode.  The directory inode
 * does not need to be locked.
 *
 * isdir determines whether a directory/non-directory check should be made.
 * No check is made if isdir is set to -1.
 *
 * isopen specifies whether special unlink-with-open-descriptor handling
 * must be performed.  If set to -1 the caller is deleting a PFS and we
 * check whether the chain is mounted or not (chain->pmp != NULL).  1 is
 * implied if it is mounted.
 *
 * If isopen is 1 and nlinks drops to 0 this function must move the chain
 * to a special hidden directory until last-close occurs on the file.
 *
 * NOTE!  The underlying file can still be active with open descriptors
 *	  or if the chain is being manually held (e.g. for rename).
 *
 *	  The caller is responsible for fixing up ip->chain if e.g. a
 *	  rename occurs (see chain_duplicate()).
 */
int
hammer2_unlink_file(hammer2_trans_t *trans, hammer2_inode_t *dip,
		    const uint8_t *name, size_t name_len,
		    int isdir, int *hlinkp, struct nchandle *nch)
{
	hammer2_inode_data_t *ipdata;
	hammer2_cluster_t *cparent;
	hammer2_cluster_t *ocluster;
	hammer2_cluster_t *cluster;
	hammer2_cluster_t *dparent;
	hammer2_cluster_t *dcluster;
	hammer2_key_t key_dummy;
	hammer2_key_t key_next;
	hammer2_key_t lhc;
	int error;
	int ddflag;
	uint8_t type;

	error = 0;
	ocluster = NULL;
	lhc = hammer2_dirhash(name, name_len);

	/*
	 * Search for the filename in the directory
	 */
	if (hlinkp)
		*hlinkp = 0;
	cparent = hammer2_inode_lock_ex(dip);
	cluster = hammer2_cluster_lookup(cparent, &key_next,
				     lhc, lhc + HAMMER2_DIRHASH_LOMASK,
				     0, &ddflag);
	while (cluster) {
		if (hammer2_cluster_type(cluster) == HAMMER2_BREF_TYPE_INODE) {
			ipdata = &hammer2_cluster_data(cluster)->ipdata;
			if (ipdata->name_len == name_len &&
			    bcmp(ipdata->filename, name, name_len) == 0) {
				break;
			}
		}
		cluster = hammer2_cluster_next(cparent, cluster, &key_next,
					       key_next,
					       lhc + HAMMER2_DIRHASH_LOMASK,
					       0);
	}
	hammer2_inode_unlock_ex(dip, NULL);	/* retain parent */

	/*
	 * Not found or wrong type (isdir < 0 disables the type check).
	 * If a hardlink pointer, type checks use the hardlink target.
	 */
	if (cluster == NULL) {
		error = ENOENT;
		goto done;
	}
	ipdata = &hammer2_cluster_data(cluster)->ipdata;
	type = ipdata->type;
	if (type == HAMMER2_OBJTYPE_HARDLINK) {
		if (hlinkp)
			*hlinkp = 1;
		type = ipdata->target_type;
	}

	if (type == HAMMER2_OBJTYPE_DIRECTORY && isdir == 0) {
		error = ENOTDIR;
		goto done;
	}
	if (type != HAMMER2_OBJTYPE_DIRECTORY && isdir >= 1) {
		error = EISDIR;
		goto done;
	}

	/*
	 * Hardlink must be resolved.  We can't hold the parent locked
	 * while we do this or we could deadlock.
	 *
	 * On success cluster will be adjusted to point at the hardlink target
	 * and ocluster will point to the hardlink pointer in the original
	 * directory.  Otherwise cluster remains pointing to the original.
	 *
	 * Lock ownership is transfered to cluster.  ocluster is merely
	 * referenced.
	 */
	if (ipdata->type == HAMMER2_OBJTYPE_HARDLINK) {
		hammer2_cluster_unlock(cparent);
		cparent = NULL;

		ocluster = cluster;
		cluster = hammer2_cluster_copy(ocluster,
					       HAMMER2_CLUSTER_COPY_CHAINS);
		error = hammer2_hardlink_find(dip, cluster);
		KKASSERT(error == 0);
	}

	/*
	 * If this is a directory the directory must be empty.  However, if
	 * isdir < 0 we are doing a rename and the directory does not have
	 * to be empty, and if isdir > 1 we are deleting a PFS/snapshot
	 * and the directory does not have to be empty.
	 *
	 * NOTE: We check the full key range here which covers both visible
	 *	 and invisible entries.  Theoretically there should be no
	 *	 invisible (hardlink target) entries if there are no visible
	 *	 entries.
	 */
	if (type == HAMMER2_OBJTYPE_DIRECTORY && isdir == 1) {
		dparent = hammer2_cluster_lookup_init(cluster, 0);
		dcluster = hammer2_cluster_lookup(dparent, &key_dummy,
					          0, (hammer2_key_t)-1,
					          HAMMER2_LOOKUP_NODATA,
						  &ddflag);
		if (dcluster) {
			hammer2_cluster_unlock(dcluster);
			hammer2_cluster_lookup_done(dparent);
			error = ENOTEMPTY;
			goto done;
		}
		hammer2_cluster_lookup_done(dparent);
		dparent = NULL;
		/* dcluster NULL */
	}

	/*
	 * Ok, we can now unlink the cluster.  We always decrement nlinks even
	 * if the entry can be deleted in case someone has the file open and
	 * does an fstat().
	 *
	 * The cluster itself will no longer be in the on-media topology but
	 * can still be flushed to the media (e.g. if an open descriptor
	 * remains).  When the last vnode/ip ref goes away the cluster will
	 * be marked unmodified, avoiding any further (now unnecesary) I/O.
	 *
	 * A non-NULL ocluster indicates a hardlink.
	 */
	if (ocluster) {
		/*
		 * Delete the original hardlink pointer unconditionally.
		 * (any open descriptors will migrate to the hardlink
		 * target and have no affect on this operation).
		 *
		 * NOTE: parent from above is NULL when ocluster != NULL
		 *	 so we can reuse it.
		 */
		hammer2_cluster_lock(ocluster, HAMMER2_RESOLVE_ALWAYS);
		hammer2_cluster_delete(trans, ocluster, 0);
		hammer2_cluster_unlock(ocluster);
	}

	/*
	 * Decrement nlinks on the hardlink target (or original file if
	 * there it was not hardlinked).  Delete the target when nlinks
	 * reaches 0 with special handling if (isopen) is set.
	 *
	 * NOTE! In DragonFly the vnops function calls cache_unlink() after
	 *	 calling us here to clean out the namecache association,
	 *	 (which does not represent a ref for the open-test), and to
	 *	 force finalization of the vnode if/when the last ref gets
	 *	 dropped.
	 *
	 * NOTE! Files are unlinked by rename and then relinked.  nch will be
	 *	 passed as NULL in this situation.  hammer2_inode_connect()
	 *	 will bump nlinks.
	 */
	KKASSERT(cluster != NULL);
	hammer2_cluster_modify(trans, cluster, 0);
	ipdata = &hammer2_cluster_data(cluster)->ipdata;
	--ipdata->nlinks;
	if ((int64_t)ipdata->nlinks < 0)	/* XXX debugging */
		ipdata->nlinks = 0;
	if (ipdata->nlinks == 0) {
		if ((cluster->focus->flags & HAMMER2_CHAIN_PFSROOT) &&
		    cluster->pmp) {
			error = EINVAL;
			kprintf("hammer2: PFS \"%s\" cannot be deleted "
				"while still mounted\n",
				ipdata->filename);
			goto done;
		}
		if (nch && cache_isopen(nch)) {
			kprintf("WARNING: unlinking open file\n");
			hammer2_cluster_set_chainflags(cluster,
							HAMMER2_CHAIN_UNLINKED);
			hammer2_inode_move_to_hidden(trans, &cluster,
						     ipdata->inum);
		} else {
			hammer2_cluster_delete(trans, cluster, 0);
		}
	}
	error = 0;
done:
	if (cluster)
		hammer2_cluster_unlock(cluster);
	if (cparent)
		hammer2_cluster_lookup_done(cparent);
	if (ocluster) {
		hammer2_cluster_drop(ocluster);
	}

	return error;
}

/*
 * This is called from the mount code to initialize pmp->ihidden
 */
void
hammer2_inode_install_hidden(hammer2_pfsmount_t *pmp)
{
	hammer2_trans_t trans;
	hammer2_cluster_t *cparent;
	hammer2_cluster_t *cluster;
	hammer2_cluster_t *scan;
	hammer2_inode_data_t *ipdata;
	hammer2_key_t key_dummy;
	hammer2_key_t key_next;
	int ddflag;
	int error;
	int count;

	if (pmp->ihidden)
		return;

	/*
	 * Find the hidden directory
	 */
	bzero(&key_dummy, sizeof(key_dummy));
	hammer2_trans_init(&trans, pmp, NULL, 0);

	cparent = hammer2_inode_lock_ex(pmp->iroot);
	cluster = hammer2_cluster_lookup(cparent, &key_dummy,
					 HAMMER2_INODE_HIDDENDIR,
					 HAMMER2_INODE_HIDDENDIR,
					 0, &ddflag);
	if (cluster) {
		pmp->ihidden = hammer2_inode_get(pmp, pmp->iroot, cluster);
		hammer2_inode_ref(pmp->ihidden);

		/*
		 * Remove any unlinked files which were left open as-of
		 * any system crash.
		 */
		count = 0;
		scan = hammer2_cluster_lookup(cluster, &key_next,
					      0, HAMMER2_MAX_TID,
					      HAMMER2_LOOKUP_NODATA, &ddflag);
		while (scan) {
			if (hammer2_cluster_type(scan) ==
			    HAMMER2_BREF_TYPE_INODE) {
				hammer2_cluster_delete(&trans, scan, 0);
				++count;
			}
			scan = hammer2_cluster_next(cluster, scan, &key_next,
						    0, HAMMER2_MAX_TID,
						    HAMMER2_LOOKUP_NODATA);
		}

		hammer2_inode_unlock_ex(pmp->ihidden, cluster);
		hammer2_inode_unlock_ex(pmp->iroot, cparent);
		hammer2_trans_done(&trans);
		kprintf("hammer2: PFS loaded hidden dir, "
			"removed %d dead entries\n", count);
		return;
	}

	/*
	 * Create the hidden directory
	 */
	error = hammer2_cluster_create(&trans, cparent, &cluster,
				       HAMMER2_INODE_HIDDENDIR, 0,
				       HAMMER2_BREF_TYPE_INODE,
				       HAMMER2_INODE_BYTES);
	hammer2_inode_unlock_ex(pmp->iroot, cparent);
	hammer2_cluster_modify(&trans, cluster, 0);
	ipdata = &hammer2_cluster_data(cluster)->ipdata;
	ipdata->type = HAMMER2_OBJTYPE_DIRECTORY;
	ipdata->inum = HAMMER2_INODE_HIDDENDIR;
	ipdata->nlinks = 1;
	kprintf("hammer2: PFS root missing hidden directory, creating\n");

	pmp->ihidden = hammer2_inode_get(pmp, pmp->iroot, cluster);
	hammer2_inode_ref(pmp->ihidden);
	hammer2_inode_unlock_ex(pmp->ihidden, cluster);
	hammer2_trans_done(&trans);
}

/*
 * If an open file is unlinked H2 needs to retain the file in the topology
 * to ensure that its backing store is not recovered by the bulk free scan.
 * This also allows us to avoid having to special-case the CHAIN_DELETED flag.
 *
 * To do this the file is moved to a hidden directory in the PFS root and
 * renamed.  The hidden directory must be created if it does not exist.
 */
static
void
hammer2_inode_move_to_hidden(hammer2_trans_t *trans,
			     hammer2_cluster_t **clusterp, hammer2_tid_t inum)
{
	hammer2_cluster_t *dcluster;
	hammer2_pfsmount_t *pmp;
	int error;

	pmp = (*clusterp)->pmp;
	KKASSERT(pmp != NULL);
	KKASSERT(pmp->ihidden != NULL);

	hammer2_cluster_delete(trans, *clusterp, 0);
	dcluster = hammer2_inode_lock_ex(pmp->ihidden);
	error = hammer2_inode_connect(trans, clusterp, 0,
				      pmp->ihidden, dcluster,
				      NULL, 0, inum);
	hammer2_inode_unlock_ex(pmp->ihidden, dcluster);
	KKASSERT(error == 0);
}

/*
 * Given an exclusively locked inode and cluster we consolidate its cluster
 * for hardlink creation, adding (nlinks) to the file's link count and
 * potentially relocating the inode to a directory common to ip->pip and tdip.
 *
 * Replaces (*clusterp) if consolidation occurred, unlocking the old cluster
 * and returning a new locked cluster.
 *
 * NOTE!  This function will also replace ip->cluster.
 */
int
hammer2_hardlink_consolidate(hammer2_trans_t *trans,
			     hammer2_inode_t *ip,
			     hammer2_cluster_t **clusterp,
			     hammer2_inode_t *cdip,
			     hammer2_cluster_t *cdcluster,
			     int nlinks)
{
	hammer2_inode_data_t *ipdata;
	hammer2_cluster_t *cluster;
	hammer2_cluster_t *ncluster;
	int error;

	cluster = *clusterp;
	ipdata = &hammer2_cluster_data(cluster)->ipdata;
	if (nlinks == 0 &&			/* no hardlink needed */
	    (ipdata->name_key & HAMMER2_DIRHASH_VISIBLE)) {
		return (0);
	}

	if (hammer2_hardlink_enable == 0) {	/* disallow hardlinks */
		hammer2_cluster_unlock(cluster);
		*clusterp = NULL;
		return (ENOTSUP);
	}

	/*
	 * If no change in the hardlink's target directory is required and
	 * this is already a hardlink target, all we need to do is adjust
	 * the link count.
	 */
	ipdata = &hammer2_cluster_data(cluster)->ipdata;
	if (cdip == ip->pip &&
	    (ipdata->name_key & HAMMER2_DIRHASH_VISIBLE) == 0) {
		if (nlinks) {
			hammer2_cluster_modify(trans, cluster, 0);
			ipdata = &hammer2_cluster_data(cluster)->ipdata;
			ipdata->nlinks += nlinks;
		}
		error = 0;
		goto done;
	}


	/*
	 * cluster is the real inode.  If it's visible we have to convert it
	 * to a hardlink pointer.  If it is not visible then it is already
	 * a hardlink target and only needs to be deleted.
	 */
	KKASSERT((cluster->focus->flags & HAMMER2_CHAIN_DELETED) == 0);
	ipdata = &hammer2_cluster_data(cluster)->ipdata;
	KKASSERT(ipdata->type != HAMMER2_OBJTYPE_HARDLINK);
	if (ipdata->name_key & HAMMER2_DIRHASH_VISIBLE) {
		/*
		 * We are going to duplicate cluster later, causing its
		 * media block to be shifted to the duplicate.  Even though
		 * we are delete-duplicating ncluster here it might decide not
		 * to reallocate the block.  Set FORCECOW to force it to.
		 */
		ncluster = hammer2_cluster_copy(cluster,
						HAMMER2_CLUSTER_COPY_CHAINS |
						HAMMER2_CLUSTER_COPY_NOREF);
		hammer2_cluster_lock(ncluster, HAMMER2_RESOLVE_ALWAYS);
		hammer2_cluster_set_chainflags(ncluster,
					       HAMMER2_CHAIN_FORCECOW);
		hammer2_cluster_delete_duplicate(trans, ncluster,
						 HAMMER2_DELDUP_RECORE);
		KKASSERT((ncluster->focus->flags &
			 HAMMER2_CHAIN_DUPLICATED) == 0);
		ipdata = &hammer2_cluster_data(ncluster)->ipdata;
		ipdata->target_type = ipdata->type;
		ipdata->type = HAMMER2_OBJTYPE_HARDLINK;
		ipdata->uflags = 0;
		ipdata->rmajor = 0;
		ipdata->rminor = 0;
		ipdata->ctime = 0;
		ipdata->mtime = 0;
		ipdata->atime = 0;
		ipdata->btime = 0;
		bzero(&ipdata->uid, sizeof(ipdata->uid));
		bzero(&ipdata->gid, sizeof(ipdata->gid));
		ipdata->op_flags = HAMMER2_OPFLAG_DIRECTDATA;
		ipdata->cap_flags = 0;
		ipdata->mode = 0;
		ipdata->size = 0;
		ipdata->nlinks = 1;
		ipdata->iparent = 0;	/* XXX */
		ipdata->pfs_type = 0;
		ipdata->pfs_inum = 0;
		bzero(&ipdata->pfs_clid, sizeof(ipdata->pfs_clid));
		bzero(&ipdata->pfs_fsid, sizeof(ipdata->pfs_fsid));
		ipdata->data_quota = 0;
		ipdata->data_count = 0;
		ipdata->inode_quota = 0;
		ipdata->inode_count = 0;
		ipdata->attr_tid = 0;
		ipdata->dirent_tid = 0;
		bzero(&ipdata->u, sizeof(ipdata->u));
		/* XXX transaction ids */
	} else {
		hammer2_cluster_delete(trans, cluster, 0);
		ncluster = NULL;
	}

	/*
	 * cluster represents the hardlink target and is now flagged deleted.
	 * duplicate it to the parent directory and adjust nlinks.
	 *
	 * WARNING! The shiftup() call can cause ncluster to be moved into
	 *	    an indirect block, and our ncluster will wind up pointing
	 *	    to the older/original version.
	 */
	KKASSERT(cluster->focus->flags & HAMMER2_CHAIN_DELETED);
	hammer2_hardlink_shiftup(trans, cluster, cdip, cdcluster,
				 nlinks, &error);

	if (error == 0)
		hammer2_inode_repoint(ip, cdip, cluster);

	/*
	 * Unlock and destroy ncluster.
	 * Return the shifted cluster in *clusterp.
	 */
	if (ncluster)
		hammer2_cluster_unlock(ncluster);

done:
	/*
	 * Cleanup, cluster/ncluster already dealt with.
	 */
	*clusterp = cluster;
	hammer2_inode_drop(cdip);

	return (error);
}

/*
 * If (*ochainp) is non-NULL it points to the forward OBJTYPE_HARDLINK
 * inode while (*chainp) points to the resolved (hidden hardlink
 * target) inode.  In this situation when nlinks is 1 we wish to
 * deconsolidate the hardlink, moving it back to the directory that now
 * represents the only remaining link.
 */
int
hammer2_hardlink_deconsolidate(hammer2_trans_t *trans,
			       hammer2_inode_t *dip,
			       hammer2_chain_t **chainp,
			       hammer2_chain_t **ochainp)
{
	if (*ochainp == NULL)
		return (0);
	/* XXX */
	return (0);
}

/*
 * The caller presents a locked *chainp pointing to a HAMMER2_BREF_TYPE_INODE
 * with an obj_type of HAMMER2_OBJTYPE_HARDLINK.  This routine will gobble
 * the *chainp and return a new locked *chainp representing the file target
 * (the original *chainp will be unlocked).
 *
 * When a match is found the chain representing the original HARDLINK
 * will be returned in *ochainp with a ref, but not locked.
 *
 * When no match is found *chainp is set to NULL and EIO is returned.
 * (*ochainp) will still be set to the original chain with a ref but not
 * locked.
 */
int
hammer2_hardlink_find(hammer2_inode_t *dip, hammer2_cluster_t *cluster)
{
	hammer2_inode_data_t *ipdata;
	hammer2_cluster_t *cparent;
	hammer2_cluster_t *rcluster;
	hammer2_inode_t *ip;
	hammer2_inode_t *pip;
	hammer2_key_t key_dummy;
	hammer2_key_t lhc;
	int ddflag;

	pip = dip;
	hammer2_inode_ref(pip);		/* for loop */

	/*
	 * Locate the hardlink.  pip is referenced and not locked.
	 */
	ipdata = &hammer2_cluster_data(cluster)->ipdata;
	lhc = ipdata->inum;

	/*
	 * We don't need the cluster's chains, but we need to retain the
	 * cluster structure itself so we can load the hardlink search
	 * result into it.
	 */
	KKASSERT(cluster->refs == 1);
	atomic_add_int(&cluster->refs, 1);
	hammer2_cluster_unlock(cluster);	/* hack */
	cluster->nchains = 0;			/* hack */

	rcluster = NULL;

	while ((ip = pip) != NULL) {
		cparent = hammer2_inode_lock_ex(ip);
		hammer2_inode_drop(ip);			/* loop */
		KKASSERT(hammer2_cluster_type(cparent) ==
			 HAMMER2_BREF_TYPE_INODE);
		rcluster = hammer2_cluster_lookup(cparent, &key_dummy,
					     lhc, lhc, 0, &ddflag);
		hammer2_cluster_lookup_done(cparent);	/* discard parent */
		if (rcluster)
			break;
		pip = ip->pip;		/* safe, ip held locked */
		if (pip)
			hammer2_inode_ref(pip);		/* loop */
		hammer2_inode_unlock_ex(ip, NULL);
	}

	/*
	 * chain is locked, ip is locked.  Unlock ip, return the locked
	 * chain.  *ipp is already set w/a ref count and not locked.
	 *
	 * (cparent is already unlocked).
	 */
	if (ip)
		hammer2_inode_unlock_ex(ip, NULL);

	if (rcluster) {
		hammer2_cluster_replace(cluster, rcluster);
		hammer2_cluster_drop(rcluster);
		return (0);
	} else {
		return (EIO);
	}
}

/*
 * Find the directory common to both fdip and tdip, hold and return
 * its inode.
 */
hammer2_inode_t *
hammer2_inode_common_parent(hammer2_inode_t *fdip, hammer2_inode_t *tdip)
{
	hammer2_inode_t *scan1;
	hammer2_inode_t *scan2;

	/*
	 * We used to have a depth field but it complicated matters too
	 * much for directory renames.  So now its ugly.  Check for
	 * simple cases before giving up and doing it the expensive way.
	 *
	 * XXX need a bottom-up topology stability lock
	 */
	if (fdip == tdip || fdip == tdip->pip) {
		hammer2_inode_ref(fdip);
		return(fdip);
	}
	if (fdip->pip == tdip) {
		hammer2_inode_ref(tdip);
		return(tdip);
	}

	/*
	 * XXX not MPSAFE
	 */
	for (scan1 = fdip; scan1->pmp == fdip->pmp; scan1 = scan1->pip) {
		scan2 = tdip;
		while (scan2->pmp == tdip->pmp) {
			if (scan1 == scan2) {
				hammer2_inode_ref(scan1);
				return(scan1);
			}
			scan2 = scan2->pip;
			if (scan2 == NULL)
				break;
		}
	}
	panic("hammer2_inode_common_parent: no common parent %p %p\n",
	      fdip, tdip);
	/* NOT REACHED */
	return(NULL);
}

/*
 * Synchronize the inode's frontend state with the chain state prior
 * to any explicit flush of the inode or any strategy write call.
 *
 * Called with a locked inode.
 */
void
hammer2_inode_fsync(hammer2_trans_t *trans, hammer2_inode_t *ip, 
		    hammer2_cluster_t *cparent)
{
	hammer2_inode_data_t *ipdata;
	hammer2_cluster_t *dparent;
	hammer2_cluster_t *cluster;
	hammer2_key_t lbase;
	hammer2_key_t key_next;
	int ddflag;

	ipdata = &hammer2_cluster_data(cparent)->ipdata;    /* target file */

	if (ip->flags & HAMMER2_INODE_MTIME) {
		ipdata = hammer2_cluster_modify_ip(trans, ip, cparent, 0);
		atomic_clear_int(&ip->flags, HAMMER2_INODE_MTIME);
		ipdata->mtime = ip->mtime;
	}
	if ((ip->flags & HAMMER2_INODE_RESIZED) && ip->size < ipdata->size) {
		ipdata = hammer2_cluster_modify_ip(trans, ip, cparent, 0);
		ipdata->size = ip->size;
		atomic_clear_int(&ip->flags, HAMMER2_INODE_RESIZED);

		/*
		 * We must delete any chains beyond the EOF.  The chain
		 * straddling the EOF will be pending in the bioq.
		 */
		lbase = (ipdata->size + HAMMER2_PBUFMASK64) &
			~HAMMER2_PBUFMASK64;
		dparent = hammer2_cluster_lookup_init(&ip->cluster, 0);
		cluster = hammer2_cluster_lookup(dparent, &key_next,
					         lbase, (hammer2_key_t)-1,
						 HAMMER2_LOOKUP_NODATA,
						 &ddflag);
		while (cluster) {
			/*
			 * Degenerate embedded case, nothing to loop on
			 */
			switch (hammer2_cluster_type(cluster)) {
			case HAMMER2_BREF_TYPE_INODE:
				hammer2_cluster_unlock(cluster);
				cluster = NULL;
				break;
			case HAMMER2_BREF_TYPE_DATA:
				hammer2_cluster_delete(trans, cluster, 0);
				/* fall through */
			default:
				cluster = hammer2_cluster_next(dparent, cluster,
						   &key_next,
						   key_next, (hammer2_key_t)-1,
						   HAMMER2_LOOKUP_NODATA);
				break;
			}
		}
		hammer2_cluster_lookup_done(dparent);
	} else
	if ((ip->flags & HAMMER2_INODE_RESIZED) && ip->size > ipdata->size) {
		ipdata = hammer2_cluster_modify_ip(trans, ip, cparent, 0);
		ipdata->size = ip->size;
		atomic_clear_int(&ip->flags, HAMMER2_INODE_RESIZED);

		/*
		 * When resizing larger we may not have any direct-data
		 * available.
		 */
		if ((ipdata->op_flags & HAMMER2_OPFLAG_DIRECTDATA) &&
		    ip->size > HAMMER2_EMBEDDED_BYTES) {
			ipdata->op_flags &= ~HAMMER2_OPFLAG_DIRECTDATA;
			bzero(&ipdata->u.blockset, sizeof(ipdata->u.blockset));
		}
	}
}
