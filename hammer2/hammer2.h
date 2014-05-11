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

#include "hammer2_disk.h"
#include "hammer2_mount.h"
#include "hammer2_ioctl.h"
#include "hammer2_ccms.h"

unsigned long curthread;

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

#define RB_LOOKUP(name, root, value)     name##_RB_LOOKUP(root, value)
#define RB_SCAN(name, root, cmp, callback, data)  
#define RB_EMPTY(head)                  (RB_ROOT(head) == NULL)

static uint32_t iscsiCrc32Table[256] = {
    0x00000000L, 0xF26B8303L, 0xE13B70F7L, 0x1350F3F4L,
    0xC79A971FL, 0x35F1141CL, 0x26A1E7E8L, 0xD4CA64EBL,
    0x8AD958CFL, 0x78B2DBCCL, 0x6BE22838L, 0x9989AB3BL,
    0x4D43CFD0L, 0xBF284CD3L, 0xAC78BF27L, 0x5E133C24L,
    0x105EC76FL, 0xE235446CL, 0xF165B798L, 0x030E349BL,
    0xD7C45070L, 0x25AFD373L, 0x36FF2087L, 0xC494A384L,
    0x9A879FA0L, 0x68EC1CA3L, 0x7BBCEF57L, 0x89D76C54L,
    0x5D1D08BFL, 0xAF768BBCL, 0xBC267848L, 0x4E4DFB4BL,
    0x20BD8EDEL, 0xD2D60DDDL, 0xC186FE29L, 0x33ED7D2AL,
    0xE72719C1L, 0x154C9AC2L, 0x061C6936L, 0xF477EA35L,
    0xAA64D611L, 0x580F5512L, 0x4B5FA6E6L, 0xB93425E5L,
    0x6DFE410EL, 0x9F95C20DL, 0x8CC531F9L, 0x7EAEB2FAL,
    0x30E349B1L, 0xC288CAB2L, 0xD1D83946L, 0x23B3BA45L,
    0xF779DEAEL, 0x05125DADL, 0x1642AE59L, 0xE4292D5AL,
    0xBA3A117EL, 0x4851927DL, 0x5B016189L, 0xA96AE28AL,
    0x7DA08661L, 0x8FCB0562L, 0x9C9BF696L, 0x6EF07595L,
    0x417B1DBCL, 0xB3109EBFL, 0xA0406D4BL, 0x522BEE48L,
    0x86E18AA3L, 0x748A09A0L, 0x67DAFA54L, 0x95B17957L,
    0xCBA24573L, 0x39C9C670L, 0x2A993584L, 0xD8F2B687L,
    0x0C38D26CL, 0xFE53516FL, 0xED03A29BL, 0x1F682198L,
    0x5125DAD3L, 0xA34E59D0L, 0xB01EAA24L, 0x42752927L,
    0x96BF4DCCL, 0x64D4CECFL, 0x77843D3BL, 0x85EFBE38L,
    0xDBFC821CL, 0x2997011FL, 0x3AC7F2EBL, 0xC8AC71E8L,
    0x1C661503L, 0xEE0D9600L, 0xFD5D65F4L, 0x0F36E6F7L,
    0x61C69362L, 0x93AD1061L, 0x80FDE395L, 0x72966096L,
    0xA65C047DL, 0x5437877EL, 0x4767748AL, 0xB50CF789L,
    0xEB1FCBADL, 0x197448AEL, 0x0A24BB5AL, 0xF84F3859L,
    0x2C855CB2L, 0xDEEEDFB1L, 0xCDBE2C45L, 0x3FD5AF46L,
    0x7198540DL, 0x83F3D70EL, 0x90A324FAL, 0x62C8A7F9L,
    0xB602C312L, 0x44694011L, 0x5739B3E5L, 0xA55230E6L,
    0xFB410CC2L, 0x092A8FC1L, 0x1A7A7C35L, 0xE811FF36L,
    0x3CDB9BDDL, 0xCEB018DEL, 0xDDE0EB2AL, 0x2F8B6829L,
    0x82F63B78L, 0x709DB87BL, 0x63CD4B8FL, 0x91A6C88CL,
    0x456CAC67L, 0xB7072F64L, 0xA457DC90L, 0x563C5F93L,
    0x082F63B7L, 0xFA44E0B4L, 0xE9141340L, 0x1B7F9043L,
    0xCFB5F4A8L, 0x3DDE77ABL, 0x2E8E845FL, 0xDCE5075CL,
    0x92A8FC17L, 0x60C37F14L, 0x73938CE0L, 0x81F80FE3L,
    0x55326B08L, 0xA759E80BL, 0xB4091BFFL, 0x466298FCL,
    0x1871A4D8L, 0xEA1A27DBL, 0xF94AD42FL, 0x0B21572CL,
    0xDFEB33C7L, 0x2D80B0C4L, 0x3ED04330L, 0xCCBBC033L,
    0xA24BB5A6L, 0x502036A5L, 0x4370C551L, 0xB11B4652L,
    0x65D122B9L, 0x97BAA1BAL, 0x84EA524EL, 0x7681D14DL,
    0x2892ED69L, 0xDAF96E6AL, 0xC9A99D9EL, 0x3BC21E9DL,
    0xEF087A76L, 0x1D63F975L, 0x0E330A81L, 0xFC588982L,
    0xB21572C9L, 0x407EF1CAL, 0x532E023EL, 0xA145813DL,
    0x758FE5D6L, 0x87E466D5L, 0x94B49521L, 0x66DF1622L,
    0x38CC2A06L, 0xCAA7A905L, 0xD9F75AF1L, 0x2B9CD9F2L,
    0xFF56BD19L, 0x0D3D3E1AL, 0x1E6DCDEEL, 0xEC064EEDL,
    0xC38D26C4L, 0x31E6A5C7L, 0x22B65633L, 0xD0DDD530L,
    0x0417B1DBL, 0xF67C32D8L, 0xE52CC12CL, 0x1747422FL,
    0x49547E0BL, 0xBB3FFD08L, 0xA86F0EFCL, 0x5A048DFFL,
    0x8ECEE914L, 0x7CA56A17L, 0x6FF599E3L, 0x9D9E1AE0L,
    0xD3D3E1ABL, 0x21B862A8L, 0x32E8915CL, 0xC083125FL,
    0x144976B4L, 0xE622F5B7L, 0xF5720643L, 0x07198540L,
    0x590AB964L, 0xAB613A67L, 0xB831C993L, 0x4A5A4A90L,
    0x9E902E7BL, 0x6CFBAD78L, 0x7FAB5E8CL, 0x8DC0DD8FL,
    0xE330A81AL, 0x115B2B19L, 0x020BD8EDL, 0xF0605BEEL,
    0x24AA3F05L, 0xD6C1BC06L, 0xC5914FF2L, 0x37FACCF1L,
    0x69E9F0D5L, 0x9B8273D6L, 0x88D28022L, 0x7AB90321L,
    0xAE7367CAL, 0x5C18E4C9L, 0x4F48173DL, 0xBD23943EL,
    0xF36E6F75L, 0x0105EC76L, 0x12551F82L, 0xE03E9C81L,
    0x34F4F86AL, 0xC69F7B69L, 0xD5CF889DL, 0x27A40B9EL,
    0x79B737BAL, 0x8BDCB4B9L, 0x988C474DL, 0x6AE7C44EL,
    0xBE2DA0A5L, 0x4C4623A6L, 0x5F16D052L, 0xAD7D5351L
};

