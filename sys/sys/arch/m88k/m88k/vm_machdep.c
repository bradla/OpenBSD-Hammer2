/*	$OpenBSD: vm_machdep.c,v 1.22 2013/01/16 19:04:43 miod Exp $	*/

/*
 * Copyright (c) 1998 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	from: Utah $Hdr: vm_machdep.c 1.21 91/04/06$
 *	from: @(#)vm_machdep.c	7.10 (Berkeley) 5/7/91
 *	vm_machdep.c,v 1.3 1993/07/07 07:09:32 cgd Exp
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/core.h>
#include <sys/exec.h>
#include <sys/ptrace.h>

#include <uvm/uvm_extern.h>

#include <machine/mmu.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/trap.h>

extern void savectx(struct pcb *);
extern void switch_exit(struct proc *);

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the kernel stack and pcb, making the child
 * ready to run, and marking it so that it can return differently
 * than the parent.  Returns 1 in the child process, 0 in the parent.
 * We currently double-map the user area so that the stack is at the same
 * address in each process; in the future we will probably relocate
 * the frame pointers on the stack after copying.
 */

void
cpu_fork(p1, p2, stack, stacksize, func, arg)
	struct proc *p1, *p2;
	void *stack;
	size_t stacksize;
	void (*func)(void *);
	void *arg;
{
	struct ksigframe {
		void (*func)(void *);
		void *arg;
	} *ksfp;
	extern void proc_trampoline(void);

	/* Copy pcb from p1 to p2. */
	if (p1 == curproc) {
		/* Sync the PCB before we copy it. */
		savectx(curpcb);
	}
#ifdef DIAGNOSTIC
	else if (p1 != &proc0)
		panic("cpu_fork: curproc");
#endif

	bcopy(&p1->p_addr->u_pcb, &p2->p_addr->u_pcb, sizeof(struct pcb));
	p2->p_md.md_tf = (struct trapframe *)USER_REGS(p2);

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		USER_REGS(p2)->r[31] = (u_int)stack + stacksize;

	ksfp = (struct ksigframe *)((char *)p2->p_addr + USPACE) - 1;
	ksfp->func = func;
	ksfp->arg = arg;

	/*
	 * When this process resumes, r31 will be ksfp and
	 * the process will be at the beginning of proc_trampoline().
	 * proc_trampoline will execute the function func, pop off
	 * ksfp frame, and resume to userland.
	 */

	p2->p_addr->u_pcb.kernel_state.pcb_sp = (u_int)ksfp;
	p2->p_addr->u_pcb.kernel_state.pcb_pc = (u_int)proc_trampoline;
}

/*
 * cpu_exit is called as the last action during exit.
 */
void
cpu_exit(struct proc *p)
{
	pmap_deactivate(p);
	sched_exit(p);
}

/*
 * Dump the machine specific header information at the start of a core dump.
 */
int
cpu_coredump(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	struct reg reg;
	struct coreseg cseg;
	int error;

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_MACHINE, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof(reg);

	/* Save registers. */
	error = process_read_regs(p, &reg);
	if (error)
		return error;

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_MACHINE, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE, IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&reg, sizeof(reg),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	chdr->c_nseg++;
	return 0;
}

/*
 * Map an IO request into kernel virtual address space via phys_map.
 */
void
vmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	caddr_t addr;
	vaddr_t ova, kva, off;
	paddr_t pa;
	struct pmap *pmap;
	u_int pg;

#ifdef DIAGNOSTIC
	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
#endif

	addr = (caddr_t)trunc_page((vaddr_t)(bp->b_saveaddr = bp->b_data));
	off = (vaddr_t)bp->b_saveaddr & PGOFSET;
	len = round_page(off + len);
	pmap = vm_map_pmap(&bp->b_proc->p_vmspace->vm_map);

	ova = kva = uvm_km_valloc_wait(phys_map, len);

	bp->b_data = (caddr_t)(kva + off);
	for (pg = atop(len); pg != 0; pg--) {
		if (pmap_extract(pmap, (vaddr_t)addr, &pa) == FALSE)
			panic("vmapbuf: null page frame");
		pmap_enter(vm_map_pmap(phys_map), kva, pa,
			   VM_PROT_READ | VM_PROT_WRITE,
			   VM_PROT_READ | VM_PROT_WRITE | PMAP_WIRED);
		addr += PAGE_SIZE;
		kva += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
}

/*
 * Free the io map PTEs associated with this IO operation.
 * We also restore the original b_addr.
 */
void
vunmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	vaddr_t addr, off;

#ifdef DIAGNOSTIC
	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
#endif

	addr = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data & PGOFSET;
	len = round_page(off + len);
	pmap_remove(vm_map_pmap(phys_map), addr, addr + len);
	pmap_update(vm_map_pmap(phys_map));
	uvm_km_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = 0;
}
