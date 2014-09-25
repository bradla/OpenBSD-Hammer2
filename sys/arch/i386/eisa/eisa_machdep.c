/*	$OpenBSD: eisa_machdep.c,v 1.14 2010/09/06 19:05:48 kettenis Exp $	*/
/*	$NetBSD: eisa_machdep.c,v 1.10.22.2 2000/06/25 19:36:58 sommerfeld Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
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

/*
 * Machine-specific functions for EISA autoconfiguration.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/i8259.h>

#include <dev/isa/isavar.h>
#include <dev/eisa/eisavar.h>

/*
 * EISA doesn't have any special needs; just use the generic versions
 * of these funcions.
 */
struct bus_dma_tag eisa_bus_dma_tag = {
	NULL,			/* _cookie */
	_bus_dmamap_create,
	_bus_dmamap_destroy,
	_bus_dmamap_load,
	_bus_dmamap_load_mbuf,
	_bus_dmamap_load_uio,
	_bus_dmamap_load_raw,
	_bus_dmamap_unload,
	_bus_dmamap_sync,
	_bus_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap,
};

void
eisa_attach_hook(struct device *parent, struct device *self,
    struct eisabus_attach_args *eba)
{
	/* Nothing to do */
}

int
eisa_maxslots(eisa_chipset_tag_t ec)
{
	/*
	 * Always try 16 slots.
	 */
	return (16);
}

int
eisa_intr_map(eisa_chipset_tag_t ec, u_int irq, eisa_intr_handle_t *ihp)
{
#if NIOAPIC > 0
	struct mp_intr_map *mip;
#endif

	if (irq >= ICU_LEN) {
		printf("eisa_intr_map: bad IRQ %d\n", irq);
		*ihp = -1;
		return (1);
	}
	if (irq == 2) {
		printf("eisa_intr_map: changed IRQ 2 to IRQ 9\n");
		irq = 9;
	}

#if NIOAPIC > 0
	if (mp_busses != NULL) {
		/*
		 * Assumes 1:1 mapping between PCI bus numbers and
		 * the numbers given by the MP bios.
		 * XXX Is this a valid assumption?
		 */
		
		for (mip = mp_busses[bus].mb_intrs; mip != NULL;
		    mip = mip->next) {
			if (mip->bus_pin == irq) {
				*ihp = mip->ioapic_ih | irq;
				return (0);
			}
		}
		if (mip == NULL)
			printf("eisa_intr_map: no MP mapping found\n");
	}
#endif

	*ihp = irq;
	return (0);
}

const char *
eisa_intr_string(eisa_chipset_tag_t ec, eisa_intr_handle_t ih)
{
	static char irqstr[64];

	if (ih == 0 || (ih & 0xff) >= ICU_LEN || ih == 2)
		panic("eisa_intr_string: bogus handle 0x%x", ih);

#if NIOAPIC > 0
	if (ih & APIC_INT_VIA_APIC) {
		snprintf(irqstr, sizeof irqstr, "apic %d int %d (irq %d)",
		    APIC_IRQ_APIC(ih), APIC_IRQ_PIN(ih), ih & 0xff);
		return (irqstr);
	}
#endif

	snprintf(irqstr, sizeof irqstr, "irq %d", ih);
	return (irqstr);
	
}

void *
eisa_intr_establish(eisa_chipset_tag_t ec, eisa_intr_handle_t ih, int type,
    int level, int (*func)(void *), void *arg, char *what)
{
#if NIOAPIC > 0
	if (ih != -1) {
		if (ih != -1 && (ih & APIC_INT_VIA_APIC)) {
			return (apic_intr_establish(ih, type, level, func, arg,
			    what));
		}
	}
#endif
	if (ih == 0 || ih >= ICU_LEN || ih == 2)
		panic("eisa_intr_establish: bogus handle 0x%x", ih);

	return (isa_intr_establish(NULL, ih, type, level, func, arg, what));
}

void
eisa_intr_disestablish(eisa_chipset_tag_t ec, void *cookie)
{
	return (isa_intr_disestablish(NULL, cookie));
}