uint32_t
iscsi_crc32(const void *buf, size_t size)
{
     const uint8_t *p = buf;
     uint32_t crc = 0;

     crc = crc ^ 0xffffffff;
     while (size--)
          crc = iscsiCrc32Table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
     crc = crc ^ 0xffffffff;
     return crc;
}

/*
 * This helper may be used by VFSs to implement unix chmod semantics.
 */
int
vop_helper_chmod(struct vnode *vp, mode_t new_mode, struct ucred *cred,
                 uid_t cur_uid, gid_t cur_gid, mode_t *cur_modep)
{
        int error;

        if (cred->cr_uid != cur_uid) {
                //error = priv_check_cred(cred, PRIV_VFS_CHMOD, 0);
                if (error)
                        return (error);
        }
        if (cred->cr_uid) {
                if (vp->v_type != VDIR && (*cur_modep & S_ISTXT))
                        return (EFTYPE);
                if (!groupmember(cur_gid, cred) && (*cur_modep & S_ISGID))
                        return (EPERM);
        }
        *cur_modep &= ~ALLPERMS;
        *cur_modep |= new_mode & ALLPERMS;
        return(0);
}


static
void
_cache_setunresolved(struct namecache *ncp)
{
        struct vnode *vp;

}

/*
*
*/
void
cache_setunresolved(struct nchandle *nch)
{
	printf("cache_setunresolved\n");
	// _cache_setunresolved(nch->ncp);
}

