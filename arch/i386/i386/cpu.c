/*	$OpenBSD: cpu.c,v 1.55 2014/03/29 18:09:29 guenther Exp $	*/
/* $NetBSD: cpu.c,v 1.1.2.7 2000/06/26 02:04:05 sommerfeld Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by RedBack Networks Inc.
 *
 * Author: Bill Sommerfeld
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
 * Copyright (c) 1999 Stefan Grefen
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR AND CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "lapic.h"
#include "ioapic.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/memrange.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/cpuvar.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/mpbiosvar.h>
#include <machine/npx.h>
#include <machine/pcb.h>
#include <machine/specialreg.h>
#include <machine/segments.h>
#include <machine/gdt.h>
#include <machine/pio.h>
#include <dev/rndvar.h>

#if NLAPIC > 0
#include <machine/apicvar.h>
#include <machine/i82489reg.h>
#include <machine/i82489var.h>
#endif

#if NIOAPIC > 0
#include <machine/i82093reg.h>
#include <machine/i82093var.h>
#endif

#include <dev/ic/mc146818reg.h>
#include <i386/isa/nvram.h>
#include <dev/isa/isareg.h>

int     cpu_match(struct device *, void *, void *);
void    cpu_attach(struct device *, struct device *, void *);
void	patinit(struct cpu_info *ci);
void	cpu_idle_mwait_cycle(void);
void	cpu_init_mwait(struct device *);
void	cpu_enable_mwait(void);

#ifdef MULTIPROCESSOR
int mp_cpu_start(struct cpu_info *);
void mp_cpu_start_cleanup(struct cpu_info *);
struct cpu_functions mp_cpu_funcs =
    { mp_cpu_start, NULL, mp_cpu_start_cleanup };
#endif

/*
 * Statically-allocated CPU info for the primary CPU (or the only
 * CPU, on uniprocessors).  The CPU info list is initialized to
 * point at it.
 */
struct cpu_info cpu_info_primary;
struct cpu_info *cpu_info_list = &cpu_info_primary;

void	cpu_init_tss(struct i386tss *, void *, void *);
void	cpu_set_tss_gates(struct cpu_info *);

#ifdef MULTIPROCESSOR
/*
 * Array of CPU info structures.  Must be statically-allocated because
 * curproc, etc. are used early.
 */

struct cpu_info *cpu_info[MAXCPUS] = { &cpu_info_primary };

void   	cpu_hatch(void *);
void   	cpu_boot_secondary(struct cpu_info *);
void	cpu_copy_trampoline(void);

/*
 * Runs once per boot once multiprocessor goo has been detected and
 * the local APIC has been mapped.
 * Called from mpbios_scan();
 */
void
cpu_init_first()
{
	cpu_copy_trampoline();
}
#endif

struct cfattach cpu_ca = {
	sizeof(struct cpu_info), cpu_match, cpu_attach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL /* XXX DV_CPU */
};

#ifndef SMALL_KERNEL
void	replacesmap(void);

extern int _copyout_stac;
extern int _copyout_clac;
extern int _copyin_stac;
extern int _copyin_clac;
extern int _copy_fault_clac;
extern int _copyoutstr_stac;
extern int _copyinstr_stac;
extern int _copystr_fault_clac;
extern int _ucas_32_stac;
extern int _ucas_32_clac;
extern int _stac;
extern int _clac;

static const struct {
	void *daddr;
	void *saddr;
} ireplace[] = {
	{ &_copyout_stac, &_stac },
	{ &_copyout_clac, &_clac },
	{ &_copyin_stac, &_stac },
	{ &_copyin_clac, &_clac },
	{ &_copy_fault_clac, &_clac },
	{ &_copyoutstr_stac, &_stac },
	{ &_copyinstr_stac, &_stac },
	{ &_copystr_fault_clac, &_clac },
	{ &_ucas_32_stac, &_stac },
	{ &_ucas_32_clac, &_clac },
};

