/*-
 * Copyright (c) 2011-2014 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 * by Daniel Flores (GSOC 2013 - mentored by Matthew Dillon, compression)
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
//#include <sys/nlookup.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/fcntl.h>
#include <sys/buf.h>
#include <sys/uuid.h>
//#include <sys/vfsops.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/stdint.h>
#include <sys/objcache.h>

#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/uio.h>
#include <sys/mutex.h>
#include <sys/dmsg.h>

#include <machine/mplock.h>
#include <lib/libz/zlib.h>

#include "hammer2.h"
#include "hammer2_lz4.h"

#define REPORT_REFS_ERRORS 1	/* XXX remove me */
#define VOP_FSYNC(vp, waitfor, flags)                   \
        vop_fsync(*(vp)->v_op, vp, waitfor, flags)

int vop_fsync(void *);

struct hammer2_sync_info {
	hammer2_trans_t trans;
	int error;
	int waitfor;
};

TAILQ_HEAD(hammer2_mntlist, hammer2_mount);
TAILQ_HEAD(hammer2_pfslist, hammer2_pfsmount);
static struct hammer2_mntlist hammer2_mntlist;
static struct hammer2_pfslist hammer2_pfslist;
static struct lock hammer2_mntlk;

//unsigned long curthread;

int bootverbose;
int hammer2_debug;
int hammer2_cluster_enable = 1;
int hammer2_hardlink_enable = 1;
int hammer2_flush_pipe = 100;
int hammer2_synchronous_flush = 1;
int hammer2_dio_count;
long hammer2_limit_dirty_chains;
long hammer2_iod_file_read;
long hammer2_iod_meta_read;
long hammer2_iod_indr_read;
long hammer2_iod_fmap_read;
long hammer2_iod_volu_read;
long hammer2_iod_file_write;
long hammer2_iod_meta_write;
long hammer2_iod_indr_write;
long hammer2_iod_fmap_write;
long hammer2_iod_volu_write;
long hammer2_ioa_file_read;
long hammer2_ioa_meta_read;
long hammer2_ioa_indr_read;
long hammer2_ioa_fmap_read;
long hammer2_ioa_volu_read;
long hammer2_ioa_fmap_write;
long hammer2_ioa_file_write;
long hammer2_ioa_meta_write;
long hammer2_ioa_indr_write;
long hammer2_ioa_volu_write;

#define C_BUFFER "compbuffer Buffer used for compression"
#define D_BUFFER "decompbuffer Buffer used for decompression."

int hammer2_vfs_init(struct vfsconf *);
int hammer2_vfs_uninit(struct vfsconf *);
int hammer2_vfs_mount(struct mount *mp, char *path, caddr_t data,
				struct ucred *cred);
static int hammer2_remount(hammer2_mount_t *, struct mount *, char *,
				struct vnode *, struct ucred *);
static int hammer2_recovery(hammer2_mount_t *hmp);
static int hammer2_vfs_unmount(struct mount *mp, int mntflags);
static int hammer2_vfs_root(struct mount *mp, struct vnode **vpp);
static int hammer2_vfs_statfs(struct mount *mp, struct statfs *sbp,
				struct ucred *cred);
static int hammer2_vfs_statvfs(struct mount *, struct statfs *, struct ucred *);
static int hammer2_vfs_vget(struct mount *mp, struct vnode *dvp,
				ino_t ino, struct vnode **vpp);
static int hammer2_vfs_fhtovp(struct mount *mp, struct vnode *rootvp,
				struct fid *fhp, struct vnode **vpp);
static int hammer2_vfs_vptofh(struct vnode *vp, struct fid *fhp);
static int hammer2_vfs_checkexp(struct mount *mp, struct sockaddr *nam,
				int *exflagsp, struct ucred **credanonp);

static int hammer2_install_volume_header(hammer2_mount_t *hmp);
static int hammer2_sync_scan2(struct mount *, struct vnode *, void *);

static void hammer2_write_thread(void *arg);

static void hammer2_vfs_unmount_hmp1(struct mount *mp, hammer2_mount_t *hmp);
static void hammer2_vfs_unmount_hmp2(struct mount *mp, hammer2_mount_t *hmp);

/* 
 * Functions for compression in threads,
 * from hammer2_vnops.c
 */
static void hammer2_write_file_core(struct buf *bp, hammer2_trans_t *trans,
				hammer2_inode_t *ip,
				hammer2_inode_data_t *ipdata,
				hammer2_cluster_t *cparent,
				hammer2_key_t lbase, int ioflag, int pblksize,
				int *errorp);

static void hammer2_compress_and_write(struct buf *bp, hammer2_trans_t *,
				hammer2_inode_t *, hammer2_inode_data_t *,
				hammer2_cluster_t *,
				hammer2_key_t lbase, int, int,
				int *, int, int);
static void hammer2_zero_check_and_write(struct buf *bp,
				hammer2_trans_t *trans, hammer2_inode_t *ip,
				hammer2_inode_data_t *ipdata,
				hammer2_cluster_t *cparent,
				hammer2_key_t lbase,
				int ioflag, int pblksize, int *errorp, int check_algo);
static int test_block_zeros(const char *buf, size_t bytes);
static void zero_write(struct buf *bp, hammer2_trans_t *trans,
				hammer2_inode_t *ip,
				hammer2_inode_data_t *ipdata,
				hammer2_cluster_t *cparent,
				hammer2_key_t lbase,
				int *errorp);
static void hammer2_write_bp(hammer2_cluster_t *cluster, struct buf *bp,
				int ioflag, int pblksize, int *errorp, int);

static int hammer2_rcvdmsg(kdmsg_msg_t *msg);
static void hammer2_autodmsg(kdmsg_msg_t *msg);
static int hammer2_lnk_span_reply(kdmsg_state_t *state, kdmsg_msg_t *msg);

//int mtxsleep(void *, struct mtx *, int, const char *, int);
//void vclrisdirty(struct vnode *vp);
int vfs_init(struct vfsconf *vfc);
//int vfs_uninit(struct vfsconf *vfc, struct vfsconf *vfsp);
//int vfs_mount(struct mount *mp, char *path, caddr_t data, struct ucred *cred);
//int vfs_start(struct mount *mp, int flags);
int vfs_unmount(struct mount *mp, int mntflags);
int vfs_root(struct mount *mp, struct vnode **vpp);
//extern long atomic_cmpset_long(volatile u_long *_dst, u_long _exp, u_long _src);
void bioq_init(struct bio_queue_head *head);
struct bio *
bioq_first(struct bio_queue_head *head);
int nlookup_init(struct nlookupdata *, const char *, enum uio_seg, int);
int nlookup(struct nlookupdata *);
void nlookup_done(struct nlookupdata *);
struct buf *getpbuf (int *);
void relpbuf (struct buf *, int *);
int cache_vref(struct nchandle *nch, struct ucred *cred, struct vnode **vpp);
struct buf * getpbuf(int *pfreecnt);
void relpbuf(struct buf *bp, int *pfreecnt);

//struct file *holdfp(struct filedesc *fdp, int fd, int flag);
//void fhold(struct file *fp);

static __inline void spin_init(struct __mp_lock *spin, const char *descr);
int vsyncscan(struct mount *mp, int vmsc_flags, int x(struct mount *mp1, struct vnode *vp, void *data), void *data);
/*
 * Caller sets up iocom->auto_lnk_conn and iocom->auto_lnk_span, then calls
 * this function to handle the state machine for LNK_CONN and LNK_SPAN.
 */

/*
 * This is very similar to vmntvnodescan() but it only scans the
 * vnodes on the syncer list.  VFS's which support faster VFS_SYNC
 * operations use the VISDIRTY flag on the vnode to ensure that vnodes
 * with dirty inodes are added to the syncer in addition to vnodes
 * with dirty buffers, and can use this function instead of nmntvnodescan().
 * 
 * This is important when a system has millions of vnodes.
 */
int
vsyncscan(struct mount *mp, int vmsc_flags, int x(struct mount *mp1, struct vnode *vp, void *data), void *data)
{
	//struct syncer_ctx *ctx;
	//struct synclist *slp;
	//struct vnode *vp;
	int b;
	int i;
	int lkflags;

	if (vmsc_flags & VMSC_NOWAIT)
		lkflags = LK_NOWAIT;
	else
		lkflags = 0;

	/*
	 * Syncer list context.  This API requires a dedicated syncer thread.
	 * (MNTK_THR_SYNC).
	 */
	KKASSERT(mp->mnt_kern_flag & MNTK_THR_SYNC);
	//ctx = mp->mnt_syncer_ctx;
	//lwkt_gettoken(&ctx->sc_token);

	/*
	 * Setup for loop.  Allow races against the syncer thread but
	 * require that the syncer thread no be lazy if we were told
	 * not to be lazy.
	 */
	//b = ctx->syncer_delayno & ctx->syncer_mask;
	i = b;
	//if ((vmsc_flags & VMSC_NOWAIT) == 0)
	//	++ctx->syncer_forced;

	do {
		//slp = &ctx->syncer_workitem_pending[i];

		/* while ((vp = LIST_FIRST(slp)) != NULL) {
			KKASSERT(vp->v_mount == mp);
			if (vmsc_flags & VMSC_GETVP) {
				if (vget(vp, LK_EXCLUSIVE | lkflags) == 0) {
					slowfunc(mp, vp, data);
					vput(vp);
				}
			} else if (vmsc_flags & VMSC_GETVX) {
				vx_get(vp);
				slowfunc(mp, vp, data);
				vx_put(vp);
			} else {
				vhold(vp);
				slowfunc(mp, vp, data);
				vdrop(vp);
			}
			if (LIST_FIRST(slp) == vp)
				vn_syncer_add(vp, -(i + syncdelay));
		}
		*/
		//i = (i + 1) & ctx->syncer_mask;
	} while (i != b);

//	if ((vmsc_flags & VMSC_NOWAIT) == 0)
		//--ctx->syncer_forced;
	//lwkt_reltoken(&ctx->sc_token);

	return(0);
}

/*
int lwkt_create (void *, void *, struct thread **, struct thread *, int, int, const char *, ...)
{
	// fix me
	return 0;
}
*/

static __inline void
spin_init(struct __mp_lock *spin, const char *descr)
{
/*
	spin->counta = 0;
	spin->countb = 0;
	spin->descr = descr;
FIx me */
}

void
relpbuf(struct buf *bp, int *pfreecnt)
{
	printf("relpbuf\n");
}

struct buf *
getpbuf(int *pfreecnt)
{
	struct buf *bp;
	return (bp);
}

int
cache_vref(struct nchandle *nch, struct ucred *cred, struct vnode **vpp)
{
	printf("cache_vref\n");
	return(0);
}

static __inline void
bioq_insert_tail(struct bio_queue_head *bioq, struct bio *bio)
{
	bioq->transition = NULL;
	//TAILQ_INSERT_TAIL(&bioq->queue, bio, bio_act);
}

static __inline void
bioq_remove(struct bio_queue_head *bioq, struct bio *bio)
{
	/*
	* Adjust read insertion point when removing the bioq.  The
	* bio after the insert point is a write so move backwards
	* one (NULL will indicate all the reads have cleared).
	*/
//	if (bio == bioq->transition)
//		bioq->transition = TAILQ_NEXT(bio, bio_act);
	//TAILQ_REMOVE(&bioq->queue, bio, bio_act);
}

static __inline struct bio *
bioq_takefirst(struct bio_queue_head *bioq)
{
	struct bio *bp;
 
	bp = TAILQ_FIRST(&bioq->queue);
	if (bp != NULL)
		bioq_remove(bioq, bp);
	return (bp);
}

static __inline long
atomic_cmpset_long(volatile u_long *_dst, u_long _old, u_long _new)
{
        u_long res = _old;
 
	__asm __volatile("cmpxchgq %2,%1; " \
		: "+a" (res), "=m" (*_dst) \
                : "r" (_new), "m" (*_dst) \
		: "memory");
	return (res == _old);
}

/*
 * Initialize a nlookup() structure, early error return for copyin faults
 * or a degenerate empty string (which is not allowed).
 *
 * The first process proc0's credentials are used if the calling thread
 * is not associated with a process context.
 *
 * MPSAFE
 */

int
nlookup_init(struct nlookupdata *nd, 
	const char *path, enum uio_seg seg, int flags)
{
	int error;
	  return(error);
}

int
nlookup(struct nlookupdata *nd)
{
	int error;
	return(error);
}

/*
* Cleanup a nlookupdata structure after we are through with it.  This may
* be called on any nlookupdata structure initialized with nlookup_init().
* Calling nlookup_done() is mandatory in all cases except where nlookup_init()
* returns an error, even if as a consumer you believe you have taken all
* dynamic elements out of the nlookupdata structure.
*/
void
nlookup_done(struct nlookupdata *nd)
{
}

/*
int
vfs_init(struct vfsconf *vfc)
{
	int error;

	// xx get_mplock();
	error = (vfc->vfc_vfsops->vfs_init)(vfc);
	// xx rel_mplock();

	return (error);
}
*/


/*
* MPSAFE
*/
int
vfs_unmount(struct mount *mp, int mntflags)
{
//	VFS_MPLOCK_DECLARE;
	int error;
	int flags;
//	VFS_MPLOCK1(mp);
//	VFS_ACDONE(mp);
	flags = mp->mnt_kern_flag;
	//error = (mp->mnt_op->vfs_unmount)(mp, mntflags);
//	if (error == 0)
//		vn_syncer_thr_stop(mp);
//	VFS_MPUNLOCK(mp);
	return (error);
}

/*
* MPSAFE
*/
int
vfs_root(struct mount *mp, struct vnode **vpp)
{
//	VFS_MPLOCK_DECLARE;
	int error;
//	VFS_MPLOCK1(mp);
	error = (mp->mnt_op->vfs_root)(mp, vpp);
//	VFS_MPUNLOCK(mp);
	return (error);
}

