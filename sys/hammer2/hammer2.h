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
 * Almost everything is represented by a hammer2_chain structure in-memory.
 * Other higher-level structures typically map to chains.
 *
 * A great deal of data is accessed simply via its buffer cache buffer,
 * which is mapped for the duration of the chain's lock.  Hammer2 must
 * implement its own buffer cache layer on top of the system layer to
 * allow for different threads to lock different sub-block-sized buffers.
 *
 * When modifications are made to a chain a new filesystem block must be
 * allocated.  Multiple modifications do not typically allocate new blocks
 * until the current block has been flushed.  Flushes do not block the
 * front-end unless the front-end operation crosses the current inode being
 * flushed.
 *
 * The in-memory representation may remain cached (for example in order to
 * placemark clustering locks) even after the related data has been
 * detached.
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
#include <sys/mplock.h>

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/errno.h>

#include <machine/atomic.h>
#include <machine/i8259.h>
#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/cpufunc.h>
#include <machine/intr.h>
//#include <sys/namecache.h>
#include <sys/objcache.h>
#include <dev/pci/drm/drm_atomic.h>

#include <sys/dmsg.h>

#include "hammer2_disk.h"
#include "hammer2_mount.h"
#include "hammer2_ioctl.h"
#include "hammer2_ccms.h"

struct hammer2_chain;
struct hammer2_cluster;
struct hammer2_inode;
struct hammer2_mount;
struct hammer2_pfsmount;
struct hammer2_span;
struct hammer2_state;
struct hammer2_msg;

/*
 * The xid tracks internal transactional updates.
 *
 * XXX fix-me, really needs to be 64-bits
 */
typedef uint32_t hammer2_xid_t;

#define HAMMER2_XID_MIN	0x00000000U
#define HAMMER2_XID_MAX 0x7FFFFFFFU

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
//#define RB_GENERATE2(name, type, field, cmp, datatype, indexfield)

#define RB_LOOKUP(name, root, value)     name##_RB_LOOKUP(root, value)
//#define hammer2_io_tree_RB_LOOKUP(name, root, value, value2)     name##_RB_LOOKUP(root, value, value2)
#define RB_SCAN(name, root, cmp, callback, data)  
#define RB_EMPTY(head)                  (RB_ROOT(head) == NULL)
#define RB_PROTOTYPE2(name, type, field, cmp, datatype) RB_PROTOTYPE(name, type, field, cmp); \
struct type *name##_RB_LOOKUP(struct name *, datatype) \

/*
 * Macros that define a red-black tree
 */

#define RB_SCAN_INFO(name, type)					\
struct name##_scan_info {						\
	struct name##_scan_info *link;					\
	struct type	*node;						\
}

#define _RB_PROTOTYPE(name, type, field, cmp, STORQUAL)			\
STORQUAL void name##_RB_INSERT_COLOR(struct name *, struct type *);	\
STORQUAL void name##_RB_REMOVE_COLOR(struct name *, struct type *, struct type *);\
STORQUAL struct type *name##_RB_REMOVE(struct name *, struct type *);	\
STORQUAL struct type *name##_RB_INSERT(struct name *, struct type *);	\
STORQUAL struct type *name##_RB_FIND(struct name *, struct type *);	\
STORQUAL int name##_RB_SCAN(struct name *, int (*)(struct type *, void *),\
			int (*)(struct type *, void *), void *);	\
STORQUAL struct type *name##_RB_NEXT(struct type *);			\
STORQUAL struct type *name##_RB_PREV(struct type *);			\
STORQUAL struct type *name##_RB_MINMAX(struct name *, int);		\
RB_SCAN_INFO(name, type)						\

/*
 * This extended version implements a fast LOOKUP function given
 * a numeric data type.
 *
 * The element whos index/offset field is exactly the specified value
 * will be returned, or NULL.
 */
#define RB_GENERATE2(name, type, field, cmp, datatype, indexfield)	\
RB_GENERATE(name, type, field, cmp)					\
									\
struct type *								\
name##_RB_LOOKUP(struct name *head, datatype value)			\
{									\
	struct type *tmp;						\
									\
	tmp = RB_ROOT(head);						\
	while (tmp) {							\
		if (value > tmp->indexfield) 				\
			tmp = RB_RIGHT(tmp, field);			\
		else if (value < tmp->indexfield) 			\
			tmp = RB_LEFT(tmp, field);			\
		else 							\
			return(tmp);					\
	}								\
	return(NULL);							\
}									\

/* #define RB_SCAN(name, root, cmp, callback, data) 			\
				name##_RB_SCAN(root, cmp, callback, data)
*/


 
typedef struct globaldata *globaldata_t;

#define VOP_FSYNC(vp, waitfor, flags)                   \
	vop_fsync(*(vp)->v_op, vp, waitfor, flags)
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


uint32_t iscsi_crc32(const void *, size_t);
int vop_helper_chmod(struct vnode *, mode_t, struct ucred *, uid_t, gid_t, mode_t *);
int vop_helper_setattr_flags(u_int32_t *, u_int32_t,uid_t, struct ucred *);
int vop_helper_chown(struct vnode *vp, uid_t new_uid, gid_t new_gid,
                 struct ucred *cred,
                 uid_t *cur_uidp, gid_t *cur_gidp, mode_t *cur_modep);
