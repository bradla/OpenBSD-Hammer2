/*	$OpenBSD: uvm_mmap.c,v 1.94 2014/04/13 23:14:15 tedu Exp $	*/
/*	$NetBSD: uvm_mmap.c,v 1.49 2001/02/18 21:19:08 chs Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993 The Regents of the University of California.  
 * Copyright (c) 1988 University of Utah.
 * 
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Charles D. Cranor,
 *	Washington University, University of California, Berkeley and 
 *	its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: vm_mmap.c 1.6 91/10/21$
 *      @(#)vm_mmap.c   8.5 (Berkeley) 5/19/94
 * from: Id: uvm_mmap.c,v 1.1.2.14 1998/01/05 21:04:26 chuck Exp
 */

/*
 * uvm_mmap.c: system call interface into VM system, plus kernel vm_mmap
 * function.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/resourcevar.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/specdev.h>

#include <machine/exec.h>	/* for __LDPGSZ */

#include <sys/syscallargs.h>

#include <uvm/uvm.h>
#include <uvm/uvm_device.h>
#include <uvm/uvm_vnode.h>

/*
 * Page align addr and size, returning EINVAL on wraparound.
 */
#define ALIGN_ADDR(addr, size, pageoff)	do {				\
	pageoff = (addr & PAGE_MASK);					\
	if (pageoff != 0) {						\
		if (size > SIZE_MAX - pageoff)				\
			return (EINVAL);	/* wraparound */	\
		addr -= pageoff;					\
		size += pageoff;					\
	}								\
	if (size != 0) {						\
		size = (vsize_t)round_page(size);			\
		if (size == 0)						\
			return (EINVAL);	/* wraparound */	\
	}								\
} while (0)

/*
 * sys_mquery: provide mapping hints to applications that do fixed mappings
 *
 * flags: 0 or MAP_FIXED (MAP_FIXED - means that we insist on this addr and
 *	don't care about PMAP_PREFER or such)
 * addr: hint where we'd like to place the mapping.
 * size: size of the mapping
 * fd: fd of the file we want to map
 * off: offset within the file
 */
int
sys_mquery(struct proc *p, void *v, register_t *retval)
{
	struct sys_mquery_args /* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(int) prot;
		syscallarg(int) flags;
		syscallarg(int) fd;
		syscallarg(long) pad;
		syscallarg(off_t) pos;
	} */ *uap = v;
	struct file *fp;
	struct uvm_object *uobj;
	voff_t uoff;
	int error;
	vaddr_t vaddr;
	int flags = 0;
	vsize_t size;
	vm_prot_t prot;
	int fd;

	vaddr = (vaddr_t) SCARG(uap, addr);
	prot = SCARG(uap, prot);
	size = (vsize_t) SCARG(uap, len);
	fd = SCARG(uap, fd);

	if ((prot & VM_PROT_ALL) != prot)
		return (EINVAL);

	if (SCARG(uap, flags) & MAP_FIXED)
		flags |= UVM_FLAG_FIXED;

	if (fd >= 0) {
		if ((error = getvnode(p->p_fd, fd, &fp)) != 0)
			return (error);
		uobj = &((struct vnode *)fp->f_data)->v_uvm.u_obj;
		uoff = SCARG(uap, pos);
	} else {
		fp = NULL;
		uobj = NULL;
		uoff = UVM_UNKNOWN_OFFSET;
	}

	if (vaddr == 0)
		vaddr = uvm_map_hint(p->p_vmspace, prot);

	error = uvm_map_mquery(&p->p_vmspace->vm_map, &vaddr, size, uoff,
	    flags);
	if (error == 0)
		*retval = (register_t)(vaddr);

	if (fp != NULL)
		FRELE(fp, p);
	return (error);
}

/*
 * sys_mincore: determine if pages are in core or not.
 */