/*
 * HAMMER2 vfs operations.
 */


//MALLOC_DEFINE(M_HAMMER2, "HAMMER2-mount", "");

/*
* BIO queue implementation
*/
void
bioq_init(struct bio_queue_head *head)
{
	TAILQ_INIT(&head->queue);
	head->last_offset = 0;
	head->insert_point = NULL;
	// XX head->switch_point = NULL;
}

struct bio *
bioq_first(struct bio_queue_head *head)
{

	return (TAILQ_FIRST(&head->queue));
}
   

/*
 * Optimized unlock cases.
 *
 * NOTE: mtx_unlock() handles any type of mutex: exclusive, shared, and
 *       both blocking and spin methods.
 *
 *       The mtx_unlock_ex/sh() forms are optimized for exclusive or shared
 *       mutexes and produce less code, but it is ok for code to just use
 *       mtx_unlock() and, in fact, if code uses the short-form mtx_lock()
 *       or mtx_spinlock() to lock it should also use mtx_unlock() to unlock.
 */

 /*
 * Interlocked mutex sleep.  An exclusively held mutex must be passed
 * to mtxsleep().  The function will atomically release the mutex
 * and tsleep on the ident, then reacquire the mutex and return.
 */

int
mtxsleep(void *ident, struct mutex *mtx, int flags,
        const char *wmesg, int timo)
{
       // XX globaldata_t gd  = mycpu;
        int error;
	//struct mutext *m1 = mtx;

       // XX _tsleep_interlock(gd, ident, flags);
        mtx_leave(mtx);
        error = tsleep(ident, flags | PINTERLOCKED, wmesg, timo);
       //  mtx_lock_ex_quick(mtx, wmesg);
        mtx_init(mtx, timo /*wmesg*/);

        return (error);
}

int
hammer2_vfs_init(struct vfsconf *conf)
{
	static struct objcache_malloc_args margs_read;
	static struct objcache_malloc_args margs_write;

	int error;

	error = 0;

	if (HAMMER2_BLOCKREF_BYTES != sizeof(struct hammer2_blockref))
		error = EINVAL;
	if (HAMMER2_INODE_BYTES != sizeof(struct hammer2_inode_data))
		error = EINVAL;
	if (HAMMER2_VOLUME_BYTES != sizeof(struct hammer2_volume_data))
		error = EINVAL;

	if (error)
		printf("HAMMER2 structure size mismatch; cannot continue.\n");
	
	margs_read.objsize = 65536;
	margs_read.mtype = (malloc_type_t)D_BUFFER;
	
	margs_write.objsize = 32768;
	margs_write.mtype = (malloc_type_t)C_BUFFER;
	
/* XX fix	cache_buffer_read = cache_enter(margs_read.mtype->ks_shortdesc,
				objcache_malloc_alloc,
				&margs_read);
	cache_buffer_write = cache_enter(margs_write.mtype->ks_shortdesc,
				0, 1, NULL, NULL, NULL, objcache_malloc_alloc,
				objcache_malloc_free, &margs_write);
*/

	lockinit(&hammer2_mntlk, 0, "mntlk", 0, 0);
	TAILQ_INIT(&hammer2_mntlist);
	TAILQ_INIT(&hammer2_pfslist);

	hammer2_limit_dirty_chains = desiredvnodes / 10;

	hammer2_trans_manage_init();

	return (error);
}

int
hammer2_vfs_uninit(struct vfsconf *vfsp __unused)
{
	cache_purge((struct vnode*)cache_buffer_read);
	cache_purge((struct vnode*)cache_buffer_write);
	return 0;
}

/*
 * Core PFS allocator.  Used to allocate the pmp structure for PFS cluster
 * mounts and the spmp structure for media (hmp) structures.
 */
static hammer2_pfsmount_t *
hammer2_pfsalloc(const hammer2_inode_data_t *ipdata, hammer2_tid_t alloc_tid)
{
	hammer2_pfsmount_t *pmp;

	pmp = malloc(sizeof(*pmp), M_HAMMER2, M_WAITOK | M_ZERO);
	malloc(sizeof(&pmp->minode), (long long)"HAMMER2-inodes", M_WAITOK | M_ZERO);
	malloc(sizeof(&pmp->mmsg), (long long)"HAMMER2-pfsmsg", M_WAITOK | M_ZERO);
	lockinit(&pmp->lock, 0, "pfslk", 0,0);
	spin_init((struct __mp_lock *)&pmp->inum_spin, "hm2pfsalloc_inum");
	RB_INIT(&pmp->inum_tree);
	TAILQ_INIT(&pmp->unlinkq);
	spin_init((struct __mp_lock *)&pmp->list_spin, "hm2pfsalloc_list");

	pmp->alloc_tid = alloc_tid + 1;	  /* our first media transaction id */
	pmp->flush_tid = pmp->alloc_tid;
	if (ipdata) {
		pmp->inode_tid = ipdata->pfs_inum + 1;
		pmp->pfs_clid = ipdata->pfs_clid;
	}
	mtx_enter(pmp->wthread_mtx);
	bioq_init(&pmp->wthread_bioq);

	return pmp;
}

/*
 * Mount or remount HAMMER2 fileystem from physical media
 *
 *	mountroot
 *		mp		mount point structure
 *		path		NULL
 *		data		<unused>
 *		cred		<unused>
 *
 *	mount
 *		mp		mount point structure
 *		path		path to mount point
 *		data		pointer to argument structure in user space
 *			volume	volume path (device@LABEL form)
 *			hflags	user mount flags
 *		cred		user credentials
 *
 * RETURNS:	0	Success
 *		!0	error number
 */
int
hammer2_vfs_mount(struct mount *mp, char *path, caddr_t data,
		  struct ucred *cred)
{
	struct hammer2_mount_info info;
	hammer2_pfsmount_t *pmp;
	hammer2_pfsmount_t *spmp;
	hammer2_mount_t *hmp;
	hammer2_key_t key_next;
	hammer2_key_t key_dummy;
	hammer2_key_t lhc;
	struct vnode *devvp;
	struct nlookupdata nd;
	hammer2_chain_t *parent;
	hammer2_chain_t *rchain;
	hammer2_cluster_t *cluster;
	hammer2_cluster_t *cparent;
	const hammer2_inode_data_t *ipdata;
	hammer2_blockref_t bref;
	struct file *fp;
	char devstr[MNAMELEN];
	size_t size;
	size_t done;
	char *dev;
	char *label;
	int ronly = 1;
	int error;
	int cache_index;
	int ddflag;
	int i;

	hmp = NULL;
	pmp = NULL;
	dev = NULL;
	label = NULL;
	devvp = NULL;
	cache_index = -1;

	printf("hammer2_mount\n");

	if (path == NULL) {
		/*
		 * Root mount
		 */
		bzero(&info, sizeof(info));
		info.cluster_fd = -1;
		return (EOPNOTSUPP);
	} else {
		/*
		 * Non-root mount or updating a mount
		 */
		error = copyin(data, &info, sizeof(info));
		if (error)
			return (error);

		error = copyinstr(info.volume, devstr, MNAMELEN - 1, &done);
		if (error)
			return (error);

		/* Extract device and label */
		dev = devstr;
		label = strchr(devstr, '@');
		if (label == NULL ||
		    ((label + 1) - dev) > done) {
			return (EINVAL);
		}
		*label = '\0';
		label++;
		if (*label == '\0')
			return (EINVAL);

		if (mp->mnt_flag & MNT_UPDATE) {
			/* Update mount */
			/* HAMMER2 implements NFS export via mountctl */
			pmp = MPTOPMP(mp);
			for (i = 0; i < pmp->iroot->cluster.nchains; ++i) {
				hmp = pmp->iroot->cluster.array[i]->hmp;
				devvp = hmp->devvp;
				error = hammer2_remount(hmp, mp, path,
							devvp, cred);
				if (error)
					break;
			}
			/*hammer2_inode_install_hidden(pmp);*/

			return error;
		}
	}

	/*
	 * HMP device mount
	 *
	 * Lookup name and verify it refers to a block device.
	 */
	error = nlookup_init(&nd, dev, UIO_SYSSPACE, NLC_FOLLOW);
	if (error == 0)
		error = nlookup(&nd);
	if (error == 0)
		error = cache_vref(nd.nl_nch, nd.nl_cred, &devvp);
	nlookup_done(&nd);

	if (error == 0) {
		if (vn_isdisk(devvp, &error))
			error = vfs_mountedon(devvp);
	}

	/*
	 * Determine if the device has already been mounted.  After this
	 * check hmp will be non-NULL if we are doing the second or more
	 * hammer2 mounts from the same device.
	 */
	lockmgr(&hammer2_mntlk, LK_EXCLUSIVE, NULL);
	TAILQ_FOREACH(hmp, &hammer2_mntlist, mntentry) {
		if (hmp->devvp == devvp)
			break;
	}

	/*
	 * Open the device if this isn't a secondary mount and construct
	 * the H2 device mount (hmp).
	 */
	if (hmp == NULL) {
		hammer2_chain_t *schain;
		hammer2_xid_t xid;

		if (error == 0 && vcount(devvp) > 0)
			error = EBUSY;

		/*
		 * Now open the device
		 */
		if (error == 0) {
			ronly = ((mp->mnt_flag & MNT_RDONLY) != 0);
			vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, NULL);
			error = vinvalbuf(devvp, V_SAVE, 0, 0, 0, 0);
			if (error == 0) {
				error = VOP_OPEN(devvp,
						 ronly ? FREAD : FREAD | FWRITE,
						 FSCRED, NULL);
			}
			vn_close(devvp, 0, NULL, NULL);
		}
		if (error && devvp) {
			vrele(devvp);
			devvp = NULL;
		}
		if (error) {
			lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
			return error;
		}
		hmp = malloc(sizeof(*hmp), M_HAMMER2, M_WAITOK | M_ZERO);
		hmp->ronly = ronly;
		hmp->devvp = devvp;
		malloc(sizeof(&hmp->mchain), (long long)"HAMMER2-chains", M_WAITOK | M_ZERO);
		TAILQ_INSERT_TAIL(&hammer2_mntlist, hmp, mntentry);
		RB_INIT(&hmp->iotree);
		spin_init((struct __mp_lock *)&hmp->io_spin, "hm2mount_io");
		spin_init((struct __mp_lock *)&hmp->list_spin, "hm2mount_list");
		TAILQ_INIT(&hmp->flushq);

		lockinit(&hmp->vollk, 0,  "h2vol", 0, 0);

		/*
		 * vchain setup. vchain.data is embedded.
		 * vchain.refs is initialized and will never drop to 0.
		 *
		 * NOTE! voldata is not yet loaded.
		 */
		hmp->vchain.hmp = hmp;
		hmp->vchain.refs = 1;
		hmp->vchain.data = (void *)&hmp->voldata;
		hmp->vchain.bref.type = HAMMER2_BREF_TYPE_VOLUME;
		hmp->vchain.bref.data_off = 0 | HAMMER2_PBUFRADIX;
		hmp->vchain.bref.mirror_tid = hmp->voldata.mirror_tid;

		hammer2_chain_core_alloc(NULL, &hmp->vchain);
		/* hmp->vchain.u.xxx is left NULL */

		/*
		 * fchain setup.  fchain.data is embedded.
		 * fchain.refs is initialized and will never drop to 0.
		 *
		 * The data is not used but needs to be initialized to
		 * pass assertion muster.  We use this chain primarily
		 * as a placeholder for the freemap's top-level RBTREE
		 * so it does not interfere with the volume's topology
		 * RBTREE.
		 */
		hmp->fchain.hmp = hmp;
		hmp->fchain.refs = 1;
		hmp->fchain.data = (void *)&hmp->voldata.freemap_blockset;
		hmp->fchain.bref.type = HAMMER2_BREF_TYPE_FREEMAP;
		hmp->fchain.bref.data_off = 0 | HAMMER2_PBUFRADIX;
		hmp->fchain.bref.mirror_tid = hmp->voldata.freemap_tid;
		hmp->fchain.bref.methods =
			HAMMER2_ENC_CHECK(HAMMER2_CHECK_FREEMAP) |
			HAMMER2_ENC_COMP(HAMMER2_COMP_NONE);

		hammer2_chain_core_alloc(NULL, &hmp->fchain);
		/* hmp->fchain.u.xxx is left NULL */

		/*
		 * Install the volume header and initialize fields from
		 * voldata.
		 */
		error = hammer2_install_volume_header(hmp);
		if (error) {
			++hmp->pmp_count;
			hammer2_vfs_unmount_hmp1(mp, hmp);
			hammer2_vfs_unmount_hmp2(mp, hmp);
			lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
			hammer2_vfs_unmount(mp, MNT_FORCE);
			return error;
		}

		/*
		 * Really important to get these right or flush will get
		 * confused.
		 */
		hmp->spmp = hammer2_pfsalloc(NULL, hmp->voldata.mirror_tid);
		printf("alloc spmp %p tid %016x\n",
			hmp->spmp, (unsigned int)hmp->voldata.mirror_tid);
		spmp = hmp->spmp;
		spmp->inode_tid = 1;

		xid = 0;
		hmp->vchain.bref.mirror_tid = hmp->voldata.mirror_tid;
		hmp->vchain.bref.modify_tid = hmp->vchain.bref.mirror_tid;
		hmp->vchain.pmp = spmp;
		hmp->fchain.bref.mirror_tid = hmp->voldata.freemap_tid;
		hmp->fchain.bref.modify_tid = hmp->fchain.bref.mirror_tid;
		hmp->fchain.pmp = spmp;

