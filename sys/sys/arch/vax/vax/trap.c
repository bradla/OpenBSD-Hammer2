/*	$OpenBSD: trap.c,v 1.51 2014/04/18 11:51:17 guenther Exp $     */
/*	$NetBSD: trap.c,v 1.47 1999/08/21 19:26:20 matt Exp $     */
/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /* All bugs are subject to removal without further notice */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/syscall_mi.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/exec.h>

#include <uvm/uvm_extern.h>

#include <machine/mtpr.h>
#include <machine/pte.h>
#include <machine/pcb.h>
#include <machine/trap.h>
#include <machine/pmap.h>
#include <machine/cpu.h>

#ifdef DDB
#include <machine/db_machdep.h>
#endif
#include <kern/syscalls.c>

#ifdef TRAPDEBUG
volatile int startsysc = 0, faultdebug = 0;
#endif

void	arithflt(struct trapframe *);
void	syscall(struct trapframe *);

char *traptypes[]={
	"reserved addressing",
	"privileged instruction",
	"reserved operand",
	"breakpoint instruction",
	"XFC instruction",
	"system call ",
	"arithmetic trap",
	"asynchronous system trap",
	"page table length fault",
	"translation violation fault",
	"trace trap",
	"compatibility mode fault",
	"access violation fault",
	"",
	"",
	"KSP invalid",
	"",
	"kernel debugger trap"
};
int no_traps = 18;

#define USERMODE(framep)   ((((framep)->psl) & (PSL_U)) == PSL_U)
#define FAULTCHK						\
	do if (p->p_addr->u_pcb.iftrap) {			\
		frame->pc = (unsigned)p->p_addr->u_pcb.iftrap;	\
		frame->psl &= ~PSL_FPD;				\
		frame->r0 = EFAULT;				\
		return;						\
	} while (0)

void
arithflt(frame)
	struct trapframe *frame;
{
	u_int	sig = 0, type = frame->trap, trapsig = 1;
	u_int	rv, addr, umode;
	struct	proc *p = curproc;
	struct vm_map *map;
	vm_prot_t ftype;
	int typ;
	union sigval sv;
	
	sv.sival_int = frame->pc;
	uvmexp.traps++;
	if ((umode = USERMODE(frame))) {
		type |= T_USER;
		p->p_addr->u_pcb.framep = frame;
		refreshcreds(p);
	}

	type&=~(T_WRITE|T_PTEFETCH);


#ifdef TRAPDEBUG
if(frame->trap==7) goto fram;
if(faultdebug)printf("Trap: type %lx, code %lx, pc %lx, psl %lx\n",
		frame->trap, frame->code, frame->pc, frame->psl);
fram:
#endif
	switch(type){

	default:
#ifdef DDB
		kdb_trap(frame);
#endif
		printf("Trap: type %x, code %x, pc %x, psl %x\n",
		    (u_int)frame->trap, (u_int)frame->code,
		    (u_int)frame->pc, (u_int)frame->psl);
		panic("trap");

	case T_KSPNOTVAL:
		panic("kernel stack invalid");

	case T_TRANSFLT|T_USER:
	case T_TRANSFLT:
		/*
		 * BUG! BUG! BUG! BUG! BUG!
		 * Due to a hardware bug (at in least KA65x CPUs) a double
		 * page table fetch trap will cause a translation fault
		 * even if access in the SPT PTE entry specifies 'no access'.
		 * In for example section 6.4.2 in VAX Architecture
		 * Reference Manual it states that if a page both are invalid
		 * and have no access set, a 'access violation fault' occurs.
		 * Therefore, we must fall through here...
		 */
#ifdef nohwbug
		panic("translation fault");
#endif
	case T_PTELEN|T_USER:	/* Page table length exceeded */
	case T_ACCFLT|T_USER:
		if (frame->code < 0) { /* Check for kernel space */
			sv.sival_int = frame->code;
			sig = SIGSEGV;
			typ = SEGV_ACCERR;
			break;
		}
		/* FALLTHROUGH */

	case T_PTELEN:
	case T_ACCFLT:
#ifdef TRAPDEBUG
if(faultdebug)printf("trap accflt type %lx, code %lx, pc %lx, psl %lx\n",
			frame->trap, frame->code, frame->pc, frame->psl);
#endif
#ifdef DIAGNOSTIC
		if (p == 0)
			panic("trap: access fault: addr %lx code %lx",
			    frame->pc, frame->code);
#endif

		/*
		 * Page tables are allocated in pmap_enter(). We get
		 * info from below if it is a page table fault, but
		 * UVM may want to map in pages without faults, so
		 * because we must check for PTE pages anyway we don't
		 * bother doing it here.
		 */
		sv.sival_int = frame->code;
		if ((umode == 0) && (frame->code < 0))
			map = kernel_map;
		else
			map = &p->p_vmspace->vm_map;

		if (frame->trap & T_WRITE)
			ftype = VM_PROT_WRITE|VM_PROT_READ;
		else
			ftype = VM_PROT_READ;

		addr = trunc_page((vaddr_t)frame->code);
		rv = uvm_fault(map, addr, 0, ftype);
		if (rv) {
			if (umode == 0) {
				FAULTCHK;
				panic("Segv in kernel mode: pc %x addr %x",
				    (u_int)frame->pc, (u_int)frame->code);
			}
			if (rv == ENOMEM) {
				printf("UVM: pid %d (%s), uid %d killed: "
			           "out of swap\n", p->p_pid, p->p_comm,
			           p->p_ucred ? (int)p->p_ucred->cr_uid : -1);
				sig = SIGKILL;
				typ = 0;
			} else {
				sig = SIGSEGV;
				typ = SEGV_MAPERR;
			}
		} else {
			trapsig = 0;
			if (umode != 0)
				uvm_grow(p, addr);
		}
		break;

	case T_BPTFLT|T_USER:
		typ = TRAP_BRKPT;
		sig = SIGTRAP;
		frame->psl &= ~PSL_T;
		break;

	case T_TRCTRAP|T_USER:
		typ = TRAP_TRACE;
		sig = SIGTRAP;
		frame->psl &= ~PSL_T;
		break;

	case T_PRIVINFLT|T_USER:
	case T_RESOPFLT|T_USER:
		typ = ILL_ILLOPC;
		sig = SIGILL;
		break;

	case T_RESADFLT|T_USER:
		typ = ILL_ILLADR;
		sig = SIGILL;
		break;

	case T_XFCFLT|T_USER:
		typ = EMT_TAGOVF;
		sig = SIGEMT;
		break;

	case T_ARITHFLT|T_USER:
		sv.sival_int = frame->code;
		typ = FPE_FLTINV;
		sig = SIGFPE;
		break;

	case T_ASTFLT|T_USER:
		mtpr(AST_NO,PR_ASTLVL);
		trapsig = 0;
		if (p->p_flag & P_OWEUPC) {
			ADDUPROF(p);
		}
		if (want_resched)
			preempt(NULL);
		break;

#ifdef DDB
	case T_BPTFLT: /* Kernel breakpoint */
	case T_KDBTRAP:
	case T_KDBTRAP|T_USER:
	case T_TRCTRAP:
		kdb_trap(frame);
		return;
#endif
	}