void
replacesmap(void)
{
	static int replacedone = 0;
	int i, s;
	vaddr_t nva;

	if (replacedone)
		return;
	replacedone = 1;

	s = splhigh();
	/*
	 * Create writeable aliases of memory we need
	 * to write to as kernel is mapped read-only
	 */
	nva = (vaddr_t)km_alloc(2 * PAGE_SIZE, &kv_any, &kp_none, &kd_waitok);

	for (i = 0; i < nitems(ireplace); i++) {
		paddr_t kva = trunc_page((paddr_t)ireplace[i].daddr);
		paddr_t po = (paddr_t)ireplace[i].daddr & PAGE_MASK;
		paddr_t pa1, pa2;

		pmap_extract(pmap_kernel(), kva, &pa1);
		pmap_extract(pmap_kernel(), kva + PAGE_SIZE, &pa2);
		pmap_kenter_pa(nva, pa1, VM_PROT_READ | VM_PROT_WRITE);
		pmap_kenter_pa(nva + PAGE_SIZE, pa2, VM_PROT_READ | 
		    VM_PROT_WRITE);
		pmap_update(pmap_kernel());

		/* replace 3 byte nops with stac/clac instructions */
		bcopy(ireplace[i].saddr, (void *)(nva + po), 3);
	}

	km_free((void *)nva, 2 * PAGE_SIZE, &kv_any, &kp_none);

	splx(s);
}
#endif /* !SMALL_KERNEL */

int
cpu_match(struct device *parent, void *match, void *aux)
{
  	struct cfdata *cf = match;
	struct cpu_attach_args *caa = aux;

	if (strcmp(caa->caa_name, cf->cf_driver->cd_name) != 0)
		return 0;

	if (cf->cf_unit >= MAXCPUS)
		return 0;

	return 1;
}

void
cpu_attach(struct device *parent, struct device *self, void *aux)
{
	struct cpu_info *ci = (struct cpu_info *)self;
	struct cpu_attach_args *caa = (struct cpu_attach_args *)aux;

#ifdef MULTIPROCESSOR
	int cpunum = ci->ci_dev.dv_unit;
	vaddr_t kstack;
	struct pcb *pcb;
#endif

	if (caa->cpu_role == CPU_ROLE_AP) {
#ifdef MULTIPROCESSOR
		if (cpu_info[cpunum] != NULL)
			panic("cpu at apic id %d already attached?", cpunum);
		cpu_info[cpunum] = ci;
#endif
	} else {
		ci = &cpu_info_primary;
#ifdef MULTIPROCESSOR
		if (caa->cpu_number != lapic_cpu_number()) {
			panic("%s: running cpu is at apic %d"
			    " instead of at expected %d",
			    self->dv_xname, lapic_cpu_number(), caa->cpu_number);
		}
#endif
		bcopy(self, &ci->ci_dev, sizeof *self);
	}

	ci->ci_self = ci;
	ci->ci_apicid = caa->cpu_number;
#ifdef MULTIPROCESSOR
	ci->ci_cpuid = cpunum;
#else
	ci->ci_cpuid = 0;	/* False for APs, so what, they're not used */
#endif
	ci->ci_signature = caa->cpu_signature;
	ci->ci_feature_flags = caa->feature_flags;
	ci->ci_func = caa->cpu_func;

#ifdef MULTIPROCESSOR
	/*
	 * Allocate UPAGES contiguous pages for the idle PCB and stack.
	 */

	kstack = uvm_km_alloc(kernel_map, USPACE);
	if (kstack == 0) {
		if (cpunum == 0) { /* XXX */
			panic("cpu_attach: unable to allocate idle stack for"
			    " primary");
		}
		printf("%s: unable to allocate idle stack\n",
		    ci->ci_dev.dv_xname);
		return;
	}
	pcb = ci->ci_idle_pcb = (struct pcb *)kstack;
	memset(pcb, 0, USPACE);

	pcb->pcb_tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	pcb->pcb_tss.tss_esp0 = kstack + USPACE - 16 -
	    sizeof (struct trapframe);
	pcb->pcb_tss.tss_esp = kstack + USPACE - 16 -
	    sizeof (struct trapframe);
	pcb->pcb_pmap = pmap_kernel();
	pcb->pcb_cr3 = pcb->pcb_pmap->pm_pdirpa;
	/* pcb->pcb_cr3 = pcb->pcb_pmap->pm_pdir - KERNBASE; XXX ??? */

	cpu_default_ldt(ci);	/* Use the `global' ldt until one alloc'd */
#endif
	ci->ci_curpmap = pmap_kernel();

	/* further PCB init done later. */

	printf(": ");

	switch (caa->cpu_role) {
	case CPU_ROLE_SP:
		printf("(uniprocessor)\n");
		ci->ci_flags |= CPUF_PRESENT | CPUF_SP | CPUF_PRIMARY;
		identifycpu(ci);
#ifdef MTRR
		mem_range_attach();
#endif
		cpu_init(ci);
		break;

	case CPU_ROLE_BP:
		printf("apid %d (boot processor)\n", caa->cpu_number);
		ci->ci_flags |= CPUF_PRESENT | CPUF_BSP | CPUF_PRIMARY;
		identifycpu(ci);
#ifdef MTRR
		mem_range_attach();
#endif
		cpu_init(ci);

#if NLAPIC > 0
		/*
		 * Enable local apic
		 */
		lapic_enable();
		lapic_calibrate_timer(ci);
#endif
#if NIOAPIC > 0
		ioapic_bsp_id = caa->cpu_number;
#endif
#if defined(MULTIPROCESSOR)
		cpu_init_mwait(&ci->ci_dev);
#endif
		break;

	case CPU_ROLE_AP:
		/*
		 * report on an AP
		 */
		printf("apid %d (application processor)\n", caa->cpu_number);

#ifdef MULTIPROCESSOR
		gdt_alloc_cpu(ci);
		cpu_alloc_ldt(ci);
		ci->ci_flags |= CPUF_PRESENT | CPUF_AP;
		identifycpu(ci);
		sched_init_cpu(ci);
		ci->ci_next = cpu_info_list->ci_next;
		cpu_info_list->ci_next = ci;
		ncpus++;
#endif
		break;

	default:
		panic("unknown processor type??");
	}

#ifdef MULTIPROCESSOR
	if (mp_verbose) {
		printf("%s: kstack at 0x%lx for %d bytes\n",
		    ci->ci_dev.dv_xname, kstack, USPACE);
		printf("%s: idle pcb at %p, idle sp at 0x%x\n",
		    ci->ci_dev.dv_xname, pcb, pcb->pcb_esp);
	}
#endif
}

