#include "hammer2_ccms.h"

struct hammer2_chain;
struct hammer2_cluster;
struct hammer2_io;
struct hammer2_pfsmount;

struct hammer2_io {
        RB_ENTRY(hammer2_io) rbnode;    /* indexed by device offset */
        struct _atomic_lock *spin;  //spinlock spin;
        struct hammer2_mount *hmp;
        struct buf      *bp;
        struct bio      *bio;
        off_t           pbase;
        int             psize;
        void            (*callback)(struct hammer2_io *dio,
                                    struct hammer2_cluster *cluster,
                                    struct hammer2_chain *chain,
                                    void *arg1, off_t arg2);
        struct hammer2_cluster *arg_l;          /* INPROG I/O only */
        struct hammer2_chain *arg_c;            /* INPROG I/O only */
        void            *arg_p;                 /* INPROG I/O only */
        off_t           arg_o;                  /* INPROG I/O only */
        int             refs;
        int             act;                    /* activity */
};

typedef struct hammer2_io hammer2_io_t;

/*
 * Primary chain structure keeps track of the topology in-memory.
 */
struct hammer2_chain {
        TAILQ_ENTRY(hammer2_chain) core_entry;  /* contemporary chains */
        RB_ENTRY(hammer2_chain) rbnode;         /* live chain(s) */
        TAILQ_ENTRY(hammer2_chain) db_entry;    /* non bmapped deletions */
        hammer2_blockref_t      bref;
        hammer2_chain_core_t    *core;
        hammer2_chain_core_t    *above;
        struct hammer2_state    *state;         /* if active cache msg */
        struct hammer2_mount    *hmp;
        struct hammer2_pfsmount *pmp;           /* can be NULL */

        hammer2_blockref_t      dsrc;                   /* DEBUG */
        int                     ninserts;               /* DEBUG */
        int                     nremoves;               /* DEBUG */
        hammer2_tid_t           dsrc_dupfromat;         /* DEBUG */
        uint32_t                dsrc_dupfromflags;      /* DEBUG */
        int                     dsrc_reason;            /* DEBUG */
        int                     dsrc_ninserts;          /* DEBUG */
        uint32_t                dsrc_flags;             /* DEBUG */
        hammer2_tid_t           dsrc_modify;            /* DEBUG */
        hammer2_tid_t           dsrc_delete;            /* DEBUG */
        hammer2_tid_t           dsrc_update_lo;         /* DEBUG */
        struct hammer2_chain    *dsrc_original;         /* DEBUG */

        hammer2_tid_t   modify_tid;             /* flush filter */
        hammer2_tid_t   delete_tid;             /* flush filter */
        hammer2_tid_t   update_lo;              /* flush propagation */
        hammer2_tid_t   update_hi;              /* setsubmod propagation */
        hammer2_key_t   data_count;             /* delta's to apply */
        hammer2_key_t   inode_count;            /* delta's to apply */
        hammer2_io_t    *dio;                   /* physical data buffer */
        u_int           bytes;                  /* physical data size */
        unsigned int           flags;
        u_int           refs;
        u_int           lockcnt;
        hammer2_media_data_t *data;             /* data pointer shortcut */
        TAILQ_ENTRY(hammer2_chain) flush_node;  /* flush deferral list */

        int             inode_reason;
};

typedef struct hammer2_chain hammer2_chain_t;

#define HAMMER2_MAXCLUSTER      8

struct hammer2_cluster {
        int                     status;         /* operational status */
        int                     refs;           /* track for deallocation */
        struct hammer2_pfsmount *pmp;
        uint32_t                flags;
        int                     nchains;
        hammer2_chain_t         *focus;         /* current focus (or mod) */
        hammer2_chain_t         *array[HAMMER2_MAXCLUSTER];
        int                     cache_index[HAMMER2_MAXCLUSTER];
};

typedef struct hammer2_cluster hammer2_cluster_t;

