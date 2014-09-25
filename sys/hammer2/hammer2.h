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

/*
 * HAMMER2 IN-MEMORY CACHE OF MEDIA STRUCTURES
 *
 * This header file contains structures used internally by the HAMMER2
 * implementation.  See hammer2_disk.h for on-disk structures.
 *
 * There is an in-memory representation of all on-media data structure.
 * Basically everything is represented by a hammer2_chain structure
 * in-memory and other higher-level structures map to chains.
 *
 * A great deal of data is accessed simply via its buffer cache buffer,
 * which is mapped for the duration of the chain's lock.  However, because
 * chains may represent blocks smaller than the 16KB minimum we impose
 * on buffer cache buffers, we cannot hold related buffer cache buffers
 * locked for smaller blocks.  In these situations we kmalloc() a copy
 * of the block.
 *
 * When modifications are made to a chain a new filesystem block must be
 * allocated.  Multiple modifications do not necessarily allocate new
 * blocks.  However, when a flush occurs a flush synchronization point
 * is created and any new modifications made after this point will allocate
 * a new block even if the chain is already in a modified state.
 *
 * The in-memory representation may remain cached (for example in order to
 * placemark clustering locks) even after the related data has been
 * detached.
 *
 *				CORE SHARING
 *
 * In order to support concurrent flushes a flush synchronization point
 * is created represented by a transaction id.  Among other things,
 * operations may move filesystem objects from one part of the topology
 * to another (for example, if you rename a file or when indirect blocks
 * are created or destroyed, and a few other things).  When this occurs
 * across a flush synchronization point the flusher needs to be able to
 * recurse down BOTH the 'before' version of the topology and the 'after'
 * version.
 *
 * To facilitate this modifications to chains do what is called a
 * DELETE-DUPLICATE operation.  Chains are not actually moved in-memory.
 * Instead the chain we wish to move is deleted and a new chain is created
 * at the target location in the topology.  ANY SUBCHAINS PLACED UNDER THE
 * CHAIN BEING MOVED HAVE TO EXIST IN BOTH PLACES.  To make this work
 * all sub-chains are managed by the hammer2_chain_core structure.  This
 * structure can be multi-homed, meaning that it can have more than one
 * chain as its parent.  When a chain is delete-duplicated the chain's core
 * becomes shared under both the old and new chain.
 *
 *				STALE CHAINS
 *
 * When a chain is delete-duplicated the old chain typically becomes stale.
 * This is detected via the HAMMER2_CHAIN_DUPLICATED flag in chain->flags.
 * To avoid executing live filesystem operations on stale chains, the inode
 * locking code will follow stale chains via core->ownerq until it finds
 * the live chain.  The lock prevents ripups by other threads.  Lookups
 * must properly order locking operations to prevent other threads from
 * racing the lookup operation and will also follow stale chains when
 * required.
 */

#ifndef _VFS_HAMMER2_HAMMER2_H_
#define _VFS_HAMMER2_HAMMER2_H_

#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/limits.h>
#include <sys/mutex.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/stdint.h>
#include <sys/lockf.h>
#include <machine/spinlock.h>
#include <sys/atomic.h>
#include <sys/namecache.h>
#include <sys/objcache.h>
#include <dev/pci/drm/drm_atomic.h>

#include "hammer2_disk.h"
#include "hammer2_mount.h"
#include "hammer2_ioctl.h"
//#include "hammer2_ccms.h"

/* XXX temporary porting goop */
/* #define KKASSERT(cond) if (!(cond)) panic("KKASSERT: %s in %s", #cond, __func__)
#undef KASSERT
#define KASSERT(cond, complaint) if (!(cond)) panic complaint
*/

#define M_HAMMER2 1001
#define atomic_set_int(p, bits)         atomic_setbits_int(p,bits)

#define VA_UID_UUID_VALID    0x0004  /* uuid fields also populated */
#define VA_GID_UUID_VALID    0x0008  /* uuid fields also populated */
#define GETBLK_BHEAVY     0x0002  /* heavy weight buffer */
#define IO_DIRECT       0x0100          /* attempt to bypass buffer cache */
#define   NOOFFSET        (-1LL)          /* No buffer offset calculated yet */
#define VA_FSID_UUID_VALID   0x0010  /* uuid fields also populated */
#define BKVASIZE        MAXBSIZE        /* must be power of 2 */
#define IO_RECURSE      0x0200          /* possibly device-recursive (vn) */
#define FILTEROP_MPSAFE 0x0002
#define MOUNTCTL_SET_EXPORT          16      /* sys/mount.h:export_args */
#define B_NOTMETA       0x00000004      /* This really isn't metadata */
#define EV_NODATA       0x1000          /* EOF and no more data */
#define NOTE_OLDAPI     0x04000000      /* select/poll note */
#define NLC_FOLLOW            0x00000001      /* follow leaf symlink */
#define IO_ASYNC        0x0080          /* bawrite rather then bdwrite */
#define MNTK_WANTRDWR   0x04000000      /* upgrade to read/write requested */
#define VMSC_ONEPASS    0x20
#define VMSC_NOWAIT     0x10
#define DMSG_PFSTYPE_CLIENT      2
#define BIO_DONE  0x40000000
#define BIO_SYNC  0x00000001
#define M_USE_RESERVE   0x0200  /* can eat into free list reserve */
#define M_USE_INTERRUPT_RESERVE \
                        0x1000  /* can exhaust free list entirely */
#define PINTERLOCKED    0x00000400      /* Interlocked tsleep */
#define GETBLK_NOWAIT     0x0008  /* non-blocking */
#define   B_RELBUF        0x00400000      /* Release VMIO buffer. */
#define   B_CLUSTEROK     0x00020000      /* Pagein op, so swap() can count it. */
#define        M_INTWAIT       (M_WAITOK | M_USE_RESERVE | M_USE_INTERRUPT_RESERVE)
#define RB_GENERATE2(name, type, field, cmp, datatype, indexfield)

//#define RB_LOOKUP(name, root, value)     name##_RB_LOOKUP(root, value)
//#define hammer2_io_tree_RB_LOOKUP(name, root, value, value2)     name##_RB_LOOKUP(root, value, value2)
#define RB_SCAN(name, root, cmp, callback, data)  
#define RB_EMPTY(head)                  (RB_ROOT(head) == NULL)

/* int vfsync(struct vnode *vp, int waitfor, int passes,
        int (*checkdef)(struct buf *),
        int (*waitoutput)(struct vnode *, struct thread *));
*/