/* ARGSUSED */
int
sys_mincore(struct proc *p, void *v, register_t *retval)
{
	struct sys_mincore_args /* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(char *) vec;
	} */ *uap = v;
	vm_page_t m;
	char *vec, *pgi, *pgs;
	struct uvm_object *uobj;
	struct vm_amap *amap;
	struct vm_anon *anon;
	vm_map_entry_t entry, next;
	vaddr_t start, end, lim;
	vm_map_t map;
	vsize_t len, npgs;
	int error = 0; 

	map = &p->p_vmspace->vm_map;

	start = (vaddr_t)SCARG(uap, addr);
	len = SCARG(uap, len);
	vec = SCARG(uap, vec);

	if (start & PAGE_MASK)
		return (EINVAL);
	len = round_page(len);
	end = start + len;
	if (end <= start)
		return (EINVAL);

	npgs = len >> PAGE_SHIFT;

	/*
 	 * < art> Anyone trying to mincore more than 4GB of address space is
	 *	clearly insane.
	 */
	if (npgs >= (0xffffffff >> PAGE_SHIFT))
		return (E2BIG);
	pgs = malloc(sizeof(*pgs) * npgs, M_TEMP, M_WAITOK | M_CANFAIL);
	if (pgs == NULL)
		return (ENOMEM);
	pgi = pgs;

	/*
	 * Lock down vec, so our returned status isn't outdated by
	 * storing the status byte for a page.
	 */
	if ((error = uvm_vslock(p, vec, npgs, VM_PROT_WRITE)) != 0) {
		free(pgs, M_TEMP);
		return (error);
	}

	vm_map_lock_read(map);

	if (uvm_map_lookup_entry(map, start, &entry) == FALSE) {
		error = ENOMEM;
		goto out;
	}

	for (/* nothing */;
	     entry != NULL && entry->start < end;
	     entry = RB_NEXT(uvm_map_addr, &map->addr, entry)) {
		KASSERT(!UVM_ET_ISSUBMAP(entry));
		KASSERT(start >= entry->start);

		/* Make sure there are no holes. */
		next = RB_NEXT(uvm_map_addr, &map->addr, entry);
		if (entry->end < end &&
		     (next == NULL ||
		      next->start > entry->end)) {
			error = ENOMEM;
			goto out;
		}

		lim = end < entry->end ? end : entry->end;

		/*
		 * Special case for objects with no "real" pages.  Those
		 * are always considered resident (mapped devices).
		 */
		if (UVM_ET_ISOBJ(entry)) {
			KASSERT(!UVM_OBJ_IS_KERN_OBJECT(entry->object.uvm_obj));
			if (entry->object.uvm_obj->pgops->pgo_fault != NULL) {
				for (/* nothing */; start < lim;
				     start += PAGE_SIZE, pgi++)
					*pgi = 1;
				continue;
			}
		}

		amap = entry->aref.ar_amap;	/* top layer */
		uobj = entry->object.uvm_obj;	/* bottom layer */

		for (/* nothing */; start < lim; start += PAGE_SIZE, pgi++) {
			*pgi = 0;
			if (amap != NULL) {
				/* Check the top layer first. */
				anon = amap_lookup(&entry->aref,
				    start - entry->start);
				if (anon != NULL && anon->an_page != NULL) {
					/*
					 * Anon has the page for this entry
					 * offset.
					 */
					*pgi = 1;
				}
			}

			if (uobj != NULL && *pgi == 0) {
				/* Check the bottom layer. */
				m = uvm_pagelookup(uobj,
				    entry->offset + (start - entry->start));
				if (m != NULL) {
					/*
					 * Object has the page for this entry
					 * offset.
					 */
					*pgi = 1;
				}
			}
		}
	}

 out:
	vm_map_unlock_read(map);
	uvm_vsunlock(p, SCARG(uap, vec), npgs);
	/* now the map is unlocked we can copyout without fear. */
	if (error == 0)
		copyout(pgs, vec, npgs * sizeof(char));
	free(pgs, M_TEMP);
	return (error);
}

