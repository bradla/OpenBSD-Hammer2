int vinitvmio(struct vnode *, off_t, int, int);
void vm_object_hold(struct vm_object *obj);
void addaliasu(struct vnode *nvp, int x, int y);
static __inline void _vsetflags(struct vnode *vp, int flags);
void vsetflags(struct vnode *vp, int flags);
uid_t vop_helper_create_uid(struct mount *mp, mode_t dmode, uid_t duid,
                      struct ucred *cred, mode_t *modep);


#define OBJ_DEAD	0x0008 /* dead objects (during rundown) */
#define VOBJBUF	0x00002000 /* Allocate buffers in VM object */

#define v_umajor v_un.vu_cdev.vu_umajor
#define v_uminor v_un.vu_cdev.vu_uminor

/*
 * This helper function may be used by VFSs to implement UNIX initial
 * ownership semantics when creating new objects inside directories.
 */
uid_t
vop_helper_create_uid(struct mount *mp, mode_t dmode, uid_t duid,
		      struct ucred *cred, mode_t *modep)
{
#ifdef SUIDDIR
	if ((mp->mnt_flag & MNT_SUIDDIR) && (dmode & S_ISUID) &&
	    duid != cred->cr_uid && duid) {
		*modep &= ~07111;
		return(duid);
	}
#endif
	return(cred->cr_uid);
}

/*
 * Misc functions
 */
static __inline
void
_vsetflags(struct vnode *vp, int flags)
{
	atomic_set_int(&vp->v_flag, flags);
}

void
vsetflags(struct vnode *vp, int flags)
{
	_vsetflags(vp, flags);
}

/*
* Add a vnode to the alias list hung off the cdev_t. We only associate
* the device number with the vnode. The actual device is not associated
* until the vnode is opened (usually in spec_open()), and will be
* disassociated on last close.
*/
void
addaliasu(struct vnode *nvp, int x, int y)
{
	if (nvp->v_type != VBLK && nvp->v_type != VCHR)
		panic("addaliasu on non-special vnode");
	nvp->v_umajor = x;
	nvp->v_uminor = y;
}

void
vm_object_hold(struct vm_object *obj)
{
	//vn_lock(&obj->token); // XXX lk_gettoken ????
}

/*
 * Initialize VMIO for a vnode.  This routine MUST be called before a
 * VFS can issue buffer cache ops on a vnode.  It is typically called
 * when a vnode is initialized from its inode.
 */
int
vinitvmio(struct vnode *vp, off_t filesize, int blksize, int boff)
{
	struct vm_object *object;
	int error = 0;

	object = vp->v_object;
/* WWW
	if (object) {
		vm_object_hold(object);
		KKASSERT(vp->v_object == object);
	}

	if (object == NULL) {
		object = vnode_pager_alloc(vp, filesize, 0, 0, blksize, boff);
		VOP_CREATE(vp, filesize, blksize, boff);

		vm_object_hold(object);
		atomic_add_int(&object->ref_count, -1);
		vrele(vp);
	} else {
		KKASSERT((object->flags & OBJ_DEAD) == 0);
	}
	KASSERT(vp->v_object != NULL, ("vinitvmio: NULL object"));
	vsetflags(vp, VOBJBUF);
	vm_object_drop(object);
*/

	return (error);
}