struct globaldata {
	struct privatespace *gd_prvspace;       /* self-reference */
	struct thread   *gd_curthread;
  	struct thread   *gd_freetd;             /* cache one free td */
	__uint32_t      gd_reqflags;            /* (see note above) */
	long            gd_flags;
	//lwkt_queue      gd_tdallq;              /* all threads */
	//lwkt_queue      gd_tdrunq;              /* runnable threads */
	__uint32_t      gd_cpuid;
	//cpumask_t       gd_cpumask;             /* mask = CPUMASK(cpuid) */
	//cpumask_t       gd_other_cpus;          /* mask of 'other' cpus */
	struct timeval  gd_stattv;
	int             gd_intr_nesting_level;  /* hard code, intrs, ipis */
	//struct vmmeter  gd_cnt;
	struct vmtotal  gd_vmtotal;
	//cpumask_t       gd_ipimask;             /* pending ipis from cpus */
	//struct lwkt_ipiq *gd_ipiq;              /* array[ncpu] of ipiq's */
	//struct lwkt_ipiq gd_cpusyncq;           /* ipiq for cpu synchro */
	u_int           gd_npoll;               /* ipiq synchronization */
	int             gd_tdrunqcount;
	//struct thread   gd_unused02B;
	//struct thread   gd_idlethread;
	//SLGlobalData    gd_slab;                /* slab allocator */
	int             gd_trap_nesting_level;  /* track traps */
	int             gd_vme_avail;           /* vm_map_entry reservation */
	struct vm_map_entry *gd_vme_base;       /* vm_map_entry reservation */
	//struct systimerq gd_systimerq;          /* per-cpu system timers */
	int             gd_syst_nest;
	//struct systimer gd_hardclock;           /* scheduler periodic */
	//struct systimer gd_statclock;           /* statistics periodic */
	//struct systimer gd_schedclock;          /* scheduler periodic */
	volatile __uint32_t gd_time_seconds;    /* uptime in seconds */
	//volatile sysclock_t gd_cpuclock_base;   /* cpuclock relative base */
 
	struct pipe     *gd_pipeq;              /* cache pipe structures */
	struct nchstats *gd_nchstats;           /* namecache effectiveness */
	int             gd_pipeqcount;          /* number of structures */
	//sysid_t         gd_sysid_alloc;         /* allocate unique sysid */

	struct tslpque  *gd_tsleep_hash;        /* tsleep/wakeup support */
	long            gd_processing_ipiq;
	int             gd_spinlocks;           /* Exclusive spinlocks held */
	struct systimer *gd_systimer_inprog;    /* in-progress systimer */
	int             gd_timer_running;
	u_int           gd_idle_repeat;         /* repeated switches to idle */
	int             gd_ireserved[7];
	const char      *gd_infomsg;            /* debugging */
	//struct lwkt_tokref gd_handoff;          /* hand-off tokref */
	void            *gd_delayed_wakeup[2];
	void            *gd_sample_pc;          /* sample program ctr/tr */
	void            *gd_preserved[5];       /* future fields */
	/* extended by <machine/globaldata.h> */
};
 
typedef struct globaldata *globaldata_t;

#define VOP_FSYNC(vp, waitfor, flags)                   \
	vop_fsync(*(vp)->v_ops, vp, waitfor, flags)

/*
 * vnode must be locked
 */

/*
 * Two-level object cache consisting of NUMA cluster-level depots of
 * fully loaded or completely empty magazines and cpu-level caches of
 * individual objects.
 */
struct objcache {
	char                    *name;
 
	/* object constructor and destructor from blank storage */
	//objcache_ctor_fn        *ctor;
	//objcache_dtor_fn        *dtor;
	void                    *privdata;
 
	/* interface to underlying allocator */
	//objcache_alloc_fn       *alloc;
	//objcache_free_fn        *free;
	void                    *allocator_args;
  
	LIST_ENTRY(objcache)    oc_next;
	int                     exhausted;      /* oops */
 
        /* NUMA-cluster level caches */
  	//struct magazinedepot    depot[1];
  
  	//struct percpu_objcache  cache_percpu[];         /* per-cpu caches */
};

//void vfs_bio_clrbuf(struct buf *);
uint32_t iscsi_crc32(const void *, size_t);
int vop_helper_chmod(struct vnode *, mode_t, struct ucred *, uid_t, gid_t, mode_t *);
int vop_helper_setattr_flags(u_int32_t *, u_int32_t,uid_t, struct ucred *);
int vop_helper_chown(struct vnode *vp, uid_t new_uid, gid_t new_gid,
                 struct ucred *cred,
                 uid_t *cur_uidp, gid_t *cur_gidp, mode_t *cur_modep);
//int nvtruncbuf(struct vnode *vp, off_t length, int blksize, int boff, int trivial);
void bheavy(struct buf *bp);
//viod bioq_insert_tail(struct bio_queue_head *bioq, struct bio *bio);
//void objcache_destroy(struct objcache *);

/*
 * vfs_bio_clrbuf:
 *
 *      Clear a buffer.  This routine essentially fakes an I/O, so we need
 *      to clear B_ERROR and B_INVAL.
 *
 *      Note that while we only theoretically need to clear through b_bcount,
 *      we go ahead and clear through b_bufsize.
 */
/*
void
vfs_bio_clrbuf(struct buf *bp)
{
}
*/

struct bio_queue_head {
	TAILQ_HEAD(bio_queue, bio) queue;
	off_t last_offset;
	struct  bioh2 *insert_point;
	struct  bioh2 *transition;
	struct  bioh2 *bio_unused;
};

/*
 * Warning: a_dvp is only held, not ref'd.  The target must still vget()
 * it.
 */
struct vop_nrmdir_args {
	struct vop_generic_args a_head;
	struct nchandle *a_nch;		/* locked namespace */
	struct vnode *a_dvp;
	struct ucred *a_cred;
};

/*
 * Warning: a_dvp is only held, not ref'd.  The target must still vget()
 * it.
 */
struct vop_nremove_args {
	struct vop_generic_args a_head;
	struct nchandle *a_nch;		/* locked namespace */
	struct vnode *a_dvp;
	struct ucred *a_cred;
};

/*
 * Warning: a_dvp is only held, not ref'd.  The target must still vget()
 * it.
 */
struct vop_nlink_args {
	struct vop_generic_args a_head;
	struct nchandle *a_nch;
	struct vnode *a_dvp;
	struct vnode *a_vp;
	struct ucred *a_cred;
};

struct vop_nlookupdotdot_args {
	struct vop_generic_args a_head;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct ucred *a_cred;
	char **a_fakename;
};
/*
 * Warning: a_dvp is only held, not ref'd.  The target must still vget()
 * it.
 */
struct vop_nmkdir_args {
	struct vop_generic_args a_head;
	struct nchandle *a_nch;		/* locked namespace */
	struct vnode *a_dvp;
	struct vnode **a_vpp;			/* returned refd & locked */
	struct ucred *a_cred;
	struct vattr *a_vap;
};

/*
 * Warning: a_dvp is only held, not ref'd.  The target must still vget()
 * it.
 */
struct vop_nmknod_args {
        struct vop_generic_args a_head;
        struct nchandle *a_nch;
        struct vnode *a_dvp;
        struct vnode **a_vpp;
        struct ucred *a_cred;
        struct vattr *a_vap;
};

/*
 * Warning: a_dvp is only held, not ref'd.  The target must still vget()
 * it.
 */
struct vop_ncreate_args {
        struct vop_generic_args a_head;
        struct nchandle *a_nch;         /* locked namespace */
        struct vnode *a_dvp;            /* held directory vnode */
        struct vnode **a_vpp;           /* returned refd & locked */
        struct ucred *a_cred;
        struct vattr *a_vap;
};

