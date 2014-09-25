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

#define HAMMER2_CLUSTER_PFS     0x00000001      /* embedded in pfsmount */
#define HAMMER2_CLUSTER_INODE   0x00000002      /* embedded in inode */

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