/*
 * Initialize the processor appropriately.
 */

void
cpu_init(struct cpu_info *ci)
{
	/* configure the CPU if needed */
	if (ci->cpu_setup != NULL)
		(*ci->cpu_setup)(ci);

	/*
	 * We do this here after identifycpu() because errata may affect
	 * what we do.
	 */
	patinit(ci);
 
	/*
	 * Enable ring 0 write protection (486 or above, but 386
	 * no longer supported).
	 */
	lcr0(rcr0() | CR0_WP);

	if (cpu_feature & CPUID_PGE)
		lcr4(rcr4() | CR4_PGE);	/* enable global TLB caching */

	if (ci->ci_feature_sefflags & SEFF0EBX_SMEP)
		lcr4(rcr4() | CR4_SMEP);
#ifndef SMALL_KERNEL
	if (ci->ci_feature_sefflags & SEFF0EBX_SMAP)
		lcr4(rcr4() | CR4_SMAP);
#endif

#ifdef MULTIPROCESSOR
	ci->ci_flags |= CPUF_RUNNING;
	tlbflushg();
#endif

	/*
	 * If we have FXSAVE/FXRESTOR, use them.
	 */
	if (cpu_feature & CPUID_FXSR) {
		lcr4(rcr4() | CR4_OSFXSR);

		/*
		 * If we have SSE/SSE2, enable XMM exceptions.
		 */
		if (cpu_feature & (CPUID_SSE|CPUID_SSE2))
			lcr4(rcr4() | CR4_OSXMMEXCPT);
	}
}

void
patinit(struct cpu_info *ci)
{
	extern int	pmap_pg_wc;
	u_int64_t	reg;

	if ((ci->ci_feature_flags & CPUID_PAT) == 0)
		return;

#define PATENTRY(n, type)	((u_int64_t)type << ((n) * 8))
#define	PAT_UC		0x0UL
#define	PAT_WC		0x1UL
#define	PAT_WT		0x4UL
#define	PAT_WP		0x5UL
#define	PAT_WB		0x6UL
#define	PAT_UCMINUS	0x7UL
	/* 
	 * Set up PAT bits.
	 * The default pat table is the following:
	 * WB, WT, UC- UC, WB, WT, UC-, UC
	 * We change it to:
	 * WB, WC, UC-, UC, WB, WC, UC-, UC.
	 * i.e change the WT bit to be WC.
	 */
	reg = PATENTRY(0, PAT_WB) | PATENTRY(1, PAT_WC) |
	    PATENTRY(2, PAT_UCMINUS) | PATENTRY(3, PAT_UC) |
	    PATENTRY(4, PAT_WB) | PATENTRY(5, PAT_WC) |
	    PATENTRY(6, PAT_UCMINUS) | PATENTRY(7, PAT_UC);

	wrmsr(MSR_CR_PAT, reg);
	pmap_pg_wc = PG_WC;
}

