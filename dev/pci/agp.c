/* $OpenBSD: agp.c,v 1.43 2014/03/26 14:41:41 mpi Exp $ */
/*-
 * Copyright (c) 2000 Doug Rabson
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD: src/sys/pci/agp.c,v 1.12 2001/05/19 01:28:07 alfred Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/rwlock.h>

#include <uvm/uvm_extern.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>

#include <dev/pci/agpvar.h>
#include <dev/pci/agpreg.h>

/*
 * the enable and {alloc, free, bind, unbind} memory routines have default
 * fallbacks, these macros do the right thing. The callbacks with no fallback
 * are called directly. These are mostly hacks around the weirdness of intel
 * integrated graphics, since they are not technically a true agp chipset,
 * but provide an almost identical interface.
 */
int	agp_generic_enable(struct agp_softc *, u_int32_t);
struct agp_memory *
	agp_generic_alloc_memory(struct agp_softc *, int, vsize_t size);
int	agp_generic_free_memory(struct agp_softc *, struct agp_memory *);
void	agp_attach(struct device *, struct device *, void *);
int	agp_probe(struct device *, void *, void *);

struct agp_memory *agp_find_memory(struct agp_softc *, int);
struct agp_memory *agp_lookup_memory(struct agp_softc *, off_t);

int	agpvga_match(struct pci_attach_args *);
int	agp_acquire_helper(void *dev, enum agp_acquire_state state);
int	agp_release_helper(void *dev, enum agp_acquire_state state);

int
agpdev_print(void *aux, const char *pnp)
{
	if (pnp) {
		printf("agp at %s", pnp);
	}
	return (UNCONF);
}

int
agpbus_probe(struct agp_attach_args *aa)
{
	struct pci_attach_args	*pa = aa->aa_pa;

	if (strncmp(aa->aa_busname, "agp", 3) == 0 &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE && 
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_HOST)
		return (1);
	return (0);
}

/*
 * Find the video card hanging off the agp bus XXX assumes only one bus
 */
int
agpvga_match(struct pci_attach_args *pa)
{
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_DISPLAY &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_DISPLAY_VGA) {
		if (pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_AGP,
		    NULL, NULL))
			return (1);
	}
	return (0);
}

struct device *
agp_attach_bus(struct pci_attach_args *pa, const struct agp_methods *methods,
    bus_addr_t apaddr, bus_size_t apsize, struct device *dev)
{
	struct agpbus_attach_args arg;

	arg.aa_methods = methods;
	arg.aa_pa = pa;
	arg.aa_apaddr = apaddr;
	arg.aa_apsize = apsize;

	printf("\n"); /* newline from the driver that called us */
	return (config_found(dev, &arg, agpdev_print));
}

int
agp_probe(struct device *parent, void *match, void *aux)
{
	/*
	 * we don't do any checking here, driver we're attaching this
	 * interface to should have already done it.
	 */
	return (1);
}

void
agp_attach(struct device *parent, struct device *self, void *aux)
{
	struct agpbus_attach_args *aa = aux;
	struct pci_attach_args *pa = aa->aa_pa;
	struct agp_softc *sc = (struct agp_softc *)self;
	u_int memsize;
	int i;

	sc->sc_chipc = parent;
	sc->sc_methods = aa->aa_methods;
	sc->sc_apaddr = aa->aa_apaddr;
	sc->sc_apsize = aa->aa_apsize;

	static const int agp_max[][2] = {
		{0,		0},
		{32,		4},
		{64,		28},
		{128,		96},
		{256,		204},
		{512,		440},
		{1024,		942},
		{2048,		1920},
		{4096,		3932}
	};

	/*
	 * Work out an upper bound for agp memory allocation. This
	 * uses a heuristic table from the Linux driver.
	 */
	memsize = ptoa(physmem) >> 20;

	for (i = 0; i < nitems(agp_max) && memsize > agp_max[i][0]; i++)
		;
	if (i == nitems(agp_max))
		i = nitems(agp_max) - 1;
	sc->sc_maxmem = agp_max[i][1] << 20;

	/*
	 * The lock is used to prevent re-entry to
	 * agp_generic_bind_memory() since that function can sleep.
	 */
	rw_init(&sc->sc_lock, "agplk");

	TAILQ_INIT(&sc->sc_memory);

	sc->sc_pcitag = pa->pa_tag;
	sc->sc_pc = pa->pa_pc;
	sc->sc_id = pa->pa_id;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_memt = pa->pa_memt;

	pci_get_capability(sc->sc_pc, sc->sc_pcitag, PCI_CAP_AGP,
	    &sc->sc_capoff, NULL);

	printf(": aperture at 0x%lx, size 0x%lx\n", (u_long)sc->sc_apaddr,
	    (u_long)sc->sc_apsize);
}