/* 
struct vop_readdir_args {
        struct vop_generic_args a_head;
        struct vnode *a_vp;
        struct uio *a_uio;
        struct ucred *a_cred;
        int *a_eofflag;
        int *a_ncookies;
        off_t **a_cookies;
};
*/

/* struct bio_queue_head {
        TAILQ_HEAD(bio_queue, bio) queue;
        off_t   off_unused;
        int     reorder;
        struct  bio *transition;
        struct  bio *bio_unused;
};
*/

struct bio_queue_head {
TAILQ_HEAD(bio_queue, bio) queue;
off_t last_offset;
struct  bioh2 *insert_point;
struct  bioh2 *transition;
struct  bioh2 *bio_unused;
};


static void
bioq_insert_tail(struct bio_queue_head *bioq, struct bioh2 *bio)
{
        bioq->transition = NULL;
// XX fixme       TAILQ_INSERT_TAIL(&bioq->queue, bio, bio_act);
}

/*
 * Return an object to the object cache.
 */
void
objcache_put(struct objcache *oc, void *obj)
{
	printf("objcache_put\n");
}

/*
 * Get an object from the object cache.
 *
 * WARNING!  ocflags are only used when we have to go to the underlying
 * allocator, so we cannot depend on flags such as M_ZERO.
 */
void *
objcache_get(struct objcache *oc, int ocflags)
{
	printf("objcache_get");
	return (NULL);
}

/*
 * Destroy an object cache.  Must have no existing references.
 */
void
objcache_destroy(struct objcache *oc)
{
	struct percpu_objcache *cache_percpu;
	struct magazinedepot *depot;
	int clusterid, cpuid;
	//struct magazinelist tmplist;

	//LIST_REMOVE(oc, oc_next);

	//SLIST_INIT(&tmplist);
	for (clusterid = 0; clusterid < 16; clusterid++) {
		//depot = &oc->depot[clusterid];
		//depot_disassociate(depot, &tmplist);
	}
	//maglist_purge(oc, &tmplist);

	for (cpuid = 0; cpuid < ncpus; cpuid++) {
		//cache_percpu = &oc->cache_percpu[cpuid];

		//crit_enter();
		//mag_purge(oc, &cache_percpu->loaded_magazine, TRUE);
		//mag_purge(oc, &cache_percpu->previous_magazine, TRUE);
		//cache_percpu->loaded_magazine = NULL;
		//cache_percpu->previous_magazine = NULL;
		/* don't bother adjusting depot->unallocated_objects */
	}

//	free(oc->name, M_TEMP);
////	free(oc, M_OBJCACHE);
}

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
struct nchandle {
    struct namecache *ncp;              /* ncp in underlying filesystem */
    struct mount *mount;                /* mount pt (possible overlay) */
};