void bheavy(struct buf *bp);

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
/*
struct vop_nlink_args {
	struct vop_generic_args a_head;
	struct nchandle *a_nch;
	struct vnode *a_dvp;
	struct vnode *a_vp;
	struct ucred *a_cred;
};
*/

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
        //struct buf_rb_hash v_rbhash_tree;       /* RB tree general lookup */
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
        struct nchandle *nl_nch;         /* start-point and result */
        //struct nchandle nl_rootnch;     /* root directory */
        //struct nchandle nl_jailnch;     /* jail directory */

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

/*
 * The chain structure tracks a portion of the media topology from the
 * root (volume) down.  Chains represent volumes, inodes, indirect blocks,
 * data blocks, and freemap nodes and leafs.
 *
 * The chain structure utilizes a simple singly-homed topology and the
 * chain's in-memory topology will move around as the chains do, due mainly
 * to renames and indirect block creation.
 *
 * Block Table Updates
 *
 *	Block table updates for insertions and updates are delayed until the
 *	flush.  This allows us to avoid having to modify the parent chain
 *	all the way to the root.
 *
 *	Block table deletions are performed immediately (modifying the parent
 *	in the process) because the flush code uses the chain structure to
 *	track delayed updates and the chain will be (likely) gone or moved to
 *	another location in the topology after a deletion.
 *
 *	A prior iteration of the code tried to keep the relationship intact
 *	on deletes by doing a delete-duplicate operation on the chain, but
 *	it added way too much complexity to the codebase.
 *
 * Flush Synchronization
 *
 *	The flush code must flush modified chains bottom-up.  Because chain
 *	structures can shift around and are NOT topologically stable,
 *	modified chains are independently indexed for the flush.  As the flush
 *	runs it modifies (or further modifies) and updates the parents,
 *	propagating the flush all the way to the volume root.
 *
 *	Modifying front-end operations can occur during a flush but will block
 *	in two cases: (1) when the front-end tries to operate on the inode
 *	currently in the midst of being flushed and (2) if the front-end
 *	crosses an inode currently being flushed (such as during a rename).
 *	So, for example, if you rename directory "x" to "a/b/c/d/e/f/g/x" and
 *	the flusher is currently working on "a/b/c", the rename will block
 *	temporarily in order to ensure that "x" exists in one place or the
 *	other.
 *
 *	Meta-data statistics are updated by the flusher.  The front-end will
 *	make estimates but meta-data must be fully synchronized only during a
 *	flush in order to ensure that it remains correct across a crash.
 *
 *	Multiple flush synchronizations can theoretically be in-flight at the
 *	same time but the implementation is not coded to handle the case and
 *	currently serializes them.
 *
 * Snapshots:
 *
 *	Snapshots currently require the subdirectory tree being snapshotted
 *	to be flushed.  The snapshot then creates a new super-root inode which
 *	copies the flushed blockdata of the directory or file that was
 *	snapshotted.
 *
 * RBTREE NOTES:
 *
 *	- Note that the radix tree runs in powers of 2 only so sub-trees
 *	  cannot straddle edges.
 */
RB_HEAD(hammer2_chain_tree, hammer2_chain);
TAILQ_HEAD(h2_flush_list, hammer2_chain);

#define CHAIN_CORE_DELETE_BMAP_ENTRIES	\
	(HAMMER2_PBUFSIZE / sizeof(hammer2_blockref_t) / sizeof(uint32_t))

struct hammer2_chain_core {
	struct ccms_cst	cst;
	struct hammer2_chain_tree rbtree; /* sub-chains */
	int		live_zero;	/* blockref array opt */
	u_int		flags;
	u_int		live_count;	/* live (not deleted) chains in tree */
	u_int		chain_count;	/* live + deleted chains under core */
	int		generation;	/* generation number (inserts only) */
};

typedef struct hammer2_chain_core hammer2_chain_core_t;

#define HAMMER2_CORE_UNUSED0001		0x0001
#define HAMMER2_CORE_COUNTEDBREFS	0x0002

/*
 * H2 is a copy-on-write filesystem.  In order to allow chains to allocate
 * smaller blocks (down to 64-bytes), but improve performance and make
 * clustered I/O possible using larger block sizes, the kernel buffer cache
 * is abstracted via the hammer2_io structure.
 */
RB_HEAD(hammer2_io_tree, hammer2_io);

struct hammer2_io {
	RB_ENTRY(hammer2_io) rbnode;	/* indexed by device offset */
	struct _atomic_lock *spin;
	struct hammer2_mount *hmp;
	struct buf	*bp;
	struct bio	*bio;
	off_t		pbase;
	int		psize;
	void		(*callback)(struct hammer2_io *dio,
				    struct hammer2_cluster *cluster,
				    struct hammer2_chain *chain,
				    void *arg1, off_t arg2);
	struct hammer2_cluster *arg_l;		/* INPROG I/O only */
	struct hammer2_chain *arg_c;		/* INPROG I/O only */
	void		*arg_p;			/* INPROG I/O only */
	off_t		arg_o;			/* INPROG I/O only */
	int		refs;
	int		act;			/* activity */
};

typedef struct hammer2_io hammer2_io_t;

/*
 * Primary chain structure keeps track of the topology in-memory.
 */
struct hammer2_chain {
	hammer2_chain_core_t	core;
	RB_ENTRY(hammer2_chain) rbnode;		/* live chain(s) */
	hammer2_blockref_t	bref;
	struct hammer2_chain	*parent;
	struct hammer2_state	*state;		/* if active cache msg */
	struct hammer2_mount	*hmp;
	struct hammer2_pfsmount	*pmp;		/* (pfs-cluster pmp or spmp) */

