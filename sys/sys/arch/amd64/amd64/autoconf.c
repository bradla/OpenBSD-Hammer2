/*	$OpenBSD: autoconf.c,v 1.40 2013/11/19 09:00:43 mpi Exp $	*/
/*	$NetBSD: autoconf.c,v 1.1 2003/04/26 18:39:26 fvdl Exp $	*/

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
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/dkio.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/timeout.h>
#include <sys/hibernate.h>

#include <net/if.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/biosvar.h>

#include <dev/cons.h>

#include "ioapic.h"
#include "lapic.h"

#if NIOAPIC > 0
#include <machine/i82093var.h>
#endif

#if NLAPIC > 0
#include <machine/i82489var.h>
#endif

#if 0
#include "bios32.h"
#if NBIOS32 > 0
#include <machine/bios32.h>
#endif
#endif

int	cold = 1;	/* if 1, still working on cold-start */
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
extern int	amd64_has_xcrypt;

void		aesni_setup(void);
extern int	amd64_has_aesni;
#endif

/*
 * Determine i/o configuration for a machine.
 */
void
cpu_configure(void)
{
#if NBIOS32 > 0
	bios32_init();
#endif

	x86_64_proc0_tss_ldt_init();

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("configure: mainbus not configured");

	intr_printconfig();

#if NIOAPIC > 0
	lapic_set_lvt();
	ioapic_enable();
#endif

#ifdef MULTIPROCESSOR
	cpu_init_idle_pcbs();
#endif

	lcr8(0);
	spl0();
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
	if (amd64_has_xcrypt)
		viac3_crypto_setup();

	if (amd64_has_aesni)
		aesni_setup();
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
	{ "vnd",	14 },
	{ "rd",		17 },
	{ "raid",	19 },
	{ NULL,		-1 }
};
