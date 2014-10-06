/*	$OpenBSD: cache_octeon.c,v 1.8 2014/03/31 20:21:19 miod Exp $	*/
/*
 * Copyright (c) 2010 Takuya ASADA.
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
/*
 * Copyright (c) 1998-2004 Opsycon AB (www.opsycon.se)
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <uvm/uvm_extern.h>

#include <mips64/cache.h>
#include <machine/cpu.h>

#define SYNCI() \
	asm volatile( \
		".set push\n" \
		".set mips64r2\n" \
		".word 0x041f0000\n" \
		"nop\n" \
		".set pop")

void 
Octeon_ConfigCache(struct cpu_info *ci)
{
	ci->ci_l1inst.size = 32 * 1024;
	ci->ci_l1inst.linesize = 128;
	ci->ci_l1inst.setsize = 4;
	ci->ci_l1inst.sets = ci->ci_l1inst.size / ci->ci_l1inst.setsize;

	ci->ci_l1data.size = 16 * 1024;
	ci->ci_l1data.linesize = 128;
	ci->ci_l1data.setsize = 4;
	ci->ci_l1data.sets = ci->ci_l1data.size / ci->ci_l1data.setsize;

	ci->ci_l2.size = 128 * 1024;
	ci->ci_l2.linesize = 128;
	ci->ci_l2.setsize = 4;
	ci->ci_l2.sets = ci->ci_l2.size / ci->ci_l2.setsize;

	memset(&ci->ci_l3, 0, sizeof(struct cache_info));

	ci->ci_SyncCache = Octeon_SyncCache;
	ci->ci_InvalidateICache = Octeon_InvalidateICache;
	ci->ci_InvalidateICachePage = Octeon_InvalidateICachePage;
	ci->ci_SyncICache = Octeon_SyncICache;
	ci->ci_SyncDCachePage = Octeon_SyncDCachePage;
	ci->ci_HitSyncDCache = Octeon_HitSyncDCache;
	ci->ci_HitInvalidateDCache = Octeon_HitInvalidateDCache;
	ci->ci_IOSyncDCache = Octeon_IOSyncDCache;
}

void
Octeon_SyncCache(struct cpu_info *ci)
{
	mips_sync();
}

void
Octeon_InvalidateICache(struct cpu_info *ci, vaddr_t va, size_t len)
{
	/* A SYNCI flushes the entire icache on OCTEON */
	SYNCI();
}

/*
 * Register a given page for I$ invalidation.
 */
void
Octeon_InvalidateICachePage(struct cpu_info *ci, vaddr_t va)
{
	/*
	 * Since there is apparently no way to operate on a subset of I$,
	 * all we need to do here is remember there are postponed flushes.
	 */
	ci->ci_cachepending_l1i = 1;
}

/*
 * Perform postponed I$ invalidation.
 */
void
Octeon_SyncICache(struct cpu_info *ci)
{
	if (ci->ci_cachepending_l1i != 0) {
		SYNCI(); /* Octeon_InvalidateICache(ci, 0, PAGE_SIZE); */
		ci->ci_cachepending_l1i = 0;
	}
}

void
Octeon_SyncDCachePage(struct cpu_info *ci, vaddr_t va, paddr_t pa)
{
}

void
Octeon_HitSyncDCache(struct cpu_info *ci, vaddr_t va, size_t len)
{
}

void
Octeon_HitInvalidateDCache(struct cpu_info *ci, vaddr_t va, size_t len)
{
}

void
Octeon_IOSyncDCache(struct cpu_info *ci, vaddr_t va, size_t len, int how)
{
	switch (how) {
	default:
	case CACHE_SYNC_R:
		break;
	case CACHE_SYNC_W: /* writeback */
	case CACHE_SYNC_X: /* writeback and invalidate */
		mips_sync();
		break;
	}
}