		/*
		 * First locate the super-root inode, which is key 0
		 * relative to the volume header's blockset.
		 *
		 * Then locate the root inode by scanning the directory keyspace
		 * represented by the label.
		 */
		parent = hammer2_chain_lookup_init(&hmp->vchain, 0);
		schain = hammer2_chain_lookup(&parent, &key_dummy,
				      HAMMER2_SROOT_KEY, HAMMER2_SROOT_KEY,
				      &cache_index, 0, &ddflag);
		hammer2_chain_lookup_done(parent);
		if (schain == NULL) {
			printf("hammer2_mount: invalid super-root\n");
			++hmp->pmp_count;
			hammer2_vfs_unmount_hmp1(mp, hmp);
			hammer2_vfs_unmount_hmp2(mp, hmp);
			lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
			hammer2_vfs_unmount(mp, MNT_FORCE);
			return EINVAL;
		}

		/*
		 * Sanity-check schain's pmp, finish initializing spmp.
		 */
		KKASSERT(schain->pmp == spmp);
		spmp->pfs_clid = schain->data->ipdata.pfs_clid;

		/*
		 * NOTE: inode_get sucks up schain's lock.
		 */
		cluster = hammer2_cluster_from_chain(schain);
		spmp->iroot = hammer2_inode_get(spmp, NULL, cluster);
		spmp->spmp_hmp = hmp;
		hammer2_inode_ref(spmp->iroot);
		hammer2_inode_unlock_ex(spmp->iroot, cluster);
		schain = NULL;
		/* leave spmp->iroot with one ref */

		if ((mp->mnt_flag & MNT_RDONLY) == 0) {
			error = hammer2_recovery(hmp);
			/* XXX do something with error */
		}
		++hmp->pmp_count;

		/*
		 * XXX RDONLY stuff is totally broken FIXME XXX
		 *
		 * Automatic LNK_CONN
		 * Automatic handling of received LNK_SPAN
		 * Automatic handling of received LNK_CIRC
		 * No automatic LNK_SPAN generation - we do this ourselves
		 * No automatic LNK_CIRC generation - we do this ourselves
		 */
		kdmsg_iocom_init(&hmp->iocom, hmp,
				 KDMSG_IOCOMF_AUTOCONN |
				 KDMSG_IOCOMF_AUTORXSPAN,
				 hmp->mchain, hammer2_rcvdmsg);

		/*
		 * Ref the cluster management messaging descriptor.  The mount
		 * program deals with the other end of the communications pipe.
		 */
		fp = holdfp(curproc->p_fd, info.cluster_fd, -1);
		if (fp) {
			hammer2_cluster_reconnect(hmp, fp);
		} else {
			printf("hammer2_mount: bad cluster_fd!\n");
		}
	} else {
		spmp = hmp->spmp;
		++hmp->pmp_count;
	}

	/*
	 * Lookup mount point under the media-localized super-root.
	 *
	 * cluster->pmp will incorrectly point to spmp and must be fixed
	 * up later on.
	 */
	cparent = hammer2_inode_lock_ex(spmp->iroot);
	lhc = hammer2_dirhash(label, strlen(label));
	cluster = hammer2_cluster_lookup(cparent, &key_next,
				      lhc, lhc + HAMMER2_DIRHASH_LOMASK,
				      0, &ddflag);
	while (cluster) {
		if (hammer2_cluster_type(cluster) == HAMMER2_BREF_TYPE_INODE &&
		    strcmp(label,
		       hammer2_cluster_data(cluster)->ipdata.filename) == 0) {
			break;
		}
		cluster = hammer2_cluster_next(cparent, cluster, &key_next,
					    key_next,
					    lhc + HAMMER2_DIRHASH_LOMASK, 0);
	}
	hammer2_inode_unlock_ex(spmp->iroot, cparent);

	if (cluster == NULL) {
		printf("hammer2_mount: PFS label not found\n");
		hammer2_vfs_unmount_hmp1(mp, hmp);
		hammer2_vfs_unmount_hmp2(mp, hmp);
		lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
		hammer2_vfs_unmount(mp, MNT_FORCE);
		return EINVAL;
	}

	for (i = 0; i < cluster->nchains; ++i) {
		rchain = cluster->array[i];
		if (rchain->flags & HAMMER2_CHAIN_MOUNTED) {
			printf("hammer2_mount: PFS label already mounted!\n");
			hammer2_cluster_unlock(cluster);
			hammer2_vfs_unmount_hmp1(mp, hmp);
			hammer2_vfs_unmount_hmp2(mp, hmp);
			lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
			hammer2_vfs_unmount(mp, MNT_FORCE);
			return EBUSY;
		}
		KKASSERT(rchain->pmp == NULL);
#if 0
		if (rchain->flags & HAMMER2_CHAIN_RECYCLE) {
			printf("hammer2_mount: PFS label is recycling\n");
			hammer2_cluster_unlock(cluster);
			hammer2_vfs_unmount_hmp1(mp, hmp);
			hammer2_vfs_unmount_hmp2(mp, hmp);
			lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
			hammer2_vfs_unmount(mp, MNT_FORCE);
			return EBUSY;
		}
#endif
	}

	/*
	 * Check to see if the cluster id is already mounted at the mount
	 * point.  If it is, add us to the cluster.
	 */
	ipdata = &hammer2_cluster_data(cluster)->ipdata;
	hammer2_cluster_bref(cluster, &bref);
	TAILQ_FOREACH(pmp, &hammer2_pfslist, mntentry) {
		if (pmp->spmp_hmp == NULL &&
		    bcmp(&pmp->pfs_clid, &ipdata->pfs_clid,
			 sizeof(pmp->pfs_clid)) == 0) {
			break;
		}
	}

	if (pmp) {
		int i;
		int j;

		hammer2_inode_ref(pmp->iroot);
		ccms_thread_lock(&pmp->iroot->topo_cst, CCMS_STATE_EXCLUSIVE);

		if (pmp->iroot->cluster.nchains + cluster->nchains >
		    HAMMER2_MAXCLUSTER) {
			printf("hammer2_mount: cluster full!\n");

			ccms_thread_unlock(&pmp->iroot->topo_cst);
			hammer2_inode_drop(pmp->iroot);

			hammer2_cluster_unlock(cluster);
			hammer2_vfs_unmount_hmp1(mp, hmp);
			hammer2_vfs_unmount_hmp2(mp, hmp);
			lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
			hammer2_vfs_unmount(mp, MNT_FORCE);
			return EBUSY;
		}
		printf("hammer2_vfs_mount: Adding pfs to existing cluster\n");
		j = pmp->iroot->cluster.nchains;
		for (i = 0; i < cluster->nchains; ++i) {
			rchain = cluster->array[i];
			KKASSERT(rchain->pmp == NULL);
			rchain->pmp = pmp;
			hammer2_chain_ref(cluster->array[i]);
			pmp->iroot->cluster.array[j] = cluster->array[i];
			++j;
		}
		pmp->iroot->cluster.nchains = j;
		ccms_thread_unlock(&pmp->iroot->topo_cst);
		hammer2_inode_drop(pmp->iroot);
		hammer2_cluster_unlock(cluster);
		lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);

		printf("ok\n");
		hammer2_inode_install_hidden(pmp);

		return ERANGE;
	}

	/*
	 * Block device opened successfully, finish initializing the
	 * mount structure.
	 *
	 * From this point on we have to call hammer2_unmount() on failure.
	 */
	pmp = hammer2_pfsalloc(ipdata, bref.mirror_tid);
	printf("PMP mirror_tid is %016x\n", (unsigned int)bref.mirror_tid);
	for (i = 0; i < cluster->nchains; ++i) {
		rchain = cluster->array[i];
		KKASSERT(rchain->pmp == NULL);
		rchain->pmp = pmp;
		atomic_set_int(&rchain->flags, HAMMER2_CHAIN_MOUNTED);
	}
	cluster->pmp = pmp;

	ccms_domain_init(&pmp->ccms_dom);
	TAILQ_INSERT_TAIL(&hammer2_pfslist, pmp, mntentry);
	lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);

	printf("hammer2_mount hmp=%p pmp=%p pmpcnt=%d\n",
		hmp, pmp, hmp->pmp_count);

	mp->mnt_flag = MNT_LOCAL;
	mp->mnt_kern_flag |= MNTK_ALL_MPSAFE;	/* all entry pts are SMP */
	mp->mnt_kern_flag |= MNTK_THR_SYNC;	/* new vsyncscan semantics */

	/*
	 * required mount structure initializations
	 */
	mp->mnt_stat.f_iosize = HAMMER2_PBUFSIZE;
	mp->mnt_stat.f_bsize = HAMMER2_PBUFSIZE;

	/* XXX fix me poiter deref mp->mnt_vstat->f_frsize = HAMMER2_PBUFSIZE;
	mp->mnt_vstat->f_bsize = HAMMER2_PBUFSIZE;
	*/

	/*
	 * Optional fields
	 */
	mp->mnt_iosize_max = MAXPHYS;
	mp->mnt_data = (qaddr_t)pmp;
	pmp->mp = mp;

	/*
	 * After this point hammer2_vfs_unmount() has visibility on hmp
	 * and manual hmp1/hmp2 calls are not needed on fatal errors.
	 */
	pmp->iroot = hammer2_inode_get(pmp, NULL, cluster);
	hammer2_inode_ref(pmp->iroot);		/* ref for pmp->iroot */
	hammer2_inode_unlock_ex(pmp->iroot, cluster);

	/*
	 * The logical file buffer bio write thread handles things
	 * like physical block assignment and compression.
	 *
	 * (only applicable to pfs mounts, not applicable to spmp)
	 */
	pmp->wthread_destroy = 0;
	// toss
	free(hammer2_write_thread, 0, 0);
	/* XXX fix me !!! lwkt_create(hammer2_write_thread, pmp,
		    &pmp->wthread_td, NULL, 0, -1, "hwrite-%s", label);
	*/

	/*
	 * With the cluster operational install ihidden.
	 * (only applicable to pfs mounts, not applicable to spmp)
	 */
	hammer2_inode_install_hidden(pmp);

	/*
	 * Finish setup
	 */
	vfs_getnewfsid(mp);
/* XXX fix vfs	vfs_add_vnodeops(mp, &hammer2_vnode_vops, &mp->mnt_vn_norm_ops);
	vfs_add_vnodeops(mp, &hammer2_spec_vops, &mp->mnt_vn_spec_ops);
	vfs_add_vnodeops(mp, &hammer2_fifo_vops, &mp->mnt_vn_fifo_ops); */

	copyinstr(info.volume, mp->mnt_stat.f_mntfromname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	bzero(mp->mnt_stat.f_mntonname, sizeof(mp->mnt_stat.f_mntonname));
	copyinstr(path, mp->mnt_stat.f_mntonname,
		  sizeof(mp->mnt_stat.f_mntonname) - 1,
		  &size);

	/*
	 * Initial statfs to prime mnt_stat.
	 */
	hammer2_vfs_statfs(mp, &mp->mnt_stat, cred);
	
	return 0;
}

/*
 * Handle bioq for strategy write
 */
static
void
hammer2_write_thread(void *arg)
{
	hammer2_pfsmount_t *pmp;
	struct bio *bio;
	struct buf *bp;
	hammer2_trans_t trans;
	struct vnode *vp;
	hammer2_inode_t *ip;
	hammer2_cluster_t *cparent;
	hammer2_inode_data_t *wipdata;
	hammer2_key_t lbase;
	// XX int lblksize;
	int pblksize;
	int error;
	
	pmp = arg;
	
	mtx_enter((struct mutex *)&pmp->wthread_mtx);
	while (pmp->wthread_destroy == 0) {
		if (bioq_first(&pmp->wthread_bioq) == NULL) {
			mtxsleep(&pmp->wthread_bioq, (struct mutex *)&pmp->wthread_mtx,
				 0, "h2bioqw", 0);
		}
		cparent = NULL;

		hammer2_trans_init(&trans, pmp, HAMMER2_TRANS_BUFCACHE);

		while ((bio = bioq_takefirst(&pmp->wthread_bioq)) != NULL) {
			/*
			 * dummy bio for synchronization.  The transaction
			 * must be reinitialized.
			 */
			/* XXX if (bio->bio_buf == NULL) {
				bio->bio_flags |= BIO_DONE;
				wakeup(bio);
				hammer2_trans_done(&trans);
				hammer2_trans_init(&trans, pmp,
						   HAMMER2_TRANS_BUFCACHE);
				continue;
			} */

			/*
			 * else normal bio processing
			 */
			mtx_leave((struct mutex *)&pmp->wthread_mtx);

			hammer2_lwinprog_drop(pmp);
			
			error = 0;
			/* XX bp = bio->bio_buf; */
			vp = bp->b_vp;
			ip = VTOI(vp);

			/*
			 * Inode is modified, flush size and mtime changes
			 * to ensure that the file size remains consistent
			 * with the buffers being flushed.
			 *
			 * NOTE: The inode_fsync() call only flushes the
			 *	 inode's meta-data state, it doesn't try
			 *	 to flush underlying buffers or chains.
			 */
			cparent = hammer2_inode_lock_ex(ip);
			if (ip->flags & (HAMMER2_INODE_RESIZED |
					 HAMMER2_INODE_MTIME)) {
				hammer2_inode_fsync(&trans, ip, cparent);
			}
			wipdata = hammer2_cluster_modify_ip(&trans, ip,
							 cparent, 0);
			/* XXX fix me pointer deref lblksize = hammer2_calc_logical(ip, bio->bio_offset,
							&lbase, NULL);
			*/
			pblksize = hammer2_calc_physical(ip, wipdata, lbase);
			hammer2_write_file_core(bp, &trans, ip, wipdata,
						cparent,
						lbase, IO_ASYNC,
						pblksize, &error);
			hammer2_cluster_modsync(cparent);
			hammer2_inode_unlock_ex(ip, cparent);
			if (error) {
				printf("hammer2: error in buffer write\n");
				bp->b_flags |= B_ERROR;
				bp->b_error = EIO;
			}
			biodone((struct buf *)bio);
			mtx_enter((struct mutex *)&pmp->wthread_mtx);
		}
		hammer2_trans_done(&trans);
	}
	pmp->wthread_destroy = -1;
	wakeup(&pmp->wthread_destroy);
	
	mtx_leave((struct mutex *)&pmp->wthread_mtx);
}