/*
 * Warning: a_fdvp and a_tdvp are only held, not ref'd.  The target must
 * still vget() it.
 */
struct vop_nrename_args {
        struct vop_generic_args a_head;
        struct nchandle *a_fnch;                /* locked namespace / from */
        struct nchandle *a_tnch;                /* locked namespace / to */
        struct vnode *a_fdvp;
        struct vnode *a_tdvp;
        struct ucred *a_cred;
};

/*
 * Warning: a_dvp is only held, not ref'd.  The target must still vget()
 * it.
 */
struct vop_nsymlink_args {
        struct vop_generic_args a_head;
        struct nchandle *a_nch;
        struct vnode *a_dvp;
        struct vnode **a_vpp;
        struct ucred *a_cred;
        struct vattr *a_vap;
        char *a_target;
};

/*
 * XXX temporary
 */
#define b_bio1          b_bio_array[0]  /* logical layer */
#define b_bio2          b_bio_array[1]  /* (typically) the disk layer */
#define b_loffset       b_bio1.bio_offset

#define MNTK_UNMOUNTF   0x00000001      /* forced unmount in progress */
#define MNTK_MPSAFE     0x00010000      /* call vops without mnt_token lock */
#define MNTK_RD_MPSAFE  0x00020000      /* vop_read is MPSAFE */
#define MNTK_WR_MPSAFE  0x00040000      /* vop_write is MPSAFE */
#define MNTK_GA_MPSAFE  0x00080000      /* vop_getattr is MPSAFE */
#define MNTK_IN_MPSAFE  0x00100000      /* vop_inactive is MPSAFE */
#define MNTK_SG_MPSAFE  0x00200000      /* vop_strategy is MPSAFE */
#define MNTK_NCALIASED  0x00800000      /* namecached aliased */
#define MNTK_UNMOUNT    0x01000000      /* unmount in progress */
#define MNTK_MWAIT      0x02000000      /* waiting for unmount to finish */
#define MNTK_WANTRDWR   0x04000000      /* upgrade to read/write requested */
#define MNTK_FSMID      0x08000000      /* getattr supports FSMIDs */
#define MNTK_NOSTKMNT   0x10000000      /* no stacked mount point allowed */
#define MNTK_NOMSYNC    0x20000000      /* used by tmpfs */
#define MNTK_THR_SYNC   0x40000000      /* fs sync thread requested */
#define MNTK_ST_MPSAFE  0x80000000      /* (mfs) vfs_start is MPSAFE */

#define MNTK_ALL_MPSAFE (MNTK_MPSAFE | MNTK_RD_MPSAFE | MNTK_WR_MPSAFE | \
                         MNTK_GA_MPSAFE | MNTK_IN_MPSAFE | MNTK_SG_MPSAFE | \
                         MNTK_ST_MPSAFE)


/*
 * The message (cmd) field also encodes various flags and the total size
 * of the message header.  This allows the protocol processors to validate
 * persistency and structural settings for every command simply by
 * switch()ing on the (cmd) field.
 */
#define DMSGF_CREATE            0x80000000U     /* msg start */
#define DMSGF_DELETE            0x40000000U     /* msg end */
#define DMSGF_REPLY             0x20000000U     /* reply path */
#define DMSGF_ABORT             0x10000000U     /* abort req */
#define DMSGF_AUXOOB            0x08000000U     /* aux-data is OOB */
#define DMSGF_FLAG2             0x04000000U
#define DMSGF_FLAG1             0x02000000U
#define DMSGF_FLAG0             0x01000000U

#define DMSGF_FLAGS             0xFF000000U     /* all flags */
#define DMSGF_PROTOS            0x00F00000U     /* all protos */
#define DMSGF_CMDS              0x000FFF00U     /* all cmds */
#define DMSGF_SIZE              0x000000FFU     /* N*32 */

#define DMSG_PEER_NONE          0
#define DMSG_PEER_CLUSTER       1       /* a cluster controller */
#define DMSG_PEER_BLOCK         2       /* block devices */
#define DMSG_PEER_HAMMER2       3       /* hammer2-mounted volumes */

#define DMSGF_CMDSWMASK         (DMSGF_CMDS |   \
                                         DMSGF_SIZE |   \
                                         DMSGF_PROTOS | \
                                         DMSGF_REPLY)
#define DMSGF_TRANSMASK         (DMSGF_CMDS |   \
                                         DMSGF_SIZE |   \
                                         DMSGF_PROTOS | \
                                         DMSGF_REPLY |  \
                                         DMSGF_CREATE | \
                                         DMSGF_DELETE)

typedef enum buf_cmd {
        BUF_CMD_DONE = 0,
        BUF_CMD_READ,
        BUF_CMD_WRITE,
        BUF_CMD_FREEBLKS,
        BUF_CMD_FORMAT,
        BUF_CMD_FLUSH
} buf_cmd_t;

/*
 * The buffer header describes an I/O operation in the kernel.
 *
 * NOTES:
 *	b_bufsize represents the filesystem block size (for this particular
 *	block) and/or the allocation size or original request size.  This
 *	field is NOT USED by lower device layers.  VNode and device
 *	strategy routines WILL NEVER ACCESS THIS FIELD.
 *
 *	b_bcount represents the I/O request size.  Unless B_NOBCLIP is set,
 *	the device chain is allowed to clip b_bcount to accomodate the device
 *	EOF.  Note that this is different from the byte oriented file EOF.
 *	If B_NOBCLIP is set, the device chain is required to generate an
 *	error if it would othrewise have to clip the request.  Buffers 
 *	obtained via getblk() automatically set B_NOBCLIP.  It is important
 *	to note that EOF clipping via b_bcount is different from EOF clipping
 *	via returning a b_actual < b_bcount.  B_NOBCLIP only effects block
 *	oriented EOF clipping (b_bcount modifications).
 *
 *	b_actual represents the number of bytes of I/O that actually occured,
 *	whether an error occured or not.  b_actual must be initialized to 0
 *	prior to initiating I/O as the device drivers will assume it to
 *	start at 0.
 *
 *	b_dirtyoff, b_dirtyend.  Buffers support piecemeal, unaligned
 *	ranges of dirty data that need to be written to backing store.
 *	The range is typically clipped at b_bcount (not b_bufsize).
 *
 *	b_bio1 and b_bio2 represent the two primary I/O layers.  Additional
 *	I/O layers are allocated out of the object cache and may also exist.
 *
 *	b_bio1 is the logical layer and contains offset or block number 
 *	data for the primary vnode, b_vp.  I/O operations are almost
 *	universally initiated from the logical layer, so you will often
 *	see things like:  vn_strategy(bp->b_vp, &bp->b_bio1).
 *
 *	b_bio2 is the first physical layer (typically the slice-relative
 *	layer) and contains the translated offset or block number for
 *	the block device underlying a filesystem.   Filesystems such as UFS
 *	will maintain cached translations and you may see them initiate
 *	a 'physical' I/O using vn_strategy(devvp, &bp->b_bio2).  BUT, 
 *	remember that the layering is relative to bp->b_vp, so the
 *	device-relative block numbers for buffer cache operations that occur
 *	directly on a block device will be in the first BIO layer.
 *
 *	b_ops - initialized if a buffer has a bio_ops
 *
 *	NOTE!!! Only the BIO subsystem accesses b_bio1 and b_bio2 directly.
 *	ALL STRATEGY LAYERS FOR BOTH VNODES AND DEVICES ONLY ACCESS THE BIO
 *	PASSED TO THEM, AND WILL PUSH ANOTHER BIO LAYER IF FORWARDING THE
 *	I/O DEEPER.  In particular, a vn_strategy() or dev_dstrategy()
 *	call should not ever access buf->b_vp as this vnode may be totally
 *	unrelated to the vnode/device whos strategy routine was called.
 */