/*
 * sys_mmap: mmap system call.
 *
 * => file offset and address may not be page aligned
 *    - if MAP_FIXED, offset and address must have remainder mod PAGE_SIZE
 *    - if address isn't page aligned the mapping starts at trunc_page(addr)
 *      and the return value is adjusted up by the page offset.
 */
int
sys_mmap(struct proc *p, void *v, register_t *retval)
{
	struct sys_mmap_args /* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(int) prot;
		syscallarg(int) flags;
		syscallarg(int) fd;
		syscallarg(long) pad;
		syscallarg(off_t) pos;
	} */ *uap = v;
	vaddr_t addr;
	struct vattr va;
	off_t pos;
	vsize_t size, pageoff;
	vm_prot_t prot, maxprot;
	int flags, fd;
	vaddr_t vm_min_address = VM_MIN_ADDRESS;
	struct filedesc *fdp = p->p_fd;
	struct file *fp = NULL;
	struct vnode *vp;
	caddr_t handle;
	int error;

	/* first, extract syscall args from the uap. */
	addr = (vaddr_t) SCARG(uap, addr);
	size = (vsize_t) SCARG(uap, len);
	prot = SCARG(uap, prot);
	flags = SCARG(uap, flags);
	fd = SCARG(uap, fd);
	pos = SCARG(uap, pos);

	/*
	 * Fixup the old deprecated MAP_COPY into MAP_PRIVATE, and
	 * validate the flags.
	 */
	if ((prot & VM_PROT_ALL) != prot)
		return (EINVAL);
	if ((flags & MAP_FLAGMASK) != flags)
		return (EINVAL);
	if (flags & MAP_COPY)
		flags = (flags & ~MAP_COPY) | MAP_PRIVATE;
	if ((flags & (MAP_SHARED|MAP_PRIVATE)) == (MAP_SHARED|MAP_PRIVATE))
		return (EINVAL);
	if ((flags & (MAP_FIXED|__MAP_NOREPLACE)) == __MAP_NOREPLACE)
		return (EINVAL);
	if (size == 0)
		return (EINVAL);

	/* align file position and save offset.  adjust size. */
	ALIGN_ADDR(pos, size, pageoff);

	/* now check (MAP_FIXED) or get (!MAP_FIXED) the "addr" */
	if (flags & MAP_FIXED) {
		/* adjust address by the same amount as we did the offset */
		addr -= pageoff;
		if (addr & PAGE_MASK)
			return (EINVAL);		/* not page aligned */

		if (addr > SIZE_MAX - size)
			return (EINVAL);		/* no wrapping! */
		if (VM_MAXUSER_ADDRESS > 0 &&
		    (addr + size) > VM_MAXUSER_ADDRESS)
			return (EINVAL);
		if (vm_min_address > 0 && addr < vm_min_address)
			return (EINVAL);

	}

	/* check for file mappings (i.e. not anonymous) and verify file. */
	if ((flags & MAP_ANON) == 0) {
		if ((fp = fd_getfile(fdp, fd)) == NULL)
			return (EBADF);

		FREF(fp);

		if (fp->f_type != DTYPE_VNODE) {
			error = ENODEV;		/* only mmap vnodes! */
			goto out;
		}
		vp = (struct vnode *)fp->f_data;	/* convert to vnode */

		if (vp->v_type != VREG && vp->v_type != VCHR &&
		    vp->v_type != VBLK) {
			error = ENODEV; /* only REG/CHR/BLK support mmap */
			goto out;
		}

		if (vp->v_type == VREG && (pos + size) < pos) {
			error = EINVAL;		/* no offset wrapping */
			goto out;
		}

		/* special case: catch SunOS style /dev/zero */
		if (vp->v_type == VCHR && iszerodev(vp->v_rdev)) {
			flags |= MAP_ANON;
			FRELE(fp, p);
			fp = NULL;
			goto is_anon;
		}

		/*
		 * Old programs may not select a specific sharing type, so
		 * default to an appropriate one.
		 *
		 * XXX: how does MAP_ANON fit in the picture?
		 */
		if ((flags & (MAP_SHARED|MAP_PRIVATE)) == 0) {
#if defined(DEBUG)
			printf("WARNING: defaulted mmap() share type to "
			   "%s (pid %d comm %s)\n", vp->v_type == VCHR ?
			   "MAP_SHARED" : "MAP_PRIVATE", p->p_pid,
			    p->p_comm);
#endif
			if (vp->v_type == VCHR)
				flags |= MAP_SHARED;	/* for a device */
			else
				flags |= MAP_PRIVATE;	/* for a file */
		}

		/* 
		 * MAP_PRIVATE device mappings don't make sense (and aren't
		 * supported anyway).  However, some programs rely on this,
		 * so just change it to MAP_SHARED.
		 */
		if (vp->v_type == VCHR && (flags & MAP_PRIVATE) != 0) {
			flags = (flags & ~MAP_PRIVATE) | MAP_SHARED;
		}

		/* now check protection */
		maxprot = VM_PROT_EXECUTE;

		/* check read access */
		if (fp->f_flag & FREAD)
			maxprot |= VM_PROT_READ;
		else if (prot & PROT_READ) {
			error = EACCES;
			goto out;
		}

		/* check write access, shared case first */
		if (flags & MAP_SHARED) {
			/*
			 * if the file is writable, only add PROT_WRITE to
			 * maxprot if the file is not immutable, append-only.
			 * otherwise, if we have asked for PROT_WRITE, return
			 * EPERM.
			 */
			if (fp->f_flag & FWRITE) {
				if ((error =
				    VOP_GETATTR(vp, &va, p->p_ucred, p)))
					goto out;
				if ((va.va_flags & (IMMUTABLE|APPEND)) == 0)
					maxprot |= VM_PROT_WRITE;
				else if (prot & PROT_WRITE) {
					error = EPERM;
					goto out;
				}
			} else if (prot & PROT_WRITE) {
				error = EACCES;
				goto out;
			}
		} else {
			/* MAP_PRIVATE mappings can always write to */
			maxprot |= VM_PROT_WRITE;
		}

		/* set handle to vnode */
		handle = (caddr_t)vp;
	} else {		/* MAP_ANON case */
		/*
		 * XXX What do we do about (MAP_SHARED|MAP_PRIVATE) == 0?
		 */
		if (fd != -1) {
			error = EINVAL;
			goto out;
		}

is_anon:		/* label for SunOS style /dev/zero */
		handle = NULL;
		maxprot = VM_PROT_ALL;
		pos = 0;
	}

	if ((flags & MAP_ANON) != 0 ||
	    ((flags & MAP_PRIVATE) != 0 && (prot & PROT_WRITE) != 0)) {
		if (size >
		    (p->p_rlimit[RLIMIT_DATA].rlim_cur - ptoa(p->p_vmspace->vm_dused))) {
			error = ENOMEM;
			goto out;
		}
	}

	/* now let kernel internal function uvm_mmap do the work. */
	error = uvm_mmap(&p->p_vmspace->vm_map, &addr, size, prot, maxprot,
	    flags, handle, pos, p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur, p);

	if (error == 0)
		/* remember to add offset */
		*retval = (register_t)(addr + pageoff);

out:
	if (fp)
		FRELE(fp, p);	
	return (error);
}