void
hammer2_bioq_sync(hammer2_pfsmount_t *pmp)
{
	struct bio *sync_bio;

	bzero(&sync_bio, sizeof(sync_bio));	/* dummy with no bio_buf */
	mtx_enter((struct mutex *)&pmp->wthread_mtx);
	if (pmp->wthread_destroy == 0 &&
	    TAILQ_FIRST(&pmp->wthread_bioq.queue)) {
		bioq_insert_tail(&pmp->wthread_bioq, sync_bio);
		while ((/* XXX Fix sync_bio->bio_flags & */ BIO_DONE) == 0)
			mtxsleep(&sync_bio, (struct mutex *)&pmp->wthread_mtx, 0, "h2bioq", 0);
	}
	mtx_leave((struct mutex *)&pmp->wthread_mtx);
}

/* 
 * Return a chain suitable for I/O, creating the chain if necessary
 * and assigning its physical block.
 */
static
hammer2_cluster_t *
hammer2_assign_physical(hammer2_trans_t *trans,
			hammer2_inode_t *ip, hammer2_cluster_t *cparent,
			hammer2_key_t lbase, int pblksize, int *errorp)
{
	hammer2_cluster_t *cluster;
	hammer2_cluster_t *dparent;
	hammer2_key_t key_dummy;
	int pradix = hammer2_getradix(pblksize);
	int ddflag;

	/*
	 * Locate the chain associated with lbase, return a locked chain.
	 * However, do not instantiate any data reference (which utilizes a
	 * device buffer) because we will be using direct IO via the
	 * logical buffer cache buffer.
	 */
	*errorp = 0;
	KKASSERT(pblksize >= HAMMER2_ALLOC_MIN);
retry:
	dparent = hammer2_cluster_lookup_init(cparent, 0);
	cluster = hammer2_cluster_lookup(dparent, &key_dummy,
				     lbase, lbase,
				     HAMMER2_LOOKUP_NODATA, &ddflag);

	if (cluster == NULL) {
		/*
		 * We found a hole, create a new chain entry.
		 *
		 * NOTE: DATA chains are created without device backing
		 *	 store (nor do we want any).
		 */
		*errorp = hammer2_cluster_create(trans, dparent, &cluster,
					       lbase, HAMMER2_PBUFRADIX,
					       HAMMER2_BREF_TYPE_DATA,
					       pblksize, 0);
		if (cluster == NULL) {
			hammer2_cluster_lookup_done(dparent);
			panic("hammer2_cluster_create: par=%p error=%d\n",
				dparent->focus, *errorp);
			goto retry;
		}
		/*ip->delta_dcount += pblksize;*/
	} else {
		switch (hammer2_cluster_type(cluster)) {
		case HAMMER2_BREF_TYPE_INODE:
			/*
			 * The data is embedded in the inode.  The
			 * caller is responsible for marking the inode
			 * modified and copying the data to the embedded
			 * area.
			 */
			break;
		case HAMMER2_BREF_TYPE_DATA:
			if (hammer2_cluster_need_resize(cluster, pblksize)) {
				hammer2_cluster_resize(trans, ip,
						     dparent, cluster,
						     pradix,
						     HAMMER2_MODIFY_OPTDATA);
			}

			/*
			 * DATA buffers must be marked modified whether the
			 * data is in a logical buffer or not.  We also have
			 * to make this call to fixup the chain data pointers
			 * after resizing in case this is an encrypted or
			 * compressed buffer.
			 */
			hammer2_cluster_modify(trans, cluster,
					       HAMMER2_MODIFY_OPTDATA);
			break;
		default:
			panic("hammer2_assign_physical: bad type");
			/* NOT REACHED */
			break;
		}
	}

	/*
	 * Cleanup.  If cluster wound up being the inode itself, i.e.
	 * the DIRECTDATA case for offset 0, then we need to update cparent.
	 * The caller expects cparent to not become stale.
	 */
	hammer2_cluster_lookup_done(dparent);
	/* dparent = NULL; safety */
	if (cluster && ddflag)
		hammer2_cluster_replace_locked(cparent, cluster);
	return (cluster);
}

/* 
 * bio queued from hammer2_vnops.c.
 *
 * The core write function which determines which path to take
 * depending on compression settings.  We also have to locate the
 * related clusters so we can calculate and set the check data for
 * the blockref.
 */
static
void
hammer2_write_file_core(struct buf *bp, hammer2_trans_t *trans,
			hammer2_inode_t *ip, hammer2_inode_data_t *ipdata,
			hammer2_cluster_t *cparent,
			hammer2_key_t lbase, int ioflag, int pblksize,
			int *errorp)
{
	hammer2_cluster_t *cluster;

	switch(HAMMER2_DEC_ALGO(ipdata->comp_algo)) {
	case HAMMER2_COMP_NONE:
		/*
		 * We have to assign physical storage to the buffer
		 * we intend to dirty or write now to avoid deadlocks
		 * in the strategy code later.
		 *
		 * This can return NOOFFSET for inode-embedded data.
		 * The strategy code will take care of it in that case.
		 */
		cluster = hammer2_assign_physical(trans, ip, cparent,
						lbase, pblksize,
						errorp);
		hammer2_write_bp(cluster, bp, ioflag, pblksize, errorp,
				 ipdata->check_algo);
		if (cluster)
			hammer2_cluster_unlock(cluster);
		break;
	case HAMMER2_COMP_AUTOZERO:
		/*
		 * Check for zero-fill only
		 */
		hammer2_zero_check_and_write(bp, trans, ip,
				    ipdata, cparent, lbase,
				    ioflag, pblksize, errorp,
				    ipdata->check_algo);
		break;
	case HAMMER2_COMP_LZ4:
	case HAMMER2_COMP_ZLIB:
	default:
		/*
		 * Check for zero-fill and attempt compression.
		 */
		hammer2_compress_and_write(bp, trans, ip,
					   ipdata, cparent,
					   lbase, ioflag,
					   pblksize, errorp,
					   ipdata->comp_algo,
					   ipdata->check_algo);
		break;
	}
}

/*
 * Generic function that will perform the compression in compression
 * write path. The compression algorithm is determined by the settings
 * obtained from inode.
 */
static
void
hammer2_compress_and_write(struct buf *bp, hammer2_trans_t *trans,
	hammer2_inode_t *ip, hammer2_inode_data_t *ipdata,
	hammer2_cluster_t *cparent,
	hammer2_key_t lbase, int ioflag, int pblksize,
	int *errorp, int comp_algo, int check_algo)
{
	hammer2_cluster_t *cluster;
	hammer2_chain_t *chain;
	int comp_size;
	int comp_block_size;
	int i;
	char *comp_buffer;

	if (test_block_zeros(bp->b_data, pblksize)) {
		zero_write(bp, trans, ip, ipdata, cparent, lbase, errorp);
		return;
	}

	comp_size = 0;
	comp_buffer = NULL;

	KKASSERT(pblksize / 2 <= 32768);
		
	if (ip->comp_heuristic < 8 || (ip->comp_heuristic & 7) == 0) {
		z_stream strm_compress;
		int comp_level;
		int ret;

		switch(HAMMER2_DEC_ALGO(comp_algo)) {
		case HAMMER2_COMP_LZ4:
			/* XXX comp_buffer = objcache_get(cache_buffer_write,
						   M_INTWAIT);
			*/
			comp_size = LZ4_compress_limitedOutput(
					bp->b_data,
					&comp_buffer[sizeof(int)],
					pblksize,
					pblksize / 2 - sizeof(int));
			/*
			 * We need to prefix with the size, LZ4
			 * doesn't do it for us.  Add the related
			 * overhead.
			 */
			*(int *)comp_buffer = comp_size;
			if (comp_size)
				comp_size += sizeof(int);
			break;
		case HAMMER2_COMP_ZLIB:
			comp_level = HAMMER2_DEC_LEVEL(comp_algo);
			if (comp_level == 0)
				comp_level = 6;	/* default zlib compression */
			else if (comp_level < 6)
				comp_level = 6;
			else if (comp_level > 9)
				comp_level = 9;
			ret = deflateInit(&strm_compress, comp_level);
			if (ret != Z_OK) {
				printf("HAMMER2 ZLIB: fatal error "
					"on deflateInit.\n");
			}

			/* XXX comp_buffer = objcache_get(cache_buffer_write,
						   M_INTWAIT);
			*/
			strm_compress.next_in = bp->b_data;
			strm_compress.avail_in = pblksize;
			strm_compress.next_out = comp_buffer;
			strm_compress.avail_out = pblksize / 2;
			ret = deflate(&strm_compress, Z_FINISH);
			if (ret == Z_STREAM_END) {
				comp_size = pblksize / 2 -
					    strm_compress.avail_out;
			} else {
				comp_size = 0;
			}
			ret = deflateEnd(&strm_compress);
			break;
		default:
			printf("Error: Unknown compression method.\n");
			printf("Comp_method = %d.\n", comp_algo);
			break;
		}
	}

	if (comp_size == 0) {
		/*
		 * compression failed or turned off
		 */
		comp_block_size = pblksize;	/* safety */
		if (++ip->comp_heuristic > 128)
			ip->comp_heuristic = 8;
	} else {
		/*
		 * compression succeeded
		 */
		ip->comp_heuristic = 0;
		if (comp_size <= 1024) {
			comp_block_size = 1024;
		} else if (comp_size <= 2048) {
			comp_block_size = 2048;
		} else if (comp_size <= 4096) {
			comp_block_size = 4096;
		} else if (comp_size <= 8192) {
			comp_block_size = 8192;
		} else if (comp_size <= 16384) {
			comp_block_size = 16384;
		} else if (comp_size <= 32768) {
			comp_block_size = 32768;
		} else {
			panic("hammer2: WRITE PATH: "
			      "Weird comp_size value.");
			/* NOT REACHED */
			comp_block_size = pblksize;
		}
	}

	cluster = hammer2_assign_physical(trans, ip, cparent,
					  lbase, comp_block_size,
					  errorp);
	ipdata = &hammer2_cluster_data(cparent)->ipdata;

	if (*errorp) {
		printf("WRITE PATH: An error occurred while "
			"assigning physical space.\n");
		KKASSERT(cluster == NULL);
		goto done;
	}

	for (i = 0; i < cluster->nchains; ++i) {
		hammer2_io_t *dio;
		char *bdata;

		chain = cluster->array[i];
		KKASSERT(chain->flags & HAMMER2_CHAIN_MODIFIED);

		switch(chain->bref.type) {
		case HAMMER2_BREF_TYPE_INODE:
			KKASSERT(chain->data->ipdata.op_flags &
				 HAMMER2_OPFLAG_DIRECTDATA);
			KKASSERT(bp->b_loffset == 0);
			bcopy(bp->b_data, chain->data->ipdata.u.data,
			      HAMMER2_EMBEDDED_BYTES);
			break;
		case HAMMER2_BREF_TYPE_DATA:
			/*
			 * Optimize out the read-before-write
			 * if possible.
			 */
			*errorp = hammer2_io_newnz(chain->hmp,
						   chain->bref.data_off,
						   chain->bytes,
						   &dio);
			if (*errorp) {
				hammer2_io_brelse(&dio);
				printf("hammer2: WRITE PATH: "
					"dbp bread error\n");
				break;
			}
			bdata = hammer2_io_data(dio, chain->bref.data_off);

			/*
			 * When loading the block make sure we don't
			 * leave garbage after the compressed data.
			 */
			if (comp_size) {
				chain->bref.methods =
					HAMMER2_ENC_COMP(comp_algo) +
					HAMMER2_ENC_CHECK(check_algo);
				bcopy(comp_buffer, bdata, comp_size);
				if (comp_size != comp_block_size) {
					bzero(bdata + comp_size,
					      comp_block_size - comp_size);
				}
			} else {
				chain->bref.methods =
					HAMMER2_ENC_COMP(
						HAMMER2_COMP_NONE) +
					HAMMER2_ENC_CHECK(check_algo);
				bcopy(bp->b_data, bdata, pblksize);
			}

			/*
			 * The flush code doesn't calculate check codes for
			 * file data (doing so can result in excessive I/O),
			 * so we do it here.
			 */
			hammer2_chain_setcheck(chain, bdata);

			/*
			 * Device buffer is now valid, chain is no longer in
			 * the initial state.
			 *
			 * (No blockref table worries with file data)
			 */
			atomic_clear_int(&chain->flags, HAMMER2_CHAIN_INITIAL);

			/* Now write the related bdp. */
			if (ioflag & IO_SYNC) {
				/*
				 * Synchronous I/O requested.
				 */
				hammer2_io_bwrite(&dio);
			/*
			} else if ((ioflag & IO_DIRECT) &&
				   loff + n == pblksize) {
				hammer2_io_bdwrite(&dio);
			*/
			} else if (ioflag & IO_ASYNC) {
				hammer2_io_bawrite(&dio);
			} else {
				hammer2_io_bdwrite(&dio);
			}
			break;
		default:
			panic("hammer2_write_bp: bad chain type %d\n",
				chain->bref.type);
			/* NOT REACHED */
			break;
		}
	}
done:
	if (cluster)
		hammer2_cluster_unlock(cluster);
	// XX if (comp_buffer)
		// XX objcache_put(cache_buffer_write, comp_buffer);
}

