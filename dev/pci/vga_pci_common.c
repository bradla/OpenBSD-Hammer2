/*
 * Copyright (c) 2008 Owain G. Ainsworth <oga@nicotinebsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "drm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/vga_pcivar.h>


#if NDRM > 0
int
vga_drmsubmatch(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;
	struct cfdriver *cd;
	size_t len = 0;
	char *sm;

	cd = cf->cf_driver;

	/* is this a *drm device? */
	len = strlen(cd->cd_name);
	sm = cd->cd_name + len - 3;
	if (strncmp(sm, "drm", 3) == 0)
		return ((*cf->cf_attach->ca_match)(parent, match, aux));

	return (0);
}
#endif

/*
 * Prepare dev->bars to be used for information. we do this at startup
 * so we can do the whole array at once, dealing with 64-bit BARs correctly.
 */
void
vga_pci_bar_init(struct vga_pci_softc *dev, struct pci_attach_args *pa)
{
	pcireg_t type;
	int addr = PCI_MAPREG_START, i = 0;
	memcpy(&dev->pa, pa, sizeof(dev->pa));

	while (i < VGA_PCI_MAX_BARS) {
		dev->bars[i] = malloc(sizeof((*dev->bars[i])), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (dev->bars[i] == NULL) {
			return;
		}

		dev->bars[i]->addr = addr;

		type = dev->bars[i]->maptype = pci_mapreg_type(pa->pa_pc,
		    pa->pa_tag, addr);
		if (pci_mapreg_info(pa->pa_pc, pa->pa_tag, addr,
		    dev->bars[i]->maptype, &dev->bars[i]->base,
		    &dev->bars[i]->maxsize, &dev->bars[i]->flags) != 0) {
			free(dev->bars[i], M_DEVBUF);
			dev->bars[i] = NULL;
		}

		if (type == PCI_MAPREG_MEM_TYPE_64BIT) {
			addr += 8;
			i += 2;
		} else {
			addr += 4;
			i++;
		}
	}
}

/*
 * Get the vga_pci_bar struct for the address in question. returns NULL if
 * invalid BAR is passed.
 */
struct vga_pci_bar*
vga_pci_bar_info(struct vga_pci_softc *dev, int no)
{
	if (dev == NULL || no >= VGA_PCI_MAX_BARS)
		return (NULL);
	return (dev->bars[no]);
}

/*
 * map the BAR in question, returning the vga_pci_bar struct in case any more
 * processing needs to be done. Returns NULL on failure. Can be called multiple
 * times.
 */
struct vga_pci_bar*
vga_pci_bar_map(struct vga_pci_softc *dev, int addr, bus_size_t size,
    int busflags)
{
	struct vga_pci_bar *bar = NULL;
	int i;

	if (dev == NULL)
		return (NULL);

	for (i = 0; i < VGA_PCI_MAX_BARS; i++) {
		if (dev->bars[i] && dev->bars[i]->addr == addr) {
			bar = dev->bars[i];
			break;
		}
	}
	if (bar == NULL) {
		printf("vga_pci_bar_map: given invalid address 0x%x\n", addr);
		return (NULL);
	}

	if (bar->mapped == 0) {
		if (pci_mapreg_map(&dev->pa, bar->addr, bar->maptype,
		    bar->flags | busflags, &bar->bst, &bar->bsh, NULL,
		    &bar->size, size)) {
			printf("vga_pci_bar_map: can't map bar 0x%x\n", addr);
			return (NULL);
		}
	}

	bar->mapped++;
	return (bar);
}

/*
 * "unmap" the BAR referred to by argument. If more than one place has mapped it
 * we just decrement the reference counter so nothing untoward happens.
 */
void
vga_pci_bar_unmap(struct vga_pci_bar *bar)
{
	if (bar != NULL && bar->mapped != 0) {
		if (--bar->mapped == 0)
			bus_space_unmap(bar->bst, bar->bsh, bar->size);
	}
}
