/*	$OpenBSD: tmpfs_specops.c,v 1.4 2013/12/23 20:35:19 tedu Exp $	*/
/*	$NetBSD: tmpfs_specops.c,v 1.10 2011/05/24 20:17:49 rmind Exp $	*/

/*
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * tmpfs vnode interface for special devices.
 */

#if 0
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tmpfs_specops.c,v 1.10 2011/05/24 20:17:49 rmind Exp $");
#endif

#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/specdev.h>
#include <tmpfs/tmpfs_vnops.h>

#include <tmpfs/tmpfs.h>

int	tmpfs_spec_close	(void *);
int	tmpfs_spec_read		(void *);
int	tmpfs_spec_write	(void *);

/*
 * vnode operations vector used for special devices stored in a tmpfs
 * file system.
 */

struct vops tmpfs_specvops = {
	.vop_close	= spec_close,
	.vop_access	= tmpfs_access,
	.vop_getattr	= tmpfs_getattr,
	.vop_setattr	= tmpfs_setattr,
	.vop_read	= tmpfs_spec_read,
	.vop_write	= tmpfs_spec_write,
	.vop_fsync	= spec_fsync,
	.vop_inactive	= tmpfs_inactive,
	.vop_reclaim	= tmpfs_reclaim,
	.vop_lock	= tmpfs_lock,
	.vop_unlock	= tmpfs_unlock,
	.vop_print	= tmpfs_print,
	.vop_islocked	= tmpfs_islocked,

	/* keep in sync with spec_vops */
	.vop_lookup	= vop_generic_lookup,
	.vop_create	= spec_badop,
	.vop_mknod	= spec_badop,
	.vop_open	= spec_open,
	.vop_ioctl	= spec_ioctl,
	.vop_poll	= spec_poll,
	.vop_kqfilter	= spec_kqfilter,
	.vop_revoke	= vop_generic_revoke,
	.vop_remove	= spec_badop,
	.vop_link	= spec_badop,
	.vop_rename	= spec_badop,
	.vop_mkdir	= spec_badop,
	.vop_rmdir	= spec_badop,
	.vop_symlink	= spec_badop,
	.vop_readdir	= spec_badop,
	.vop_readlink	= spec_badop,
	.vop_abortop	= spec_badop,
	.vop_bmap	= vop_generic_bmap,
	.vop_strategy	= spec_strategy,
	.vop_pathconf	= spec_pathconf,
	.vop_advlock	= spec_advlock,
	.vop_bwrite	= vop_generic_bwrite,
};

int
tmpfs_spec_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	tmpfs_update(VP_TO_TMPFS_NODE(vp), TMPFS_NODE_ACCESSED);
	return (spec_read(ap));
}

int
tmpfs_spec_write(void *v)
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	tmpfs_update(VP_TO_TMPFS_NODE(vp), TMPFS_NODE_MODIFIED);
	return (spec_write(ap));
}