/*
 * sys_msync: the msync system call (a front-end for flush)
 */

int
sys_msync(struct proc *p, void *v, register_t *retval)
{
	struct sys_msync_args /* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(int) flags;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	vm_map_t map;
	int flags, uvmflags;

	/* extract syscall args from the uap */
	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	flags = SCARG(uap, flags);

	/* sanity check flags */
	if ((flags & ~(MS_ASYNC | MS_SYNC | MS_INVALIDATE)) != 0 ||
			(flags & (MS_ASYNC | MS_SYNC | MS_INVALIDATE)) == 0 ||
			(flags & (MS_ASYNC | MS_SYNC)) == (MS_ASYNC | MS_SYNC))
		return (EINVAL);
	if ((flags & (MS_ASYNC | MS_SYNC)) == 0)
		flags |= MS_SYNC;

	/* align the address to a page boundary, and adjust the size accordingly */
	ALIGN_ADDR(addr, size, pageoff);
	if (addr > SIZE_MAX - size)
		return (EINVAL);		/* disallow wrap-around. */

	/* get map */
	map = &p->p_vmspace->vm_map;

	/* translate MS_ flags into PGO_ flags */
	uvmflags = PGO_CLEANIT;
	if (flags & MS_INVALIDATE)
		uvmflags |= PGO_FREE;
	if (flags & MS_SYNC)
		uvmflags |= PGO_SYNCIO;
	else
		uvmflags |= PGO_SYNCIO;	 /* XXXCDC: force sync for now! */

	return (uvm_map_clean(map, addr, addr+size, uvmflags));
}