/*
 * Function that performs zero-checking and writing without compression,
 * it corresponds to default zero-checking path.
 */
static
void
hammer2_zero_check_and_write(struct buf *bp, hammer2_trans_t *trans,
	hammer2_inode_t *ip, hammer2_inode_data_t *ipdata,
	hammer2_cluster_t *cparent,
	hammer2_key_t lbase, int ioflag, int pblksize, int *errorp,
	int check_algo)
{
	hammer2_cluster_t *cluster;

	if (test_block_zeros(bp->b_data, pblksize)) {
		zero_write(bp, trans, ip, ipdata, cparent, lbase, errorp);
	} else {
		cluster = hammer2_assign_physical(trans, ip, cparent,
						  lbase, pblksize, errorp);
		hammer2_write_bp(cluster, bp, ioflag, pblksize, errorp,
				 check_algo);
		if (cluster)
			hammer2_cluster_unlock(cluster);
	}
}

/*
 * A function to test whether a block of data contains only zeros,
 * returns TRUE (non-zero) if the block is all zeros.
 */
static
int
test_block_zeros(const char *buf, size_t bytes)
{
	size_t i;

	for (i = 0; i < bytes; i += sizeof(long)) {
		if (*(const long *)(buf + i) != 0)
			return (0);
	}
	return (1);
}

/*
 * Function to "write" a block that contains only zeros.
 */
static
void
zero_write(struct buf *bp, hammer2_trans_t *trans,
	   hammer2_inode_t *ip, hammer2_inode_data_t *ipdata,
	   hammer2_cluster_t *cparent,
	   hammer2_key_t lbase, int *errorp __unused)
{
	hammer2_cluster_t *cluster;
	hammer2_media_data_t *data;
	hammer2_key_t key_dummy;
	int ddflag;

	cparent = hammer2_cluster_lookup_init(cparent, 0);
	cluster = hammer2_cluster_lookup(cparent, &key_dummy, lbase, lbase,
				     HAMMER2_LOOKUP_NODATA, &ddflag);
	if (cluster) {
		data = hammer2_cluster_wdata(cluster);

		if (ddflag) {
			KKASSERT(cluster->focus->flags &
				 HAMMER2_CHAIN_MODIFIED);
			bzero(data->ipdata.u.data, HAMMER2_EMBEDDED_BYTES);
			hammer2_cluster_modsync(cluster);
		} else {
			hammer2_cluster_delete(trans, cparent, cluster,
					       HAMMER2_DELETE_PERMANENT);
		}
		hammer2_cluster_unlock(cluster);
	}
	hammer2_cluster_lookup_done(cparent);
}

/*
 * Function to write the data as it is, without performing any sort of
 * compression. This function is used in path without compression and
 * default zero-checking path.
 */
static
void
hammer2_write_bp(hammer2_cluster_t *cluster, struct buf *bp, int ioflag,
				int pblksize, int *errorp, int check_algo)
{
	hammer2_chain_t *chain;
	hammer2_io_t *dio;
	char *bdata;
	int error;
	int i;

	error = 0;	/* XXX TODO below */

	for (i = 0; i < cluster->nchains; ++i) {
		chain = cluster->array[i];

		KKASSERT(chain->flags & HAMMER2_CHAIN_MODIFIED);

		switch(chain->bref.type) {
		case HAMMER2_BREF_TYPE_INODE:
			KKASSERT(chain->data->ipdata.op_flags &
				 HAMMER2_OPFLAG_DIRECTDATA);
			KKASSERT(bp->b_loffset == 0);
			bcopy(bp->b_data, chain->data->ipdata.u.data,
			      HAMMER2_EMBEDDED_BYTES);
			error = 0;
			break;
		case HAMMER2_BREF_TYPE_DATA:
			error = hammer2_io_newnz(chain->hmp,
						 chain->bref.data_off,
						 chain->bytes, &dio);
			if (error) {
				hammer2_io_bqrelse(&dio);
				printf("hammer2: WRITE PATH: "
					"dbp bread error\n");
				break;
			}
			bdata = hammer2_io_data(dio, chain->bref.data_off);

			chain->bref.methods = HAMMER2_ENC_COMP(
							HAMMER2_COMP_NONE) +
					      HAMMER2_ENC_CHECK(check_algo);
			bcopy(bp->b_data, bdata, chain->bytes);

			/*
			 * The flush code doesn't calculate check codes for
			 * file data (doing so can result in excessive I/O),
			 * so we do it here.
			 */
			hammer2_chain_setcheck(chain, bdata);

			/*
			 * Device buffer is now valid, chain is no longer in
			 * the initial state.
			 *
			 * (No blockref table worries with file data)
			 */
			atomic_clear_int(&chain->flags, HAMMER2_CHAIN_INITIAL);

			if (ioflag & IO_SYNC) {
				/*
				 * Synchronous I/O requested.
				 */
				hammer2_io_bwrite(&dio);
			/*
			} else if ((ioflag & IO_DIRECT) &&
				   loff + n == pblksize) {
				hammer2_io_bdwrite(&dio);
			*/
			} else if (ioflag & IO_ASYNC) {
				hammer2_io_bawrite(&dio);
			} else {
				hammer2_io_bdwrite(&dio);
			}
			break;
		default:
			panic("hammer2_write_bp: bad chain type %d\n",
			      chain->bref.type);
			/* NOT REACHED */
			error = 0;
			break;
		}
		KKASSERT(error == 0);	/* XXX TODO */
	}
	*errorp = error;
}

static
int
hammer2_remount(hammer2_mount_t *hmp, struct mount *mp, char *path,
		struct vnode *devvp, struct ucred *cred)
{
	int error;

	if (hmp->ronly && (mp->mnt_kern_flag & MNTK_WANTRDWR)) {
		error = hammer2_recovery(hmp);
	} else {
		error = 0;
	}
	return error;
}

static 
int
hammer2_vfs_unmount(struct mount *mp, int mntflags)
{
	hammer2_pfsmount_t *pmp;
	hammer2_mount_t *hmp;
	hammer2_chain_t *rchain;
	hammer2_cluster_t *cluster;
	int flags;
	int error = 0;
	int i;

	pmp = MPTOPMP(mp);

	if (pmp == NULL)
		return(0);

	lockmgr(&hammer2_mntlk, LK_EXCLUSIVE, NULL);
	TAILQ_REMOVE(&hammer2_pfslist, pmp, mntentry);

	/*
	 * If mount initialization proceeded far enough we must flush
	 * its vnodes.
	 */
	if (mntflags & MNT_FORCE)
		flags = FORCECLOSE;
	else
		flags = 0;
	if (pmp->iroot) {
		error = vflush(mp, 0, flags);
		if (error)
			goto failed;
	}

	ccms_domain_uninit(&pmp->ccms_dom);

	if (pmp->wthread_td) {
		mtx_enter((struct mutex *)&pmp->wthread_mtx);
		pmp->wthread_destroy = 1;
		wakeup(&pmp->wthread_bioq);
		while (pmp->wthread_destroy != -1) {
			mtxsleep(&pmp->wthread_destroy,
				(struct mutex *)&pmp->wthread_mtx, 0,
				"umount-sleep",	0);
		}
		mtx_leave((struct mutex *)&pmp->wthread_mtx);
		pmp->wthread_td = NULL;
	}

	/*
	 * Cleanup our reference on ihidden.
	 */
	if (pmp->ihidden) {
		hammer2_inode_drop(pmp->ihidden);
		pmp->ihidden = NULL;
	}

	/*
	 * Cleanup our reference on iroot.  iroot is (should) not be needed
	 * by the flush code.
	 */
	if (pmp->iroot) {
		cluster = &pmp->iroot->cluster;
		for (i = 0; i < pmp->iroot->cluster.nchains; ++i) {
			rchain = pmp->iroot->cluster.array[i];
			if (rchain == NULL)
				continue;
			hmp = rchain->hmp;
			hammer2_vfs_unmount_hmp1(mp, hmp);

			atomic_clear_int(&rchain->flags, HAMMER2_CHAIN_MOUNTED);
#if REPORT_REFS_ERRORS
			if (rchain->refs != 1)
				printf("PMP->RCHAIN %p REFS WRONG %d\n",
					rchain, rchain->refs);
#else
			KKASSERT(rchain->refs == 1);
#endif
			hammer2_chain_drop(rchain);
			cluster->array[i] = NULL;
			hammer2_vfs_unmount_hmp2(mp, hmp);
		}
		cluster->focus = NULL;

#if REPORT_REFS_ERRORS
		if (pmp->iroot->refs != 1)
			printf("PMP->IROOT %p REFS WRONG %d\n",
				pmp->iroot, pmp->iroot->refs);
#else
		KKASSERT(pmp->iroot->refs == 1);
#endif
		/* ref for pmp->iroot */
		hammer2_inode_drop(pmp->iroot);
		pmp->iroot = NULL;
	}

	pmp->mp = NULL;
	mp->mnt_data = NULL;

	free(&pmp->mmsg, M_HAMMER2, 0);
	free(&pmp->minode, M_HAMMER2, 0);

	free(pmp, M_HAMMER2, 0);
	error = 0;

failed:
	lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);

	return (error);
}

static
void
hammer2_vfs_unmount_hmp1(struct mount *mp, hammer2_mount_t *hmp)
{
	hammer2_mount_exlock(hmp);
	--hmp->pmp_count;

	printf("hammer2_unmount hmp=%p pmpcnt=%d\n", hmp, hmp->pmp_count);

	kdmsg_iocom_uninit(&hmp->iocom);	/* XXX chain depend deadlck? */

	/*
	 * Cycle the volume data lock as a safety (probably not needed any
	 * more).  To ensure everything is out we need to flush at least
	 * three times.  (1) The running of the unlinkq can dirty the
	 * filesystem, (2) A normal flush can dirty the freemap, and
	 * (3) ensure that the freemap is fully synchronized.
	 *
	 * The next mount's recovery scan can clean everything up but we want
	 * to leave the filesystem in a 100% clean state on a normal unmount.
	 */
	hammer2_voldata_lock(hmp);
	hammer2_voldata_unlock(hmp);
	if (mp->mnt_data) {
		hammer2_vfs_sync(mp, MNT_WAIT);
		hammer2_vfs_sync(mp, MNT_WAIT);
		hammer2_vfs_sync(mp, MNT_WAIT);
	}

	if (hmp->pmp_count == 0) {
		if ((hmp->vchain.flags | hmp->fchain.flags) &
		    HAMMER2_CHAIN_FLUSH_MASK) {
			printf("hammer2_unmount: chains left over "
				"after final sync\n");
			printf("    vchain %08x\n", hmp->vchain.flags);
			printf("    fchain %08x\n", hmp->fchain.flags);

			if (hammer2_debug & 0x0010)
				printf("entered debugger");
		}
	}
}

static
void
hammer2_vfs_unmount_hmp2(struct mount *mp, hammer2_mount_t *hmp)
{
	hammer2_pfsmount_t *spmp;
	struct vnode *devvp;
	int dumpcnt;
	int ronly = ((mp->mnt_flag & MNT_RDONLY) != 0);

	/*
	 * If no PFS's left drop the master hammer2_mount for the
	 * device.
	 */
	if (hmp->pmp_count == 0) {
		/*
		 * Clean up SPMP and the super-root inode
		 */
		spmp = hmp->spmp;
		if (spmp) {
			if (spmp->iroot) {
				hammer2_inode_drop(spmp->iroot);
				spmp->iroot = NULL;
			}
			hmp->spmp = NULL;
			free(&spmp->mmsg, M_TEMP, 0);
			free(&spmp->minode, M_TEMP, 0);
			free(spmp, M_HAMMER2, 0);
		}

		/*
		 * Finish up with the device vnode
		 */
		if ((devvp = hmp->devvp) != NULL) {
			vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, NULL);
			vinvalbuf(devvp, (ronly ? 0 : V_SAVE), 0, 0, 0,0);
			hmp->devvp = NULL;
			VOP_CLOSE(devvp, (ronly ? FREAD : FREAD|FWRITE), NULL, NULL);
			vn_close(devvp, 0, NULL, NULL);
			vrele(devvp);
			devvp = NULL;
		}

		/*
		 * Clear vchain/fchain flags that might prevent final cleanup
		 * of these chains.
		 */
		if (hmp->vchain.flags & HAMMER2_CHAIN_MODIFIED) {
			atomic_clear_int(&hmp->vchain.flags,
					 HAMMER2_CHAIN_MODIFIED);
			hammer2_pfs_memory_wakeup(hmp->vchain.pmp);
			hammer2_chain_drop(&hmp->vchain);
		}
		if (hmp->vchain.flags & HAMMER2_CHAIN_UPDATE) {
			atomic_clear_int(&hmp->vchain.flags,
					 HAMMER2_CHAIN_UPDATE);
			hammer2_chain_drop(&hmp->vchain);
		}

		if (hmp->fchain.flags & HAMMER2_CHAIN_MODIFIED) {
			atomic_clear_int(&hmp->fchain.flags,
					 HAMMER2_CHAIN_MODIFIED);
			hammer2_pfs_memory_wakeup(hmp->fchain.pmp);
			hammer2_chain_drop(&hmp->fchain);
		}
		if (hmp->fchain.flags & HAMMER2_CHAIN_UPDATE) {
			atomic_clear_int(&hmp->fchain.flags,
					 HAMMER2_CHAIN_UPDATE);
			hammer2_chain_drop(&hmp->fchain);
		}

		/*
		 * Final drop of embedded freemap root chain to
		 * clean up fchain.core (fchain structure is not
		 * flagged ALLOCATED so it is cleaned out and then
		 * left to rot).
		 */
		hammer2_chain_drop(&hmp->fchain);

		/*
		 * Final drop of embedded volume root chain to clean
		 * up vchain.core (vchain structure is not flagged
		 * ALLOCATED so it is cleaned out and then left to
		 * rot).
		 */
		dumpcnt = 50;
		hammer2_dump_chain(&hmp->vchain, 0, &dumpcnt, 'v');
		dumpcnt = 50;
		hammer2_dump_chain(&hmp->fchain, 0, &dumpcnt, 'f');
		hammer2_mount_unlock(hmp);
		hammer2_chain_drop(&hmp->vchain);

		hammer2_io_cleanup(hmp, &hmp->iotree);
		if (hmp->iofree_count) {
			printf("io_cleanup: %d I/O's left hanging\n",
				hmp->iofree_count);
		}

		TAILQ_REMOVE(&hammer2_mntlist, hmp, mntentry);
		free(&hmp->mchain, M_HAMMER2, 0);
		free(hmp, M_HAMMER2, 0);
	} else {
		hammer2_mount_unlock(hmp);
	}
}