#define NBUF_BIO	6
struct bufh2 {
	RB_ENTRY(buf) b_rbnode;		/* RB node in vnode clean/dirty tree */
	RB_ENTRY(buf) b_rbhash;		/* RB node in vnode hash tree */
	TAILQ_ENTRY(buf) b_freelist;	/* Free list position if not active. */
	struct buf *b_cluster_next;	/* Next buffer (cluster code) */
	struct vnode *b_vp;		/* (vp, loffset) index */
	//struct bioh2 b_bio_array[NBUF_BIO]; /* BIO translation layers */ 
	u_int32_t b_flags;		/* B_* flags. */
	unsigned int b_qindex;		/* buffer queue index */
	unsigned int b_qcpu;		/* buffer queue cpu */
	unsigned char b_act_count;	/* similar to vm_page act_count */
	unsigned char b_unused01;
	struct lock b_lock;		/* Buffer lock */
	void	*b_iosched;		/* I/O scheduler priv data */
	buf_cmd_t b_cmd;		/* I/O command */
	int	b_bufsize;		/* Allocated buffer size. */
	int	b_runningbufspace;	/* when I/O is running, pipelining */
	int	b_bcount;		/* Valid bytes in buffer. */
	int	b_resid;		/* Remaining I/O */
	int	b_error;		/* Error return */
	caddr_t	b_data;			/* Memory, superblocks, indirect etc. */
	caddr_t	b_kvabase;		/* base kva for buffer */
	int	b_kvasize;		/* size of kva for buffer */
	int	b_dirtyoff;		/* Offset in buffer of dirty region. */
	int	b_dirtyend;		/* Offset of end of dirty region. */
	int	b_refs;			/* FINDBLK_REF/bqhold()/bqdrop() */
	//struct	xio b_xio;  		/* data buffer page list management */
	struct  bio_ops *b_ops;		/* bio_ops used w/ b_dep */
	struct	workhead b_dep;		/* List of filesystem dependencies. */
};

RB_HEAD(buf_rb_tree, buf);
RB_HEAD(buf_rb_hash, buf);

struct vnodeh2 {
        struct _atomic_lock *v_spin;
        int     v_flag;                         /* vnode flags (see below) */
        int     v_writecount;
        int     v_opencount;                    /* number of explicit opens */
        int     v_auxrefs;                      /* auxiliary references */
        int     v_refcnt;
        //struct bio_track v_track_read;          /* track I/O's in progress */
        //struct bio_track v_track_write;         /* track I/O's in progress */
        struct mount *v_mount;                  /* ptr to vfs we are in */
        struct vop_ops **v_ops;                 /* vnode operations vector */
        TAILQ_ENTRY(vnode) v_list;              /* vnode act/inact/cache/free */
        TAILQ_ENTRY(vnode) v_nmntvnodes;        /* vnodes for mount point */
        LIST_ENTRY(vnode) v_synclist;           /* vnodes with dirty buffers */
        struct buf_rb_tree v_rbclean_tree;      /* RB tree of clean bufs */
        struct buf_rb_tree v_rbdirty_tree;      /* RB tree of dirty bufs */
        struct buf_rb_hash v_rbhash_tree;       /* RB tree general lookup */
        enum    vtype v_type;                   /* vnode type */
        int16_t         v_act;                  /* use heuristic */
        int16_t         v_state;                /* active/free/cached */
        union {
                struct socket   *vu_socket;     /* unix ipc (VSOCK) */
                struct {
                        int     vu_umajor;      /* device number for attach */
                        int     vu_uminor;
                        struct cdev     *vu_cdevinfo; /* device (VCHR, VBLK) */
                        SLIST_ENTRY(vnode) vu_cdevnext;
                } vu_cdev;
                struct fifoinfo *vu_fifoinfo;   /* fifo (VFIFO) */
        } v_un;
        off_t   v_filesize;                     /* file EOF or NOOFFSET */
        off_t   v_lazyw;                        /* lazy write iterator */
        struct vm_object *v_object;             /* Place to store VM object */
        struct  lock v_lock;                    /* file/dir ops lock */
        enum    vtagtype v_tag;                 /* type of underlying data */
        void    *v_data;                        /* private data for fs */
        //struct namecache_list v_namecache;      /* (S) associated nc entries */
        //struct  {
         //       struct  kqinfo vpi_kqinfo;      /* identity of poller(s) */
        //} v_pollinfo;
        struct vmresident *v_resident;          /* optional vmresident */
        struct mount *v_pfsmp;                  /* real mp for pfs/nullfs mt */
#ifdef  DEBUG_LOCKS
        const char *filename;                   /* Source file doing locking */
        int line;                               /* Line number doing locking */
#endif
};

/*
 * Namecache handles include a mount reference allowing topologies
 * to be replicated with mount overlays (nullfs mounts).
 */

struct vop_markatime_args {
	struct vop_generic_args a_head;
	int a_op;
	struct vnode *a_vp;
	struct ucred *a_cred;
};

struct vop_mountctl_args {
        struct vop_generic_args a_head;
        int a_op;
        struct file *a_fp;
        const void *a_ctl;
        int a_ctllen;
        void *a_buf;
        int a_buflen;
        int *a_res;
        struct vnode *a_vp;
};

/*
 * Warning: a_dvp is only held, not ref'd.  The target must still vget()
 * it.
 */
struct vop_nresolve_args {
        struct vop_generic_args a_head;
        struct nchandle *a_nch;
        struct vnode *a_dvp;
        struct ucred *a_cred;
};

/*
 * Encapsulation of nlookup parameters.
 *
 * Note on nl_flags and nl_op: nl_flags supports a simplified subset of
 * namei's original CNP flags.  nl_op (e.g. NAMEI_*) does no in any way
 * effect the state of the returned namecache and is only used to enforce
 * access checks.
 */
struct nlookupdata {
        /*
         * These fields are setup by nlookup_init() with nl_nch set to
         * the current directory if a process or the root directory if
         * a pure thread.  The result from nlookup() will be returned in
         * nl_nch.
         */
        struct nchandle nl_nch;         /* start-point and result */
        struct nchandle nl_rootnch;     /* root directory */
        struct nchandle nl_jailnch;     /* jail directory */

        char            *nl_path;       /* path buffer */
        struct thread   *nl_td;         /* thread requesting the nlookup */
        struct ucred    *nl_cred;       /* credentials for nlookup */
        struct vnode    *nl_dvp;        /* NLC_REFDVP */

        int             nl_flags;       /* operations flags */
        int             nl_loopcnt;     /* symlinks encountered */