	hammer2_xid_t	flush_xid;		/* flush sequencing */
	hammer2_key_t   data_count;		/* delta's to apply */
	hammer2_key_t   inode_count;		/* delta's to apply */
	hammer2_key_t   data_count_up;		/* delta's to apply */
	hammer2_key_t   inode_count_up;		/* delta's to apply */
	hammer2_io_t	*dio;			/* physical data buffer */
	u_int		bytes;			/* physical data size */
	u_int		flags;
	u_int		refs;
	u_int		lockcnt;
	hammer2_media_data_t *data;		/* data pointer shortcut */
	TAILQ_ENTRY(hammer2_chain) flush_node;	/* flush list */
};

typedef struct hammer2_chain hammer2_chain_t;

int hammer2_chain_cmp(hammer2_chain_t *chain1, hammer2_chain_t *chain2);
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
 * UPDATE  - Chain might not be modified but parent blocktable needs update
 *
 * BMAPPED - Indicates that the chain is present in the parent blockmap.
 * BMAPUPD - Indicates that the chain is present but needs to be updated
 *	     in the parent blockmap.
 */
#define HAMMER2_CHAIN_MODIFIED		0x00000001	/* dirty chain data */
#define HAMMER2_CHAIN_ALLOCATED		0x00000002	/* kmalloc'd chain */
#define HAMMER2_CHAIN_DESTROY		0x00000004
#define HAMMER2_CHAIN_UNUSED00000008	0x00000008
#define HAMMER2_CHAIN_DELETED		0x00000010	/* deleted chain */
#define HAMMER2_CHAIN_INITIAL		0x00000020	/* initial create */
#define HAMMER2_CHAIN_UPDATE		0x00000040	/* need parent update */
#define HAMMER2_CHAIN_DEFERRED		0x00000080	/* flush depth defer */
#define HAMMER2_CHAIN_IOFLUSH		0x00000100	/* bawrite on put */
#define HAMMER2_CHAIN_ONFLUSH		0x00000200	/* on a flush list */
#define HAMMER2_CHAIN_UNUSED00000400	0x00000400
#define HAMMER2_CHAIN_VOLUMESYNC	0x00000800	/* needs volume sync */
#define HAMMER2_CHAIN_UNUSED00001000	0x00001000
#define HAMMER2_CHAIN_MOUNTED		0x00002000	/* PFS is mounted */
#define HAMMER2_CHAIN_ONRBTREE		0x00004000	/* on parent RB tree */
#define HAMMER2_CHAIN_SNAPSHOT		0x00008000	/* snapshot special */
#define HAMMER2_CHAIN_EMBEDDED		0x00010000	/* embedded data */
#define HAMMER2_CHAIN_RELEASE		0x00020000	/* don't keep around */
#define HAMMER2_CHAIN_BMAPPED		0x00040000	/* present in blkmap */
#define HAMMER2_CHAIN_BMAPUPD		0x00080000	/* +needs updating */
#define HAMMER2_CHAIN_UNUSED00100000	0x00100000
#define HAMMER2_CHAIN_UNUSED00200000	0x00200000
#define HAMMER2_CHAIN_PFSBOUNDARY	0x00400000	/* super->pfs inode */

#define HAMMER2_CHAIN_FLUSH_MASK	(HAMMER2_CHAIN_MODIFIED |	\
					 HAMMER2_CHAIN_UPDATE |		\
					 HAMMER2_CHAIN_ONFLUSH)

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
#define HAMMER2_MODIFY_UNUSED0008	0x00000008
#define HAMMER2_MODIFY_NOREALLOC	0x00000010

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
#define HAMMER2_DELETE_PERMANENT	0x0001
#define HAMMER2_DELETE_NOSTATS		0x0002

#define HAMMER2_INSERT_NOSTATS		0x0002

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

struct hammer2_cluster {
	int			status;		/* operational status */
	int			refs;		/* track for deallocation */
	struct hammer2_pfsmount	*pmp;
	uint32_t		flags;
	int			nchains;
	hammer2_chain_t		*focus;		/* current focus (or mod) */
	hammer2_chain_t		*array[HAMMER2_MAXCLUSTER];
	char			missed[HAMMER2_MAXCLUSTER];
	int			cache_index[HAMMER2_MAXCLUSTER];
};

typedef struct hammer2_cluster hammer2_cluster_t;

#define HAMMER2_CLUSTER_INODE	0x00000001	/* embedded in inode */
#define HAMMER2_CLUSTER_NOSYNC	0x00000002	/* not in sync (cumulative) */


RB_HEAD(hammer2_inode_tree, hammer2_inode);

/*
 * A hammer2 inode.
 *
 * NOTE: The inode's attribute CST which is also used to lock the inode
 *	 is embedded in the chain (chain.cst) and aliased w/ attr_cst.
 */
struct hammer2_inode {
	RB_ENTRY(hammer2_inode) rbnode;		/* inumber lookup (HL) */
	ccms_cst_t		topo_cst;	/* directory topology cst */
	struct hammer2_pfsmount	*pmp;		/* PFS mount */
	struct hammer2_inode	*pip;		/* parent inode */
	struct vnode		*vp;
	hammer2_cluster_t	cluster;
	struct lockf		advlock;
	hammer2_tid_t		inum;
	u_int			flags;
	u_int			refs;		/* +vpref, +flushref */
	uint8_t			comp_heuristic;
	hammer2_off_t		size;
	uint64_t		mtime;
};

