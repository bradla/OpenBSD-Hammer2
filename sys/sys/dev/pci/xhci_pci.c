/*	$OpenBSD: xhci_pci.c,v 1.3 2014/04/07 23:32:41 brad Exp $ */

/*
 * Copyright (c) 2001, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net).
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/rwlock.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/queue.h>

#include <machine/bus.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/xhcireg.h>
#include <dev/usb/xhcivar.h>

#ifdef XHCI_DEBUG
#define DPRINTF(x)	if (xhcidebug) printf x
extern int xhcidebug;
#else
#define DPRINTF(x)
#endif

struct xhci_pci_softc {
	struct xhci_softc	sc;
	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;
	void 			*sc_ih;		/* interrupt vectoring */
};

int	xhci_pci_match(struct device *, void *, void *);
void	xhci_pci_attach(struct device *, struct device *, void *);
int	xhci_pci_detach(struct device *, int);
void	xhci_pci_takecontroller(struct xhci_pci_softc *, int);

struct cfattach xhci_pci_ca = {
	sizeof(struct xhci_pci_softc), xhci_pci_match, xhci_pci_attach,
	xhci_pci_detach, xhci_activate
};

int
xhci_pci_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_SERIALBUS &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_SERIALBUS_USB &&
	    PCI_INTERFACE(pa->pa_class) == PCI_INTERFACE_XHCI)
		return (1);

	return (0);
}

void
xhci_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct xhci_pci_softc *psc = (struct xhci_pci_softc *)self;
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	const char *intrstr;
	const char *vendor;
	pci_intr_handle_t ih;
	pcireg_t reg;
	int error;

	reg = pci_mapreg_type(pa->pa_pc, pa->pa_tag, PCI_CBMEM);
	if (pci_mapreg_map(pa, PCI_CBMEM, reg, 0, &psc->sc.iot, &psc->sc.ioh,
	    NULL, &psc->sc.sc_size, 0)) {
		printf(": can't map mem space\n");
		return;
	}

	psc->sc_pc = pa->pa_pc;
	psc->sc_tag = pa->pa_tag;
	psc->sc.sc_bus.dmatag = pa->pa_dmat;

	/* Handle quirks */
	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_FRESCO:
		/* FL1000 / FL1400 claim MSI support but do not support MSI */
                if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_FRESCO_FL1000 ||
                    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_FRESCO_FL1400)
			pa->pa_flags &= ~PCI_FLAGS_MSI_ENABLED;
		break;
	}

	/* Map and establish the interrupt. */
	if (pci_intr_map_msi(pa, &ih) != 0 && pci_intr_map(pa, &ih) != 0) {
		printf(": couldn't map interrupt\n");
		goto unmap_ret;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);

	psc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_USB,
	    xhci_intr, psc, psc->sc.sc_bus.bdev.dv_xname);
	if (psc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto unmap_ret;
	}
	printf(": %s\n", intrstr);

	/* Figure out vendor for root hub descriptor. */
	vendor = pci_findvendor(pa->pa_id);
	psc->sc.sc_id_vendor = PCI_VENDOR(pa->pa_id);
	if (vendor)
		strlcpy(psc->sc.sc_vendor, vendor, sizeof(psc->sc.sc_vendor));
	else
		snprintf(psc->sc.sc_vendor, sizeof(psc->sc.sc_vendor),
		    "vendor 0x%04x", PCI_VENDOR(pa->pa_id));

	xhci_pci_takecontroller(psc, 0);

	if (xhci_init(&psc->sc)) {
		printf("%s: init failed, error=%d\n",
		    psc->sc.sc_bus.bdev.dv_xname, error);
		goto disestablish_ret;
	}

	/* Attach usb device. */
	config_found(self, &psc->sc.sc_bus, usbctlprint);

	return;

disestablish_ret:
	pci_intr_disestablish(psc->sc_pc, psc->sc_ih);
unmap_ret:
	bus_space_unmap(psc->sc.iot, psc->sc.ioh, psc->sc.sc_size);
}

int
xhci_pci_detach(struct device *self, int flags)
{
	struct xhci_pci_softc *psc = (struct xhci_pci_softc *)self;
	int rv;

	rv = xhci_detach(self, flags);
	if (rv)
		return (rv);
	if (psc->sc_ih != NULL) {
		pci_intr_disestablish(psc->sc_pc, psc->sc_ih);
		psc->sc_ih = NULL;
	}
	if (psc->sc.sc_size) {
		bus_space_unmap(psc->sc.iot, psc->sc.ioh, psc->sc.sc_size);
		psc->sc.sc_size = 0;
	}
	return (0);
}

void
xhci_pci_takecontroller(struct xhci_pci_softc *psc, int silent)
{
	uint32_t cparams, xecp, eec;
	uint8_t bios_sem;
	int i;

	cparams = XREAD4(&psc->sc, XHCI_HCCPARAMS);
	eec = -1;

	/* Synchronise with the BIOS if it owns the controller. */
	for (xecp = XHCI_HCC_XECP(cparams) << 2; xecp != 0;
	    xecp = XHCI_XECP_NEXT(eec) << 2) {
		eec = XREAD4(&psc->sc, xecp);
		if (XHCI_XECP_ID(eec) != XHCI_ID_USB_LEGACY)
			continue;
		bios_sem = XREAD1(&psc->sc, xecp + XHCI_XECP_BIOS_SEM);
		if (bios_sem) {
			XWRITE1(&psc->sc, xecp + XHCI_XECP_OS_SEM, 1);
			DPRINTF(("%s: waiting for BIOS to give up control\n",
			    psc->sc.sc_bus.bdev.dv_xname));
			for (i = 0; i < 5000; i++) {
				bios_sem = XREAD1(&psc->sc, xecp +
				    XHCI_XECP_BIOS_SEM);
				if (bios_sem == 0)
					break;
				DELAY(1000);
			}
			if (silent == 0 && bios_sem)
				printf("%s: timed out waiting for BIOS\n",
				    psc->sc.sc_bus.bdev.dv_xname);
		}
	}
}