        /*
         * These fields are populated by vn_open().  nlookup_done() will
         * vn_close() a non-NULL vp so if you extract it be sure to NULL out
         * nl_open_vp.
         */
        struct  vnode   *nl_open_vp;
        int             nl_vp_fmode;
};


/*
 * BIO encapsulation for storage drivers and systems that do not require
 * caching support for underlying blocks.
 *
 * NOTE: bio_done and bio_caller_info belong to the caller, while
 * bio_driver_info belongs to the driver.
 *
 * bio_track is only non-NULL when an I/O is in progress.
 */
struct bioh2 {
        TAILQ_ENTRY(bio) bio_act;       /* driver queue when active */
        TAILQ_ENTRY(bio) link;
        struct bio_track *bio_track;    /* BIO tracking structure */
        struct disk     *bio_disk;
        struct bio      *bio_prev;      /* BIO stack */
        struct bio      *bio_next;      /* BIO stack / cached translations */
        struct buf      *bio_buf;       /* High-level buffer back-pointer. */
        //biodone_t       *bio_done;      /* MPSAFE caller completion function */
        off_t           bio_offset;     /* Logical offset relative to device */
        void            *bio_driver_info;
        int             bio_flags;
        union {
                void    *ptr;
                off_t   offset;
                int     index;
                u_int32_t uvalue32;
                struct buf *cluster_head;
                struct bio *cluster_parent;
        } bio_caller_info1;
        union {
                void    *ptr;
                off_t   offset;
                int     index;
                struct buf *cluster_tail;
        } bio_caller_info2;
        union {
                void    *ptr;
                int     value;
                long    lvalue;
                struct  timeval tv;
        } bio_caller_info3;
};

struct dmsg_hdr {
        uint16_t        magic;          /* 00 sanity, synchro, endian */
        uint16_t        reserved02;     /* 02 */
        uint32_t        salt;           /* 04 random salt helps w/crypto */

        uint64_t        msgid;          /* 08 message transaction id */
        uint64_t        circuit;        /* 10 circuit id or 0   */
        uint64_t        reserved18;     /* 18 */

        uint32_t        cmd;            /* 20 flags | cmd | hdr_size / ALIGN */
        uint32_t        aux_crc;        /* 24 auxillary data crc */
        uint32_t        aux_bytes;      /* 28 auxillary data length (bytes) */
        uint32_t        error;          /* 2C error code or 0 */
        uint64_t        aux_descr;      /* 30 negotiated OOB data descr */
        uint32_t        reserved38;     /* 38 */
        uint32_t        hdr_crc;        /* 3C (aligned) extended header crc */
};

typedef struct dmsg_hdr dmsg_hdr_t;

#define DMSG_PROTO_LNK          0x00000000U
#define DMSG_PROTO_DBG          0x00100000U
#define DMSG_PROTO_DOM          0x00200000U
#define DMSG_PROTO_CAC          0x00300000U
#define DMSG_PROTO_QRM          0x00400000U
#define DMSG_PROTO_BLK          0x00500000U
#define DMSG_PROTO_VOP          0x00600000U

/*
 * Message command constructors, sans flags
 */
#define DMSG_ALIGN              64
#define DMSG_ALIGNMASK          (DMSG_ALIGN - 1)
#define DMSG_DOALIGN(bytes)     (((bytes) + DMSG_ALIGNMASK) &           \
                                 ~DMSG_ALIGNMASK)

#define DMSG_HDR_ENCODE(elm)    (((uint32_t)sizeof(struct elm) +        \
                                  DMSG_ALIGNMASK) /                     \
                                 DMSG_ALIGN)

#define DMSG_LNK(cmd, elm)      (DMSG_PROTO_LNK |                       \
                                         ((cmd) << 8) |                 \
                                         DMSG_HDR_ENCODE(elm))

#define DMSG_DBG(cmd, elm)      (DMSG_PROTO_DBG |                       \
                                         ((cmd) << 8) |                 \
                                         DMSG_HDR_ENCODE(elm))


#define DMSG_DBG_SHELL          DMSG_DBG(0x001, dmsg_dbg_shell)

#define DMSG_LNK_CONN           DMSG_LNK(0x011, dmsg_lnk_conn)
#define DMSG_LNK_VOLCONF        DMSG_LNK(0x020, dmsg_lnk_volconf)

struct dmsg_dbg_shell {
        dmsg_hdr_t      head;
};

struct dmsg_lnk_conn {
        dmsg_hdr_t      head;
        uuid_t          mediaid;        /* media configuration id */
        uuid_t          pfs_clid;       /* rendezvous pfs uuid */
        uuid_t          pfs_fsid;       /* unique pfs uuid */
        uint64_t        peer_mask;      /* PEER mask for SPAN filtering */
        uint8_t         peer_type;      /* see DMSG_PEER_xxx */
        uint8_t         pfs_type;       /* pfs type */
        uint16_t        proto_version;  /* high level protocol support */
        uint32_t        status;         /* status flags */
        uint32_t        rnss;           /* node's generated rnss */
        uint8_t         reserved02[8];
        uint32_t        reserved03[12];
        uint64_t        pfs_mask;       /* PFS mask for SPAN filtering */
        char            cl_label[128];  /* cluster label (for PEER_BLOCK) */
        char            fs_label[128];  /* PFS label (for PEER_HAMMER2) */
};

typedef struct dmsg_lnk_conn dmsg_lnk_conn_t;

#define DMSG_ERR_NOSUPP         0x20

struct dmsg_lnk_span {
        dmsg_hdr_t      head;
        uuid_t          pfs_clid;       /* rendezvous pfs uuid */
        uuid_t          pfs_fsid;       /* unique pfs id (differentiate node) */
        uint8_t         pfs_type;       /* PFS type */
        uint8_t         peer_type;      /* PEER type */
        uint16_t        proto_version;  /* high level protocol support */
        uint32_t        status;         /* status flags */
        uint8_t         reserved02[8];
        uint32_t        dist;           /* span distance */
        uint32_t        rnss;           /* random number sub-sort */
        union {
                uint32_t        reserved03[14];
                //dmsg_media_block_t block;
        } media;

        /*
         * NOTE: for PEER_HAMMER2 cl_label is typically empty and fs_label
         *       is the superroot directory name.
         *
         *       for PEER_BLOCK cl_label is typically host/device and
         *       fs_label is typically the serial number string.
         */
        char            cl_label[128];  /* cluster label */
        char            fs_label[128];  /* PFS label */
};

typedef struct dmsg_lnk_span dmsg_lnk_span_t;

#define DMSG_SPAN_PROTO_1       1

/*
* Structure embedded in e.g. mount, master control structure for
* DMSG stream handling.
*/
struct kdmsg_iocom {
        struct malloc_type      *mmsg;
        struct file             *msg_fp;        /* cluster pipe->userland */
        int                     msg_ctl;        /* wakeup flags */
        int                     msg_seq;        /* cluster msg sequence id */
        uint32_t                flags;
        struct lock             msglk;          /* lockmgr lock */
        TAILQ_HEAD(, kdmsg_msg) msgq;           /* transmit queue */
        void                    *handle;
        void                    (*exit_func)(struct kdmsg_iocom *);
        struct kdmsg_state      *conn_state;    /* active LNK_CONN state */
        struct kdmsg_state      *freerd_state;  /* allocation cache */
        struct kdmsg_state      *freewr_state;  /* allocation cache */
        dmsg_lnk_conn_t         auto_lnk_conn;
        dmsg_lnk_span_t         auto_lnk_span;
};