typedef struct hammer2_inode hammer2_inode_t;

#define HAMMER2_INODE_MODIFIED		0x0001
#define HAMMER2_INODE_SROOT		0x0002	/* kmalloc special case */
#define HAMMER2_INODE_RENAME_INPROG	0x0004
#define HAMMER2_INODE_ONRBTREE		0x0008
#define HAMMER2_INODE_RESIZED		0x0010
#define HAMMER2_INODE_MTIME		0x0020
#define HAMMER2_INODE_UNLINKED		0x0040

int hammer2_inode_cmp(hammer2_inode_t *ip1, hammer2_inode_t *ip2);

/*
* A version which supplies a fast lookup routine for an exact match
* on a numeric field.
*/

RB_PROTOTYPE2(hammer2_inode_tree, hammer2_inode, rbnode, hammer2_inode_cmp,
		hammer2_tid_t);

/*
 * inode-unlink side-structure
 */
struct hammer2_inode_unlink {
	TAILQ_ENTRY(hammer2_inode_unlink) entry;
	hammer2_inode_t	*ip;
};
TAILQ_HEAD(h2_unlk_list, hammer2_inode_unlink);

typedef struct hammer2_inode_unlink hammer2_inode_unlink_t;

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
	struct hammer2_pfsmount *pmp;
	hammer2_xid_t		sync_xid;
	hammer2_tid_t		inode_tid;	/* inode number assignment */
	thread_t		td;		/* pointer */
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
#define HAMMER2_TRANS_FREEBATCH		0x0010	/* batch freeing code */
#define HAMMER2_TRANS_PREFLUSH		0x0020	/* preflush state */

#define HAMMER2_FREEMAP_HEUR_NRADIX	4	/* pwr 2 PBUFRADIX-MINIORADIX */
#define HAMMER2_FREEMAP_HEUR_TYPES	8
#define HAMMER2_FREEMAP_HEUR		(HAMMER2_FREEMAP_HEUR_NRADIX * \
					 HAMMER2_FREEMAP_HEUR_TYPES)

#define HAMMER2_CLUSTER_COPY_NOCHAINS	0x0001	/* do not copy or ref chains */
#define HAMMER2_CLUSTER_COPY_NOREF	0x0002	/* do not ref chains or cl */

/*
 * Transaction Rendezvous
 */
TAILQ_HEAD(hammer2_trans_queue, hammer2_trans);

struct hammer2_trans_manage {
	hammer2_xid_t		flush_xid;	/* last flush transaction */
	hammer2_xid_t		alloc_xid;
	struct lock		translk;	/* lockmgr lock */
	struct hammer2_trans_queue transq;	/* modifying transactions */
	int			flushcnt;	/* track flush trans */
};

typedef struct hammer2_trans_manage hammer2_trans_manage_t;

/*
 * Global (per device) mount structure for device (aka vp->v_mount->hmp)
 */
struct hammer2_mount {
	struct vnode	*devvp;		/* device vnode */
	int		ronly;		/* read-only mount */
	int		pmp_count;	/* PFS mounts backed by us */
	TAILQ_ENTRY(hammer2_mount) mntentry; /* hammer2_mntlist */

	struct malloc_type *mchain;
	int		nipstacks;
	int		maxipstacks;
	kdmsg_iocom_t	iocom;		/* volume-level dmsg interface */
	struct _atomic_lock *io_spin;	/* iotree access */
	struct hammer2_io_tree iotree;
	int		iofree_count;
	hammer2_chain_t vchain;		/* anchor chain (topology) */
	hammer2_chain_t fchain;		/* anchor chain (freemap) */
	struct _atomic_lock *list_spin;
	struct h2_flush_list	flushq;	/* flush seeds */
	struct hammer2_pfsmount *spmp;	/* super-root pmp for transactions */
	struct lock	vollk;		/* lockmgr lock */
	hammer2_off_t	heur_freemap[HAMMER2_FREEMAP_HEUR];
	int		volhdrno;	/* last volhdrno written */
	hammer2_volume_data_t voldata;
	hammer2_volume_data_t volsync;	/* synchronized voldata */
};

typedef struct hammer2_mount hammer2_mount_t;

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
struct hammer2_pfsmount {
	struct mount		*mp;
	TAILQ_ENTRY(hammer2_pfsmount) mntentry; /* hammer2_pfslist */
	uuid_t			pfs_clid;
	uuid_t			pfs_fsid;
	hammer2_mount_t		*spmp_hmp;	/* (spmp only) */
	hammer2_inode_t		*iroot;		/* PFS root inode */
	hammer2_inode_t		*ihidden;	/* PFS hidden directory */
	struct lock		lock;		/* PFS lock for certain ops */
	hammer2_off_t		inode_count;	/* copy of inode_count */
	ccms_domain_t		ccms_dom;
	struct netexport	export;		/* nfs export */
	int			ronly;		/* read-only mount */
	struct malloc_type	*minode;
	struct malloc_type	*mmsg;
	struct i_atomic_lock    *inum_spin;	/* inumber lookup */
	struct hammer2_inode_tree inum_tree;	/* (not applicable to spmp) */
	hammer2_tid_t		alloc_tid;
	hammer2_tid_t		flush_tid;
	hammer2_tid_t		inode_tid;
	long			inmem_inodes;
	uint32_t		inmem_dirty_chains;
	int			count_lwinprog;	/* logical write in prog */
	struct i_atomic_lock *list_spin;
	struct h2_unlk_list	unlinkq;	/* last-close unlink */
	thread_t		wthread_td;	/* write thread td */
	struct bio_queue_head	wthread_bioq;	/* logical buffer bioq */
	struct mutex		*wthread_mtx;	/* interlock */
	int			wthread_destroy;/* termination sequencing */
};