struct timeout rdrand_tmo;
void rdrand(void *);

void
rdrand(void *v)
{
	struct timeout *tmo = v;
	uint32_t r, valid;
	int i;

	for (i = 0; i < 4; i++) {
		__asm volatile(
		    "xor        %1, %1\n\t"
		    "rdrand     %0\n\t"
		    "rcl        $1, %1\n"
		    : "=r" (r), "=r" (valid) );
		if (valid)
			add_true_randomness(r);
	}

	timeout_add_msec(tmo, 10);
}

#ifdef MULTIPROCESSOR
void
cpu_boot_secondary_processors()
{
	struct cpu_info *ci;
	u_long i;

	cpu_enable_mwait();

	for (i = 0; i < MAXCPUS; i++) {
		ci = cpu_info[i];
		if (ci == NULL)
			continue;
		if (ci->ci_idle_pcb == NULL)
			continue;
		if ((ci->ci_flags & CPUF_PRESENT) == 0)
			continue;
		if (ci->ci_flags & (CPUF_BSP|CPUF_SP|CPUF_PRIMARY))
			continue;
		ci->ci_randseed = (arc4random() & 0x7fffffff) + 1;
		cpu_boot_secondary(ci);
	}
}

void
cpu_init_idle_pcbs()
{
	struct cpu_info *ci;
	u_long i;

	for (i=0; i < MAXCPUS; i++) {
		ci = cpu_info[i];
		if (ci == NULL)
			continue;
		if (ci->ci_idle_pcb == NULL)
			continue;
		if ((ci->ci_flags & CPUF_PRESENT) == 0)
			continue;
		i386_init_pcb_tss_ldt(ci);
	}
}

void
cpu_boot_secondary(struct cpu_info *ci)
{
	struct pcb *pcb;
	int i;
	struct pmap *kpm = pmap_kernel();
	extern u_int32_t mp_pdirpa;

	if (mp_verbose)
		printf("%s: starting", ci->ci_dev.dv_xname);

	/* XXX move elsewhere, not per CPU. */
	mp_pdirpa = kpm->pm_pdirpa;

	pcb = ci->ci_idle_pcb;

	if (mp_verbose)
		printf(", init idle stack ptr is 0x%x\n", pcb->pcb_esp);

	CPU_STARTUP(ci);

	/*
	 * wait for it to become ready
	 */
	for (i = 100000; (!(ci->ci_flags & CPUF_RUNNING)) && i > 0; i--) {
		delay(10);
	}
	if (!(ci->ci_flags & CPUF_RUNNING)) {
		printf("%s failed to become ready\n", ci->ci_dev.dv_xname);
#ifdef DDB
		Debugger();
#endif
	}

	CPU_START_CLEANUP(ci);
}

/*
 * The CPU ends up here when it's ready to run
 * XXX should share some of this with init386 in machdep.c
 * for now it jumps into an infinite loop.
 */
void
cpu_hatch(void *v)
{
	struct cpu_info *ci = (struct cpu_info *)v;
	int s;

	cpu_init_idt();
	lapic_enable();
	lapic_startclock();
	lapic_set_lvt();
	gdt_init_cpu(ci);
	cpu_init_ldt(ci);

	lldt(GSEL(GLDT_SEL, SEL_KPL));

	npxinit(ci);

	cpu_init(ci);

	/* Re-initialise memory range handling on AP */
	if (mem_range_softc.mr_op != NULL)
		mem_range_softc.mr_op->initAP(&mem_range_softc);

	s = splhigh();		/* XXX prevent softints from running here.. */
	lapic_tpr = 0;
	enable_intr();
	if (mp_verbose)
		printf("%s: CPU at apid %ld running\n",
		    ci->ci_dev.dv_xname, ci->ci_cpuid);
	nanouptime(&ci->ci_schedstate.spc_runtime);
	splx(s);

	SCHED_LOCK(s);
	cpu_switchto(NULL, sched_chooseproc());
}