typedef struct kdmsg_iocom      kdmsg_iocom_t;

struct dmsg_lnk_volconf {
        dmsg_hdr_t              head;
        dmsg_vol_data_t         copy;   /* copy spec */
        int32_t                 index;
        int32_t                 unused01;
        uuid_t                  mediaid;
        int64_t                 reserved02[32];
};

typedef struct dmsg_lnk_volconf dmsg_lnk_volconf_t;

struct dmsg_lnk_circ {
        dmsg_hdr_t      head;
        uint64_t        reserved01;
        uint64_t        target;
};

typedef struct dmsg_lnk_circ dmsg_lnk_circ_t;
typedef struct dmsg_blk_open            dmsg_blk_open_t;
typedef struct dmsg_blk_error           dmsg_blk_error_t;

union dmsg_any {
        char                    buf[2048];
        dmsg_hdr_t              head;

        dmsg_lnk_conn_t         lnk_conn;
        dmsg_lnk_span_t         lnk_span;
        dmsg_lnk_circ_t         lnk_circ;
        dmsg_lnk_volconf_t      lnk_volconf;

        //dmsg_blk_open_t         blk_open;
        //dmsg_blk_error_t        blk_error;
        //dmsg_blk_read_t         blk_read;
        //dmsg_blk_write_t        blk_write;
        //dmsg_blk_flush_t        blk_flush;
        //dmsg_blk_freeblks_t     blk_freeblks;
};

typedef union dmsg_any dmsg_any_t;

struct kdmsg_msg {
        TAILQ_ENTRY(kdmsg_msg) qentry;          /* serialized queue */
        struct kdmsg_iocom *iocom;
        struct kdmsg_state *state;
        struct kdmsg_circuit *circ;
        size_t          hdr_size;
        size_t          aux_size;
        char            *aux_data;
        int             flags;
        dmsg_any_t      any;
};

#define KDMSG_FLAG_AUXALLOC     0x0001
#define KDMSG_IOCOMF_AUTOSPAN    0x0002  /* handle received LNK_SPAN */
#define KDMSG_IOCOMF_AUTOCONN   0x0001  /* handle received LNK_CONN */
#define KDMSG_IOCOMF_AUTOSPAN   0x0002  /* handle received LNK_SPAN */
#define KDMSG_IOCOMF_AUTOCIRC   0x0004  /* handle received LNK_CIRC */

typedef struct kdmsg_link kdmsg_link_t;
typedef struct kdmsg_state kdmsg_state_t;
typedef struct kdmsg_msg kdmsg_msg_t;

struct hammer2_inode;
struct hammer2_mount;
struct hammer2_pfsmount;
struct hammer2_span;
struct hammer2_state;
struct hammer2_msg;

/*
 * The chain structure tracks a portion of the media topology from the
 * root (volume) down.  Chains represent volumes, inodes, indirect blocks,
 * data blocks, and freemap nodes and leafs.
 *
 * The chain structure can be multi-homed and its topological recursion
 * (chain->core) can be shared amongst several chains.  Chain structures
 * are topologically stable once placed in the in-memory topology (they
 * don't move around).  Modifications which cross flush synchronization
 * boundaries, renames, resizing, or any move of the chain to elsewhere
 * in the topology is accomplished via the DELETE-DUPLICATE mechanism.
 *
 * Deletions and delete-duplicates:
 *
 *	Any movement of chains within the topology utilize a delete-duplicate
 *	operation instead of a simple rename.  That is, the chain must be
 *	deleted from its original location and then duplicated to the new
 *	location.  A new chain structure is allocated while the old is
 *	deleted.  Deleted chains are removed from the above chain_core's
 *	rbtree but remain linked via the shadow topology for flush
 *	synchronization purposes.
 *
 *	delete_bmap is allocated and a bit set if the chain was originally
 *	loaded via the blockmap.
 *
 * Flush synchronization:
 *
 *	Flushes must synchronize chains up through the root.  To do this
 *	the in-memory topology would normally have to be frozen during the
 *	flush.  To avoid freezing the topology and to allow concurrent
 *	foreground / flush activity, any new modifications made while a
 *	flush is in progress retains the original chain in a shadow topology
 *	that is only visible to the flush code.  Only one flush can be
 *	running at a time so the shadow hierarchy can be implemented with
 *	just a few link fields in our in-memory data structures.
 *
 * Advantages:
 *
 *	(1) Fully coherent snapshots can be taken without requiring
 *	    a pre-flush, resulting in extremely fast (sub-millisecond)
 *	    snapshots.
 *
 *	(2) Multiple synchronization points can be in-flight at the same
 *	    time, representing multiple snapshots or flushes.
 *
 *	(3) The algorithms needed to keep track of everything are actually
 *	    not that complex.
 *
 * Special Considerations:
 *
 *	A chain is ref-counted on a per-chain basis, but the chain's lock
 *	is associated with the shared chain_core and is not per-chain.
 *
 *	The power-of-2 nature of the media radix tree ensures that there
 *	will be no overlaps which straddle edges.
 */
RB_HEAD(hammer2_chain_tree, hammer2_chain);

#define CHAIN_CORE_DELETE_BMAP_ENTRIES	\
	(HAMMER2_PBUFSIZE / sizeof(hammer2_blockref_t) / sizeof(uint32_t))

#define HAMMER2_CORE_UNUSED0001		0x0001
#define HAMMER2_CORE_COUNTEDBREFS	0x0002


/*
 * Primary chain structure keeps track of the topology in-memory.
 */

//int hammer2_chain_cmp(hammer2_chain_t *chain1, hammer2_chain_t *chain2);
RB_PROTOTYPE(hammer2_chain_tree, hammer2_chain, rbnode, hammer2_chain_cmp);

/*
 * Special notes on flags:
 *
 * INITIAL - This flag allows a chain to be created and for storage to
 *	     be allocated without having to immediately instantiate the
 *	     related buffer.  The data is assumed to be all-zeros.  It
 *	     is primarily used for indirect blocks.
 *
 * MODIFIED- The chain's media data has been modified.
 */
