/*	$OpenBSD: vm_machdep.c,v 1.46 2013/01/16 19:04:43 miod Exp $	*/
/*	$NetBSD: vm_machdep.c,v 1.1 1996/09/30 16:34:57 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/core.h>
#include <sys/exec.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/ptrace.h>

#include <uvm/uvm_extern.h>

#include <machine/pcb.h>
#include <machine/fpu.h>

/*
 * Finish a fork operation, with process p2 nearly set up.
 */
void
cpu_fork(struct proc *p1, struct proc *p2, void *stack, size_t stacksize,
    void (*func)(void *), void *arg)
{
	struct trapframe *tf;
	struct callframe *cf;
	struct switchframe *sf;
	caddr_t stktop1, stktop2;
	extern void fork_trampoline(void);
	struct pcb *pcb = &p2->p_addr->u_pcb;
	struct cpu_info *ci = curcpu();

	if (p1 == ci->ci_fpuproc)
		save_fpu();
	*pcb = p1->p_addr->u_pcb;
	
#ifdef ALTIVEC
	if (p1->p_addr->u_pcb.pcb_vr != NULL) {
		if (p1 == ci->ci_vecproc)
			save_vec(p1);
		pcb->pcb_vr = pool_get(&ppc_vecpl, PR_WAITOK);
		*pcb->pcb_vr = *p1->p_addr->u_pcb.pcb_vr;
	} else
		pcb->pcb_vr = NULL;

#endif /* ALTIVEC */

	pcb->pcb_pm = p2->p_vmspace->vm_map.pmap;

	pmap_extract(pmap_kernel(),
	    (vaddr_t)pcb->pcb_pm, (paddr_t *)&pcb->pcb_pmreal);
	
	/*
	 * Setup the trap frame for the new process
	 */
	stktop1 = (caddr_t)trapframe(p1);
	stktop2 = (caddr_t)trapframe(p2);
	bcopy(stktop1, stktop2, sizeof(struct trapframe));

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL) {
		tf = trapframe(p2);
		tf->fixreg[1] = (register_t)stack + stacksize;
	}

	stktop2 = (caddr_t)((u_long)stktop2 & ~15);  /* Align stack pointer */
	
	/*
	 * There happens to be a callframe, too.
	 */
	cf = (struct callframe *)stktop2;
	cf->lr = (int)fork_trampoline;
	
	/*
	 * Below the trap frame, there is another call frame:
	 */
	stktop2 -= 16;
	cf = (struct callframe *)stktop2;
	cf->r31 = (register_t)func;
	cf->r30 = (register_t)arg;
	
	/*
	 * Below that, we allocate the switch frame:
	 */
	/* must match SFRAMELEN in genassym */
	stktop2 -= roundup(sizeof *sf, 16);

	sf = (struct switchframe *)stktop2;
	bzero((void *)sf, sizeof *sf);		/* just in case */
	sf->sp = (int)cf;
	sf->user_sr = pmap_kernel()->pm_sr[PPC_USER_SR]; /* just in case */
	pcb->pcb_sp = (int)stktop2;
}

/*
 * cpu_exit is called as the last action during exit.
 * We release the address space and machine-dependent resources,
 * including the memory for the user structure and kernel stack.
 *
 * Since we don't have curproc anymore, we cannot sleep, and therefor
 * this is at least incorrect for the multiprocessor version.
 * Not sure whether we can get away with this in the single proc version.		XXX
 */
void
cpu_exit(struct proc *p)
{
	struct cpu_info *ci = curcpu();
#ifdef ALTIVEC
	struct pcb *pcb = &p->p_addr->u_pcb;
#endif /* ALTIVEC */
	
	/* XXX What if the state is on the other cpu? */
	if (p == ci->ci_fpuproc) 	/* release the fpu */
		ci->ci_fpuproc = NULL;

#ifdef ALTIVEC
	/* XXX What if the state is on the other cpu? */
	if (p == ci->ci_vecproc)
		ci->ci_vecproc = NULL; 	/* release the Altivec Unit */
	if (pcb->pcb_vr != NULL)
		pool_put(&ppc_vecpl, pcb->pcb_vr);
#endif /* ALTIVEC */
	
	pmap_deactivate(p);
	sched_exit(p);
}

/*
 * Write the machine-dependent part of a core dump.
 */
int
cpu_coredump(struct proc *p, struct vnode *vp, struct ucred *cred,
    struct core *chdr)
{
	struct coreseg cseg;
	struct md_coredump md_core;
	int error;
	
	CORE_SETMAGIC(*chdr, COREMAGIC, MID_POWERPC, 0);
	chdr->c_hdrsize = ALIGN(sizeof *chdr);
	chdr->c_seghdrsize = ALIGN(sizeof cseg);
	chdr->c_cpusize = sizeof md_core;

	process_read_regs(p, &(md_core.regs));
	
	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_POWERPC, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE, IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&md_core, sizeof md_core,
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	chdr->c_nseg++;
	return 0;
}

/*
 * Map an IO request into kernel virtual address space.
 */
void
vmapbuf(struct buf *bp, vsize_t len)
{
	vaddr_t faddr, taddr, off;
	paddr_t pa;
	
#ifdef	DIAGNOSTIC
	if (!(bp->b_flags & B_PHYS))
		panic("vmapbuf");
#endif
	faddr = trunc_page((vaddr_t)(bp->b_saveaddr = bp->b_data));
	off = (vaddr_t)bp->b_data - faddr;
	len = round_page(off + len);
	taddr = uvm_km_valloc_wait(phys_map, len);
	bp->b_data = (caddr_t)(taddr + off);
	for (; len > 0; len -= NBPG) {
		pmap_extract(vm_map_pmap(&bp->b_proc->p_vmspace->vm_map),
		    faddr, &pa);
		pmap_enter(vm_map_pmap(phys_map), taddr, pa,
		    VM_PROT_READ | VM_PROT_WRITE, PMAP_WIRED);
		faddr += NBPG;
		taddr += NBPG;
	}
	pmap_update(vm_map_pmap(phys_map));
}

/*
 * Free the io map addresses associated with this IO operation.
 */
void
vunmapbuf(struct buf *bp, vsize_t len)
{
	vaddr_t addr, off;
	
#ifdef	DIAGNOSTIC
	if (!(bp->b_flags & B_PHYS))
		panic("vunmapbuf");
#endif
	addr = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data - addr;
	len = round_page(off + len);
	pmap_remove(vm_map_pmap(phys_map), addr, addr + len);
	pmap_update(vm_map_pmap(phys_map));
	uvm_km_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = 0;
}