/*
 * sys_munmap: unmap a users memory
 */
int
sys_munmap(struct proc *p, void *v, register_t *retval)
{
	struct sys_munmap_args /* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	vm_map_t map;
	vaddr_t vm_min_address = VM_MIN_ADDRESS;
	struct uvm_map_deadq dead_entries;

	/* get syscall args... */
	addr = (vaddr_t) SCARG(uap, addr);
	size = (vsize_t) SCARG(uap, len);
	
	/* align address to a page boundary, and adjust size accordingly */
	ALIGN_ADDR(addr, size, pageoff);

	/*
	 * Check for illegal addresses.  Watch out for address wrap...
	 * Note that VM_*_ADDRESS are not constants due to casts (argh).
	 */
	if (addr > SIZE_MAX - size)
		return (EINVAL);
	if (VM_MAXUSER_ADDRESS > 0 && addr + size > VM_MAXUSER_ADDRESS)
		return (EINVAL);
	if (vm_min_address > 0 && addr < vm_min_address)
		return (EINVAL);
	map = &p->p_vmspace->vm_map;


	vm_map_lock(map);	/* lock map so we can checkprot */

	/*
	 * interesting system call semantic: make sure entire range is 
	 * allocated before allowing an unmap.
	 */
	if (!uvm_map_checkprot(map, addr, addr + size, VM_PROT_NONE)) {
		vm_map_unlock(map);
		return (EINVAL);
	}

	TAILQ_INIT(&dead_entries);
	uvm_unmap_remove(map, addr, addr + size, &dead_entries, FALSE, TRUE);

	vm_map_unlock(map);	/* and unlock */

	uvm_unmap_detach(&dead_entries, 0);

	return (0);
}

/*
 * sys_mprotect: the mprotect system call
 */
int
sys_mprotect(struct proc *p, void *v, register_t *retval)
{
	struct sys_mprotect_args /* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(int) prot;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	vm_prot_t prot;

	/*
	 * extract syscall args from uap
	 */

	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	prot = SCARG(uap, prot);
	
	if ((prot & VM_PROT_ALL) != prot)
		return (EINVAL);

	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */
	ALIGN_ADDR(addr, size, pageoff);
	if (addr > SIZE_MAX - size)
		return (EINVAL);		/* disallow wrap-around. */

	return (uvm_map_protect(&p->p_vmspace->vm_map, addr, addr+size,
	    prot, FALSE));
}

/*
 * sys_minherit: the minherit system call
 */
int
sys_minherit(struct proc *p, void *v, register_t *retval)
{
	struct sys_minherit_args /* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(int) inherit;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	vm_inherit_t inherit;
	
	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	inherit = SCARG(uap, inherit);

	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */
	ALIGN_ADDR(addr, size, pageoff);
	if (addr > SIZE_MAX - size)
		return (EINVAL);		/* disallow wrap-around. */
	
	return (uvm_map_inherit(&p->p_vmspace->vm_map, addr, addr+size,
	    inherit));
}

