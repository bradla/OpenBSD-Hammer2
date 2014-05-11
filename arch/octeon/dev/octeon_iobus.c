/*	$OpenBSD: octeon_iobus.c,v 1.5 2013/10/24 20:45:03 pirofti Exp $ */

/*
 * Copyright (c) 2000-2004 Opsycon AB  (www.opsycon.se)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * This is a iobus driver.
 * It handles configuration of all devices on the processor bus except UART.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/autoconf.h>
#include <machine/atomic.h>
#include <machine/intr.h>
#include <machine/octeonvar.h>
#include <machine/octeonreg.h>

#include <octeon/dev/iobusvar.h>
#include <octeon/dev/cn30xxgmxreg.h>

int	 iobusmatch(struct device *, void *, void *);
void	 iobusattach(struct device *, struct device *, void *);
int	 iobusprint(void *, const char *);
int	 iobussubmatch(struct device *, void *, void *);

u_int8_t iobus_read_1(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int16_t iobus_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int32_t iobus_read_4(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int64_t iobus_read_8(bus_space_tag_t, bus_space_handle_t, bus_size_t);

void	 iobus_write_1(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int8_t);
void	 iobus_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int16_t);
void	 iobus_write_4(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int32_t);
void	 iobus_write_8(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int64_t);

void	 iobus_read_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 iobus_write_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);
void	 iobus_read_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 iobus_write_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);
void	 iobus_read_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 iobus_write_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);

int	 iobus_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
void	 iobus_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int	 iobus_space_region(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    bus_size_t, bus_space_handle_t *);

void	*iobus_space_vaddr(bus_space_tag_t, bus_space_handle_t);

bus_addr_t iobus_pa_to_device(paddr_t);
paddr_t	 iobus_device_to_pa(bus_addr_t);

struct cfattach iobus_ca = {
	sizeof(struct device), iobusmatch, iobusattach
};

struct cfdriver iobus_cd = {
	NULL, "iobus", DV_DULL
};

bus_space_t iobus_tag = {
	.bus_base = PHYS_TO_XKPHYS(0, CCA_NC),
	.bus_private = NULL,
	._space_read_1 =	generic_space_read_1,
	._space_write_1 =	generic_space_write_1,
	._space_read_2 =	generic_space_read_2,
	._space_write_2 =	generic_space_write_2,
	._space_read_4 =	generic_space_read_4,
	._space_write_4 =	generic_space_write_4,
	._space_read_8 =	generic_space_read_8,
	._space_write_8 =	generic_space_write_8,
	._space_read_raw_2 =	generic_space_read_raw_2,
	._space_write_raw_2 =	generic_space_write_raw_2,
	._space_read_raw_4 =	generic_space_read_raw_4,
	._space_write_raw_4 =	generic_space_write_raw_4,
	._space_read_raw_8 =	generic_space_read_raw_8,
	._space_write_raw_8 =	generic_space_write_raw_8,
	._space_map =		iobus_space_map,
	._space_unmap =		iobus_space_unmap,
	._space_subregion =	generic_space_region,
	._space_vaddr =		generic_space_vaddr
};

bus_space_handle_t iobus_h;

struct machine_bus_dma_tag iobus_bus_dma_tag = {
	NULL,			/* _cookie */
	_dmamap_create,
	_dmamap_destroy,
	_dmamap_load,
	_dmamap_load_mbuf,
	_dmamap_load_uio,
	_dmamap_load_raw,
	_dmamap_load_buffer,
	_dmamap_unload,
	_dmamap_sync,
	_dmamem_alloc,
	_dmamem_free,
	_dmamem_map,
	_dmamem_unmap,
	_dmamem_mmap,
	iobus_pa_to_device,
	iobus_device_to_pa,
	0
};

/*
 * List of iobus child devices.
 */

#define	IOBUSDEV(name, unitno, unit)	\
	{ name, unitno, unit, &iobus_tag, &iobus_bus_dma_tag }
const struct iobus_unit iobus_units[] = {
	{ OCTEON_CF_BASE, 0 },			/* octcf */
	{ 0, 0 },				/* pcibus */
	{ GMX0_BASE_PORT0, CIU_INT_GMX_DRP0 },	/* cn30xxgmx */
	{ OCTEON_RNG_BASE, 0 },			/* octrng */
};
struct iobus_attach_args iobus_children[] = {
	IOBUSDEV("octcf", 0, &iobus_units[0]),
	IOBUSDEV("pcibus", 0, &iobus_units[1]),
	IOBUSDEV("cn30xxgmx", 0, &iobus_units[2]),
	IOBUSDEV("octrng", 0, &iobus_units[3])
};
#undef	IOBUSDEV

/*
 * Match bus only to targets which have this bus.
 */
int
iobusmatch(struct device *parent, void *match, void *aux)
{
	return (1);
}

int
iobusprint(void *aux, const char *iobus)
{
	struct iobus_attach_args *aa = aux;

	if (iobus != NULL)
		printf("%s at %s", aa->aa_name, iobus);

	if (aa->aa_unit->addr != 0)
		printf(" base 0x%llx", aa->aa_unit->addr);
	if (aa->aa_unit->irq >= 0)
		printf(" irq %d", aa->aa_unit->irq);

	return (UNCONF);
}

int
iobussubmatch(struct device *parent, void *vcf, void *args)
{
	struct cfdata *cf = vcf;
	struct iobus_attach_args *aa = args;

	if (strcmp(cf->cf_driver->cd_name, aa->aa_name) != 0)
		return 0;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != (int)aa->aa_unit->addr)
		return 0;

	return (*cf->cf_attach->ca_match)(parent, cf, aa);
}

void
iobusattach(struct device *parent, struct device *self, void *aux)
{
	struct octeon_config oc;
	uint i;

	/*
	 * Map and setup CRIME control registers.
	 */
	if (bus_space_map(&iobus_tag, OCTEON_CIU_BASE, OCTEON_CIU_SIZE, 0,
		&iobus_h)) {
		printf(": can't map CIU control registers\n");
		return;
	}

	printf("\n");

	octeon_intr_init();

	/* XXX */
	oc.mc_iobus_bust = &iobus_tag;
	oc.mc_iobus_dmat = &iobus_bus_dma_tag;
	void	cn30xxfpa_bootstrap(struct octeon_config *);
	cn30xxfpa_bootstrap(&oc);
	void	cn30xxpow_bootstrap(struct octeon_config *);
	cn30xxpow_bootstrap(&oc);

	/*
	 * Attach subdevices.
	 */
	for (i = 0; i < nitems(iobus_children); i++)
		config_found_sm(self, iobus_children + i,
		    iobusprint, iobussubmatch);
}

int
iobus_space_map(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	if (ISSET(flags, BUS_SPACE_MAP_KSEG0)) {
		*bshp = PHYS_TO_CKSEG0(offs);
		return 0;
	}
	if (ISSET(flags, BUS_SPACE_MAP_CACHEABLE))
		offs +=
		    PHYS_TO_XKPHYS(0, CCA_CACHED) - PHYS_TO_XKPHYS(0, CCA_NC);
	*bshp = t->bus_base + offs;
	return 0;
}

void
iobus_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{
}

/*
 * Iobus bus_dma helpers.
 */

bus_addr_t
iobus_pa_to_device(paddr_t pa)
{
	return (bus_addr_t)pa;
}

paddr_t
iobus_device_to_pa(bus_addr_t addr)
{
	return (paddr_t)addr;
}
