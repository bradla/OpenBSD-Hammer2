/*	$OpenBSD: hibernate_var.h,v 1.9 2014/01/05 20:23:57 mlarkin Exp $	*/

/*
 * Copyright (c) 2011 Mike Larkin <mlarkin@openbsd.org>
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

/* i386 hibernate support definitions */

#define PAGE_SHIFT_4M 22
#define PAGE_MASK_4M (NBPD - 1)
#define PMAP_PA_MASK_4M ~((paddr_t)PAGE_MASK_4M)

#define PIGLET_PAGE_MASK ~((paddr_t)PAGE_MASK_4M)

#define HIBERNATE_PD_PAGE	(PAGE_SIZE * 18)
#define HIBERNATE_PT_PAGE	(PAGE_SIZE * 19)
/* 2 pages for stack */
#define HIBERNATE_STACK_PAGE	(PAGE_SIZE * 21)
#define HIBERNATE_INFLATE_PAGE	(PAGE_SIZE * 22)
/* HIBERNATE_HIBALLOC_PAGE must be the last stolen page (see machdep.c) */
#define HIBERNATE_HIBALLOC_PAGE (PAGE_SIZE * 23)

/* Use 4MB hibernation chunks */
#define HIBERNATE_CHUNK_SIZE		0x400000

#define HIBERNATE_CHUNK_TABLE_SIZE	0x100000

#define HIBERNATE_STACK_OFFSET	0x0F00

#define atop_4m(x) ((x) >> PAGE_SHIFT_4M)
#define atop_4k(x) ((x) >> PAGE_SHIFT)
#define s4pde_4m(va) ((pt_entry_t *)HIBERNATE_PD_PAGE + atop_4m(va))
#define s4pde_4k(va) ((pt_entry_t *)HIBERNATE_PD_PAGE + atop_4k(va))
#define s4pte_4k(va) ((pt_entry_t *)HIBERNATE_PT_PAGE + atop_4k(va))
