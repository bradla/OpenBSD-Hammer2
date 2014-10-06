int hammer2_freemap_alloc(hammer2_trans_t *trans, hammer2_chain_t *chain,
                                size_t bytes);
void hammer2_freemap_adjust(hammer2_trans_t *trans, hammer2_mount_t *hmp,
                                hammer2_blockref_t *bref, int how);
