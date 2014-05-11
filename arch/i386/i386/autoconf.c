/*	$OpenBSD: autoconf.c,v 1.92 2013/11/19 09:00:43 mpi Exp $	*/
/*	$NetBSD: autoconf.c,v 1.20 1996/05/03 19:41:56 christos Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)autoconf.c	7.1 (Berkeley) 5/9/91
 */

/*
 * Setup the system to run on the current machine.
 *
 * cpu_configure() is called at boot time and initializes the vba
 * device tables and the memory controller monitoring.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/hibernate.h>

#include <net/if.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <uvm/uvm_extern.h>

#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/gdt.h>
#include <machine/biosvar.h>
#include <machine/kvm86.h>

#include <dev/cons.h>

#include "ioapic.h"

#if NIOAPIC > 0
#include <machine/i82093var.h>
#endif

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
extern dev_t bootdev;

/* Support for VIA C3 RNG */
extern struct timeout viac3_rnd_tmo;
extern int	viac3_rnd_present;
void		viac3_rnd(void *);

extern struct timeout rdrand_tmo;
extern int	has_rdrand;
void		rdrand(void *);

#ifdef CRYPTO
void		viac3_crypto_setup(void);
extern int	i386_has_xcrypt;
#endif

/*
 * Determine i/o configuration for a machine.
 */
void
cpu_configure(void)
{
	/*
	 * Note, on i386, configure is not running under splhigh unlike other
	 * architectures.  This fact is used by the pcmcia irq line probing.
	 */

	gdt_init();		/* XXX - pcibios uses gdt stuff */

	/* Set up proc0's TSS and LDT */
	i386_proc0_tss_ldt_init();

#ifdef KVM86
	kvm86_init();
#endif

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("cpu_configure: mainbus not configured");

#if NIOAPIC > 0
	ioapic_enable();
#endif

	proc0.p_addr->u_pcb.pcb_cr0 = rcr0();

#ifdef MULTIPROCESSOR
	/* propagate TSS and LDT configuration to the idle pcb's. */
	cpu_init_idle_pcbs();
#endif
	spl0();

	/*
	 * We can not know which is our root disk, defer
	 * until we can checksum blocks to figure it out.
	 */
	cold = 0;

	/*
	 * At this point the RNG is running, and if FSXR is set we can
	 * use it.  Here we setup a periodic timeout to collect the data.
	 */
	if (viac3_rnd_present) {
		timeout_set(&viac3_rnd_tmo, viac3_rnd, &viac3_rnd_tmo);
		viac3_rnd(&viac3_rnd_tmo);
	}
	if (has_rdrand) {
		timeout_set(&rdrand_tmo, rdrand, &rdrand_tmo);
		rdrand(&rdrand_tmo);
	}

#ifdef CRYPTO
	/*
	 * Also, if the chip has crypto available, enable it.
	 */
	if (i386_has_xcrypt)
		viac3_crypto_setup();
#endif
}

void
device_register(struct device *dev, void *aux)
{
}

/*
 * Now that we are fully operational, we can checksum the
 * disks, and using some heuristics, hopefully are able to
 * always determine the correct root disk.
 */
void
diskconf(void)
{
	int majdev, unit, part = 0;
	struct device *bootdv = NULL;
	dev_t tmpdev;
	char buf[128];
	extern bios_bootmac_t *bios_bootmac;

	dkcsumattach();

	if ((bootdev & B_MAGICMASK) == (u_int)B_DEVMAGIC) {
		majdev = B_TYPE(bootdev);
		unit = B_UNIT(bootdev);
		part = B_PARTITION(bootdev);
		snprintf(buf, sizeof buf, "%s%d%c", findblkname(majdev),
		    unit, part + 'a');
		bootdv = parsedisk(buf, strlen(buf), part, &tmpdev);
	}

	if (bios_bootmac) {
		struct ifnet *ifp;

		for (ifp = TAILQ_FIRST(&ifnet); ifp != NULL;
		    ifp = TAILQ_NEXT(ifp, if_list)) {
			if (ifp->if_type == IFT_ETHER &&
			    bcmp(bios_bootmac->mac,
			    ((struct arpcom *)ifp)->ac_enaddr,
			    ETHER_ADDR_LEN) == 0)
				break;
		}
		if (ifp) {
#if defined(NFSCLIENT)
			printf("PXE boot MAC address %s, interface %s\n",
			    ether_sprintf(bios_bootmac->mac), ifp->if_xname);
			bootdv = parsedisk(ifp->if_xname, strlen(ifp->if_xname),
			    0, &tmpdev);
			part = 0;
#endif
		} else
			printf("PXE boot MAC address %s, interface %s\n",
			    ether_sprintf(bios_bootmac->mac), "unknown");
	}

	setroot(bootdv, part, RB_USERREQ);
	dumpconf();

#ifdef HIBERNATE
	hibernate_resume();
#endif /* HIBERNATE */
}

struct nam2blk nam2blk[] = {
	{ "wd",		0 },
	{ "fd",		2 },
	{ "sd",		4 },
	{ "cd",		6 },
	{ "rd",		17 },
	{ "raid",	19 },
	{ "vnd",	14 },
	{ NULL,		-1 }
};
