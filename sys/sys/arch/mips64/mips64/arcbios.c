/*	$OpenBSD: arcbios.c,v 1.35 2014/03/29 18:09:30 guenther Exp $	*/
/*-
 * Copyright (c) 1996 M. Warner Losh.  All rights reserved.
 * Copyright (c) 1996-2004 Opsycon AB.  All rights reserved.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/memconf.h>
#include <machine/vmparam.h>

#include <mips64/arcbios.h>
#include <mips64/archtype.h>

#include <uvm/uvm_extern.h>

#ifdef TGT_ORIGIN
#include <machine/mnode.h>
#endif

int bios_is_32bit;
void	*bios_base;

/*
 * If we cannot get the onboard Ethernet address to override this bogus
 * value, ether_ifattach() will pick a valid address.
 */
char bios_enaddr[20] = "ff:ff:ff:ff:ff:ff";

int bios_consrate;			/* Serial console speed. */
char bios_console[30];			/* Primary console. */
char bios_graphics[6];			/* Graphics state. */
char bios_keyboard[6];			/* Keyboard layout. */

void (*bios_halt)(void);
void (*bios_powerdown)(void);
void (*bios_restart)(void);
void (*bios_reboot)(void);
void (*bios_eim)(void);

extern int	physmem;		/* Total physical memory size */
extern int	rsvdmem;		/* Total reserved memory size */

void bios_configure_memory(void);
int bios_get_system_type(void);

arc_dsp_stat_t	displayinfo;		/* Save area for display status info. */

static struct systypes {
	char *sys_vend;		/* Vendor ID if name is ambiguous */
	char *sys_name;		/* May be left NULL if name is sufficient */
	int  sys_type;
} sys_types[] = {
#if 0
    { NULL,		"PICA-61",			ACER_PICA_61 },
    { NULL,		"NEC-R94",			ACER_PICA_61 },
    { NULL,		"DESKTECH-TYNE",		DESKSTATION_TYNE },
    { NULL,		"DESKTECH-ARCStation I",	DESKSTATION_RPC44 },
    { NULL,		"Microsoft-Jazz",		MAGNUM },
    { NULL,		"RM200PCI",			SNI_RM200 },
#endif
    { NULL,		"SGI-IP17",			SGI_IP17 },
    { NULL,		"SGI-IP19",			SGI_IP19 },
    { NULL,		"SGI-IP20",			SGI_IP20 },
    { NULL,		"SGI-IP21",			SGI_IP21 },
    { NULL,		"SGI-IP22",			SGI_IP22 },
    { NULL,		"SGI-IP25",			SGI_IP25 },
    { NULL,		"SGI-IP26",			SGI_IP26 },
    { NULL,		"SGI-IP28",			SGI_IP28 },
    { NULL,		"SGI-IP30",			SGI_OCTANE },
    { NULL,		"SGI-IP32",			SGI_O2 }
};

#define KNOWNSYSTEMS (sizeof(sys_types) / sizeof(struct systypes))

/*
 *	ARC Bios trampoline code.
 */

