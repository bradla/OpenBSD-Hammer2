/*	$OpenBSD: hibernate_machdep.c,v 1.3 2013/06/05 01:33:02 pirofti Exp $	*/

/*
 * Copyright (c) 2013 Paul Irofti.
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


#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/hibernate.h>
#include <sys/timeout.h>
#include <sys/malloc.h>
#include <sys/kcore.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_pmemrange.h>

#include <mips64/cache.h>

#include <machine/hibernate.h>
#include <machine/hibernate_var.h>
#include <machine/kcore.h>
#include <machine/pmap.h>
#include <machine/memconf.h>

#include "wd.h"
#include "ahci.h"
#include "sd.h"

#if NWD > 0
#include <dev/ata/atavar.h>
#include <dev/ata/wdvar.h>
#endif


/*
 * Loongson MD Hibernate functions
 *
 * see Loongson hibernate.h for lowmem layout used during hibernate
 */

/*
 * Returns the hibernate write I/O function to use on this machine
 */
hibio_fn
get_hibernate_io_function(void)
{
	char *blkname = findblkname(major(swdevt[0].sw_dev));

	if (blkname == NULL)
		return NULL;

#if NWD > 0
	if (strcmp(blkname, "wd") == 0)
		return wd_hibernate_io;
#endif
#if NAHCI > 0 && NSD > 0
	if (strcmp(blkname, "sd") == 0) {
		extern struct cfdriver sd_cd;
		extern int ahci_hibernate_io(dev_t dev, daddr_t blkno,
		    vaddr_t addr, size_t size, int op, void *page);
		struct device *dv;

		dv = disk_lookup(&sd_cd, DISKUNIT(swdevt[0].sw_dev));
		if (dv && dv->dv_parent && dv->dv_parent->dv_parent &&
		    strcmp(dv->dv_parent->dv_parent->dv_cfdata->cf_driver->cd_name,
		    "ahci") == 0)
			return ahci_hibernate_io;
	}
#endif
	return NULL;
}

/*
 * Gather MD-specific data and store into hiber_info
 */
int
get_hibernate_info_md(union hibernate_info *hiber_info)
{
	int i;

	/* Calculate memory ranges */
	hiber_info->nranges = 0;
	hiber_info->image_size = 0;

	for (i = 0; i < MAXMEMSEGS && mem_layout[i].mem_last_page != 0; i++) {
		/* XXX: Adjust for stolen pages later */
		hiber_info->ranges[i].base =
		    mem_layout[i].mem_first_page >> PAGE_SHIFT;
		hiber_info->ranges[i].end =
		    mem_layout[i].mem_last_page >> PAGE_SHIFT;
		hiber_info->image_size +=
		    hiber_info->ranges[i].end - hiber_info->ranges[i].base;
		hiber_info->nranges++;
	}

	return (0);
}

/*
 * Enter a mapping for va->pa in the resume pagetable
 */
void
hibernate_enter_resume_mapping(vaddr_t va, paddr_t pa, int size)
{
	/* XXX TBD */
}

/*
 * Create the resume-time page table. This table maps the image(pig) area,
 * the kernel text area, and various utility pages for use during resume,
 * since we cannot overwrite the resuming kernel's page table during inflate
 * and expect things to work properly.
 */
void
hibernate_populate_resume_pt(union hibernate_info *hib_info,
    paddr_t image_start, paddr_t image_end)
{
	/* XXX TBD */
}

/*
 * MD-specific resume preparation (creating resume time pagetables,
 * stacks, etc).
 */
void
hibernate_prepare_resume_machdep(union hibernate_info *hib_info)
{
	paddr_t pa, piglet_end;
	vaddr_t va;

	/*
	 * At this point, we are sure that the piglet's phys space is going to
	 * have been unused by the suspending kernel, but the vaddrs used by
	 * the suspending kernel may or may not be available to us here in the
	 * resuming kernel, so we allocate a new range of VAs for the piglet.
	 * Those VAs will be temporary and will cease to exist as soon as we
	 * switch to the resume PT, so we need to ensure that any VAs required
	 * during inflate are also entered into that map.
	 */

        hib_info->piglet_va = (vaddr_t)km_alloc(HIBERNATE_CHUNK_SIZE*3,
	    &kv_any, &kp_none, &kd_nowait);
        if (!hib_info->piglet_va)
                panic("Unable to allocate vaddr for hibernate resume piglet\n");

	piglet_end = hib_info->piglet_pa + HIBERNATE_CHUNK_SIZE*3;

	for (pa = hib_info->piglet_pa,va = hib_info->piglet_va;
	    pa <= piglet_end; pa += PAGE_SIZE, va += PAGE_SIZE)
		pmap_kenter_pa(va, pa, VM_PROT_ALL);

	pmap_activate(curproc);
}

/*
 * During inflate, certain pages that contain our bookkeeping information
 * (eg, the chunk table, scratch pages, etc) need to be skipped over and
 * not inflated into.
 *
 * Returns 1 if the physical page at dest should be skipped, 0 otherwise
 */
int
hibernate_inflate_skip(union hibernate_info *hib_info, paddr_t dest)
{
	if (dest >= hib_info->piglet_pa &&
	    dest <= (hib_info->piglet_pa + 3 * HIBERNATE_CHUNK_SIZE))
		return (1);

	return (0);
}

void
hibernate_enable_intr_machdep(void)
{
	enableintr();
}

void
hibernate_disable_intr_machdep(void)
{
	disableintr();
}

void
hibernate_flush(void)
{
	Mips_SyncCache(curcpu());
}