typedef struct hammer2_pfsmount hammer2_pfsmount_t;

#define HAMMER2_DIRTYCHAIN_WAITING	0x80000000
#define HAMMER2_DIRTYCHAIN_MASK		0x7FFFFFFF

#define HAMMER2_LWINPROG_WAITING	0x80000000
#define HAMMER2_LWINPROG_MASK		0x7FFFFFFF

#if defined(_KERNEL)

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

static __inline
size_t
hammer2_devblksize(size_t bytes)
{
	if (bytes <= HAMMER2_LBUFSIZE) {
		return(HAMMER2_LBUFSIZE);
	} else {
		KKASSERT(bytes <= HAMMER2_PBUFSIZE &&
			 (bytes ^ (bytes - 1)) == ((bytes << 1) - 1));
		return (HAMMER2_PBUFSIZE);
	}
}

static __inline
hammer2_pfsmount_t *
MPTOPMP(struct mount *mp)
{
	return ((hammer2_pfsmount_t *)mp->mnt_data);
}

//struct thread *curthread;

#define LOCKSTART	int __nlocks = curthread->td_locks
#define LOCKENTER	(++curthread->td_locks)
#define LOCKEXIT	(--curthread->td_locks)
#define LOCKSTOP	KKASSERT(curthread->td_locks == __nlocks)

extern int hammer2_debug;
extern int hammer2_cluster_enable;
extern int hammer2_hardlink_enable;
extern int hammer2_flush_pipe;
extern int hammer2_synchronous_flush;
extern int hammer2_dio_count;
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

extern struct vnode *cache_buffer_read;
extern struct objcache *cache_buffer_write;

extern int destroy;
extern int write_thread_wakeup;

/*
 * hammer2_subr.c
 */
#define hammer2_icrc32(buf, size)	iscsi_crc32((buf), (size))
#define hammer2_icrc32c(buf, size, crc)	iscsi_crc32_ext((buf), (size), (crc))

hammer2_cluster_t *hammer2_inode_lock_ex(hammer2_inode_t *ip);
hammer2_cluster_t *hammer2_inode_lock_sh(hammer2_inode_t *ip);
void hammer2_inode_unlock_ex(hammer2_inode_t *ip, hammer2_cluster_t *chain);
void hammer2_inode_unlock_sh(hammer2_inode_t *ip, hammer2_cluster_t *chain);
ccms_state_t hammer2_inode_lock_temp_release(hammer2_inode_t *ip);
void hammer2_inode_lock_temp_restore(hammer2_inode_t *ip, ccms_state_t ostate);
ccms_state_t hammer2_inode_lock_upgrade(hammer2_inode_t *ip);
void hammer2_inode_lock_downgrade(hammer2_inode_t *ip, ccms_state_t ostate);

void hammer2_mount_exlock(hammer2_mount_t *hmp);
void hammer2_mount_shlock(hammer2_mount_t *hmp);
void hammer2_mount_unlock(hammer2_mount_t *hmp);

int hammer2_get_dtype(const hammer2_inode_data_t *ipdata);
int hammer2_get_vtype(const hammer2_inode_data_t *ipdata);
u_int8_t hammer2_get_obj_type(enum vtype vtype);
void hammer2_time_to_timespec(u_int64_t xtime, struct timespec *ts);
u_int64_t hammer2_timespec_to_time(const struct timespec *ts);
u_int32_t hammer2_to_unix_xid(const uuid_t *uuid);
void hammer2_guid_to_uuid(uuid_t *uuid, u_int32_t guid);
hammer2_xid_t hammer2_trans_newxid(hammer2_pfsmount_t *pmp);
void hammer2_trans_manage_init(void);

hammer2_key_t hammer2_dirhash(const unsigned char *name, size_t len);
int hammer2_getradix(size_t bytes);

int hammer2_calc_logical(hammer2_inode_t *ip, hammer2_off_t uoff,
			hammer2_key_t *lbasep, hammer2_key_t *leofp);
int hammer2_calc_physical(hammer2_inode_t *ip,
			const hammer2_inode_data_t *ipdata,
			hammer2_key_t lbase);
void hammer2_update_time(uint64_t *timep);
void hammer2_adjreadcounter(hammer2_blockref_t *bref, size_t bytes);

/*
 * hammer2_inode.c
 */
struct vnode *hammer2_igetv(hammer2_inode_t *ip, hammer2_cluster_t *cparent,
			int *errorp);
void hammer2_inode_lock_nlinks(hammer2_inode_t *ip);
void hammer2_inode_unlock_nlinks(hammer2_inode_t *ip);
hammer2_inode_t *hammer2_inode_lookup(hammer2_pfsmount_t *pmp,
			hammer2_tid_t inum);
hammer2_inode_t *hammer2_inode_get(hammer2_pfsmount_t *pmp,
			hammer2_inode_t *dip, hammer2_cluster_t *cluster);