#define HAMMER2_CHAIN_MODIFIED		0x00000001	/* dirty chain data */
#define HAMMER2_CHAIN_ALLOCATED		0x00000002	/* kmalloc'd chain */
#define HAMMER2_CHAIN_FLUSH_TEMPORARY	0x00000004
#define HAMMER2_CHAIN_FORCECOW		0x00000008	/* force copy-on-wr */
#define HAMMER2_CHAIN_DELETED		0x00000010	/* deleted chain */
#define HAMMER2_CHAIN_INITIAL		0x00000020	/* initial create */
#define HAMMER2_CHAIN_FLUSH_CREATE	0x00000040	/* needs flush blkadd */
#define HAMMER2_CHAIN_FLUSH_DELETE	0x00000080	/* needs flush blkdel */
#define HAMMER2_CHAIN_IOFLUSH		0x00000100	/* bawrite on put */
#define HAMMER2_CHAIN_DEFERRED		0x00000200	/* on a deferral list */
#define HAMMER2_CHAIN_UNLINKED		0x00000400	/* delete on reclaim */
#define HAMMER2_CHAIN_VOLUMESYNC	0x00000800	/* needs volume sync */
#define HAMMER2_CHAIN_ONDBQ		0x00001000	/* !bmapped deletes */
#define HAMMER2_CHAIN_MOUNTED		0x00002000	/* PFS is mounted */
#define HAMMER2_CHAIN_ONRBTREE		0x00004000	/* on parent RB tree */
#define HAMMER2_CHAIN_SNAPSHOT		0x00008000	/* snapshot special */
#define HAMMER2_CHAIN_EMBEDDED		0x00010000	/* embedded data */
#define HAMMER2_CHAIN_RELEASE		0x00020000	/* don't keep around */
#define HAMMER2_CHAIN_BMAPPED		0x00040000	/* in parent blkmap */
#define HAMMER2_CHAIN_ONDBTREE		0x00080000	/* bmapped deletes */
#define HAMMER2_CHAIN_DUPLICATED	0x00100000	/* fwd delete-dup */
#define HAMMER2_CHAIN_PFSROOT		0x00200000	/* in pfs->cluster */

/*
 * Flags passed to hammer2_chain_lookup() and hammer2_chain_next()
 *
 * NOTE: MATCHIND allows an indirect block / freemap node to be returned
 *	 when the passed key range matches the radix.  Remember that key_end
 *	 is inclusive (e.g. {0x000,0xFFF}, not {0x000,0x1000}).
 */
#define HAMMER2_LOOKUP_NOLOCK		0x00000001	/* ref only */
#define HAMMER2_LOOKUP_NODATA		0x00000002	/* data left NULL */
#define HAMMER2_LOOKUP_SHARED		0x00000100
#define HAMMER2_LOOKUP_MATCHIND		0x00000200	/* return all chains */
#define HAMMER2_LOOKUP_UNUSED0400	0x00000400
#define HAMMER2_LOOKUP_ALWAYS		0x00000800	/* resolve data */

/*
 * Flags passed to hammer2_chain_modify() and hammer2_chain_resize()
 *
 * NOTE: OPTDATA allows us to avoid instantiating buffers for INDIRECT
 *	 blocks in the INITIAL-create state.
 */
#define HAMMER2_MODIFY_OPTDATA		0x00000002	/* data can be NULL */
#define HAMMER2_MODIFY_NO_MODIFY_TID	0x00000004
#define HAMMER2_MODIFY_ASSERTNOCOPY	0x00000008	/* assert no del-dup */
#define HAMMER2_MODIFY_NOREALLOC	0x00000010
#define HAMMER2_MODIFY_INPLACE		0x00000020	/* don't del-dup */

/*
 * Flags passed to hammer2_chain_lock()
 */
#define HAMMER2_RESOLVE_NEVER		1
#define HAMMER2_RESOLVE_MAYBE		2
#define HAMMER2_RESOLVE_ALWAYS		3
#define HAMMER2_RESOLVE_MASK		0x0F

#define HAMMER2_RESOLVE_SHARED		0x10	/* request shared lock */
#define HAMMER2_RESOLVE_NOREF		0x20	/* already ref'd on lock */

/*
 * Flags passed to hammer2_chain_delete()
 */
#define HAMMER2_DELETE_UNUSED0001	0x0001

/*
 * Flags passed to hammer2_chain_delete_duplicate()
 */
#define HAMMER2_DELDUP_RECORE		0x0001

/*
 * Cluster different types of storage together for allocations
 */
#define HAMMER2_FREECACHE_INODE		0
#define HAMMER2_FREECACHE_INDIR		1
#define HAMMER2_FREECACHE_DATA		2
#define HAMMER2_FREECACHE_UNUSED3	3
#define HAMMER2_FREECACHE_TYPES		4

/*
 * hammer2_freemap_alloc() block preference
 */
#define HAMMER2_OFF_NOPREF		((hammer2_off_t)-1)

/*
 * BMAP read-ahead maximum parameters
 */
#define HAMMER2_BMAP_COUNT		16	/* max bmap read-ahead */
#define HAMMER2_BMAP_BYTES		(HAMMER2_PBUFSIZE * HAMMER2_BMAP_COUNT)

/*
 * Misc
 */
#define HAMMER2_FLUSH_DEPTH_LIMIT	10	/* stack recursion limit */

/*
 * hammer2_freemap_adjust()
 */
#define HAMMER2_FREEMAP_DORECOVER	1
#define HAMMER2_FREEMAP_DOMAYFREE	2
#define HAMMER2_FREEMAP_DOREALFREE	3

/*
 * HAMMER2 cluster - A set of chains representing the same entity.
 *
 * The hammer2_pfsmount structure embeds a hammer2_cluster.  All other
 * hammer2_cluster use cases use temporary allocations.
 *
 * The cluster API mimics the chain API.  Except as used in the pfsmount,
 * the cluster structure is a temporary 'working copy' of a set of chains
 * representing targets compatible with the operation.  However, for
 * performance reasons the cluster API does not necessarily issue concurrent
 * requests to the underlying chain API for all compatible chains all the
 * time.  This may sometimes necessitate revisiting parent cluster nodes
 * to 'flesh out' (validate more chains).
 *
 * If an insufficient number of chains remain in a working copy, the operation
 * may have to be downgraded, retried, or stall until the requisit number
 * of chains are available.
 */
#define HAMMER2_MAXCLUSTER	8


#define HAMMER2_CLUSTER_PFS	0x00000001	/* embedded in pfsmount */
#define HAMMER2_CLUSTER_INODE	0x00000002	/* embedded in inode */


//RB_HEAD(hammer2_inode_tree, hammer2_inode);

/*
 * A hammer2 inode.
 *
 * NOTE: The inode's attribute CST which is also used to lock the inode
 *	 is embedded in the chain (chain.cst) and aliased w/ attr_cst.
 */

#define HAMMER2_INODE_MODIFIED		0x0001
#define HAMMER2_INODE_SROOT		0x0002	/* kmalloc special case */
#define HAMMER2_INODE_RENAME_INPROG	0x0004
#define HAMMER2_INODE_ONRBTREE		0x0008
#define HAMMER2_INODE_RESIZED		0x0010
#define HAMMER2_INODE_MTIME		0x0020

#define RB_PROTOTYPE2(name, type, field, cmp, datatype)			
//RB_PROTOTYPE2(hammer2_inode_tree, hammer2_inode, rbnode, hammer2_inode_cmp,
//		hammer2_tid_t);