/*
 * sys_madvise: give advice about memory usage.
 */
/* ARGSUSED */
int
sys_madvise(struct proc *p, void *v, register_t *retval)
{
	struct sys_madvise_args /* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(int) behav;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	int advice, error;
	
	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	advice = SCARG(uap, behav);

	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */
	ALIGN_ADDR(addr, size, pageoff);
	if (addr > SIZE_MAX - size)
		return (EINVAL);		/* disallow wrap-around. */

	switch (advice) {
	case MADV_NORMAL:
	case MADV_RANDOM:
	case MADV_SEQUENTIAL:
		error = uvm_map_advice(&p->p_vmspace->vm_map, addr,
		    addr + size, advice);
		break;

	case MADV_WILLNEED:
		/*
		 * Activate all these pages, pre-faulting them in if
		 * necessary.
		 */
		/*
		 * XXX IMPLEMENT ME.
		 * Should invent a "weak" mode for uvm_fault()
		 * which would only do the PGO_LOCKED pgo_get().
		 */
		return (0);

	case MADV_DONTNEED:
		/*
		 * Deactivate all these pages.  We don't need them
		 * any more.  We don't, however, toss the data in
		 * the pages.
		 */
		error = uvm_map_clean(&p->p_vmspace->vm_map, addr, addr + size,
		    PGO_DEACTIVATE);
		break;

	case MADV_FREE:
		/*
		 * These pages contain no valid data, and may be
		 * garbage-collected.  Toss all resources, including
		 * any swap space in use.
		 */
		error = uvm_map_clean(&p->p_vmspace->vm_map, addr, addr + size,
		    PGO_FREE);
		break;

	case MADV_SPACEAVAIL:
		/*
		 * XXXMRG What is this?  I think it's:
		 *
		 *	Ensure that we have allocated backing-store
		 *	for these pages.
		 *
		 * This is going to require changes to the page daemon,
		 * as it will free swap space allocated to pages in core.
		 * There's also what to do for device/file/anonymous memory.
		 */
		return (EINVAL);

	default:
		return (EINVAL);
	}

	return (error);
}

/*
 * sys_mlock: memory lock
 */

int
sys_mlock(struct proc *p, void *v, register_t *retval)
{
	struct sys_mlock_args /* {
		syscallarg(const void *) addr;
		syscallarg(size_t) len;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	int error;

	/* extract syscall args from uap */
	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);

	/* align address to a page boundary and adjust size accordingly */
	ALIGN_ADDR(addr, size, pageoff);
	if (addr > SIZE_MAX - size)
		return (EINVAL);		/* disallow wrap-around. */

	if (atop(size) + uvmexp.wired > uvmexp.wiredmax)
		return (EAGAIN);

#ifdef pmap_wired_count
	if (size + ptoa(pmap_wired_count(vm_map_pmap(&p->p_vmspace->vm_map))) >
			p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur)
		return (EAGAIN);
#else
	if ((error = suser(p, 0)) != 0)
		return (error);
#endif

	error = uvm_map_pageable(&p->p_vmspace->vm_map, addr, addr+size, FALSE,
	    0);
	return (error == 0 ? 0 : ENOMEM);
}

/*
 * sys_munlock: unlock wired pages
 */

int
sys_munlock(struct proc *p, void *v, register_t *retval)
{
	struct sys_munlock_args /* {
		syscallarg(const void *) addr;
		syscallarg(size_t) len;
	} */ *uap = v;
	vaddr_t addr;
	vsize_t size, pageoff;
	int error;

	/* extract syscall args from uap */
	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);

	/* align address to a page boundary, and adjust size accordingly */
	ALIGN_ADDR(addr, size, pageoff);
	if (addr > SIZE_MAX - size)
		return (EINVAL);		/* disallow wrap-around. */