struct cfattach agp_ca = {
	sizeof(struct agp_softc), agp_probe, agp_attach,
	NULL, NULL
};

struct cfdriver agp_cd = {
	NULL, "agp", DV_DULL
};

struct agp_memory *
agp_find_memory(struct agp_softc *sc, int id)
{
	struct agp_memory *mem;

	AGP_DPF("searching for memory block %d\n", id);
	TAILQ_FOREACH(mem, &sc->sc_memory, am_link) {
		AGP_DPF("considering memory block %d\n", mem->am_id);
		if (mem->am_id == id)
			return (mem);
	}
	return (NULL);
}


struct agp_memory *
agp_lookup_memory(struct agp_softc *sc, off_t off)
{
	struct agp_memory* mem;

	AGP_DPF("searching for memory offset 0x%lx\n", (unsigned long)off);
	TAILQ_FOREACH(mem, &sc->sc_memory, am_link) {
		if (mem->am_is_bound == 0)
			continue;
		if (off >= mem->am_offset &&
		    off < (mem->am_offset + mem->am_size))
			return (mem);
	}
	return (NULL);
}

struct agp_gatt *
agp_alloc_gatt(bus_dma_tag_t dmat, u_int32_t apsize)
{
	struct agp_gatt		*gatt;
	u_int32_t	 	 entries = apsize >> AGP_PAGE_SHIFT;

	gatt = malloc(sizeof(*gatt), M_AGP, M_NOWAIT | M_ZERO);
	if (!gatt)
		return (NULL);
	gatt->ag_entries = entries;
	gatt->ag_size = entries * sizeof(u_int32_t);

	if (agp_alloc_dmamem(dmat, gatt->ag_size, &gatt->ag_dmamap,
	    &gatt->ag_physical, &gatt->ag_dmaseg) != 0) {
		free(gatt, M_AGP);
		return (NULL);
	}

	if (bus_dmamem_map(dmat, &gatt->ag_dmaseg, 1, gatt->ag_size,
	    (caddr_t *)&gatt->ag_virtual, BUS_DMA_NOWAIT) != 0) {
		agp_free_dmamem(dmat, gatt->ag_size, gatt->ag_dmamap,
		    &gatt->ag_dmaseg);
		free(gatt, M_AGP);
		return (NULL);
	}

	agp_flush_cache();

	return (gatt);
}

void
agp_free_gatt(bus_dma_tag_t dmat, struct agp_gatt *gatt)
{
	bus_dmamem_unmap(dmat, (caddr_t)gatt->ag_virtual, gatt->ag_size);
	agp_free_dmamem(dmat, gatt->ag_size, gatt->ag_dmamap, &gatt->ag_dmaseg);
	free(gatt, M_AGP);
}