void
cpu_copy_trampoline()
{
	/*
	 * Copy boot code.
	 */
	extern u_char cpu_spinup_trampoline[];
	extern u_char cpu_spinup_trampoline_end[];

	bcopy(cpu_spinup_trampoline, (caddr_t)MP_TRAMPOLINE,
	    cpu_spinup_trampoline_end - cpu_spinup_trampoline);
}

#endif

#ifdef notyet
void
cpu_init_tss(struct i386tss *tss, void *stack, void *func)
{
	memset(tss, 0, sizeof *tss);
	tss->tss_esp0 = tss->tss_esp = (int)((char *)stack + USPACE - 16);
	tss->tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	tss->__tss_cs = GSEL(GCODE_SEL, SEL_KPL);
	tss->tss_fs = GSEL(GCPU_SEL, SEL_KPL);
	tss->tss_gs = tss->__tss_es = tss->__tss_ds =
	    tss->__tss_ss = GSEL(GDATA_SEL, SEL_KPL);
	tss->tss_cr3 = pmap_kernel()->pm_pdirpa;
	tss->tss_esp = (int)((char *)stack + USPACE - 16);
	tss->tss_ldt = GSEL(GLDT_SEL, SEL_KPL);
	tss->__tss_eflags = PSL_MBO | PSL_NT;	/* XXX not needed? */
	tss->__tss_eip = (int)func;
}

/* XXX */
#define IDTVEC(name)	__CONCAT(X, name)
typedef void (vector)(void);
extern vector IDTVEC(tss_trap08);
#ifdef DDB
extern vector Xintrddbipi;
extern int ddb_vec;
#endif

void
cpu_set_tss_gates(struct cpu_info *ci)
{
	struct segment_descriptor sd;

	ci->ci_doubleflt_stack = (char *)uvm_km_alloc(kernel_map, USPACE);
	cpu_init_tss(&ci->ci_doubleflt_tss, ci->ci_doubleflt_stack,
	    IDTVEC(tss_trap08));
	setsegment(&sd, &ci->ci_doubleflt_tss, sizeof(struct i386tss) - 1,
	    SDT_SYS386TSS, SEL_KPL, 0, 0);
	ci->ci_gdt[GTRAPTSS_SEL].sd = sd;
	setgate(&idt[8], NULL, 0, SDT_SYSTASKGT, SEL_KPL,
	    GSEL(GTRAPTSS_SEL, SEL_KPL));

#if defined(DDB) && defined(MULTIPROCESSOR)
	/*
	 * Set up separate handler for the DDB IPI, so that it doesn't
	 * stomp on a possibly corrupted stack.
	 *
	 * XXX overwriting the gate set in db_machine_init.
	 * Should rearrange the code so that it's set only once.
	 */
	ci->ci_ddbipi_stack = (char *)uvm_km_alloc(kernel_map, USPACE);
	cpu_init_tss(&ci->ci_ddbipi_tss, ci->ci_ddbipi_stack,
	    Xintrddbipi);

	setsegment(&sd, &ci->ci_ddbipi_tss, sizeof(struct i386tss) - 1,
	    SDT_SYS386TSS, SEL_KPL, 0, 0);
	ci->ci_gdt[GIPITSS_SEL].sd = sd;

	setgate(&idt[ddb_vec], NULL, 0, SDT_SYSTASKGT, SEL_KPL,
	    GSEL(GIPITSS_SEL, SEL_KPL));
#endif
}
#endif

#ifdef MULTIPROCESSOR
int
mp_cpu_start(struct cpu_info *ci)
{
#if NLAPIC > 0
	int error;
#endif
	unsigned short dwordptr[2];

	/*
	 * "The BSP must initialize CMOS shutdown code to 0Ah ..."
	 */

	outb(IO_RTC, NVRAM_RESET);
	outb(IO_RTC+1, NVRAM_RESET_JUMP);

	/*
	 * "and the warm reset vector (DWORD based at 40:67) to point
	 * to the AP startup code ..."
	 */

	dwordptr[0] = 0;
	dwordptr[1] = MP_TRAMPOLINE >> 4;

	pmap_activate(curproc);

	pmap_kenter_pa(0, 0, VM_PROT_READ|VM_PROT_WRITE);
	memcpy((u_int8_t *)0x467, dwordptr, 4);
	pmap_kremove(0, PAGE_SIZE);

#if NLAPIC > 0
	/*
	 * ... prior to executing the following sequence:"
	 */

	if (ci->ci_flags & CPUF_AP) {
		if ((error = i386_ipi_init(ci->ci_apicid)) != 0)
			return (error);

		delay(10000);

		if (cpu_feature & CPUID_APIC) {
			if ((error = i386_ipi(MP_TRAMPOLINE / PAGE_SIZE,
			    ci->ci_apicid, LAPIC_DLMODE_STARTUP)) != 0)
				return (error);
			delay(200);

			if ((error = i386_ipi(MP_TRAMPOLINE / PAGE_SIZE,
			    ci->ci_apicid, LAPIC_DLMODE_STARTUP)) != 0)
				return (error);
			delay(200);
		}
	}
#endif
	return (0);
}

