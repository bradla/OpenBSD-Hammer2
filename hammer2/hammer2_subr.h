#define hammer2_icrc32(buf, size)       iscsi_crc32((buf), (size))
#define hammer2_icrc32c(buf, size, crc) iscsi_crc32_ext((buf), (size), (crc))

hammer2_cluster_t *hammer2_inode_lock_ex(hammer2_inode_t *ip);
hammer2_cluster_t *hammer2_inode_lock_sh(hammer2_inode_t *ip);
//void hammer2_inode_unlock_ex(hammer2_inode_t *ip, hammer2_cluster_t *chain);
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

//hammer2_key_t hammer2_dirhash(const unsigned char *name, size_t len);
//int hammer2_getradix(size_t bytes);

void hammer2_update_time(uint64_t *timep);
void hammer2_adjreadcounter(hammer2_blockref_t *bref, size_t bytes);