/* struct vop_generic_args {
        struct syslink_desc *a_desc;     command descriptor for the call 
        struct vop_ops *a_ops;           operations vector for the call 
        int a_reserved[4];
};
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


#define BUG()                   panic("BUG")
#define BUG_ON(condition)       do { if (condition) BUG(); } while(0)
#define KKASSERT(exp) BUG_ON(!(exp))

struct hammer2_chain;
struct hammer2_cluster;
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
TAILQ_HEAD(h2_flush_deferral_list, hammer2_chain);
TAILQ_HEAD(h2_core_list, hammer2_chain);

#define CHAIN_CORE_DELETE_BMAP_ENTRIES	\
	(HAMMER2_PBUFSIZE / sizeof(hammer2_blockref_t) / sizeof(uint32_t))

struct hammer2_chain_core {
	int		good;
	struct ccms_cst	cst;
	struct h2_core_list ownerq;	  /* all chains sharing this core */
	struct hammer2_chain_tree *rbtree; /* live chains */
	struct hammer2_chain_tree *dbtree; /* bmapped deletions */
	struct h2_core_list dbq;	  /* other deletions */
	int		live_zero;	/* blockref array opt */
	u_int		sharecnt;
	u_int		flags;
	u_int		live_count;	/* live (not deleted) chains in tree */
	u_int		chain_count;	/* live + deleted chains under core */
	int		generation;	/* generation number (inserts only) */
} __packed;

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
	struct _atomic_lock *spin;  //spinlock spin;
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
	TAILQ_ENTRY(hammer2_chain) core_entry;	/* contemporary chains */
	RB_ENTRY(hammer2_chain) rbnode;		/* live chain(s) */
	TAILQ_ENTRY(hammer2_chain) db_entry;	/* non bmapped deletions */
	hammer2_blockref_t	bref;
	hammer2_chain_core_t	*core;
	hammer2_chain_core_t	*above;
	struct hammer2_state	*state;		/* if active cache msg */
	struct hammer2_mount	*hmp;
	struct hammer2_pfsmount	*pmp;		/* can be NULL */

	hammer2_blockref_t	dsrc;			/* DEBUG */
	int			ninserts;		/* DEBUG */
	int			nremoves;		/* DEBUG */
	hammer2_tid_t		dsrc_dupfromat;		/* DEBUG */
	uint32_t		dsrc_dupfromflags;	/* DEBUG */
	int			dsrc_reason;		/* DEBUG */
	int			dsrc_ninserts;		/* DEBUG */
	uint32_t		dsrc_flags;		/* DEBUG */
	hammer2_tid_t		dsrc_modify;		/* DEBUG */
	hammer2_tid_t		dsrc_delete;		/* DEBUG */
	hammer2_tid_t		dsrc_update_lo;		/* DEBUG */
	struct hammer2_chain	*dsrc_original;		/* DEBUG */

	hammer2_tid_t	modify_tid;		/* flush filter */
	hammer2_tid_t	delete_tid;		/* flush filter */
	hammer2_tid_t	update_lo;		/* flush propagation */
	hammer2_tid_t	update_hi;		/* setsubmod propagation */
	hammer2_key_t   data_count;		/* delta's to apply */
	hammer2_key_t   inode_count;		/* delta's to apply */
	hammer2_io_t	*dio;			/* physical data buffer */
	u_int		bytes;			/* physical data size */
	u_int		flags;
	u_int		refs;
	u_int		lockcnt;
	hammer2_media_data_t *data;		/* data pointer shortcut */
	TAILQ_ENTRY(hammer2_chain) flush_node;	/* flush deferral list */

	int		inode_reason;
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

struct hammer2_cluster {
	int			status;		/* operational status */
	int			refs;		/* track for deallocation */
	struct hammer2_pfsmount	*pmp;
	uint32_t		flags;
	int			nchains;
	hammer2_chain_t		*focus;		/* current focus (or mod) */
	hammer2_chain_t		*array[HAMMER2_MAXCLUSTER];
	int			cache_index[HAMMER2_MAXCLUSTER];
};

typedef struct hammer2_cluster hammer2_cluster_t;

#define HAMMER2_CLUSTER_PFS	0x00000001	/* embedded in pfsmount */
#define HAMMER2_CLUSTER_INODE	0x00000002	/* embedded in inode */


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

int hammer2_inode_cmp(hammer2_inode_t *ip1, hammer2_inode_t *ip2);

#define RB_PROTOTYPE2(name, type, field, cmp, datatype)			
RB_PROTOTYPE2(hammer2_inode_tree, hammer2_inode, rbnode, hammer2_inode_cmp,
		hammer2_tid_t);

/*
 * inode-unlink side-structure
 */
struct hammer2_inode_unlink {
	TAILQ_ENTRY(hammer2_inode_unlink) entry;
	hammer2_inode_t	*ip;
};
TAILQ_HEAD(hammer2_unlk_list, hammer2_inode_unlink);

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

struct hammer2_mount {
	struct vnode	*devvp;		/* device vnode */
	int		ronly;		/* read-only mount */
	int		pmp_count;	/* PFS mounts backed by us */
	TAILQ_ENTRY(hammer2_mount) mntentry; /* hammer2_mntlist */

