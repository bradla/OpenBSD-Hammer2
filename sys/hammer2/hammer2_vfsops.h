//void hammer2_clusterctl_wakeup(kdmsg_iocom_t *iocom);
void hammer2_volconf_update(hammer2_pfsmount_t *pmp, int index);
void hammer2_cluster_reconnect(hammer2_pfsmount_t *pmp, struct file *fp);
void hammer2_dump_chain(hammer2_chain_t *chain, int tab, int *countp, char pfx);
void hammer2_bioq_sync(hammer2_pfsmount_t *pmp);
int hammer2_vfs_sync(struct mount *mp, int waitflags);
void hammer2_lwinprog_ref(hammer2_pfsmount_t *pmp);
void hammer2_lwinprog_drop(hammer2_pfsmount_t *pmp);
void hammer2_lwinprog_wait(hammer2_pfsmount_t *pmp);
hammer2_pfsmount_t * MPTOPMP(struct mount *mp);
//void objcache_put(struct objcache *, void *);
//void *objcache_get(struct objcache *, int);
void vclrisdirty(struct vnode *vp);

/* hammer2_pfsmount_t *
MPTOPMP(struct mount *mp)
{
        return ((hammer2_pfsmount_t *)mp->mnt_data);
}
*/