int
agp_generic_enable(struct agp_softc *sc, u_int32_t mode)
{
	struct pci_attach_args	pa;
	pcireg_t		tstatus, mstatus, command;
	int			rq, sba, fw, rate, capoff;
	
	if (pci_find_device(&pa, agpvga_match) == 0 ||
	    pci_get_capability(pa.pa_pc, pa.pa_tag, PCI_CAP_AGP,
	    &capoff, NULL) == 0) {
		printf("agp_generic_enable: not an AGP capable device\n");
		return (-1);
	}

	tstatus = pci_conf_read(sc->sc_pc, sc->sc_pcitag,
	    sc->sc_capoff + AGP_STATUS);
	/* display agp mode */
	mstatus = pci_conf_read(pa.pa_pc, pa.pa_tag,
	    capoff + AGP_STATUS);

	/* Set RQ to the min of mode, tstatus and mstatus */
	rq = AGP_MODE_GET_RQ(mode);
	if (AGP_MODE_GET_RQ(tstatus) < rq)
		rq = AGP_MODE_GET_RQ(tstatus);
	if (AGP_MODE_GET_RQ(mstatus) < rq)
		rq = AGP_MODE_GET_RQ(mstatus);

	/* Set SBA if all three can deal with SBA */
	sba = (AGP_MODE_GET_SBA(tstatus)
	    & AGP_MODE_GET_SBA(mstatus)
	    & AGP_MODE_GET_SBA(mode));

	/* Similar for FW */
	fw = (AGP_MODE_GET_FW(tstatus)
	    & AGP_MODE_GET_FW(mstatus)
	    & AGP_MODE_GET_FW(mode));

	/* Figure out the max rate */
	rate = (AGP_MODE_GET_RATE(tstatus)
	    & AGP_MODE_GET_RATE(mstatus)
	    & AGP_MODE_GET_RATE(mode));
	if (rate & AGP_MODE_RATE_4x)
		rate = AGP_MODE_RATE_4x;
	else if (rate & AGP_MODE_RATE_2x)
		rate = AGP_MODE_RATE_2x;
	else
		rate = AGP_MODE_RATE_1x;

	/* Construct the new mode word and tell the hardware  */
	command = AGP_MODE_SET_RQ(0, rq);
	command = AGP_MODE_SET_SBA(command, sba);
	command = AGP_MODE_SET_FW(command, fw);
	command = AGP_MODE_SET_RATE(command, rate);
	command = AGP_MODE_SET_AGP(command, 1);

	pci_conf_write(sc->sc_pc, sc->sc_pcitag,
	    sc->sc_capoff + AGP_COMMAND, command);
	pci_conf_write(pa.pa_pc, pa.pa_tag, capoff + AGP_COMMAND, command);
	return (0);
}

struct agp_memory *
agp_generic_alloc_memory(struct agp_softc *sc, int type, vsize_t size)
{
	struct agp_memory *mem;

	if (type != 0) {
		printf("agp_generic_alloc_memory: unsupported type %d\n", type);
		return (0);
	}

	mem = malloc(sizeof *mem, M_AGP, M_WAITOK | M_ZERO);

	if (bus_dmamap_create(sc->sc_dmat, size, size / PAGE_SIZE + 1,
	    size, 0, BUS_DMA_NOWAIT, &mem->am_dmamap) != 0) {
		free(mem, M_AGP);
		return (NULL);
	}

	mem->am_id = sc->sc_nextid++;
	mem->am_size = size;
	TAILQ_INSERT_TAIL(&sc->sc_memory, mem, am_link);
	sc->sc_allocated += size;

	return (mem);
}

int
agp_generic_free_memory(struct agp_softc *sc, struct agp_memory *mem)
{
	if (mem->am_is_bound)
		return (EBUSY);

	sc->sc_allocated -= mem->am_size;
	TAILQ_REMOVE(&sc->sc_memory, mem, am_link);
	bus_dmamap_destroy(sc->sc_dmat, mem->am_dmamap);
	free(mem, M_AGP);
	return (0);
}