	struct malloc_type *mchain;
	int		nipstacks;
	int		maxipstacks;
	struct _atomic_lock *io_spin; 	/* iotree access */
	struct hammer2_io_tree iotree;
	int		iofree_count;
	hammer2_chain_t vchain;		/* anchor chain (topology) */
	hammer2_chain_t fchain;		/* anchor chain (freemap) */
	hammer2_inode_t	*sroot;		/* super-root localized to media */
	struct lock	alloclk;	/* lockmgr lock */
	struct lock	voldatalk;	/* lockmgr lock */
	struct hammer2_trans_queue transq; /* all in-progress transactions */
	hammer2_off_t	heur_freemap[HAMMER2_FREEMAP_HEUR];
	int		flushcnt;	/* #of flush trans on the list */

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

/* struct bio_queue_head {
TAILQ_HEAD(bio_queue, bio) queue;
off_t last_offset;
struct  bio *insert_point;
}; */

struct mtx {
int     owned;
struct mtx *parent;
};

struct hammer2_pfsmount {
	struct mount		*mp;
	hammer2_cluster_t	cluster;
	hammer2_inode_t		*iroot;		/* PFS root inode */
	hammer2_inode_t		*ihidden;	/* PFS hidden directory */
	struct lock		lock;		/* PFS lock for certain ops */
	hammer2_off_t		inode_count;	/* copy of inode_count */
	ccms_domain_t		ccms_dom;
	struct netexport	export;		/* nfs export */
	int			ronly;		/* read-only mount */
	struct malloc_type	*minode;
	struct malloc_type	*mmsg;
	kdmsg_iocom_t		iocom;
	struct _atomic_lock	*inum_spin;	/* inumber lookup */
	struct hammer2_inode_tree inum_tree;
	long			inmem_inodes;
	long			inmem_dirty_chains;
	int			count_lwinprog;	/* logical write in prog */
	struct _atomic_lock 	*unlinkq_spin;
	struct hammer2_unlk_list unlinkq;
 	struct proc		*wthread_td; 	/* write thread td */
	struct bio_queue_head	wthread_bioq;	/* logical buffer bioq */
	struct mtx		wthread_mtx;	/* interlock */
	int			wthread_destroy;/* termination sequencing */
};

typedef struct hammer2_pfsmount hammer2_pfsmount_t;

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
 * hammer2_subr.c
 */
#define hammer2_icrc32(buf, size)	iscsi_crc32((buf), (size))
#define hammer2_icrc32c(buf, size, crc)	iscsi_crc32_ext((buf), (size), (crc))

hammer2_cluster_t *hammer2_inode_lock_ex(hammer2_inode_t *ip);
hammer2_cluster_t *hammer2_inode_lock_sh(hammer2_inode_t *ip);
void hammer2_inode_unlock_ex(hammer2_inode_t *ip, hammer2_cluster_t *chain);
void hammer2_inode_unlock_sh(hammer2_inode_t *ip, hammer2_cluster_t *chain);
void hammer2_voldata_lock(hammer2_mount_t *hmp);
void hammer2_voldata_unlock(hammer2_mount_t *hmp, int modify);
ccms_state_t hammer2_inode_lock_temp_release(hammer2_inode_t *ip);
void hammer2_inode_lock_temp_restore(hammer2_inode_t *ip, ccms_state_t ostate);
ccms_state_t hammer2_inode_lock_upgrade(hammer2_inode_t *ip);
void hammer2_inode_lock_downgrade(hammer2_inode_t *ip, ccms_state_t ostate);

void hammer2_mount_exlock(hammer2_mount_t *hmp);
void hammer2_mount_shlock(hammer2_mount_t *hmp);
void hammer2_mount_unlock(hammer2_mount_t *hmp);

int hammer2_get_dtype(hammer2_inode_data_t *ipdata);
int hammer2_get_vtype(hammer2_inode_data_t *ipdata);
u_int8_t hammer2_get_obj_type(enum vtype vtype);
void hammer2_time_to_timespec(u_int64_t xtime, struct timespec *ts);
u_int64_t hammer2_timespec_to_time(struct timespec *ts);
u_int32_t hammer2_to_unix_xid(uuid_t *uuid);
void hammer2_guid_to_uuid(uuid_t *uuid, u_int32_t guid);

hammer2_key_t hammer2_dirhash(const unsigned char *name, size_t len);
int hammer2_getradix(size_t bytes);

int hammer2_calc_logical(hammer2_inode_t *ip, hammer2_off_t uoff,
			hammer2_key_t *lbasep, hammer2_key_t *leofp);
int hammer2_calc_physical(hammer2_inode_t *ip, hammer2_inode_data_t *ipdata,
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
/*int hammer2_unlink_file(hammer2_trans_t *trans, hammer2_inode_t *dip,
			const uint8_t *name, size_t name_len, int isdir,
			int *hlinkp, struct nchandle *nch);
X */
int hammer2_hardlink_consolidate(hammer2_trans_t *trans,
			hammer2_inode_t *ip, hammer2_cluster_t **clusterp,
			hammer2_inode_t *cdip, hammer2_cluster_t *cdcluster,
			int nlinks);
int hammer2_hardlink_deconsolidate(hammer2_trans_t *trans, hammer2_inode_t *dip,
			hammer2_chain_t **chainp, hammer2_chain_t **ochainp);
int hammer2_hardlink_find(hammer2_inode_t *dip, hammer2_cluster_t *cluster);
void hammer2_inode_install_hidden(hammer2_pfsmount_t *pmp);

/*
 * hammer2_chain.c
 */
void hammer2_modify_volume(hammer2_mount_t *hmp);
hammer2_chain_t *hammer2_chain_alloc(hammer2_mount_t *hmp,
				hammer2_pfsmount_t *pmp,
				hammer2_trans_t *trans,
				hammer2_blockref_t *bref);
void hammer2_chain_core_alloc(hammer2_trans_t *trans, hammer2_chain_t *nchain,
				hammer2_chain_t *ochain);
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
				hammer2_chain_t **chainp, int flags);
void hammer2_chain_resize(hammer2_trans_t *trans, hammer2_inode_t *ip,
				hammer2_chain_t *parent,
				hammer2_chain_t **chainp,
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

int hammer2_chain_create(hammer2_trans_t *trans,
				hammer2_chain_t **parentp,
				hammer2_chain_t **chainp,
				hammer2_key_t key, int keybits,
				int type, size_t bytes);

void hammer2_chain_duplicate(hammer2_trans_t *trans, hammer2_chain_t **parentp,
				hammer2_chain_t **chainp,
				hammer2_blockref_t *bref, int snapshot,
				int duplicate_reason);
int hammer2_chain_snapshot(hammer2_trans_t *trans, hammer2_chain_t **chainp,
				hammer2_ioc_pfs_t *pfs);
void hammer2_chain_delete(hammer2_trans_t *trans, hammer2_chain_t *chain,
				int flags);
void hammer2_chain_delete_duplicate(hammer2_trans_t *trans,
				hammer2_chain_t **chainp, int flags);
void hammer2_flush(hammer2_trans_t *trans, hammer2_chain_t **chainp);
void hammer2_chain_commit(hammer2_trans_t *trans, hammer2_chain_t *chain);
void hammer2_chain_setsubmod(hammer2_trans_t *trans, hammer2_chain_t *chain);
void hammer2_chain_countbrefs(hammer2_chain_t *chain,
				hammer2_blockref_t *base, int count);

void hammer2_pfs_memory_wait(hammer2_pfsmount_t *pmp);
void hammer2_pfs_memory_inc(hammer2_pfsmount_t *pmp);
void hammer2_pfs_memory_wakeup(hammer2_pfsmount_t *pmp);

int hammer2_base_find(hammer2_chain_t *chain,
				hammer2_blockref_t *base, int count,
				int *cache_indexp, hammer2_key_t *key_nextp,
				hammer2_key_t key_beg, hammer2_key_t key_end,
				int delete_filter);

void hammer2_base_delete(hammer2_trans_t *trans, hammer2_chain_t *chain,
				hammer2_blockref_t *base, int count,
				int *cache_indexp, hammer2_chain_t *child);
void hammer2_base_insert(hammer2_trans_t *trans, hammer2_chain_t *chain,
				hammer2_blockref_t *base, int count,
				int *cache_indexp, hammer2_chain_t *child);
void hammer2_chain_refactor(hammer2_chain_t **chainp);

/*
 * hammer2_trans.c
 */
void hammer2_trans_init(hammer2_trans_t *trans, hammer2_pfsmount_t *pmp,
				hammer2_mount_t *hmp, int flags);
void hammer2_trans_done(hammer2_trans_t *trans);

/*
 * hammer2_ioctl.c
 */
int hammer2_ioctl(hammer2_inode_t *ip, u_long com, void *data,
				int fflag, struct ucred *cred);

/*
 * hammer2_io.c
 */
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
void hammer2_io_bqrelse(hammer2_io_t **diop);
void hammer2_io_bqrelse(hammer2_io_t **diop);

/*
 * hammer2_msgops.c
 */
int hammer2_msg_dbg_rcvmsg(kdmsg_msg_t *msg);
int hammer2_msg_adhoc_input(kdmsg_msg_t *msg);


/*
 * hammer2_vfsops.c
 */
//void hammer2_clusterctl_wakeup(kdmsg_iocom_t *iocom);
void hammer2_volconf_update(hammer2_pfsmount_t *pmp, int index);
void hammer2_cluster_reconnect(hammer2_pfsmount_t *pmp, struct file *fp);
void hammer2_dump_chain(hammer2_chain_t *chain, int tab, int *countp, char pfx);
void hammer2_bioq_sync(hammer2_pfsmount_t *pmp);
int hammer2_vfs_sync(struct mount *mp, int waitflags);
void hammer2_lwinprog_ref(hammer2_pfsmount_t *pmp);
void hammer2_lwinprog_drop(hammer2_pfsmount_t *pmp);
void hammer2_lwinprog_wait(hammer2_pfsmount_t *pmp);

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

u_int hammer2_cluster_bytes(hammer2_cluster_t *cluster);
uint8_t hammer2_cluster_type(hammer2_cluster_t *cluster);
hammer2_media_data_t *hammer2_cluster_data(hammer2_cluster_t *cluster);
hammer2_cluster_t *hammer2_cluster_from_chain(hammer2_chain_t *chain);
int hammer2_cluster_modified(hammer2_cluster_t *cluster);
int hammer2_cluster_unlinked(hammer2_cluster_t *cluster);
int hammer2_cluster_duplicated(hammer2_cluster_t *cluster);
void hammer2_cluster_set_chainflags(hammer2_cluster_t *cluster, uint32_t flags);
void hammer2_cluster_bref(hammer2_cluster_t *cluster, hammer2_blockref_t *bref);
void hammer2_cluster_setsubmod(hammer2_trans_t *trans,
			hammer2_cluster_t *cluster);
hammer2_cluster_t *hammer2_cluster_alloc(hammer2_pfsmount_t *pmp,
			hammer2_trans_t *trans,
			hammer2_blockref_t *bref);
void hammer2_cluster_core_alloc(hammer2_trans_t *trans,
			hammer2_cluster_t *ncluster,
			hammer2_cluster_t *ocluster);
void hammer2_cluster_ref(hammer2_cluster_t *cluster);
void hammer2_cluster_drop(hammer2_cluster_t *cluster);
void hammer2_cluster_wait(hammer2_cluster_t *cluster);
int hammer2_cluster_lock(hammer2_cluster_t *cluster, int how);
void hammer2_cluster_replace(hammer2_cluster_t *dst, hammer2_cluster_t *src);
void hammer2_cluster_replace_locked(hammer2_cluster_t *dst,
			hammer2_cluster_t *src);
hammer2_cluster_t *hammer2_cluster_copy(hammer2_cluster_t *ocluster,
			int with_chains);
void hammer2_cluster_refactor(hammer2_cluster_t *cluster);
void hammer2_cluster_unlock(hammer2_cluster_t *cluster);
void hammer2_cluster_resize(hammer2_trans_t *trans, hammer2_inode_t *ip,
			hammer2_cluster_t *cparent, hammer2_cluster_t *cluster,
			int nradix, int flags);
hammer2_inode_data_t *hammer2_cluster_modify_ip(hammer2_trans_t *trans,
			hammer2_inode_t *ip, hammer2_cluster_t *cluster,
			int flags);
void hammer2_cluster_modify(hammer2_trans_t *trans, hammer2_cluster_t *cluster,
			int flags);
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
			hammer2_key_t key, int keybits, int type, size_t bytes);
void hammer2_cluster_duplicate(hammer2_trans_t *trans,
			hammer2_cluster_t *cparent, hammer2_cluster_t *cluster,
			hammer2_blockref_t *bref,
			int snapshot, int duplicate_reason);
void hammer2_cluster_delete_duplicate(hammer2_trans_t *trans,
			hammer2_cluster_t *cluster, int flags);
void hammer2_cluster_delete(hammer2_trans_t *trans, hammer2_cluster_t *cluster,
			int flags);
int hammer2_cluster_snapshot(hammer2_trans_t *trans,
			hammer2_cluster_t *ocluster, hammer2_ioc_pfs_t *pfs);


#endif /* !_KERNEL */
#endif /* !_VFS_HAMMER2_HAMMER2_H_ */