static
int
hammer2_vfs_vget(struct mount *mp, struct vnode *dvp,
	     ino_t ino, struct vnode **vpp)
{
	printf("hammer2_vget\n");
	return (EOPNOTSUPP);
}

static
int
hammer2_vfs_root(struct mount *mp, struct vnode **vpp)
{
	hammer2_pfsmount_t *pmp;
	hammer2_cluster_t *cparent;
	int error;
	struct vnode *vp;

	pmp = MPTOPMP(mp);
	if (pmp->iroot == NULL) {
		*vpp = NULL;
		error = EINVAL;
	} else {
		cparent = hammer2_inode_lock_sh(pmp->iroot);
		vp = hammer2_igetv(pmp->iroot, cparent, &error);
		hammer2_inode_unlock_sh(pmp->iroot, cparent);
		*vpp = vp;
		if (vp == NULL)
			printf("vnodefail\n");
	}

	return (error);
}

/*
 * Filesystem status
 *
 * XXX incorporate ipdata->inode_quota and data_quota
 */
static
int
hammer2_vfs_statfs(struct mount *mp, struct statfs *sbp, struct ucred *cred)
{
	hammer2_pfsmount_t *pmp;
	hammer2_mount_t *hmp;

	pmp = MPTOPMP(mp);
	KKASSERT(pmp->iroot->cluster.nchains >= 1);
	hmp = pmp->iroot->cluster.focus->hmp;	/* XXX */

	mp->mnt_stat.f_files = pmp->inode_count;
	mp->mnt_stat.f_ffree = 0;
	mp->mnt_stat.f_blocks = hmp->voldata.allocator_size / HAMMER2_PBUFSIZE;
	mp->mnt_stat.f_bfree =  hmp->voldata.allocator_free / HAMMER2_PBUFSIZE;
	mp->mnt_stat.f_bavail = mp->mnt_stat.f_bfree;

	*sbp = mp->mnt_stat;
	return (0);
}

static
int
hammer2_vfs_statvfs(struct mount *mp, struct statfs *sbp, struct ucred *cred)
{
	hammer2_pfsmount_t *pmp;
	hammer2_mount_t *hmp;

	pmp = MPTOPMP(mp);
	KKASSERT(pmp->iroot->cluster.nchains >= 1);
	hmp = pmp->iroot->cluster.focus->hmp;	/* XXX */

	mp->mnt_stat.f_bsize = HAMMER2_PBUFSIZE;
	mp->mnt_stat.f_files = pmp->inode_count;
	mp->mnt_stat.f_ffree = 0;
	mp->mnt_stat.f_blocks = hmp->voldata.allocator_size / HAMMER2_PBUFSIZE;
	mp->mnt_stat.f_bfree =  hmp->voldata.allocator_free / HAMMER2_PBUFSIZE;
	// XXX mp->mnt_stat.f_bavail = mp->mnt_vstat.f_bfree;

	// XXX *sbp = mp->mnt_vstat;
	return (0);
}

/*
 * Mount-time recovery (RW mounts)
 *
 * Updates to the free block table are allowed to lag flushes by one
 * transaction.  In case of a crash, then on a fresh mount we must do an
 * incremental scan of the last committed transaction id and make sure that
 * all related blocks have been marked allocated.
 *
 * The super-root topology and each PFS has its own transaction id domain,
 * so we must track PFS boundary transitions.
 */
struct hammer2_recovery_elm {
	TAILQ_ENTRY(hammer2_recovery_elm) entry;
	hammer2_chain_t *chain;
	hammer2_tid_t sync_tid;
};

TAILQ_HEAD(hammer2_recovery_list, hammer2_recovery_elm);

struct hammer2_recovery_info {
	struct hammer2_recovery_list list;
	int	depth;
};

static int hammer2_recovery_scan(hammer2_trans_t *trans, hammer2_mount_t *hmp,
			hammer2_chain_t *parent,
			struct hammer2_recovery_info *info,
			hammer2_tid_t sync_tid);

#define HAMMER2_RECOVERY_MAXDEPTH	10

static
int
hammer2_recovery(hammer2_mount_t *hmp)
{
	hammer2_trans_t trans;
	struct hammer2_recovery_info info;
	struct hammer2_recovery_elm *elm;
	hammer2_chain_t *parent;
	hammer2_tid_t sync_tid;
	int error;
	int cumulative_error = 0;

	hammer2_trans_init(&trans, hmp->spmp, 0);

	sync_tid = 0;
	TAILQ_INIT(&info.list);
	info.depth = 0;
	parent = hammer2_chain_lookup_init(&hmp->vchain, 0);
	cumulative_error = hammer2_recovery_scan(&trans, hmp, parent,
						 &info, sync_tid);
	hammer2_chain_lookup_done(parent);

	while ((elm = TAILQ_FIRST(&info.list)) != NULL) {
		TAILQ_REMOVE(&info.list, elm, entry);
		parent = elm->chain;
		sync_tid = elm->sync_tid;
		free(elm, M_HAMMER2, 0);

		hammer2_chain_lock(parent, HAMMER2_RESOLVE_ALWAYS |
					   HAMMER2_RESOLVE_NOREF);
		error = hammer2_recovery_scan(&trans, hmp, parent,
					      &info, sync_tid);
		hammer2_chain_unlock(parent);
		if (error)
			cumulative_error = error;
	}
	hammer2_trans_done(&trans);

	return cumulative_error;
}

static
int
hammer2_recovery_scan(hammer2_trans_t *trans, hammer2_mount_t *hmp,
		      hammer2_chain_t *parent,
		      struct hammer2_recovery_info *info,
		      hammer2_tid_t sync_tid)
{
	hammer2_chain_t *chain;
	int cache_index;
	int cumulative_error = 0;
	int pfs_boundary = 0;
	int error;

	/*
	 * Adjust freemap to ensure that the block(s) are marked allocated.
	 */
	if (parent->bref.type != HAMMER2_BREF_TYPE_VOLUME) {
		hammer2_freemap_adjust(trans, hmp, &parent->bref,
				       HAMMER2_FREEMAP_DORECOVER);
	}

	/*
	 * Check type for recursive scan
	 */
	switch(parent->bref.type) {
	case HAMMER2_BREF_TYPE_VOLUME:
		/* data already instantiated */
		break;
	case HAMMER2_BREF_TYPE_INODE:
		/*
		 * Must instantiate data for DIRECTDATA test and also
		 * for recursion.
		 */
		hammer2_chain_lock(parent, HAMMER2_RESOLVE_ALWAYS);
		if (parent->data->ipdata.op_flags & HAMMER2_OPFLAG_DIRECTDATA) {
			/* not applicable to recovery scan */
			hammer2_chain_unlock(parent);
			return 0;
		}
		if ((parent->data->ipdata.op_flags & HAMMER2_OPFLAG_PFSROOT) &&
		    info->depth != 0) {
			pfs_boundary = 1;
			sync_tid = parent->bref.mirror_tid - 1;
		}
		hammer2_chain_unlock(parent);
		break;
	case HAMMER2_BREF_TYPE_INDIRECT:
		/*
		 * Must instantiate data for recursion
		 */
		hammer2_chain_lock(parent, HAMMER2_RESOLVE_ALWAYS);
		hammer2_chain_unlock(parent);
		break;
	case HAMMER2_BREF_TYPE_DATA:
	case HAMMER2_BREF_TYPE_FREEMAP:
	case HAMMER2_BREF_TYPE_FREEMAP_NODE:
	case HAMMER2_BREF_TYPE_FREEMAP_LEAF:
		/* not applicable to recovery scan */
		return 0;
		break;
	default:
		return EDOM;
	}

	/*
	 * Defer operation if depth limit reached or if we are crossing a
	 * PFS boundary.
	 */
	if (info->depth >= HAMMER2_RECOVERY_MAXDEPTH || pfs_boundary) {
		struct hammer2_recovery_elm *elm;

		elm = malloc(sizeof(*elm), M_HAMMER2, M_ZERO | M_WAITOK);
		elm->chain = parent;
		elm->sync_tid = sync_tid;
		hammer2_chain_ref(parent);
		TAILQ_INSERT_TAIL(&info->list, elm, entry);
		/* unlocked by caller */

		return(0);
	}


	/*
	 * Recursive scan of the last flushed transaction only.  We are
	 * doing this without pmp assignments so don't leave the chains
	 * hanging around after we are done with them.
	 */
	cache_index = 0;
	chain = hammer2_chain_scan(parent, NULL, &cache_index,
				   HAMMER2_LOOKUP_NODATA);
	while (chain) {
		atomic_set_int(&chain->flags, HAMMER2_CHAIN_RELEASE);
		if (chain->bref.mirror_tid >= sync_tid) {
			++info->depth;
			error = hammer2_recovery_scan(trans, hmp, chain,
						      info, sync_tid);
			--info->depth;
			if (error)
				cumulative_error = error;
		}
		chain = hammer2_chain_scan(parent, chain, &cache_index,
					   HAMMER2_LOOKUP_NODATA);
	}

	return cumulative_error;
}

/*
 * Sync the entire filesystem; this is called from the filesystem syncer
 * process periodically and whenever a user calls sync(1) on the hammer
 * mountpoint.
 *
 * Currently is actually called from the syncer! \o/
 *
 * This task will have to snapshot the state of the dirty inode chain.
 * From that, it will have to make sure all of the inodes on the dirty
 * chain have IO initiated. We make sure that io is initiated for the root
 * block.
 *
 * If waitfor is set, we wait for media to acknowledge the new rootblock.
 *
 * THINKS: side A vs side B, to have sync not stall all I/O?
 */