/*
 * A hammer2 transaction and flush sequencing structure.
 *
 * This global structure is tied into hammer2_mount and is used
 * to sequence modifying operations and flushes.
 *
 * (a) Any modifying operations with sync_tid >= flush_tid will stall until
 *     all modifying operating with sync_tid < flush_tid complete.
 *
 *     The flush related to flush_tid stalls until all modifying operations
 *     with sync_tid < flush_tid complete.
 *
 * (b) Once unstalled, modifying operations with sync_tid > flush_tid are
 *     allowed to run.  All modifications cause modify/duplicate operations
 *     to occur on the related chains.  Note that most INDIRECT blocks will
 *     be unaffected because the modifications just overload the RBTREE
 *     structurally instead of actually modifying the indirect blocks.
 *
 * (c) The actual flush unstalls and RUNS CONCURRENTLY with (b), but only
 *     utilizes the chain structures with sync_tid <= flush_tid.  The
 *     flush will modify related indirect blocks and inodes in-place
 *     (rather than duplicate) since the adjustments are compatible with
 *     (b)'s RBTREE overloading
 *
 *     SPECIAL NOTE:  Inode modifications have to also propagate along any
 *		      modify/duplicate chains.  File writes detect the flush
 *		      and force out the conflicting buffer cache buffer(s)
 *		      before reusing them.
 *
 * (d) Snapshots can be made instantly but must be flushed and disconnected
 *     from their duplicative source before they can be mounted.  This is
 *     because while H2's on-media structure supports forks, its in-memory
 *     structure only supports very simple forking for background flushing
 *     purposes.
 *
 * TODO: Flush merging.  When fsync() is called on multiple discrete files
 *	 concurrently there is no reason to stall the second fsync.
 *	 The final flush that reaches to root can cover both fsync()s.
 *
 *     The chains typically terminate as they fly onto the disk.  The flush
 *     ultimately reaches the volume header.
 */
struct hammer2_trans {
	TAILQ_ENTRY(hammer2_trans) entry;
	struct hammer2_pfsmount *pmp;		/* might be NULL */
	struct hammer2_mount	*hmp_single;	/* if single-targetted */
	hammer2_tid_t		orig_tid;
	hammer2_tid_t		sync_tid;	/* effective transaction id */
	hammer2_tid_t		inode_tid;
	struct 	proc		*td;		/* pointer */
	int			flags;
	int			blocked;
	uint8_t			inodes_created;
	uint8_t			dummy[7];
};

typedef struct hammer2_trans hammer2_trans_t;

#define HAMMER2_TRANS_ISFLUSH		0x0001	/* formal flush */
#define HAMMER2_TRANS_CONCURRENT	0x0002	/* concurrent w/flush */
#define HAMMER2_TRANS_BUFCACHE		0x0004	/* from bioq strategy write */
#define HAMMER2_TRANS_NEWINODE		0x0008	/* caller allocating inode */
#define HAMMER2_TRANS_ISALLOCATING	0x0010	/* in allocator */
#define HAMMER2_TRANS_PREFLUSH		0x0020	/* preflush state */

#define HAMMER2_FREEMAP_HEUR_NRADIX	4	/* pwr 2 PBUFRADIX-MINIORADIX */
#define HAMMER2_FREEMAP_HEUR_TYPES	8
#define HAMMER2_FREEMAP_HEUR		(HAMMER2_FREEMAP_HEUR_NRADIX * \
					 HAMMER2_FREEMAP_HEUR_TYPES)

#define HAMMER2_CLUSTER_COPY_CHAINS	0x0001	/* copy chains */
#define HAMMER2_CLUSTER_COPY_NOREF	0x0002	/* do not ref chains */

/*
 * Global (per device) mount structure for device (aka vp->v_mount->hmp)
 */
TAILQ_HEAD(hammer2_trans_queue, hammer2_trans);

/*
 * HAMMER2 PFS mount point structure (aka vp->v_mount->mnt_data).
 * This has a 1:1 correspondence to struct mount (note that the
 * hammer2_mount structure has a N:1 correspondence).
 *
 * This structure represents a cluster mount and not necessarily a
 * PFS under a specific device mount (HMP).  The distinction is important
 * because the elements backing a cluster mount can change on the fly.
 *
 * Usually the first element under the cluster represents the original
 * user-requested mount that bootstraps the whole mess.  In significant
 * setups the original is usually just a read-only media image (or
 * representitive file) that simply contains a bootstrap volume header
 * listing the configuration.
 */

/* struct bio_queue_head {
TAILQ_HEAD(bio_queue, bio) queue;
off_t last_offset;
struct  bio *insert_point;
}; */

struct mtx {
int     owned;
struct mtx *parent;
};

#define HAMMER2_DIRTYCHAIN_WAITING	0x80000000
#define HAMMER2_DIRTYCHAIN_MASK		0x7FFFFFFF

#define HAMMER2_LWINPROG_WAITING	0x80000000
#define HAMMER2_LWINPROG_MASK		0x7FFFFFFF

#if defined(_KERNEL)

//MALLOC_DECLARE(M_HAMMER2);

#define VTOI(vp)	((hammer2_inode_t *)(vp)->v_data)
#define ITOV(ip)	((ip)->vp)

/*
 * Currently locked chains retain the locked buffer cache buffer for
 * indirect blocks, and indirect blocks can be one of two sizes.  The
 * device buffer has to match the case to avoid deadlocking recursive
 * chains that might otherwise try to access different offsets within
 * the same device buffer.
 */
static __inline
int
hammer2_devblkradix(int radix)
{
	if (radix <= HAMMER2_LBUFRADIX) {
		return (HAMMER2_LBUFRADIX);
	} else {
		return (HAMMER2_PBUFRADIX);
	}
}

extern struct vops hammer2_vnode_vops;
extern struct vops hammer2_spec_vops;
extern struct vops hammer2_fifo_vops;

extern int hammer2_debug;
extern int hammer2_cluster_enable;
extern int hammer2_hardlink_enable;
extern int hammer2_flush_pipe;
extern int hammer2_synchronous_flush;
extern long hammer2_limit_dirty_chains;
extern long hammer2_iod_file_read;
extern long hammer2_iod_meta_read;
extern long hammer2_iod_indr_read;
extern long hammer2_iod_fmap_read;
extern long hammer2_iod_volu_read;
extern long hammer2_iod_file_write;
extern long hammer2_iod_meta_write;
extern long hammer2_iod_indr_write;
extern long hammer2_iod_fmap_write;
extern long hammer2_iod_volu_write;
extern long hammer2_ioa_file_read;
extern long hammer2_ioa_meta_read;
extern long hammer2_ioa_indr_read;
extern long hammer2_ioa_fmap_read;
extern long hammer2_ioa_volu_read;
extern long hammer2_ioa_file_write;
extern long hammer2_ioa_meta_write;
extern long hammer2_ioa_indr_write;
extern long hammer2_ioa_fmap_write;
extern long hammer2_ioa_volu_write;

extern struct objcache *cache_buffer_read;
extern struct objcache *cache_buffer_write;

extern int destroy;
extern int write_thread_wakeup;

//  XX extern mtx_t thread_protect;

/*
 * hammer2_msgops.c
 */
int hammer2_msg_dbg_rcvmsg(kdmsg_msg_t *msg);
int hammer2_msg_adhoc_input(kdmsg_msg_t *msg);

#endif /* !_KERNEL */
#endif /* !_VFS_HAMMER2_HAMMER2_H_ */