#define ARC_Call(Name,Offset)	\
__asm__("\n"			\
"	.text\n"		\
"	.ent	" #Name "\n"	\
"	.align	3\n"		\
"	.set	noreorder\n"	\
"	.globl	" #Name "\n"	\
#Name":\n"			\
"	lw	$3, bios_is_32bit\n"\
"	ld	$2, bios_base\n"\
"	beqz	$3, 1f\n"	\
"	 nop\n"			\
"	lw	$3, 0x20($2)\n"	\
"       lw      $2," #Offset "($3)\n"\
"	jr	$2\n"		\
"	 nop\n"			\
"1:\n"				\
"	ld	$3, 2*0x20($2)\n"\
"	ld	$2, 2*" #Offset "($3)\n"\
"	jr	$2\n"		\
"	 nop\n"			\
"	.end	" #Name "\n"	);

/*
 * Invoke an ARC routine through a function pointer (set up in bios_ident()),
 * also forcing the stack to be in CKSEG0 (actually, whatever area proc0 stack
 * was allocated in) if this is a 32-bit ARCBios; used by restart functions,
 * which aren't supposed to return anyway.
 */
#define ARC_Call_Via_Pointer(Name,fnptr) \
__asm__("\n"	\
"	.text\n"		\
"	.ent	" #Name "\n"	\
"	.align	3\n"		\
"	.set	noreorder\n"	\
"	.globl	" #Name "\n"	\
#Name":\n"			\
"	lw	$3, bios_is_32bit\n"\
"	beqz	$3, 1f\n"	\
"	 nop\n"			\
"	ld	$3, proc0paddr\n" \
"	addi	$29, $3, 16384 - 64\n"		/* assumes USPACE >= 16384 */ \
"1:\n"				\
"	ld	$2, " #fnptr "\n"\
"	jr	$2\n"		\
"	 nop\n"			\
"	.end	" #Name "\n");

#if 0
ARC_Call(Bios_Load,			0x00);
ARC_Call(Bios_Invoke,			0x04);
ARC_Call(Bios_Execute,			0x08);
#endif
ARC_Call_Via_Pointer(Bios_Halt,			bios_halt);
ARC_Call_Via_Pointer(Bios_PowerDown,		bios_powerdown);
ARC_Call_Via_Pointer(Bios_Restart,		bios_restart);
ARC_Call_Via_Pointer(Bios_Reboot,		bios_reboot);
ARC_Call_Via_Pointer(Bios_EnterInteractiveMode,	bios_eim);
#if 0
ARC_Call(Bios_Unused1,			0x20);
#endif
ARC_Call(Bios_GetPeer,			0x24);
ARC_Call(Bios_GetChild,			0x28);
#if 0
ARC_Call(Bios_GetParent,		0x2c);
#endif
#if 0
ARC_Call(Bios_GetConfigurationData,	0x30);
#endif
#if 0
ARC_Call(Bios_AddChild,			0x34);
ARC_Call(Bios_DeleteComponent,		0x38);
#endif
#if 0
ARC_Call(Bios_GetComponent,		0x3c);
ARC_Call(Bios_SaveConfiguration,	0x40);
#endif
ARC_Call(Bios_GetSystemId,		0x44);
ARC_Call(Bios_GetMemoryDescriptor,	0x48);
#if 0
ARC_Call(Bios_Unused2,			0x4c);
#endif
ARC_Call(Bios_GetTime,			0x50);
#if 0
ARC_Call(Bios_GetRelativeTime,		0x54);
ARC_Call(Bios_GetDirectoryEntry,	0x58);
ARC_Call(Bios_Open,			0x5c);
ARC_Call(Bios_Close,			0x60);
ARC_Call(Bios_Read,			0x64);
ARC_Call(Bios_GetReadStatus,		0x68);
#endif
ARC_Call(Bios_Write,			0x6c);
#if 0
ARC_Call(Bios_Seek,			0x70);
ARC_Call(Bios_Mount,			0x74);
#endif
ARC_Call(Bios_GetEnvironmentVariable,	0x78);
#if 0
ARC_Call(Bios_SetEnvironmentVariable,	0x7c);
ARC_Call(Bios_GetFileInformation,	0x80);
ARC_Call(Bios_SetFileInformation,	0x84);
#endif
ARC_Call(Bios_FlushAllCaches,		0x88);
#if 0
ARC_Call(Bios_TestUnicodeCharacter,	0x8c);
ARC_Call(Bios_GetDisplayStatus,		0x90);
#endif

/*
 *	Simple getchar/putchar interface.
 */

#if 0
int
bios_getchar()
{
	char buf[4];
	long  cnt;

	if (Bios_Read(0, &buf[0], 1, &cnt) != 0)
		return(-1);
	return(buf[0] & 255);
}
#endif

void
bios_putchar(c)
char c;
{
	char buf[4];
	long  cnt;

	if (c == '\n') {
		buf[0] = '\r';
		buf[1] = c;
		cnt = 2;
#ifdef __arc__
		if (displayinfo.CursorYPosition < displayinfo.CursorMaxYPosition)
			displayinfo.CursorYPosition++;
#endif
	}
	else {
		buf[0] = c;
		cnt = 1;
	}
	Bios_Write(1, &buf[0], cnt, &cnt);
}

void
bios_putstring(s)
char *s;
{
	while (*s) {
		bios_putchar(*s++);
	}
}

void
bios_printf(const char *fmt, ...)
{
	va_list ap;
	char buf[1024];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	bios_putstring(buf);
	va_end(ap);
}

/*
 * Get memory descriptor for the memory configuration and
 * create a layout database used by pmap init to set up
 * the memory system.
 *
 * Concatenate obvious adjacent segments.
 */
void
bios_configure_memory()
{
	arc_mem_t *descr = NULL;
	uint64_t start, count, prevend = 0;
	MEMORYTYPE type, prevtype = BadMemory;
	uint64_t seg_start, seg_end;
#ifdef TGT_ORIGIN
	int seen_free = 0;
#endif
#ifdef ARCBIOS_DEBUG
	int i;
#endif

	descr = (arc_mem_t *)Bios_GetMemoryDescriptor(descr);
	while (descr != NULL) {
		if (bios_is_32bit) {
			start = descr->BasePage;
			count = descr->PageCount;
			type = descr->Type;
		} else {
			start = ((arc_mem64_t *)descr)->BasePage;
			count = ((arc_mem64_t *)descr)->PageCount;
			type = descr->Type;

#ifdef TGT_OCTANE
			/*
			 * Memory above 1GB physical (address 1.5GB)
			 * gets reported as reserved on Octane, while
			 * it isn't.
			 * Abort scan at this point, platform dependent
			 * code will add the remaining memory, if any.
			 */
			if (sys_config.system_type == SGI_OCTANE &&
			    type == FirmwarePermanent &&
			    start >= 0x60000)
				break;
#endif

#ifdef TGT_ORIGIN
			if (sys_config.system_type == SGI_IP27) {
				/*
				 * For the lack of a better way to tell
				 * IP27 apart from IP35, look at the
				 * start of the first chunk of free
				 * memory. On IP27, it starts under
				 * 0x20000 (which allows us to link
				 * kernels at 0xa800000000020000).
				 * On IP35, it starts at 0x40000.
				 */
				if (type == FreeMemory && seen_free == 0) {
					seen_free = 1;
					if (start >= 0x20)	/* IP35 */
						sys_config.system_type =
						    SGI_IP35;
				}

				/*
				 * On IP27 and IP35 systems, data after the
				 * first FirmwarePermanent entry is not
				 * reliable (entries conflict with each other),
				 * and memory after 32MB (or 64MB on IP35) is
				 * not listed anyway.
				 * So, break from the loop as soon as a
				 * FirmwarePermanent entry is found, after
				 * making it span the end of the first 32MB
				 * (64MB on IP35).
				 *
				 * The rest of the memory will be gathered
				 * from the node structures.  This loses some
				 * of the first few MB (well... all of them
				 * but the kernel image), but at least we're
				 * safe to use ARCBios after going virtual.
				 */
				if (type == FirmwarePermanent) {
					descr = NULL; /* abort loop */
					count = ((sys_config.system_type ==
					    SGI_IP27 ?  32 : 64) << (20 - 12)) -
					    start;
				}
			}
#endif	/* O200 || O300 */
		}

		switch (type) {
		case BadMemory:		/* have no use for these */
			break;
		case LoadedProgram:
			/*
			 * LoadedProgram areas are either the boot loader,
			 * if the kernel has not been directly loaded by
			 * ARCBios, or the kernel image itself.
			 * Since we will move the kernel image out of the
			 * memory segments later anyway, it makes sense to
			 * claim this memory as free.
			 */
			/* FALLTHROUGH */
		case FreeMemory:
		case FreeContigous:
			/*
			 * Convert from ARCBios page size to kernel page size.
			 * As this can yield a smaller range due to possible
			 * different page size, we try to force coalescing
			 * with the previous range if this is safe.
			 */
			seg_start = atop(round_page(start * ARCBIOS_PAGE_SIZE));
			seg_end = atop(trunc_page((start + count) *
			    ARCBIOS_PAGE_SIZE));
			if (start == prevend)
				switch (prevtype) {
				case LoadedProgram:
				case FreeMemory:
				case FreeContigous:
					seg_start = atop(trunc_page(start *
					    ARCBIOS_PAGE_SIZE));
					break;
				default:
					break;
				}
			if (seg_start < seg_end)
				memrange_register(seg_start, seg_end, 0);
			break;
		case ExceptionBlock:
		case SystemParameterBlock:
		case FirmwareTemporary:
		case FirmwarePermanent:
			rsvdmem += count;
			break;
		default:		/* Unknown type, leave it alone... */
			break;
		}
		prevtype = type;
		prevend = start + count;
#ifdef TGT_ORIGIN
		if (descr == NULL)
			break;
#endif
		descr = (arc_mem_t *)Bios_GetMemoryDescriptor(descr);
	}

	/* convert rsvdmem to kernel pages, and count it in physmem */
	rsvdmem = atop(round_page(rsvdmem * ARCBIOS_PAGE_SIZE));
	physmem += rsvdmem;

#ifdef ARCBIOS_DEBUG
	for (i = 0; i < MAXMEMSEGS; i++) {
		if (mem_layout[i].mem_last_page) {
			bios_printf("MEM %d, %p to  %p\n", i,
			    ptoa(mem_layout[i].mem_first_page),
			    ptoa(mem_layout[i].mem_last_page));
	    }
	}
#endif
}

/*
 * Find out system type.
 */
int
bios_get_system_type()
{
	arc_config_t	*cf;
	arc_sid_t	*sid;
	register_t	prid;
	char		*sysid;
	int		sysid_len;
	int		i;

	/*
	 * Figure out where ARCBios can be addressed. On R8000, we can not
	 * use compatibility space, but on IP27/IP35, we can not blindly
	 * use XKPHYS due to subspacing, while compatibility space works.
	 * Fortunately we can get the processor ID to tell these apart, even
	 * though 32-bit coprocessor 0 instructions are not supposed to be
	 * supported on the R8000 (they probably misbehave somehow if the
	 * register has bits sets in the upper 32 bits, which is not the
	 * case of the R8000 PrId register).
	 */
	__asm__ volatile ("mfc0 %0, $15" /* COP_0_PRID */ : "=r" (prid));
	if ((prid & 0xff00) == (MIPS_R8000 << 8))
		bios_base = (void *)PHYS_TO_XKPHYS(ARCBIOS_BASE, CCA_CACHED);
	else
		bios_base = (void *)PHYS_TO_CKSEG0(ARCBIOS_BASE);

	/*
	 * Figure out if this is an ARCBios machine and if it is, see if we're
	 * dealing with a 32 or 64 bit version.
	 */
	if ((ArcBiosBase32->magic == ARC_PARAM_BLK_MAGIC) ||
	    (ArcBiosBase32->magic == ARC_PARAM_BLK_MAGIC_BUG)) {
		bios_is_32bit = 1;
		bios_printf("ARCS32 Firmware Version %d.%d\n",
		    ArcBiosBase32->version, ArcBiosBase32->revision);
	} else if ((ArcBiosBase64->magic == ARC_PARAM_BLK_MAGIC) ||
	    (ArcBiosBase64->magic == ARC_PARAM_BLK_MAGIC_BUG)) {
		bios_is_32bit = 0;
		bios_printf("ARCS64 Firmware\n");
	} else {
		return -1; /* Today is not your lucky day */
	}

	sid = (arc_sid_t *)Bios_GetSystemId();

	cf = (arc_config_t *)Bios_GetChild(NULL);
	if (cf != NULL) {
		if (bios_is_32bit) {
			sysid = (char *)(long)cf->id;
			sysid_len = cf->id_len;
		} else {
			sysid = (char *)((arc_config64_t *)cf)->id;
			sysid_len = ((arc_config64_t *)cf)->id_len;
		}

		if (sysid_len > 0 && sysid != NULL) {
			sysid_len--;
			for (i = 0; i < KNOWNSYSTEMS; i++) {
				if (strlen(sys_types[i].sys_name) !=sysid_len)
					continue;
				if (strncmp(sys_types[i].sys_name, sysid,
				    sysid_len) != 0)
					continue;
				if (sys_types[i].sys_vend &&
				    strncmp(sys_types[i].sys_vend, sid->vendor,
				      8) != 0)
					continue;
				return (sys_types[i].sys_type);	/* Found it. */
			}
		}
	} else {
#ifdef TGT_ORIGIN
		if (IP27_KLD_KLCONFIG(0)->magic == IP27_KLDIR_MAGIC) {
			/*
			 * If we find a kldir assume IP27 for now.
			 * We'll decide whether this is IP27 or IP35 later.
			 */
			return SGI_IP27;
		}
#endif
	}

	bios_printf("UNRECOGNIZED SYSTEM '%s' VENDOR '%8.8s' PRODUCT '%8.8s'\n",
	    cf == NULL ? "??" : sysid, sid->vendor, sid->prodid);
	bios_printf("Halting system!\n");
	Bios_Halt();
	bios_printf("Halting failed, use manual reset!\n");
	while(1);
}

/*
 * Incomplete version of bios_ident
 */
void
bios_ident()
{
	sys_config.system_type = bios_get_system_type();

	if (bios_is_32bit) {
#define bios_fnptr(slot) \
    (int64_t)*(int32_t *)((int64_t)ArcBiosBase32->firmware_vect + (slot) * 4)
		bios_halt = (void (*)(void))bios_fnptr(3);
		bios_powerdown = (void (*)(void))bios_fnptr(4);
		bios_restart = (void (*)(void))bios_fnptr(5);
		bios_reboot = (void (*)(void))bios_fnptr(6);
		bios_eim = (void (*)(void))bios_fnptr(7);
#undef bios_fnptr
	} else {
#define bios_fnptr(slot) \
    *(int64_t *)(ArcBiosBase64->firmware_vect + (slot) * 8)
		bios_halt = (void (*)(void))bios_fnptr(3);
		bios_powerdown = (void (*)(void))bios_fnptr(4);
		bios_restart = (void (*)(void))bios_fnptr(5);
		bios_reboot = (void (*)(void))bios_fnptr(6);
		bios_eim = (void (*)(void))bios_fnptr(7);
#undef bios_fnptr
	}

	bios_configure_memory();
#ifdef __arc__
	displayinfo = *(arc_dsp_stat_t *)Bios_GetDisplayStatus(1);
#endif
}

#if 0
/*
 * Return geometry of the display. Used by pccons.c to set up the
 * display configuration.
 */
void
bios_display_info(xpos, ypos, xsize, ysize)
    int	*xpos;
    int	*ypos;
    int *xsize;
    int *ysize;
{
#ifdef __arc__
	*xpos = displayinfo.CursorXPosition;
	*ypos = displayinfo.CursorYPosition;
	*xsize = displayinfo.CursorMaxXPosition;
	*ysize = displayinfo.CursorMaxYPosition;
#endif
}
#endif