int
hammer2_vfs_sync(struct mount *mp, int waitfor)
{
	struct hammer2_sync_info info;
	hammer2_inode_t *iroot;
	hammer2_chain_t *chain;
	hammer2_chain_t *parent;
	hammer2_pfsmount_t *pmp;
	hammer2_mount_t *hmp;
	int flags;
	int error;
	int total_error;
	int force_fchain;
	int i;
	int j;

	pmp = MPTOPMP(mp);
	iroot = pmp->iroot;
	KKASSERT(iroot);
	KKASSERT(iroot->pmp == pmp);

	/*
	 * We can't acquire locks on existing vnodes while in a transaction
	 * without risking a deadlock.  This assumes that vfsync() can be
	 * called without the vnode locked (which it can in DragonFly).
	 * Otherwise we'd have to implement a multi-pass or flag the lock
	 * failures and retry.
	 *
	 * The reclamation code interlocks with the sync list's token
	 * (by removing the vnode from the scan list) before unlocking
	 * the inode, giving us time to ref the inode.
	 */
	/*flags = VMSC_GETVP;*/
	flags = 0;
	if (waitfor & MNT_LAZY)
		flags |= VMSC_ONEPASS;

	/*
	 * Start our flush transaction.  This does not return until all
	 * concurrent transactions have completed and will prevent any
	 * new transactions from running concurrently, except for the
	 * buffer cache transactions.
	 *
	 * For efficiency do an async pass before making sure with a
	 * synchronous pass on all related buffer cache buffers.  It
	 * should theoretically not be possible for any new file buffers
	 * to be instantiated during this sequence.
	 */
	hammer2_trans_init(&info.trans, pmp, HAMMER2_TRANS_ISFLUSH |
					     HAMMER2_TRANS_PREFLUSH);
	hammer2_run_unlinkq(&info.trans, pmp);

	info.error = 0;
	info.waitfor = MNT_NOWAIT;
	vsyncscan(mp, flags | VMSC_NOWAIT, hammer2_sync_scan2, &info);
	info.waitfor = MNT_WAIT;
	vsyncscan(mp, flags, hammer2_sync_scan2, &info);

	/*
	 * Clear PREFLUSH.  This prevents (or asserts on) any new logical
	 * buffer cache flushes which occur during the flush.  Device buffers
	 * are not affected.
	 */

#if 0
	if (info.error == 0 && (waitfor & MNT_WAIT)) {
		info.waitfor = waitfor;
		    vsyncscan(mp, flags, hammer2_sync_scan2, &info);

	}
#endif
	hammer2_bioq_sync(info.trans.pmp);
	atomic_clear_int(&info.trans.flags, HAMMER2_TRANS_PREFLUSH);

	total_error = 0;

	/*
	 * Flush all storage elements making up the cluster
	 *
	 * We must also flush any deleted siblings because the super-root
	 * flush won't do it for us.  They all must be staged or the
	 * super-root flush will not be able to update its block table
	 * properly.
	 *
	 * XXX currently done serially instead of concurrently
	 */
	for (i = 0; iroot && i < iroot->cluster.nchains; ++i) {
		chain = iroot->cluster.array[i];
		if (chain) {
			hammer2_chain_lock(chain, HAMMER2_RESOLVE_ALWAYS);
			hammer2_flush(&info.trans, chain);
			hammer2_chain_unlock(chain);
		}
	}
#if 0
	hammer2_trans_done(&info.trans);
#endif

	/*
	 * Flush all volume roots to synchronize PFS flushes with the
	 * storage media.  Use a super-root transaction for each one.
	 *
	 * The flush code will detect super-root -> pfs-root chain
	 * transitions using the last pfs-root flush.
	 */
	for (i = 0; iroot && i < iroot->cluster.nchains; ++i) {
		chain = iroot->cluster.array[i];
		if (chain == NULL)
			continue;

		hmp = chain->hmp;

		/*
		 * We only have to flush each hmp once
		 */
		for (j = i - 1; j >= 0; --j) {
			if (iroot->cluster.array[j] &&
			    iroot->cluster.array[j]->hmp == hmp)
				break;
		}
		if (j >= 0)
			continue;
		hammer2_trans_spmp(&info.trans, hmp->spmp);

		/*
		 * Force an update of the XID from the PFS root to the
		 * topology root.  We couldn't do this from the PFS
		 * transaction because a SPMP transaction is needed.
		 * This does not modify blocks, instead what it does is
		 * allow the flush code to find the transition point and
		 * then update on the way back up.
		 */
		parent = chain->parent;
		KKASSERT(chain->pmp != parent->pmp);
		hammer2_chain_setflush(&info.trans, parent);

		/*
		 * Media mounts have two 'roots', vchain for the topology
		 * and fchain for the free block table.  Flush both.
		 *
		 * Note that the topology and free block table are handled
		 * independently, so the free block table can wind up being
		 * ahead of the topology.  We depend on the bulk free scan
		 * code to deal with any loose ends.
		 */
		hammer2_chain_lock(&hmp->vchain, HAMMER2_RESOLVE_ALWAYS);
		hammer2_chain_lock(&hmp->fchain, HAMMER2_RESOLVE_ALWAYS);
		if (hmp->fchain.flags & HAMMER2_CHAIN_FLUSH_MASK) {
			/*
			 * This will also modify vchain as a side effect,
			 * mark vchain as modified now.
			 */
			hammer2_voldata_modify(hmp);
			chain = &hmp->fchain;
			hammer2_flush(&info.trans, chain);
			KKASSERT(chain == &hmp->fchain);
		}
		hammer2_chain_unlock(&hmp->fchain);
		hammer2_chain_unlock(&hmp->vchain);

		hammer2_chain_lock(&hmp->vchain, HAMMER2_RESOLVE_ALWAYS);
		if (hmp->vchain.flags & HAMMER2_CHAIN_FLUSH_MASK) {
			chain = &hmp->vchain;
			hammer2_flush(&info.trans, chain);
			KKASSERT(chain == &hmp->vchain);
			force_fchain = 1;
		} else {
			force_fchain = 0;
		}
		hammer2_chain_unlock(&hmp->vchain);

#if 0
		hammer2_chain_lock(&hmp->fchain, HAMMER2_RESOLVE_ALWAYS);
		if ((hmp->fchain.flags & HAMMER2_CHAIN_FLUSH_MASK) ||
		    force_fchain) {
			/* this will also modify vchain as a side effect */
			chain = &hmp->fchain;
			hammer2_flush(&info.trans, chain);
			KKASSERT(chain == &hmp->fchain);
		}
		hammer2_chain_unlock(&hmp->fchain);
#endif

		error = 0;

		/*
		 * We can't safely flush the volume header until we have
		 * flushed any device buffers which have built up.
		 *
		 * XXX this isn't being incremental
		 */
		vn_lock(hmp->devvp, LK_EXCLUSIVE | LK_RETRY, NULL);
		// XXX fix me error = VOP_FSYNC(hmp->devvp, MNT_WAIT, 0);
		vn_close(hmp->devvp, 0, NULL, NULL);

		/*
		 * The flush code sets CHAIN_VOLUMESYNC to indicate that the
		 * volume header needs synchronization via hmp->volsync.
		 *
		 * XXX synchronize the flag & data with only this flush XXX
		 */
		if (error == 0 &&
		    (hmp->vchain.flags & HAMMER2_CHAIN_VOLUMESYNC)) {
			struct buf *bp;

			/*
			 * Synchronize the disk before flushing the volume
			 * header.
			 */
			bp = getpbuf(NULL);
			bp->b_bio1.bio_offset = 0;
			bp->b_bufsize = 0;
			bp->b_bcount = 0;
			//bp->b_cmd = BUF_CMD_FLUSH; 
			//bp->b_bio1.bio_done = biodone_sync;
			bp->b_bio1.bio_flags |= BIO_SYNC;
			// XX vn_strategy(hmp->devvp, &bp->b_bio1);
			biowait((struct buf *)&bp->b_bio1 /*, "h2vol" */);
			relpbuf(bp, NULL);

			/*
			 * Then we can safely flush the version of the
			 * volume header synchronized by the flush code.
			 */
			i = hmp->volhdrno + 1;
			if (i >= HAMMER2_NUM_VOLHDRS)
				i = 0;
			if (i * HAMMER2_ZONE_BYTES64 + HAMMER2_SEGSIZE >
			    hmp->volsync.volu_size) {
				i = 0;
			}
			printf("sync volhdr %d %d\n",
				i, (unsigned int)hmp->volsync.volu_size);
			bp = getblk(hmp->devvp, i * HAMMER2_ZONE_BYTES64,
				    HAMMER2_PBUFSIZE, 0, 0);
			atomic_clear_int(&hmp->vchain.flags,
					 HAMMER2_CHAIN_VOLUMESYNC);
			bcopy(&hmp->volsync, bp->b_data, HAMMER2_PBUFSIZE);
			bawrite(bp);
			hmp->volhdrno = i;
		}
		if (error)
			total_error = error;

#if 0
		hammer2_trans_done(&info.trans);
#endif
	}
	hammer2_trans_done(&info.trans);

	return (total_error);
}

/*
 * Sync passes.
 */
static int
hammer2_sync_scan2(struct mount *mp, struct vnode *vp, void *data)
{
	struct hammer2_sync_info *info = data;
	hammer2_inode_t *ip;
	int error;

	/*
	 *
	 */
	ip = VTOI(vp);
	if (ip == NULL)
		return(0);
	if (vp->v_type == VNON || vp->v_type == VBAD) {
		vn_lock(vp, 0, NULL);
		return(0);
	}
	if ((ip->flags & HAMMER2_INODE_MODIFIED) == 0 &&
	    //RB_EMPTY(&vp->v_dirtyblkhd)) {
	      RB_EMPTY(&vp->v_rbdirty_tree)) {
		vn_lock(vp, 0, NULL);
		return(0);
	}

	/*
	 * VOP_FSYNC will start a new transaction so replicate some code
	 * here to do it inline (see hammer2_vop_fsync()).
	 *
	 * WARNING: The vfsync interacts with the buffer cache and might
	 *          block, we can't hold the inode lock at that time.
	 *	    However, we MUST ref ip before blocking to ensure that
	 *	    it isn't ripped out from under us (since we do not
	 *	    hold a lock on the vnode).
	 */
	hammer2_inode_ref(ip);
	atomic_clear_int(&ip->flags, HAMMER2_INODE_MODIFIED);
	// XXX if (vp)
		// XXX vfsync((struct vnode *)vp, MNT_NOWAIT, 1, NULL, NULL);

	hammer2_inode_drop(ip);
#if 1
	error = 0;
	if (error)
		info->error = error;
#endif
	return(0);
}

static
int
hammer2_vfs_vptofh(struct vnode *vp, struct fid *fhp)
{
	return (0);
}

static
int
hammer2_vfs_fhtovp(struct mount *mp, struct vnode *rootvp,
	       struct fid *fhp, struct vnode **vpp)
{
	return (0);
}

static
int
hammer2_vfs_checkexp(struct mount *mp, struct sockaddr *nam,
		 int *exflagsp, struct ucred **credanonp)
{
	return (0);
}

/*
 * Support code for hammer2_mount().  Read, verify, and install the volume
 * header into the HMP
 *
 * XXX read four volhdrs and use the one with the highest TID whos CRC
 *     matches.
 *
 * XXX check iCRCs.
 *
 * XXX For filesystems w/ less than 4 volhdrs, make sure to not write to
 *     nonexistant locations.
 *
 * XXX Record selected volhdr and ring updates to each of 4 volhdrs
 */
static
int
hammer2_install_volume_header(hammer2_mount_t *hmp)
{
	hammer2_volume_data_t *vd;
	struct buf *bp;
	hammer2_crc32_t crc0, crc, bcrc0, bcrc;
	int error_reported;
	int error;
	int valid;
	int i;

	error_reported = 0;
	error = 0;
	valid = 0;
	bp = NULL;

	/*
	 * There are up to 4 copies of the volume header (syncs iterate
	 * between them so there is no single master).  We don't trust the
	 * volu_size field so we don't know precisely how large the filesystem
	 * is, so depend on the OS to return an error if we go beyond the
	 * block device's EOF.
	 */
	for (i = 0; i < HAMMER2_NUM_VOLHDRS; i++) {
		error = bread(hmp->devvp, i * HAMMER2_ZONE_BYTES64,
			      HAMMER2_VOLUME_BYTES, &bp);
		if (error) {
			brelse(bp);
			bp = NULL;
			continue;
		}

		vd = (struct hammer2_volume_data *) bp->b_data;
		if ((vd->magic != HAMMER2_VOLUME_ID_HBO) &&
		    (vd->magic != HAMMER2_VOLUME_ID_ABO)) {
			brelse(bp);
			bp = NULL;
			continue;
		}

		if (vd->magic == HAMMER2_VOLUME_ID_ABO) {
			/* XXX: Reversed-endianness filesystem */
			printf("hammer2: reverse-endian filesystem detected");
			brelse(bp);
			bp = NULL;
			continue;
		}

		crc = vd->icrc_sects[HAMMER2_VOL_ICRC_SECT0];
		crc0 = hammer2_icrc32(bp->b_data + HAMMER2_VOLUME_ICRC0_OFF,
				      HAMMER2_VOLUME_ICRC0_SIZE);
		bcrc = vd->icrc_sects[HAMMER2_VOL_ICRC_SECT1];
		bcrc0 = hammer2_icrc32(bp->b_data + HAMMER2_VOLUME_ICRC1_OFF,
				       HAMMER2_VOLUME_ICRC1_SIZE);
		if ((crc0 != crc) || (bcrc0 != bcrc)) {
			printf("hammer2 volume header crc "
				"mismatch copy #%d %08x/%08x\n",
				i, crc0, crc);
			error_reported = 1;
			brelse(bp);
			bp = NULL;
			continue;
		}
		if (valid == 0 || hmp->voldata.mirror_tid < vd->mirror_tid) {
			valid = 1;
			hmp->voldata = *vd;
			hmp->volhdrno = i;
		}
		brelse(bp);
		bp = NULL;
	}
	if (valid) {
		hmp->volsync = hmp->voldata;
		error = 0;
		if (error_reported || bootverbose || 1) { /* 1/DEBUG */
			printf("hammer2: using volume header #%d\n",
				hmp->volhdrno);
		}
	} else {
		error = EINVAL;
		printf("hammer2: no valid volume headers found!\n");
	}
	return (error);
}

/*
 * Reconnect using the passed file pointer.  The caller must ref the
 * fp for us.
 */
void
hammer2_cluster_reconnect(hammer2_mount_t *hmp, struct file *fp)
{
	size_t name_len;
	const char *name = "disk-volume";

	/*
	 * Closes old comm descriptor, kills threads, cleans up
	 * states, then installs the new descriptor and creates
	 * new threads.
	 */
	// XX kdmsg_iocom_reconnect(&hmp->iocom, fp, "hammer2");

	/*
	 * Setup LNK_CONN fields for autoinitiated state machine.  We
	 * will use SPANs to advertise multiple PFSs so only pass the
	 * fsid and HAMMER2_PFSTYPE_SUPROOT for the AUTOCONN.
	 *
	 * We are not initiating a LNK_SPAN so we do not have to set-up
	 * iocom.auto_lnk_span.
	 */
	bzero(&hmp->iocom.auto_lnk_conn.pfs_clid,
	      sizeof(hmp->iocom.auto_lnk_conn.pfs_clid));
	hmp->iocom.auto_lnk_conn.pfs_fsid = hmp->voldata.fsid;
	hmp->iocom.auto_lnk_conn.pfs_type = HAMMER2_PFSTYPE_SUPROOT;
	hmp->iocom.auto_lnk_conn.proto_version = DMSG_SPAN_PROTO_1;
#if 0
	hmp->iocom.auto_lnk_conn.peer_type = hmp->voldata.peer_type;
#endif
	hmp->iocom.auto_lnk_conn.peer_type = DMSG_PEER_HAMMER2;

	/*
	 * Filter adjustment.  Clients do not need visibility into other
	 * clients (otherwise millions of clients would present a serious
	 * problem).  The fs_label also serves to restrict the namespace.
	 */
	hmp->iocom.auto_lnk_conn.peer_mask = 1LLU << DMSG_PEER_HAMMER2;
	hmp->iocom.auto_lnk_conn.pfs_mask = (uint64_t)-1;

#if 0
	switch (ipdata->pfs_type) {
	case DMSG_PFSTYPE_CLIENT:
		hmp->iocom.auto_lnk_conn.peer_mask &=
				~(1LLU << DMSG_PFSTYPE_CLIENT);
		break;
	default:
		break;
	}
#endif

	name_len = strlen(name);
	if (name_len >= sizeof(hmp->iocom.auto_lnk_conn.fs_label))
		name_len = sizeof(hmp->iocom.auto_lnk_conn.fs_label) - 1;
	bcopy(name, hmp->iocom.auto_lnk_conn.fs_label, name_len);
	hmp->iocom.auto_lnk_conn.fs_label[name_len] = 0;

	kdmsg_iocom_autoinitiate(&hmp->iocom, hammer2_autodmsg);
}

