/*	$OpenBSD: bootxx.c,v 1.13 2013/07/05 21:13:07 miod Exp $ */
/* $NetBSD: bootxx.c,v 1.16 2002/03/29 05:45:08 matt Exp $ */

/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 *	@(#)boot.c	7.15 (Berkeley) 5/4/91
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>

#include <lib/libsa/stand.h>
#include <lib/libsa/ufs.h>
#include <lib/libsa/cd9660.h>

#include <machine/pte.h>
#include <machine/sid.h>
#include <machine/mtpr.h>
#include <machine/reg.h>
#include <machine/rpb.h>
#include <arch/vax/vax/gencons.h>

#define NRSP 1 /* Kludge */
#define NCMD 1 /* Kludge */
#define LIBSA_TOO_OLD

#include <arch/vax/mscp/mscp.h>
#include <arch/vax/mscp/mscpreg.h>

#include "../boot/data.h"

#define	RF_PROTECTED_SECTORS	64	/* XXX refer to <.../rf_optnames.h> */

void	Xmain(void);
void	hoppabort(int);
void	romread_uvax(int lbn, int size, void *buf, struct rpb *rpb);
int	unit_init(int, struct rpb *, int);

struct open_file file;

unsigned *bootregs;
struct	rpb *rpb;
struct	bqo *bqo;
int	vax_cputype;
int	vax_load_failure;
struct udadevice {u_short udaip;u_short udasa;};
volatile struct udadevice *csr;
static int moved;

extern int from;
#define	FROMMV	2
#define	FROMVMB	4

/*
 * The boot blocks are used by MicroVAX II/III, VS2000,
 * VS3100, VS4000, and only when booting from disk.
 */
void
Xmain(void)
{
	union {
		struct exec aout;
		Elf32_Ehdr elf;
	} hdr;
	int io;
	u_long entry;

	vax_cputype = (mfpr(PR_SID) >> 24) & 0xFF;
	moved = 0;
	/*
	 */ 
	rpb = (void *)0xf0000; /* Safe address right now */
	bqo = (void *)0xf1000;
        if (from == FROMMV) {
		/*
		 * now relocate rpb/bqo (which are used by ROM-routines)
		 */
		bcopy ((void *)bootregs[11], rpb, sizeof(struct rpb));
		bcopy ((void*)rpb->iovec, bqo, rpb->iovecsz);
#if 0
		if (rpb->devtyp == BDEV_SDN)
			rpb->devtyp = BDEV_SD;	/* XXX until driver fixed */
#endif
	} else {
		bzero(rpb, sizeof(struct rpb));
		rpb->devtyp = bootregs[0];
		rpb->unit = bootregs[3];
		rpb->rpb_bootr5 = bootregs[5];
		rpb->csrphy = bootregs[2];
		rpb->adpphy = bootregs[1];	/* BI node on 8200 */
        }
	rpb->rpb_base = rpb;
	rpb->iovec = (int)bqo;

	io = open("/boot.vax", 0);
	if (io < 0)
		io = open("/boot", 0);
	if (io < 0)
		asm("movl $0xbeef1, %r0; halt");

	read(io, (void *)&hdr.aout, sizeof(hdr.aout));
	if (N_GETMAGIC(hdr.aout) == OMAGIC && N_GETMID(hdr.aout) == MID_VAX) {
		vax_load_failure++;
		entry = hdr.aout.a_entry;
		if (entry < sizeof(hdr.aout))
			entry = sizeof(hdr.aout);
		read(io, (void *) entry, hdr.aout.a_text + hdr.aout.a_data);
		memset((void *) (entry + hdr.aout.a_text + hdr.aout.a_data),
		       0, hdr.aout.a_bss);
	} else if (memcmp(hdr.elf.e_ident, ELFMAG, SELFMAG) == 0) {
		Elf32_Phdr ph;
		size_t off = sizeof(hdr.elf);
		vax_load_failure += 2;
		read(io, (caddr_t)(&hdr.elf) + sizeof(hdr.aout),
		     sizeof(hdr.elf) - sizeof(hdr.aout));
		if (hdr.elf.e_machine != EM_VAX || hdr.elf.e_type != ET_EXEC
		    || hdr.elf.e_phnum != 1)
			goto die;
		vax_load_failure++;
		entry = hdr.elf.e_entry;
		if (hdr.elf.e_phoff != sizeof(hdr.elf)) 
			goto die;
		vax_load_failure++;
		read(io, &ph, sizeof(ph));
		off += sizeof(ph);
		if (ph.p_type != PT_LOAD)
			goto die;
		vax_load_failure++;
		while (off < ph.p_offset) {
			u_int32_t tmp;
			read(io, &tmp, sizeof(tmp));
			off += sizeof(tmp);
		}
		read(io, (void *) ph.p_paddr, ph.p_filesz);
		memset((void *) (ph.p_paddr + ph.p_filesz), 0,
		       ph.p_memsz - ph.p_filesz);
	} else {
		goto die;
	}
	hoppabort(entry);
die:
	asm("movl $0xbeef2, %r0; halt");
}

/*
 * Write an extremely limited version of a (us)tar filesystem, suitable
 * for loading secondary-stage boot loader.
 * - Can only load file "boot".
 * - Must be the first file on tape.
 */
struct fs_ops file_system[] = {
#ifdef NEED_UFS
	{ ufs_open, 0, ufs_read, 0, 0, ufs_stat },
#endif
#ifdef NEED_CD9660
	{ cd9660_open, 0, cd9660_read, 0, 0, cd9660_stat },
#endif
#ifdef NEED_USTARFS
	{ ustarfs_open, 0, ustarfs_read, 0, 0, ustarfs_stat },
#endif
};

int nfsys = (sizeof(file_system) / sizeof(struct fs_ops));

#ifdef LIBSA_TOO_OLD
#include "../boot/vaxstand.h"

struct rom_softc {
       int part;
       int unit;
} rom_softc;

int    romstrategy(void *, int, daddr32_t, size_t, void *, size_t *);
int romopen(struct open_file *, int, int, int, int);
struct devsw   devsw[] = {
       SADEV("rom", romstrategy, romopen, nullsys, noioctl),
};
int    ndevs = (sizeof(devsw)/sizeof(devsw[0]));

int
romopen(struct open_file *f, int adapt, int ctlr, int unit, int part)
{
       rom_softc.unit = unit;
       rom_softc.part = part;
       
       f->f_devdata = (void *)&rom_softc;
       
       return 0;
}

#endif

int
devopen(struct open_file *f, const char *fname, char **file)
{

#ifdef LIBSA_TOO_OLD
	f->f_dev = &devsw[0];
#endif
	*file = (char *)fname;

	/*
	 * Reinit the VMB boot device.
	 */
	if (bqo->unit_init && (moved++ == 0)) {
		int initfn;

		initfn = rpb->iovec + bqo->unit_init;
		if (rpb->devtyp == BDEV_UDA || rpb->devtyp == BDEV_TK) {
			/*
			 * This reset do not seem to be done in the 
			 * ROM routines, so we have to do it manually.
			 */
			csr = (struct udadevice *)rpb->csrphy;
			csr->udaip = 0;
			while ((csr->udasa & MP_STEP1) == 0)
				;
		}
		/*
		 * AP (R12) have a pointer to the VMB argument list,
		 * wanted by bqo->unit_init.
		 */
		unit_init(initfn, rpb, bootregs[12]);
	}
	return 0;
}

extern struct disklabel romlabel;

int
romstrategy(sc, func, dblk, size, buf, rsize)
	void    *sc;
	int     func;
	daddr32_t dblk;
	size_t	size;
	void    *buf;
	size_t	*rsize;
{
	int	block = dblk;
	int     nsize = size;

	if (romlabel.d_magic == DISKMAGIC && romlabel.d_magic2 == DISKMAGIC) {
		if (romlabel.d_npartitions > 1) {
			block += romlabel.d_partitions[0].p_offset;
			if (romlabel.d_partitions[0].p_fstype == FS_RAID) {
				block += RF_PROTECTED_SECTORS;
			}
		}
	}

	romread_uvax(block, size, buf, rpb);

	if (rsize)
		*rsize = nsize;
	return 0;
}

extern char end[];
static char *top = (char*)end;

void *
alloc(unsigned int size)
{
	void *ut = top;
	top += size;
	return ut;
}

void
free(void *ptr, unsigned int size)
{
}

#ifdef USE_PRINTF
void
putchar(int ch)
{
	/*
	 * On KA88 we may get C-S/C-Q from the console.
	 * Must obey it.
	 */
	while (mfpr(PR_RXCS) & GC_DON) {
		if ((mfpr(PR_RXDB) & 0x7f) == 19) {
			while (1) {
				while ((mfpr(PR_RXCS) & GC_DON) == 0)
					;
				if ((mfpr(PR_RXDB) & 0x7f) == 17)
					break;
			}
		}
	}

	while ((mfpr(PR_TXCS) & GC_RDY) == 0)
		;
	mtpr(0, PR_TXCS);
	mtpr(ch & 0377, PR_TXDB);
	if (ch == 10)
		putchar(13);
}
#endif