int
agp_generic_bind_memory(struct agp_softc *sc, struct agp_memory *mem,
    bus_size_t offset)
{
	bus_dma_segment_t	*segs, *seg;
	bus_addr_t		 apaddr = sc->sc_apaddr + offset;
	bus_size_t		 done, i, j;
	int			 nseg, error;

	rw_enter_write(&sc->sc_lock);

	if (mem->am_is_bound) {
		printf("AGP: memory already bound\n");
		rw_exit_write(&sc->sc_lock);
		return (EINVAL);
	}

	if ((offset & (AGP_PAGE_SIZE - 1)) != 0 ||
	    offset + mem->am_size > sc->sc_apsize) {
		printf("AGP: binding memory at bad offset %#lx\n",
		    (unsigned long) offset);
		rw_exit_write(&sc->sc_lock);
		return (EINVAL);
	}

	/*
	 * The memory here needs to be directly accessable from the
	 * AGP video card, so it should be allocated using bus_dma.
	 * However, it need not be contiguous, since individual pages
	 * are translated using the GATT.
	 */

	nseg = (mem->am_size + PAGE_SIZE - 1) / PAGE_SIZE;
	segs = malloc(nseg * sizeof *segs, M_AGP, M_WAITOK);
	if ((error = bus_dmamem_alloc(sc->sc_dmat, mem->am_size, PAGE_SIZE, 0,
	    segs, nseg, &mem->am_nseg, BUS_DMA_ZERO | BUS_DMA_WAITOK)) != 0) {
		free(segs, M_AGP);
		rw_exit_write(&sc->sc_lock);
		AGP_DPF("bus_dmamem_alloc failed %d\n", error);
		return (error);
	}
	if ((error = bus_dmamap_load_raw(sc->sc_dmat, mem->am_dmamap, segs,
	    mem->am_nseg, mem->am_size, BUS_DMA_WAITOK)) != 0) {
		bus_dmamem_free(sc->sc_dmat, segs, mem->am_nseg);
		free(segs, M_AGP);
		rw_exit_write(&sc->sc_lock);
		AGP_DPF("bus_dmamap_load failed %d\n", error);
		return (error);
	}
	mem->am_dmaseg = segs;

	/*
	 * Install entries in the GATT, making sure that if
	 * AGP_PAGE_SIZE < PAGE_SIZE and mem->am_size is not
	 * aligned to PAGE_SIZE, we don't modify too many GATT
	 * entries. Flush chipset tlb when done.
	 */
	done = 0;
	for (i = 0; i < mem->am_dmamap->dm_nsegs; i++) {
		seg = &mem->am_dmamap->dm_segs[i];
		for (j = 0; j < seg->ds_len && (done + j) < mem->am_size;
		    j += AGP_PAGE_SIZE) {
			AGP_DPF("binding offset %#lx to pa %#lx\n",
			    (unsigned long)(offset + done + j),
			    (unsigned long)seg->ds_addr + j);
			sc->sc_methods->bind_page(sc->sc_chipc,
			    apaddr + done + j, seg->ds_addr + j, 0);
		}
		done += seg->ds_len;
	}

	/*
	 * Flush the cpu cache since we are providing a new mapping
	 * for these pages.
	 */
	agp_flush_cache();

	/*
	 * Make sure the chipset gets the new mappings.
	 */
	sc->sc_methods->flush_tlb(sc->sc_chipc);

	mem->am_offset = offset;
	mem->am_is_bound = 1;

	rw_exit_write(&sc->sc_lock);

	return (0);
}

int
agp_generic_unbind_memory(struct agp_softc *sc, struct agp_memory *mem)
{
	bus_addr_t	apaddr = sc->sc_apaddr + mem->am_offset;
	bus_size_t	i;

	rw_enter_write(&sc->sc_lock);

	if (mem->am_is_bound == 0) {
		printf("AGP: memory is not bound\n");
		rw_exit_write(&sc->sc_lock);
		return (EINVAL);
	}

	if (mem->am_mapref > 0) {
		printf("AGP: memory is mapped\n");
		rw_exit_write(&sc->sc_lock);
		return (EINVAL);
	}

	/*
	 * Unbind the individual pages and flush the chipset's
	 * TLB. Unwire the pages so they can be swapped.
	 */
	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE)
		sc->sc_methods->unbind_page(sc->sc_chipc, apaddr + i);

	agp_flush_cache();
	sc->sc_methods->flush_tlb(sc->sc_chipc);

	bus_dmamap_unload(sc->sc_dmat, mem->am_dmamap);
	bus_dmamem_free(sc->sc_dmat, mem->am_dmaseg, mem->am_nseg);

	free(mem->am_dmaseg, M_AGP);

	mem->am_offset = 0;
	mem->am_is_bound = 0;

	rw_exit_write(&sc->sc_lock);

	return (0);
}

/*
 * Allocates a single-segment block of zeroed, wired dma memory.
 */
int
agp_alloc_dmamem(bus_dma_tag_t tag, size_t size, bus_dmamap_t *mapp,
    bus_addr_t *baddr, bus_dma_segment_t *seg)
{
	int error, level = 0, nseg;

	if ((error = bus_dmamem_alloc(tag, size, PAGE_SIZE, 0,
	    seg, 1, &nseg, BUS_DMA_NOWAIT | BUS_DMA_ZERO)) != 0)
		goto out;
	level++;

	if ((error = bus_dmamap_create(tag, size, nseg, size, 0,
	    BUS_DMA_NOWAIT, mapp)) != 0)
		goto out;
	level++;

	if ((error = bus_dmamap_load_raw(tag, *mapp, seg, nseg, size,
	    BUS_DMA_NOWAIT)) != 0)
		goto out;

	*baddr = (*mapp)->dm_segs[0].ds_addr;

	return (0);
out:
	switch (level) {
	case 2:
		bus_dmamap_destroy(tag, *mapp);
		/* FALLTHROUGH */
	case 1:
		bus_dmamem_free(tag, seg, nseg);
		break;
	default:
		break;
	}

	return (error);
}