void hammer2_inode_free(hammer2_inode_t *ip);
void hammer2_inode_ref(hammer2_inode_t *ip);
void hammer2_inode_drop(hammer2_inode_t *ip);
void hammer2_inode_repoint(hammer2_inode_t *ip, hammer2_inode_t *pip,
			hammer2_cluster_t *cluster);
void hammer2_run_unlinkq(hammer2_trans_t *trans, hammer2_pfsmount_t *pmp);

hammer2_inode_t *hammer2_inode_create(hammer2_trans_t *trans,
			hammer2_inode_t *dip,
			struct vattr *vap, struct ucred *cred,
			const uint8_t *name, size_t name_len,
			hammer2_cluster_t **clusterp, int *errorp);
int hammer2_inode_connect(hammer2_trans_t *trans,
			hammer2_cluster_t **clusterp, int hlink,
			hammer2_inode_t *dip, hammer2_cluster_t *dcluster,
			const uint8_t *name, size_t name_len,
			hammer2_key_t key);
hammer2_inode_t *hammer2_inode_common_parent(hammer2_inode_t *fdip,
			hammer2_inode_t *tdip);
void hammer2_inode_fsync(hammer2_trans_t *trans, hammer2_inode_t *ip,
			hammer2_cluster_t *cparent);
int hammer2_unlink_file(hammer2_trans_t *trans, hammer2_inode_t *dip,
			const uint8_t *name, size_t name_len, int isdir,
			int *hlinkp, struct nchandle *nch, int nlinks);
int hammer2_hardlink_consolidate(hammer2_trans_t *trans,
			hammer2_inode_t *ip, hammer2_cluster_t **clusterp,
			hammer2_inode_t *cdip, hammer2_cluster_t *cdcluster,
			int nlinks);
int hammer2_hardlink_deconsolidate(hammer2_trans_t *trans, hammer2_inode_t *dip,
			hammer2_chain_t **chainp, hammer2_chain_t **ochainp);
int hammer2_hardlink_find(hammer2_inode_t *dip, hammer2_cluster_t **cparentp,
			hammer2_cluster_t *cluster);
int hammer2_parent_find(hammer2_cluster_t **cparentp,
			hammer2_cluster_t *cluster);
void hammer2_inode_install_hidden(hammer2_pfsmount_t *pmp);

/*
 * hammer2_chain.c
 */
void hammer2_voldata_lock(hammer2_mount_t *hmp);
void hammer2_voldata_unlock(hammer2_mount_t *hmp);
void hammer2_voldata_modify(hammer2_mount_t *hmp);
hammer2_chain_t *hammer2_chain_alloc(hammer2_mount_t *hmp,
				hammer2_pfsmount_t *pmp,
				hammer2_trans_t *trans,
				hammer2_blockref_t *bref);
void hammer2_chain_core_alloc(hammer2_trans_t *trans, hammer2_chain_t *chain);
void hammer2_chain_ref(hammer2_chain_t *chain);
void hammer2_chain_drop(hammer2_chain_t *chain);
int hammer2_chain_lock(hammer2_chain_t *chain, int how);
void hammer2_chain_load_async(hammer2_cluster_t *cluster,
				void (*func)(hammer2_io_t *dio,
					     hammer2_cluster_t *cluster,
					     hammer2_chain_t *chain,
					     void *arg_p, off_t arg_o),
				void *arg_p);
void hammer2_chain_moved(hammer2_chain_t *chain);
void hammer2_chain_modify(hammer2_trans_t *trans,
				hammer2_chain_t *chain, int flags);
void hammer2_chain_resize(hammer2_trans_t *trans, hammer2_inode_t *ip,
				hammer2_chain_t *parent,
				hammer2_chain_t *chain,
				int nradix, int flags);
void hammer2_chain_unlock(hammer2_chain_t *chain);
void hammer2_chain_wait(hammer2_chain_t *chain);
hammer2_chain_t *hammer2_chain_get(hammer2_chain_t *parent, int generation,
				hammer2_blockref_t *bref);
hammer2_chain_t *hammer2_chain_lookup_init(hammer2_chain_t *parent, int flags);
void hammer2_chain_lookup_done(hammer2_chain_t *parent);
hammer2_chain_t *hammer2_chain_lookup(hammer2_chain_t **parentp,
				hammer2_key_t *key_nextp,
				hammer2_key_t key_beg, hammer2_key_t key_end,
				int *cache_indexp, int flags, int *ddflagp);
hammer2_chain_t *hammer2_chain_next(hammer2_chain_t **parentp,
				hammer2_chain_t *chain,
				hammer2_key_t *key_nextp,
				hammer2_key_t key_beg, hammer2_key_t key_end,
				int *cache_indexp, int flags);
hammer2_chain_t *hammer2_chain_scan(hammer2_chain_t *parent,
				hammer2_chain_t *chain,
				int *cache_indexp, int flags);

int hammer2_chain_create(hammer2_trans_t *trans, hammer2_chain_t **parentp,
				hammer2_chain_t **chainp,
				hammer2_pfsmount_t *pmp,
				hammer2_key_t key, int keybits,
				int type, size_t bytes, int flags);
void hammer2_chain_rename(hammer2_trans_t *trans, hammer2_blockref_t *bref,
				hammer2_chain_t **parentp,
				hammer2_chain_t *chain, int flags);
int hammer2_chain_snapshot(hammer2_trans_t *trans, hammer2_chain_t **chainp,
				hammer2_ioc_pfs_t *pfs);