static int
hammer2_rcvdmsg(kdmsg_msg_t *msg)
{
	printf("RCVMSG %08x\n", msg->tcmd);

	switch(msg->tcmd) {
	case DMSG_DBG_SHELL:
		/*
		 * (non-transaction)
		 * Execute shell command (not supported atm)
		 */
		// XX kdmsg_msg_result(msg, DMSG_ERR_NOSUPP);
		break;
	case DMSG_DBG_SHELL | DMSGF_REPLY:
		/*
		 * (non-transaction)
		 */
		if (msg->aux_data) {
			msg->aux_data[msg->aux_size - 1] = 0;
			printf("HAMMER2 DBG: %s\n", msg->aux_data);
		}
		break;
	default:
		/*
		 * Unsupported message received.  We only need to
		 * reply if it's a transaction in order to close our end.
		 * Ignore any one-way messages or any further messages
		 * associated with the transaction.
		 *
		 * NOTE: This case also includes DMSG_LNK_ERROR messages
		 *	 which might be one-way, replying to those would
		 *	 cause an infinite ping-pong.
		 */
		if (msg->any.head.cmd & DMSGF_CREATE)
			// XX kdmsg_msg_reply(msg, DMSG_ERR_NOSUPP);
		break;
	}
	return(0);
}

/*
 * This function is called after KDMSG has automatically handled processing
 * of a LNK layer message (typically CONN, SPAN, or CIRC).
 *
 * We tag off the LNK_CONN to trigger our LNK_VOLCONF messages which
 * advertises all available hammer2 super-root volumes.
 */
static void hammer2_update_spans(hammer2_mount_t *hmp, kdmsg_state_t *state);

static void
hammer2_autodmsg(kdmsg_msg_t *msg)
{
	hammer2_mount_t *hmp = msg->state->iocom->handle;
	int copyid;

	printf("RCAMSG %08x\n", msg->tcmd);

	switch(msg->tcmd) {
	case DMSG_LNK_CONN | DMSGF_CREATE | DMSGF_REPLY:
	case DMSG_LNK_CONN | DMSGF_CREATE | DMSGF_DELETE | DMSGF_REPLY:
		if (msg->any.head.cmd & DMSGF_CREATE) {
			printf("HAMMER2: VOLDATA DUMP\n");

			/*
			 * Dump the configuration stored in the volume header.
			 * This will typically be import/export access rights,
			 * master encryption keys (encrypted), etc.
			 */
			hammer2_voldata_lock(hmp);
			copyid = 0;
			while (copyid < HAMMER2_COPYID_COUNT) {
				if (hmp->voldata.copyinfo[copyid].copyid)
					hammer2_volconf_update(hmp, copyid);
				++copyid;
			}
			hammer2_voldata_unlock(hmp);

			printf("HAMMER2: INITIATE SPANs\n");
			hammer2_update_spans(hmp, msg->state);
		}
		if ((msg->any.head.cmd & DMSGF_DELETE) &&
		    msg->state && (msg->state->txcmd & DMSGF_DELETE) == 0) {
			printf("HAMMER2: CONN WAS TERMINATED\n");
		}
		break;
	default:
		break;
	}
}

/*
 * Update LNK_SPAN state
 */
static void
hammer2_update_spans(hammer2_mount_t *hmp, kdmsg_state_t *state)
{
	const hammer2_inode_data_t *ipdata;
	hammer2_cluster_t *cparent;
	hammer2_cluster_t *cluster;
	hammer2_pfsmount_t *spmp;
	hammer2_key_t key_next;
	kdmsg_msg_t *rmsg;
	size_t name_len;
	int ddflag;

	/*
	 * Lookup mount point under the media-localized super-root.
	 *
	 * cluster->pmp will incorrectly point to spmp and must be fixed
	 * up later on.
	 */
	spmp = hmp->spmp;
	cparent = hammer2_inode_lock_ex(spmp->iroot);
	cluster = hammer2_cluster_lookup(cparent, &key_next,
					 HAMMER2_KEY_MIN,
					 HAMMER2_KEY_MAX,
					 0, &ddflag);
	while (cluster) {
		if (hammer2_cluster_type(cluster) != HAMMER2_BREF_TYPE_INODE)
			continue;
		ipdata = &hammer2_cluster_data(cluster)->ipdata;
		printf("UPDATE SPANS: %s\n", ipdata->filename);

		rmsg = kdmsg_msg_alloc(state, DMSG_LNK_SPAN | DMSGF_CREATE,
				       hammer2_lnk_span_reply, NULL);
		rmsg->any.lnk_span.pfs_clid = ipdata->pfs_clid;
		rmsg->any.lnk_span.pfs_fsid = ipdata->pfs_fsid;
		rmsg->any.lnk_span.pfs_type = ipdata->pfs_type;
		rmsg->any.lnk_span.peer_type = DMSG_PEER_HAMMER2;
		rmsg->any.lnk_span.proto_version = DMSG_SPAN_PROTO_1;
		name_len = ipdata->name_len;
		if (name_len >= sizeof(rmsg->any.lnk_span.fs_label))
			name_len = sizeof(rmsg->any.lnk_span.fs_label) - 1;
		bcopy(ipdata->filename, rmsg->any.lnk_span.fs_label, name_len);

		kdmsg_msg_write(rmsg);

		cluster = hammer2_cluster_next(cparent, cluster,
					       &key_next,
					       key_next,
					       HAMMER2_KEY_MAX,
					       0);
	}
	hammer2_inode_unlock_ex(spmp->iroot, cparent);
}

static
int
hammer2_lnk_span_reply(kdmsg_state_t *state, kdmsg_msg_t *msg)
{
	if ((state->txcmd & DMSGF_DELETE) == 0 &&
	    (msg->any.head.cmd & DMSGF_DELETE)) {
		kdmsg_msg_reply(msg, 0);
	}
	return 0;
}

/*
 * Volume configuration updates are passed onto the userland service
 * daemon via the open LNK_CONN transaction.
 */
void
hammer2_volconf_update(hammer2_mount_t *hmp, int index)
{
	kdmsg_msg_t *msg;

	/* XXX interlock against connection state termination */
	printf("volconf update %p\n", hmp->iocom.conn_state);
	if (hmp->iocom.conn_state) {
		printf("TRANSMIT VOLCONF VIA OPEN CONN TRANSACTION\n");
		msg = kdmsg_msg_alloc(hmp->iocom.conn_state,
				      DMSG_LNK_HAMMER2_VOLCONF,
				      NULL, NULL);
		H2_LNK_VOLCONF(msg)->copy = hmp->voldata.copyinfo[index];
		H2_LNK_VOLCONF(msg)->mediaid = hmp->voldata.fsid;
		H2_LNK_VOLCONF(msg)->index = index;
		kdmsg_msg_write(msg);
	}
}

/*
 * This handles hysteresis on regular file flushes.  Because the BIOs are
 * routed to a thread it is possible for an excessive number to build up
 * and cause long front-end stalls long before the runningbuffspace limit
 * is hit, so we implement hammer2_flush_pipe to control the
 * hysteresis.
 *
 * This is a particular problem when compression is used.
 */
void
hammer2_lwinprog_ref(hammer2_pfsmount_t *pmp)
{
	atomic_add_int(&pmp->count_lwinprog, 1);
}

void
hammer2_lwinprog_drop(hammer2_pfsmount_t *pmp)
{
	int lwinprog;

	lwinprog = atomic_fetchadd_int(&pmp->count_lwinprog, -1);
	if ((lwinprog & HAMMER2_LWINPROG_WAITING) &&
	    (lwinprog & HAMMER2_LWINPROG_MASK) <= hammer2_flush_pipe * 2 / 3) {
		atomic_clear_int(&pmp->count_lwinprog,
				 HAMMER2_LWINPROG_WAITING);
		wakeup(&pmp->count_lwinprog);
	}
}

void
hammer2_lwinprog_wait(hammer2_pfsmount_t *pmp)
{
	int lwinprog;

	for (;;) {
		lwinprog = pmp->count_lwinprog;
		cpu_ccfence();
		if ((lwinprog & HAMMER2_LWINPROG_MASK) < hammer2_flush_pipe)
			break;
		// XX tsleep_interlock(&pmp->count_lwinprog, 0);
		atomic_set_int(&pmp->count_lwinprog, HAMMER2_LWINPROG_WAITING);
		lwinprog = pmp->count_lwinprog;
		if ((lwinprog & HAMMER2_LWINPROG_MASK) < hammer2_flush_pipe)
			break;
		tsleep(&pmp->count_lwinprog, PINTERLOCKED, "h2wpipe", hz);
	}
}

/*
 * Manage excessive memory resource use for chain and related
 * structures.
 */
void
hammer2_pfs_memory_wait(hammer2_pfsmount_t *pmp)
{
	uint32_t waiting;
	uint32_t count;
	uint32_t limit;
#if 0
	static int zzticks;
#endif

	/*
	 * Atomic check condition and wait.  Also do an early speedup of
	 * the syncer to try to avoid hitting the wait.
	 */
	for (;;) {
		waiting = pmp->inmem_dirty_chains;
		cpu_ccfence();
		count = waiting & HAMMER2_DIRTYCHAIN_MASK;

		limit = pmp->mp->mnt_nvnodelistsize / 10;
		if (limit < hammer2_limit_dirty_chains)
			limit = hammer2_limit_dirty_chains;
		if (limit < 1000)
			limit = 1000;

#if 0
		if ((int)(ticks - zzticks) > hz) {
			zzticks = ticks;
			printf("count %ld %ld\n", count, limit);
		}
#endif

		/*
		 * Block if there are too many dirty chains present, wait
		 * for the flush to clean some out.
		 */
		if (count > limit) {
			// XX tsleep_interlock(&pmp->inmem_dirty_chains, 0);
			if (atomic_cmpset_int(&pmp->inmem_dirty_chains,
					       waiting,
				       waiting | HAMMER2_DIRTYCHAIN_WAITING)) {
				speedup_syncer(); // XX ?? pmp->mp);
				tsleep(&pmp->inmem_dirty_chains, PINTERLOCKED,
				       "chnmem", hz);
			}
			continue;	/* loop on success or fail */
		}

		/*
		 * Try to start an early flush before we are forced to block.
		 */
		if (count > limit * 7 / 10)
			speedup_syncer(); // XX ?? pmp->mp);
		break;
	}
}

void
hammer2_pfs_memory_inc(hammer2_pfsmount_t *pmp)
{
	if (pmp) {
		atomic_add_int(&pmp->inmem_dirty_chains, 1);
	}
}

void
hammer2_pfs_memory_wakeup(hammer2_pfsmount_t *pmp)
{
	uint32_t waiting;

	if (pmp == NULL)
		return;

	for (;;) {
		waiting = pmp->inmem_dirty_chains;
		cpu_ccfence();
		if (atomic_cmpset_long((unsigned long *)&pmp->inmem_dirty_chains,
				       (u_long)waiting,
				       (waiting - 1) &
					~HAMMER2_DIRTYCHAIN_WAITING)) {
			break;
		}
	}

	if (waiting & HAMMER2_DIRTYCHAIN_WAITING)
		wakeup(&pmp->inmem_dirty_chains);
}

/*
 * Debugging
 */
void
hammer2_dump_chain(hammer2_chain_t *chain, int tab, int *countp, char pfx)
{
	hammer2_chain_t *scan;
	hammer2_chain_t *parent;

	--*countp;
	if (*countp == 0) {
		printf("%*.*s...\n", tab, tab, "");
		return;
	}
	if (*countp < 0)
		return;
	printf("%*.*s%c-chain %p.%d %016x/%d mir=%016x\n",
		tab, tab, "", pfx,
		chain, chain->bref.type,
		(unsigned int)chain->bref.key, chain->bref.keybits,
		(unsigned int)chain->bref.mirror_tid);

	printf("%*.*s      [%08x] (%s) refs=%d\n",
		tab, tab, "",
		chain->flags,
		((chain->bref.type == HAMMER2_BREF_TYPE_INODE &&
		chain->data) ?  (char *)chain->data->ipdata.filename : "?"),
		chain->refs);

/* XXX fix me	printf("%*.*s    core %p [%08x]",
		(char *)tab, (void *)tab, " ", 
		chain->core.flags);
*/

	parent = chain->parent;
	if (parent)
		printf("\n%*.*s      p=%p [pflags %08x prefs %d",
			tab, tab, "",
			parent, parent->flags, parent->refs);
	if (RB_EMPTY(&chain->core.rbtree)) {
		printf("\n");
	} else {
		printf(" {\n");
		RB_FOREACH(scan, hammer2_chain_tree, &chain->core.rbtree)
			hammer2_dump_chain(scan, tab + 4, countp, 'a');
		if (chain->bref.type == HAMMER2_BREF_TYPE_INODE && chain->data)
			printf("%*.*s}(%s)\n", tab, tab, "",
				chain->data->ipdata.filename);
		else
			printf("%*.*s}\n", tab, tab, "");
	}
}

const struct vfsops hammer2_vfsops = {
        .vfs_init       = hammer2_vfs_init,
        //.vfs_uninit     = hammer2_vfs_uninit,
        //.vfs_sync     = hammer2_vfs_sync,
        (void *)hammer2_vfs_mount,
        (void *)hammer2_vfs_unmount,
        .vfs_root       = hammer2_vfs_root,
        //.vfs_statfs   = hammer2_vfs_statfs,
        (void *)hammer2_vfs_statvfs,
        (void *)hammer2_vfs_vget,
        .vfs_vptofh     = hammer2_vfs_vptofh,
        (void *)hammer2_vfs_fhtovp,
        (void *)hammer2_vfs_checkexp
};
