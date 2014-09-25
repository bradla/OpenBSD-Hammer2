/*
 * H2 is a copy-on-write filesystem.  In order to allow chains to allocate
 * smaller blocks (down to 64-bytes), but improve performance and make
 * clustered I/O possible using larger block sizes, the kernel buffer cache
 * is abstracted via the hammer2_io structure.
 */
//RB_HEAD(hammer2_io_tree, hammer2_io);

hammer2_io_t *hammer2_io_getblk(hammer2_mount_t *hmp, off_t lbase,
                                int lsize, int *ownerp);
void hammer2_io_putblk(hammer2_io_t **diop);
void hammer2_io_cleanup(hammer2_mount_t *hmp, struct hammer2_io_tree *tree);
char *hammer2_io_data(hammer2_io_t *dio, off_t lbase);
int hammer2_io_new(hammer2_mount_t *hmp, off_t lbase, int lsize,
                                hammer2_io_t **diop);
int hammer2_io_newnz(hammer2_mount_t *hmp, off_t lbase, int lsize,
                                hammer2_io_t **diop);
//int hammer2_io_newq(hammer2_mount_t *hmp, off_t lbase, int lsize,
 //                               hammer2_io_t **diop);
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
//void hammer2_io_bqrelse(hammer2_io_t **diop);
void hammer2_io_bqrelse(hammer2_io_t **diop);
void vfs_bio_clrbuf(struct buf *bp);