void hammer2_chain_delete(hammer2_trans_t *trans, hammer2_chain_t *parent,
				hammer2_chain_t *chain, int flags);
void hammer2_chain_delete_duplicate(hammer2_trans_t *trans,
				hammer2_chain_t **chainp, int flags);
void hammer2_flush(hammer2_trans_t *trans, hammer2_chain_t *chain);
void hammer2_chain_commit(hammer2_trans_t *trans, hammer2_chain_t *chain);
void hammer2_chain_setflush(hammer2_trans_t *trans, hammer2_chain_t *chain);
void hammer2_chain_countbrefs(hammer2_chain_t *chain,
				hammer2_blockref_t *base, int count);

void hammer2_chain_setcheck(hammer2_chain_t *chain, void *bdata);
int hammer2_chain_testcheck(hammer2_chain_t *chain, void *bdata);


void hammer2_pfs_memory_wait(hammer2_pfsmount_t *pmp);
void hammer2_pfs_memory_inc(hammer2_pfsmount_t *pmp);
void hammer2_pfs_memory_wakeup(hammer2_pfsmount_t *pmp);

void hammer2_base_delete(hammer2_trans_t *trans, hammer2_chain_t *chain,
				hammer2_blockref_t *base, int count,
				int *cache_indexp, hammer2_chain_t *child);
void hammer2_base_insert(hammer2_trans_t *trans, hammer2_chain_t *chain,
				hammer2_blockref_t *base, int count,
				int *cache_indexp, hammer2_chain_t *child);

static __inline void cpu_ccfence(void);

/*
* cpu_ccfence() prevents the compiler from reordering instructions, in
* particular stores, relative to the current cpu. Use cpu_sfence() if
* you need to guarentee ordering by both the compiler and by the cpu.
*
* This also prevents the compiler from caching memory loads into local
* variables across the routine.
*/
static __inline void
cpu_ccfence(void)
{
        __asm __volatile("" : : : "memory");
}



/*
 * hammer2_trans.c
 */
void hammer2_trans_init(hammer2_trans_t *trans, hammer2_pfsmount_t *pmp,
				int flags);
void hammer2_trans_spmp(hammer2_trans_t *trans, hammer2_pfsmount_t *pmp);
void hammer2_trans_done(hammer2_trans_t *trans);

/*
 * hammer2_ioctl.c
 */
int hammer2_ioctl(hammer2_inode_t *ip, u_long com, void *data,
				int fflag, struct ucred *cred);

/*
 * hammer2_io.c
 */
struct file *holdfp(struct filedesc *fdp, int fd, int flag);
void fhold(struct file *fp);
void vfs_bio_clrbuf(struct buf *bp);

hammer2_io_t *hammer2_io_getblk(hammer2_mount_t *hmp, off_t lbase,
				int lsize, int *ownerp);
void hammer2_io_putblk(hammer2_io_t **diop);
void hammer2_io_cleanup(hammer2_mount_t *hmp, struct hammer2_io_tree *tree);
char *hammer2_io_data(hammer2_io_t *dio, off_t lbase);
int hammer2_io_new(hammer2_mount_t *hmp, off_t lbase, int lsize,
				hammer2_io_t **diop);
int hammer2_io_newnz(hammer2_mount_t *hmp, off_t lbase, int lsize,
				hammer2_io_t **diop);
int hammer2_io_newq(hammer2_mount_t *hmp, off_t lbase, int lsize,
				hammer2_io_t **diop);
int hammer2_io_bread(hammer2_mount_t *hmp, off_t lbase, int lsize,
				hammer2_io_t **diop);
void hammer2_io_breadcb(hammer2_mount_t *hmp, off_t lbase, int lsize,
				void (*callback)(hammer2_io_t *dio,
						 hammer2_cluster_t *arg_l,
						 hammer2_chain_t *arg_c,
						 void *arg_p, off_t arg_o),
				hammer2_cluster_t *arg_l,
				hammer2_chain_t *arg_c,
				void *arg_p, off_t arg_o);
void hammer2_io_bawrite(hammer2_io_t **diop);
void hammer2_io_bdwrite(hammer2_io_t **diop);
int hammer2_io_bwrite(hammer2_io_t **diop);
int hammer2_io_isdirty(hammer2_io_t *dio);
void hammer2_io_setdirty(hammer2_io_t *dio);
void hammer2_io_setinval(hammer2_io_t *dio, u_int bytes);
void hammer2_io_brelse(hammer2_io_t **diop);
void hammer2_io_bqrelse(hammer2_io_t **diop);

static __inline int
cluster_read(struct vnode *vp, off_t filesize, off_t loffset,
daddr_t blksize, size_t minreq, size_t maxreq, struct buf **bpp)
{
        *bpp = NULL;
        // return(cluster_readx(vp, filesize, loffset, blksize, minreq,
        // fix me return(breadn(vp, filesize, loffset, (int64_t)blksize, (int *)minreq,
        //      maxreq, bpp));
        return 0;
}


/*
 * hammer2_msgops.c
 */
int hammer2_msg_dbg_rcvmsg(kdmsg_msg_t *msg);
int hammer2_msg_adhoc_input(kdmsg_msg_t *msg);

/*
 * hammer2_vfsops.c
 */
