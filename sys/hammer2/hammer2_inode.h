//RB_HEAD(hammer2_inode_tree, hammer2_inode);

/*
 * A hammer2 inode.
 *
 * NOTE: The inode's attribute CST which is also used to lock the inode
 *       is embedded in the chain (chain.cst) and aliased w/ attr_cst.
 */

#define HAMMER2_INODE_MODIFIED          0x0001
#define HAMMER2_INODE_SROOT             0x0002  /* kmalloc special case */
#define HAMMER2_INODE_RENAME_INPROG     0x0004
#define HAMMER2_INODE_ONRBTREE          0x0008
#define HAMMER2_INODE_RESIZED           0x0010
#define HAMMER2_INODE_MTIME             0x0020

int hammer2_inode_cmp(hammer2_inode_t *ip1, hammer2_inode_t *ip2);

#define RB_PROTOTYPE2(name, type, field, cmp, datatype)
RB_PROTOTYPE2(hammer2_inode_tree, hammer2_inode, rbnode, hammer2_inode_cmp,
                hammer2_tid_t);

/*
 * inode-unlink side-structure
 */
struct hammer2_inode_unlink {
        TAILQ_ENTRY(hammer2_inode_unlink) entry;
        hammer2_inode_t *ip;
};
//TAILQ_HEAD(hammer2_unlk_list, hammer2_inode_unlink);

typedef struct hammer2_inode_unlink hammer2_inode_unlink_t;

void hammer2_run_unlinkq(hammer2_trans_t *trans, hammer2_pfsmount_t *pmp);
/* inode vfs ... */
int hammer2_hardlink_consolidate(hammer2_trans_t *trans,
                        hammer2_inode_t *ip, hammer2_cluster_t **clusterp,
                        hammer2_inode_t *cdip, hammer2_cluster_t *cdcluster,
                        int nlinks);
int hammer2_hardlink_deconsolidate(hammer2_trans_t *trans, hammer2_inode_t *dip,
                        hammer2_chain_t **chainp, hammer2_chain_t **ochainp);
int hammer2_hardlink_find(hammer2_inode_t *dip, hammer2_cluster_t *cluster);
hammer2_inode_t *hammer2_inode_common_parent(hammer2_inode_t *fdip,
                        hammer2_inode_t *tdip);
void hammer2_inode_fsync(hammer2_trans_t *trans, hammer2_inode_t *ip,
                        hammer2_cluster_t *cparent);
void hammer2_inode_install_hidden(hammer2_pfsmount_t *pmp);
/* int hammer2_unlink_file(hammer2_trans_t *trans, hammer2_inode_t *dip,
                        const uint8_t *name, size_t name_len, int isdir,
                        int *hlinkp, struct nchandle *nch);
*/
int hammer2_inode_connect(hammer2_trans_t *trans,
                        hammer2_cluster_t **clusterp, int hlink,
                        hammer2_inode_t *dip, hammer2_cluster_t *dcluster,
                        const uint8_t *name, size_t name_len,
                        hammer2_key_t key);
void hammer2_guid_to_uuid(uuid_t *uuid, u_int32_t guid);
hammer2_inode_t *hammer2_inode_get(hammer2_pfsmount_t *pmp,
                        hammer2_inode_t *dip, hammer2_cluster_t *cluster);
void hammer2_inode_drop(hammer2_inode_t *ip);
struct vnode *hammer2_igetv(hammer2_inode_t *ip, hammer2_cluster_t *cparent,
                        int *errorp);
hammer2_inode_t *hammer2_inode_lookup(hammer2_pfsmount_t *pmp,
                        hammer2_tid_t inum);