void
agp_free_dmamem(bus_dma_tag_t tag, size_t size, bus_dmamap_t map,
    bus_dma_segment_t *seg)
{
	bus_dmamap_unload(tag, map);
	bus_dmamap_destroy(tag, map);
	bus_dmamem_free(tag, seg, 1);
}

/* Helper functions used in both user and kernel APIs */

int
agp_acquire_helper(void *dev, enum agp_acquire_state state)
{
	struct agp_softc *sc = (struct agp_softc *)dev;

	if (sc->sc_chipc == NULL) 
		return (EINVAL);

	if (sc->sc_state != AGP_ACQUIRE_FREE)
		return (EBUSY);
	sc->sc_state = state;

	return (0);
}

int
agp_release_helper(void *dev, enum agp_acquire_state state)
{
	struct agp_softc *sc = (struct agp_softc *)dev;
	struct agp_memory* mem;

	if (sc->sc_state == AGP_ACQUIRE_FREE)
		return (0);

	if (sc->sc_state != state) 
		return (EBUSY);

	/*
	 * Clear out the aperture and free any
	 * outstanding memory blocks.
	 */
	TAILQ_FOREACH(mem, &sc->sc_memory, am_link) {
		if (mem->am_is_bound) {
			printf("agp_release_helper: mem %d is bound\n",
			    mem->am_id);
			agp_unbind_memory(sc, mem);
		}
	}
	sc->sc_state = AGP_ACQUIRE_FREE;
	return (0);
}

/* Implementation of the kernel api */

void *
agp_find_device(int unit)
{
	if (unit >= agp_cd.cd_ndevs || unit < 0)
		return (NULL);
	return (agp_cd.cd_devs[unit]);
}

enum agp_acquire_state
agp_state(void *dev)
{
	struct agp_softc *sc = (struct agp_softc *) dev;
        return (sc->sc_state);
}

void
agp_get_info(void *dev, struct agp_info *info)
{
	struct agp_softc *sc = (struct agp_softc *)dev;

	if (sc->sc_capoff != 0)
		info->ai_mode = pci_conf_read(sc->sc_pc, sc->sc_pcitag,
		    AGP_STATUS + sc->sc_capoff);
	else
		info->ai_mode = 0; /* i810 doesn't have real AGP */
	info->ai_aperture_base = sc->sc_apaddr;
	info->ai_aperture_size = sc->sc_apsize;
	info->ai_memory_allowed = sc->sc_maxmem;
	info->ai_memory_used = sc->sc_allocated;
	info->ai_devid = sc->sc_id;
}

int
agp_acquire(void *dev)
{
	struct agp_softc *sc = (struct agp_softc *)dev;

        return (agp_acquire_helper(sc, AGP_ACQUIRE_KERNEL));
}

int
agp_release(void *dev)
{
	struct agp_softc *sc = (struct agp_softc *)dev;

        return (agp_release_helper(sc, AGP_ACQUIRE_KERNEL));
}

int
agp_enable(void *dev, u_int32_t mode)
{
	struct agp_softc	*sc = dev;
	int			 ret;

	if (sc->sc_methods->enable != NULL) {
		ret = sc->sc_methods->enable(sc->sc_chipc, mode);
	} else {
		ret = agp_generic_enable(sc, mode);
	}
	return (ret);
}

void *
agp_alloc_memory(void *dev, int type, vsize_t bytes)
{
	struct agp_softc	*sc = dev;
	struct agp_memory	*mem;

	if (sc->sc_methods->alloc_memory != NULL) {
		mem = sc->sc_methods->alloc_memory(sc->sc_chipc, type, bytes);
	} else {
		mem = agp_generic_alloc_memory(sc, type, bytes);
	}
        return  (mem);
}

void
agp_free_memory(void *dev, void *handle)
{
	struct agp_softc *sc = dev;
        struct agp_memory *mem = handle;

	if (sc->sc_methods->free_memory != NULL) {
		sc->sc_methods->free_memory(sc->sc_chipc, mem);
	} else {
		agp_generic_free_memory(sc, mem);
	}
}