#ifndef pmap_wired_count
	if ((error = suser(p, 0)) != 0)
		return (error);
#endif

	error = uvm_map_pageable(&p->p_vmspace->vm_map, addr, addr+size, TRUE,
	    0);
	return (error == 0 ? 0 : ENOMEM);
}

/*
 * sys_mlockall: lock all pages mapped into an address space.
 */
int
sys_mlockall(struct proc *p, void *v, register_t *retval)
{
	struct sys_mlockall_args /* {
		syscallarg(int) flags;
	} */ *uap = v;
	int error, flags;

	flags = SCARG(uap, flags);

	if (flags == 0 ||
	    (flags & ~(MCL_CURRENT|MCL_FUTURE)) != 0)
		return (EINVAL);

#ifndef pmap_wired_count
	if ((error = suser(p, 0)) != 0)
		return (error);
#endif

	error = uvm_map_pageable_all(&p->p_vmspace->vm_map, flags,
	    p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur);
	if (error != 0 && error != ENOMEM)
		return (EAGAIN);
	return (error);
}

/*
 * sys_munlockall: unlock all pages mapped into an address space.
 */
int
sys_munlockall(struct proc *p, void *v, register_t *retval)
{

	(void) uvm_map_pageable_all(&p->p_vmspace->vm_map, 0, 0);
	return (0);
}

/*
 * uvm_mmap: internal version of mmap
 *
 * - used by sys_mmap, exec, and sysv shm
 * - handle is a vnode pointer or NULL for MAP_ANON (XXX: not true,
 *	sysv shm uses "named anonymous memory")
 * - caller must page-align the file offset
 */