	if (trapsig) {
		/*
		 * Arithmetic exceptions can be of two kinds:
		 * - traps (codes 1..7), where pc points to the
		 *   next instruction to execute.
		 * - faults (codes 8..10), where pc points to the
		 *   faulting instruction.
		 * In the latter case, we need to advance pc by ourselves
		 * to prevent a signal loop.
		 *
		 * XXX this is gross -- miod
		 */
		if (type == (T_ARITHFLT | T_USER) && frame->code >= 8) {
			extern long skip_opcode(long);

			frame->pc = skip_opcode(frame->pc);
		}

		trapsignal(p, sig, frame->code, typ, sv);
	}

	if (umode == 0)
		return;

	userret(p);
}

void
setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	struct trapframe *exptr;

	exptr = p->p_addr->u_pcb.framep;
	exptr->pc = pack->ep_entry + 2;
	exptr->sp = stack;
	exptr->r6 = stack;			/* for ELF */
	exptr->r7 = 0;				/* for ELF */
	exptr->r8 = 0;				/* for ELF */
	exptr->r9 = (u_long) PS_STRINGS;	/* for ELF */

	retval[1] = 0;
}

void
syscall(frame)
	struct	trapframe *frame;
{
	struct sysent *callp;
	int nsys, err;
	long rval[2], args[8];
	struct trapframe *exptr;
	struct proc *p = curproc;

#ifdef TRAPDEBUG
if(startsysc)printf("trap syscall %s pc %lx, psl %lx, sp %lx, pid %d, frame %p\n",
	       syscallnames[frame->code], frame->pc, frame->psl,frame->sp,
		curproc->p_pid,frame);
#endif
	uvmexp.syscalls++;

	exptr = p->p_addr->u_pcb.framep = frame;
	callp = p->p_p->ps_emul->e_sysent;
	nsys = p->p_p->ps_emul->e_nsysent;

	if(frame->code == SYS___syscall){
		int g = *(int *)(frame->ap);

		frame->code = *(int *)(frame->ap + 4);
		frame->ap += 8;
		*(int *)(frame->ap) = g - 2;
	}

	if(frame->code < 0 || frame->code >= nsys)
		callp += p->p_p->ps_emul->e_nosys;
	else
		callp += frame->code;

	rval[0] = 0;
	rval[1] = frame->r1;
	if(callp->sy_narg) {
		if ((err = copyin((char *)frame->ap + 4, args,
		    callp->sy_argsize)))
			goto bad;
	}

	err = mi_syscall(p, frame->code, callp, args, rval);

#ifdef TRAPDEBUG
if(startsysc)
	printf("retur %s pc %lx, psl %lx, sp %lx, pid %d, v{rde %d r0 %d, r1 %d, frame %p\n",
	       syscallnames[exptr->code], exptr->pc, exptr->psl,exptr->sp,
		curproc->p_pid,err,rval[0],rval[1],exptr);
#endif

	switch (err) {
	case 0:
		exptr->r1 = rval[1];
		exptr->r0 = rval[0];
		exptr->psl &= ~PSL_C;
		break;

	case EJUSTRETURN:
		return;

	case ERESTART:
		exptr->pc -= (exptr->code > 63 ? 4 : 2);
		break;

	default:
	bad:
		exptr->r0 = err;
		exptr->psl |= PSL_C;
		break;
	}

	mi_syscall_return(p, frame->code, err, rval);
}

void
child_return(arg)
	void *arg;
{
	struct proc *p = arg;
	struct trapframe *frame;

	frame = p->p_addr->u_pcb.framep;
	frame->r1 = frame->r0 = 0;
	frame->psl &= ~PSL_C;

	mi_child_return(p);
}