int
agp_bind_memory(void *dev, void *handle, off_t offset)
{
	struct agp_softc	*sc = dev;
	struct agp_memory	*mem = handle;
	int			 ret;	

	if (sc->sc_methods->bind_memory != NULL) {
		ret = sc->sc_methods->bind_memory(sc->sc_chipc, mem, offset);
	} else {
		ret = agp_generic_bind_memory(sc, mem, offset);
	}
	return (ret);
}

int
agp_unbind_memory(void *dev, void *handle)
{
	struct agp_softc	*sc = dev;
        struct agp_memory	*mem = handle;
	int			 ret;	

	if (sc->sc_methods->unbind_memory != NULL) {
		ret = sc->sc_methods->unbind_memory(sc->sc_chipc, mem);
	} else {
		ret = agp_generic_unbind_memory(sc, mem);
	}
	return (ret);
}

void
agp_memory_info(void *dev, void *handle, struct agp_memory_info *mi)
{
        struct agp_memory *mem = (struct agp_memory *) handle;

        mi->ami_size = mem->am_size;
        mi->ami_physical = mem->am_physical;
        mi->ami_offset = mem->am_offset;
        mi->ami_is_bound = mem->am_is_bound;
}

void *
agp_map(struct agp_softc *sc, bus_size_t address, bus_size_t size,
    bus_space_handle_t *memh)
{
	struct agp_memory* mem;

	if (sc->sc_chipc == NULL)
		return (NULL);

	if (address >= sc->sc_apsize)
		return (NULL);

	if (sc->sc_apaddr) {
		if (bus_space_map(sc->sc_memt, sc->sc_apaddr + address, size,
		    BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_PREFETCHABLE, memh))
			return (NULL);
	} else {
		/*
		 * If the aperture base address is 0 assume that the AGP
		 * bridge does not support remapping for processor accesses.
		 */
		mem = agp_lookup_memory(sc, address);
		if (mem == NULL)
			return (NULL);

		/*
		 * Map the whole memory region because it is easier to
		 * do so and it is improbable that only a part of it
		 * will be used.
		 */
		if (mem->am_mapref == 0)
			if (bus_dmamem_map(sc->sc_dmat, mem->am_dmaseg,
			    mem->am_nseg, mem->am_size, &mem->am_kva,
			    BUS_DMA_NOWAIT | BUS_DMA_NOCACHE))
				return (NULL);

		mem->am_mapref++;

		/*
		 * XXX Fake a bus handle even if it is managed memory,
		 * this is needed at least by radeondrm(4).
		 */
		*memh = (bus_space_handle_t)(mem->am_kva + address);
	}

	return bus_space_vaddr(sc->sc_memt, *memh);
}

void
agp_unmap(struct agp_softc *sc, void *address, size_t size,
    bus_space_handle_t memh)
{
	struct agp_memory* mem;
	caddr_t kva;

	if (sc->sc_apaddr)
		return bus_space_unmap(sc->sc_memt, memh, size);

	kva = (caddr_t)address;
	TAILQ_FOREACH(mem, &sc->sc_memory, am_link) {
		if (mem->am_is_bound == 0)
			continue;

		if (kva >= mem->am_kva && kva < (mem->am_kva + mem->am_size)) {
			mem->am_mapref--;

			if (mem->am_mapref == 0) {
				bus_dmamem_unmap(sc->sc_dmat, mem->am_kva,
				    mem->am_size);
				mem->am_kva = 0;
			}
			break;
		}
	}
}

paddr_t
agp_mmap(struct agp_softc *sc, off_t off, int prot)
{
	struct agp_memory* mem;

	if (sc->sc_chipc == NULL)
		return (-1);

	if (off >= sc->sc_apsize)
		return (-1);

	if (sc->sc_apaddr)
		return bus_space_mmap(sc->sc_memt, sc->sc_apaddr, off, prot, 0);

	mem = agp_lookup_memory(sc, off);
	if (mem == NULL)
		return (-1);

	return bus_dmamem_mmap(sc->sc_dmat, mem->am_dmaseg, mem->am_nseg, off,
	    prot, BUS_DMA_NOCACHE);
}
