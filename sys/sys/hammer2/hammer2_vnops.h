int hammer2_ioctl(hammer2_inode_t *ip, u_long com, void *data,
                                int fflag, struct ucred *cred);
int hammer2_unlink_file(hammer2_trans_t *trans, hammer2_inode_t *dip,
                        const uint8_t *name, size_t name_len, int isdir,
                        int *hlinkp, struct nchandle *nch);
int hammer2_calc_logical(hammer2_inode_t *ip, hammer2_off_t uoff,
                        hammer2_key_t *lbasep, hammer2_key_t *leofp);
int hammer2_calc_physical(hammer2_inode_t *ip, hammer2_inode_data_t *ipdata,
                        hammer2_key_t lbase);
void hammer2_pfs_memory_wait(hammer2_pfsmount_t *pmp);

void hammer2_chain_load_async(hammer2_cluster_t *cluster,
                                void (*func)(hammer2_io_t *dio,
                                             hammer2_cluster_t *cluster,
                                             hammer2_chain_t *chain,
                                             void *arg_p, off_t arg_o),
                                void *arg_p);
int nvextendbuf(struct vnode *, off_t, off_t, int, int, int, int, int);
int nvtruncbuf(struct vnode *vp, off_t length, int blksize, int boff, int trivial);
int vfsync(struct vnode *vp, int waitfor, int passes,
        int (*checkdef)(struct buf *),
        int (*waitoutput)(struct vnode *, struct thread *));