void hammer2_clusterctl_wakeup(kdmsg_iocom_t *iocom);
void hammer2_volconf_update(hammer2_mount_t *hmp, int index);
void hammer2_cluster_reconnect(hammer2_mount_t *hmp, struct file *fp);
void hammer2_dump_chain(hammer2_chain_t *chain, int tab, int *countp, char pfx);
void hammer2_bioq_sync(hammer2_pfsmount_t *pmp);
int hammer2_vfs_sync(struct mount *mp, int waitflags);
void hammer2_lwinprog_ref(hammer2_pfsmount_t *pmp);
void hammer2_lwinprog_drop(hammer2_pfsmount_t *pmp);
void hammer2_lwinprog_wait(hammer2_pfsmount_t *pmp);

struct mtx;
int mtxsleep(void *, struct mutex *, int, const char *, int);

/*
 * hammer2_freemap.c
 */
int hammer2_freemap_alloc(hammer2_trans_t *trans, hammer2_chain_t *chain,
				size_t bytes);
void hammer2_freemap_adjust(hammer2_trans_t *trans, hammer2_mount_t *hmp,
				hammer2_blockref_t *bref, int how);

/*
 * hammer2_cluster.c
 */
int hammer2_cluster_need_resize(hammer2_cluster_t *cluster, int bytes);
uint8_t hammer2_cluster_type(hammer2_cluster_t *cluster);
hammer2_media_data_t *hammer2_cluster_data(hammer2_cluster_t *cluster);
hammer2_media_data_t *hammer2_cluster_wdata(hammer2_cluster_t *cluster);
hammer2_cluster_t *hammer2_cluster_from_chain(hammer2_chain_t *chain);
int hammer2_cluster_modified(hammer2_cluster_t *cluster);
int hammer2_cluster_duplicated(hammer2_cluster_t *cluster);
void hammer2_cluster_set_chainflags(hammer2_cluster_t *cluster, uint32_t flags);
void hammer2_cluster_bref(hammer2_cluster_t *cluster, hammer2_blockref_t *bref);
void hammer2_cluster_setflush(hammer2_trans_t *trans,
			hammer2_cluster_t *cluster);
void hammer2_cluster_setmethod_check(hammer2_trans_t *trans,
			hammer2_cluster_t *cluster, int check_algo);
hammer2_cluster_t *hammer2_cluster_alloc(hammer2_pfsmount_t *pmp,
			hammer2_trans_t *trans,
			hammer2_blockref_t *bref);
void hammer2_cluster_ref(hammer2_cluster_t *cluster);
void hammer2_cluster_drop(hammer2_cluster_t *cluster);
void hammer2_cluster_wait(hammer2_cluster_t *cluster);
int hammer2_cluster_lock(hammer2_cluster_t *cluster, int how);
void hammer2_cluster_replace(hammer2_cluster_t *dst, hammer2_cluster_t *src);
void hammer2_cluster_replace_locked(hammer2_cluster_t *dst,
			hammer2_cluster_t *src);
hammer2_cluster_t *hammer2_cluster_copy(hammer2_cluster_t *ocluster,
			int with_chains);
void hammer2_cluster_unlock(hammer2_cluster_t *cluster);
void hammer2_cluster_resize(hammer2_trans_t *trans, hammer2_inode_t *ip,
			hammer2_cluster_t *cparent, hammer2_cluster_t *cluster,
			int nradix, int flags);
hammer2_inode_data_t *hammer2_cluster_modify_ip(hammer2_trans_t *trans,
			hammer2_inode_t *ip, hammer2_cluster_t *cluster,
			int flags);
void hammer2_cluster_modify(hammer2_trans_t *trans, hammer2_cluster_t *cluster,
			int flags);
void hammer2_cluster_modsync(hammer2_cluster_t *cluster);
hammer2_cluster_t *hammer2_cluster_lookup_init(hammer2_cluster_t *cparent,
			int flags);
void hammer2_cluster_lookup_done(hammer2_cluster_t *cparent);
hammer2_cluster_t *hammer2_cluster_lookup(hammer2_cluster_t *cparent,
			hammer2_key_t *key_nextp,
			hammer2_key_t key_beg, hammer2_key_t key_end,
			int flags, int *ddflagp);
hammer2_cluster_t *hammer2_cluster_next(hammer2_cluster_t *cparent,
			hammer2_cluster_t *cluster,
			hammer2_key_t *key_nextp,
			hammer2_key_t key_beg, hammer2_key_t key_end,
			int flags);
hammer2_cluster_t *hammer2_cluster_scan(hammer2_cluster_t *cparent,
			hammer2_cluster_t *cluster, int flags);
int hammer2_cluster_create(hammer2_trans_t *trans, hammer2_cluster_t *cparent,
			hammer2_cluster_t **clusterp,
			hammer2_key_t key, int keybits,
			int type, size_t bytes, int flags);
void hammer2_cluster_rename(hammer2_trans_t *trans, hammer2_blockref_t *bref,
			hammer2_cluster_t *cparent, hammer2_cluster_t *cluster,
			int flags);
void hammer2_cluster_delete(hammer2_trans_t *trans, hammer2_cluster_t *pcluster,
			hammer2_cluster_t *cluster, int flags);
int hammer2_cluster_snapshot(hammer2_trans_t *trans,
			hammer2_cluster_t *ocluster, hammer2_ioc_pfs_t *pfs);
hammer2_cluster_t *hammer2_cluster_parent(hammer2_cluster_t *cluster);


#endif /* !_KERNEL */
#endif /* !_VFS_HAMMER2_HAMMER2_H_ */