int
uvm_mmap(vm_map_t map, vaddr_t *addr, vsize_t size, vm_prot_t prot,
    vm_prot_t maxprot, int flags, caddr_t handle, voff_t foff,
    vsize_t locklimit, struct proc *p)
{
	struct uvm_object *uobj;
	struct vnode *vp;
	int error;
	int advice = UVM_ADV_NORMAL;
	uvm_flag_t uvmflag = 0;
	vsize_t align = 0;	/* userland page size */

	/* check params */
	if (size == 0)
		return(0);
	if (foff & PAGE_MASK)
		return(EINVAL);
	if ((prot & maxprot) != prot)
		return(EINVAL);

	/*
	 * for non-fixed mappings, round off the suggested address.
	 * for fixed mappings, check alignment and zap old mappings.
	 */
	if ((flags & MAP_FIXED) == 0) {
		*addr = round_page(*addr);	/* round */
	} else {
		if (*addr & PAGE_MASK)
			return(EINVAL);

		uvmflag |= UVM_FLAG_FIXED;
		if ((flags & __MAP_NOREPLACE) == 0)
			uvm_unmap(map, *addr, *addr + size);	/* zap! */
	}

	/*
	 * handle anon vs. non-anon mappings.   for non-anon mappings attach
	 * to underlying vm object.
	 */
	if (flags & MAP_ANON) {
		if ((flags & MAP_FIXED) == 0 && size >= __LDPGSZ)
			align = __LDPGSZ;
		foff = UVM_UNKNOWN_OFFSET;
		uobj = NULL;
		if ((flags & MAP_SHARED) == 0)
			/* XXX: defer amap create */
			uvmflag |= UVM_FLAG_COPYONW;
		else
			/* shared: create amap now */
			uvmflag |= UVM_FLAG_OVERLAY;
	} else {
		vp = (struct vnode *) handle;	/* get vnode */
		if (vp->v_type != VCHR) {
			uobj = uvn_attach((void *) vp, (flags & MAP_SHARED) ?
			   maxprot : (maxprot & ~VM_PROT_WRITE));

#ifndef UBC
			/*
			 * XXXCDC: hack from old code
			 * don't allow vnodes which have been mapped
			 * shared-writeable to persist [forces them to be
			 * flushed out when last reference goes].
			 * XXXCDC: interesting side effect: avoids a bug.
			 * note that in WRITE [ufs_readwrite.c] that we
			 * allocate buffer, uncache, and then do the write.
			 * the problem with this is that if the uncache causes
			 * VM data to be flushed to the same area of the file
			 * we are writing to... in that case we've got the
			 * buffer locked and our process goes to sleep forever.
			 *
			 * XXXCDC: checking maxprot protects us from the
			 * "persistbug" program but this is not a long term
			 * solution.
			 * 
			 * XXXCDC: we don't bother calling uncache with the vp
			 * VOP_LOCKed since we know that we are already
			 * holding a valid reference to the uvn (from the
			 * uvn_attach above), and thus it is impossible for
			 * the uncache to kill the uvn and trigger I/O.
			 */
			if (flags & MAP_SHARED) {
				if ((prot & VM_PROT_WRITE) ||
				    (maxprot & VM_PROT_WRITE)) {
					uvm_vnp_uncache(vp);
				}
			}
#else
			/* XXX for now, attach doesn't gain a ref */
			vref(vp);
#endif
		} else {
			uobj = udv_attach((void *) &vp->v_rdev,
			    (flags & MAP_SHARED) ? maxprot :
			    (maxprot & ~VM_PROT_WRITE), foff, size);
			/*
			 * XXX Some devices don't like to be mapped with
			 * XXX PROT_EXEC, but we don't really have a
			 * XXX better way of handling this, right now
			 */
			if (uobj == NULL && (prot & PROT_EXEC) == 0) {
				maxprot &= ~VM_PROT_EXECUTE;
				uobj = udv_attach((void *) &vp->v_rdev,
				    (flags & MAP_SHARED) ? maxprot :
				    (maxprot & ~VM_PROT_WRITE), foff, size);
			}
			advice = UVM_ADV_RANDOM;
		}
		
		if (uobj == NULL)
			return((vp->v_type == VREG) ? ENOMEM : EINVAL);

		if ((flags & MAP_SHARED) == 0)
			uvmflag |= UVM_FLAG_COPYONW;
	}

	/* set up mapping flags */
	uvmflag = UVM_MAPFLAG(prot, maxprot, 
			(flags & MAP_SHARED) ? UVM_INH_SHARE : UVM_INH_COPY,
			advice, uvmflag);

	error = uvm_map(map, addr, size, uobj, foff, align, uvmflag);

	if (error == 0) {
		/*
		 * POSIX 1003.1b -- if our address space was configured
		 * to lock all future mappings, wire the one we just made.
		 */
		if (prot == VM_PROT_NONE) {
			/*
			 * No more work to do in this case.
			 */
			return (0);
		}
		
		vm_map_lock(map);

		if (map->flags & VM_MAP_WIREFUTURE) {
			if ((atop(size) + uvmexp.wired) > uvmexp.wiredmax
#ifdef pmap_wired_count
			    || (locklimit != 0 && (size +
			         ptoa(pmap_wired_count(vm_map_pmap(map)))) >
			        locklimit)
#endif
			) {
				error = ENOMEM;
				vm_map_unlock(map);
				/* unmap the region! */
				uvm_unmap(map, *addr, *addr + size);
				goto bad;
			}
			/*
			 * uvm_map_pageable() always returns the map
			 * unlocked.
			 */
			error = uvm_map_pageable(map, *addr, *addr + size,
			    FALSE, UVM_LK_ENTER);
			if (error != 0) {
				/* unmap the region! */
				uvm_unmap(map, *addr, *addr + size);
				goto bad;
			}
			return (0);
		}

		vm_map_unlock(map);

		return (0);
	}

	/* errors: first detach from the uobj, if any.  */
	if (uobj)
		uobj->pgops->pgo_detach(uobj);

bad:
	return (error);
}