void
mp_cpu_start_cleanup(struct cpu_info *ci)
{
	/*
	 * Ensure the NVRAM reset byte contains something vaguely sane.
	 */

	outb(IO_RTC, NVRAM_RESET);
	outb(IO_RTC+1, NVRAM_RESET_RST);
}

void
cpu_idle_mwait_cycle(void)
{
	struct cpu_info *ci = curcpu();
	volatile int *state = &ci->ci_mwait[0];

	if ((read_eflags() & PSL_I) == 0)
		panic("idle with interrupts blocked!");

	/* something already queued? */
	if (ci->ci_schedstate.spc_whichqs != 0)
		return;

	/*
	 * About to idle; setting the MWAIT_IN_IDLE bit tells
	 * cpu_unidle() that it can't be a no-op and tells cpu_kick()
	 * that it doesn't need to use an IPI.  We also set the
	 * MWAIT_KEEP_IDLING bit: those routines clear it to stop
	 * the mwait.  Once they're set, we do a final check of the
	 * queue, in case another cpu called setrunqueue() and added
	 * something to the queue and called cpu_unidle() between
	 * the check in sched_idle() and here.
	 */
	atomic_setbits_int(state, MWAIT_IDLING);
	if (ci->ci_schedstate.spc_whichqs == 0) {
		monitor(state, 0, 0);
		if ((*state & MWAIT_IDLING) == MWAIT_IDLING)
			mwait(0, 0);
	}

	/* done idling; let cpu_kick() know that an IPI is required */
	atomic_clearbits_int(state, MWAIT_IDLING);
}

unsigned int mwait_size;

void
cpu_init_mwait(struct device *dv)
{
	unsigned int smallest, largest, extensions, c_substates;

	if ((cpu_ecxfeature & CPUIDECX_MWAIT) == 0)
		return;

	/* get the monitor granularity */
	CPUID(0x5, smallest, largest, extensions, c_substates);
	smallest &= 0xffff;
	largest  &= 0xffff;

	printf("%s: mwait min=%u, max=%u", dv->dv_xname, smallest, largest);
	if (extensions & 0x1) {
		printf(", C-substates=%u.%u.%u.%u.%u",
		    0xf & (c_substates),
		    0xf & (c_substates >> 4),
		    0xf & (c_substates >> 8),
		    0xf & (c_substates >> 12),
		    0xf & (c_substates >> 16));
		if (extensions & 0x2)
			printf(", IBE");
	}

	/* paranoia: check the values */
	if (smallest < sizeof(int) || largest < smallest ||
	    (largest & (sizeof(int)-1)))
		printf(" (bogus)");
	else
		mwait_size = largest;
	printf("\n");
}

void
cpu_enable_mwait(void)
{
	unsigned long area;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	if (mwait_size == 0)
		return;

	/*
	 * Allocate the area, with a bit extra so that we can align
	 * to a multiple of mwait_size
	 */
	area = (unsigned long)malloc((ncpus * mwait_size) + mwait_size
	    - sizeof(int), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (area == 0) {
		printf("cpu0: mwait failed\n");
	} else {
		/* round to a multiple of mwait_size  */
		area = ((area + mwait_size - sizeof(int)) / mwait_size)
		    * mwait_size;
		CPU_INFO_FOREACH(cii, ci) {
			ci->ci_mwait = (int *)area;
			area += mwait_size;
		}
		cpu_idle_cycle_fcn = &cpu_idle_mwait_cycle;
	}
}
#endif /* MULTIPROCESSOR */