struct hammer2_inode {
        RB_ENTRY(hammer2_inode) rbnode;         /* inumber lookup (HL) */
        ccms_cst_t              topo_cst;       /* directory topology cst */
        struct hammer2_pfsmount *pmp;           /* PFS mount */
        struct hammer2_inode    *pip;           /* parent inode */
        struct vnode            *vp;
        hammer2_cluster_t       cluster;
        struct lockf            advlock;
        hammer2_tid_t           inum;
        u_int                   flags;
        u_int                   refs;           /* +vpref, +flushref */
        uint8_t                 comp_heuristic;
        hammer2_off_t           size;
        uint64_t                mtime;
};

typedef struct hammer2_inode hammer2_inode_t;

RB_HEAD(hammer2_inode_tree, hammer2_inode);
TAILQ_HEAD(hammer2_unlk_list, hammer2_inode_unlink);

struct hammer2_pfsmount {
        struct mount            *mp;
        hammer2_cluster_t       cluster;
        hammer2_inode_t         *iroot;         /* PFS root inode */
        hammer2_inode_t         *ihidden;       /* PFS hidden directory */
        struct lock             lock;           /* PFS lock for certain ops */
        hammer2_off_t           inode_count;    /* copy of inode_count */
        ccms_domain_t           ccms_dom;
        struct netexport        export;         /* nfs export */
        int                     ronly;          /* read-only mount */
        struct malloc_type      *minode;
        struct malloc_type      *mmsg;
        kdmsg_iocom_t           iocom;
        struct _atomic_lock     *inum_spin;     /* inumber lookup */
        struct hammer2_inode_tree inum_tree;
        long                    inmem_inodes;
        long                    inmem_dirty_chains;
        int                     count_lwinprog; /* logical write in prog */
        struct _atomic_lock     *unlinkq_spin;
        struct hammer2_unlk_list unlinkq;
        struct proc             *wthread_td;    /* write thread td */
        struct bio_queue_head   wthread_bioq;   /* logical buffer bioq */
        struct mtx              wthread_mtx;    /* interlock */
        int                     wthread_destroy;/* termination sequencing */
};

typedef struct hammer2_pfsmount hammer2_pfsmount_t;

int hammer2_chain_cmp(hammer2_chain_t *chain1, hammer2_chain_t *chain2);
RB_PROTOTYPE(hammer2_chain_tree, hammer2_chain, rbnode, hammer2_chain_cmp);

RB_HEAD(hammer2_io_tree, hammer2_io);

struct hammer2_mount {
	struct vnode    *devvp;         /* device vnode */
        int             ronly;          /* read-only mount */
        int             pmp_count;      /* PFS mounts backed by us */
        TAILQ_ENTRY(hammer2_mount) mntentry; /* hammer2_mntlist */

        struct malloc_type *mchain;
        int             nipstacks;
        int             maxipstacks;
        struct _atomic_lock *io_spin;   /* iotree access */
        struct hammer2_io_tree iotree;
        int             iofree_count;
        hammer2_chain_t vchain;         /* anchor chain (topology) */
        hammer2_chain_t fchain;         /* anchor chain (freemap) */
        hammer2_inode_t *sroot;         /* super-root localized to media */
        struct lock     alloclk;        /* lockmgr lock */
        struct lock     voldatalk;      /* lockmgr lock */
        struct hammer2_trans_queue transq; /* all in-progress transactions */
        hammer2_off_t   heur_freemap[HAMMER2_FREEMAP_HEUR];
        int             flushcnt;       /* #of flush trans on the list */

        int             volhdrno;       /* last volhdrno written */
        hammer2_volume_data_t voldata;
        hammer2_volume_data_t volsync;  /* synchronized voldata */
};

typedef struct hammer2_mount hammer2_mount_t;

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

void hammer2_inode_repoint(hammer2_inode_t *ip, hammer2_inode_t *pip,
                        hammer2_cluster_t *cluster);

void hammer2_chain_setsubmod(hammer2_trans_t *trans, hammer2_chain_t *chain);
hammer2_chain_t *hammer2_chain_alloc(hammer2_mount_t *hmp,
                                hammer2_pfsmount_t *pmp,
                                hammer2_trans_t *trans,
                                hammer2_blockref_t *bref);

void hammer2_chain_core_alloc(hammer2_trans_t *trans, hammer2_chain_t *nchain,
                                hammer2_chain_t *ochain);
void hammer2_chain_ref(hammer2_chain_t *chain);
void hammer2_chain_drop(hammer2_chain_t *chain);
int hammer2_chain_lock(hammer2_chain_t *chain, int how);
void hammer2_chain_unlock(hammer2_chain_t *chain);
void hammer2_chain_refactor(hammer2_chain_t **chainp);

/* cluster */
void hammer2_chain_resize(hammer2_trans_t *trans, hammer2_inode_t *ip,
                                hammer2_chain_t *parent,
                                hammer2_chain_t **chainp,
                                int nradix, int flags);
void hammer2_chain_modify(hammer2_trans_t *trans,
                                hammer2_chain_t **chainp, int flags);
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
int hammer2_chain_create(hammer2_trans_t *trans,
                                hammer2_chain_t **parentp,
                                hammer2_chain_t **chainp,
                                hammer2_key_t key, int keybits,
                                int type, size_t bytes);
void hammer2_chain_duplicate(hammer2_trans_t *trans, hammer2_chain_t **parentp,
                                hammer2_chain_t **chainp,
                                hammer2_blockref_t *bref, int snapshot,
                                int duplicate_reason);
void hammer2_chain_delete_duplicate(hammer2_trans_t *trans,
                                hammer2_chain_t **chainp, int flags);
void hammer2_chain_delete(hammer2_trans_t *trans, hammer2_chain_t *chain,
                                int flags);
hammer2_key_t hammer2_dirhash(const unsigned char *name, size_t len);
hammer2_inode_t *hammer2_inode_create(hammer2_trans_t *trans,
                        hammer2_inode_t *dip,
                        struct vattr *vap, struct ucred *cred,
                        const uint8_t *name, size_t name_len,
                        hammer2_cluster_t **clusterp, int *errorp);
void hammer2_inode_unlock_ex(hammer2_inode_t *ip, hammer2_cluster_t *chain);

void hammer2_cluster_resize(hammer2_trans_t *trans, hammer2_inode_t *ip,
                        hammer2_cluster_t *cparent, hammer2_cluster_t *cluster,
                        int nradix, int flags);
hammer2_inode_data_t *hammer2_cluster_modify_ip(hammer2_trans_t *trans,
                        hammer2_inode_t *ip, hammer2_cluster_t *cluster,
                        int flags);
/* flush.h */
void hammer2_voldata_lock(hammer2_mount_t *hmp);
void hammer2_voldata_unlock(hammer2_mount_t *hmp, int modify);
void hammer2_pfs_memory_wakeup(hammer2_pfsmount_t *pmp);
void hammer2_modify_volume(hammer2_mount_t *hmp);
void hammer2_base_delete(hammer2_trans_t *trans, hammer2_chain_t *chain,
                                hammer2_blockref_t *base, int count,
                                int *cache_indexp, hammer2_chain_t *child);
void hammer2_base_insert(hammer2_trans_t *trans, hammer2_chain_t *chain,
                                hammer2_blockref_t *base, int count,
                                int *cache_indexp, hammer2_chain_t *child);
void hammer2_flush(hammer2_trans_t *trans, hammer2_chain_t **chainp);

/* freemap */
int hammer2_getradix(size_t bytes);
void hammer2_io_bqrelse(hammer2_io_t **diop);
int hammer2_io_newq(hammer2_mount_t *hmp, off_t lbase, int lsize,
                                hammer2_io_t **diop);

/* inode */
void hammer2_inode_ref(hammer2_inode_t *ip);
void hammer2_pfs_memory_inc(hammer2_pfsmount_t *pmp);

/* vfsops */
hammer2_chain_t *hammer2_chain_scan(hammer2_chain_t *parent,
                                hammer2_chain_t *chain,
                                int *cache_indexp, int flags);
