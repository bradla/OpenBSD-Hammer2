/*	$OpenBSD: wscons_machdep.c,v 1.11 2012/04/18 17:28:24 miod Exp $ */

/*
 * Copyright (c) 2010 Miodrag Vallat.
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
 * Copyright (c) 2001 Aaron Campbell
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/extent.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#if defined(TGT_ORIGIN)
#include <machine/mnode.h>
#endif

#include <mips64/arcbios.h>
#include <mips64/archtype.h>

#include <dev/cons.h>
#include <dev/ic/i8042reg.h>
#include <dev/ic/pckbcvar.h>
#include <dev/usb/ukbdvar.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

#include <sgi/dev/gbereg.h>
#include <sgi/dev/impactvar.h>
#include <sgi/dev/iockbcvar.h>
#include <sgi/dev/mkbcreg.h>
#include <sgi/gio/giovar.h>
#include <sgi/hpc/hpcreg.h>
#include <sgi/hpc/hpcvar.h>
#include <sgi/hpc/iocreg.h>
#include <sgi/localbus/crimebus.h>
#include <sgi/localbus/macebus.h>
#include <sgi/localbus/macebusvar.h>
#include <sgi/xbow/odysseyvar.h>

#if defined(TGT_OCTANE)
#include <sgi/sgi/ip30.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#endif

#include "gbe.h"
#include "gio.h"
#include "iockbc.h"
#include "impact.h"
#include "mkbc.h"
#include "odyssey.h"
#include "pckbc.h"
#include "ukbd.h"
#include "zskbd.h"

cons_decl(ws);
extern bus_addr_t comconsaddr;

#if defined(TGT_OCTANE) || defined(TGT_ORIGIN)
struct	sgi_device_location	console_output;
struct	sgi_device_location	console_input;
int	(*output_widget_cninit)(void) = NULL;

int	widget_cnprobe(void);
void	widget_cnattach(void);
#endif

void
wscnprobe(struct consdev *cp)
{
	int maj;

	/* Locate the major number. */
	for (maj = 0; maj < nchrdev; maj++) {
		if (cdevsw[maj].d_open == wsdisplayopen)
			break;
	}

	if (maj == nchrdev) {
		/* We are not in cdevsw[], give up. */
		panic("wsdisplay is not in cdevsw[]");
	}

	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_DEAD;

        switch (sys_config.system_type) {
#if defined(TGT_INDIGO) || defined(TGT_INDY) || defined(TGT_INDIGO2)
	case SGI_IP20:
	case SGI_IP22:
	case SGI_IP26:
	case SGI_IP28:
#if NGIO > 0
		if (giofb_cnprobe() == 0) {
			if (strncmp(bios_console, "video", 5) == 0)
				cp->cn_pri = CN_FORCED;
			else
				cp->cn_pri = CN_MIDPRI;
		}
#endif
		break;
#endif	/* TGT_INDIGO || TGT_INDY || TGT_INDIGO2 */

#if defined(TGT_O2)
	case SGI_O2:
#if NGBE > 0
		if (gbe_cnprobe(&crimebus_tag, GBE_BASE) != 0) {
			if (strncmp(bios_console, "video", 5) == 0)
				cp->cn_pri = CN_FORCED;
			else
				cp->cn_pri = CN_MIDPRI;
		}
#endif
		break;
#endif	/* TGT_O2 */

#if defined(TGT_ORIGIN)
	case SGI_IP27:
	case SGI_IP35:
		if (widget_cnprobe() != 0) {
			if (strncmp(bios_console,
			    "/dev/graphics/textport", 22) == 0)
				cp->cn_pri = CN_FORCED;
			else
				cp->cn_pri = CN_MIDPRI;
		}
		break;
#endif	/* TGT_ORIGIN */

#if defined(TGT_OCTANE)
	case SGI_OCTANE:
		if (widget_cnprobe() != 0) {
			if (strncmp(bios_console, "video", 5) == 0)
				cp->cn_pri = CN_FORCED;
			else
				cp->cn_pri = CN_MIDPRI;
		}
		break;
#endif	/* TGT_OCTANE */

	default:
		break;
	}
}

void
wscninit(struct consdev *cp)
{
static int initted;

	if (initted)
		return;

	initted = 1;

        switch (sys_config.system_type) {
#if defined(TGT_INDIGO) || defined(TGT_INDY) || defined(TGT_INDIGO2)
	case SGI_IP20:
	case SGI_IP22:
	case SGI_IP26:
	case SGI_IP28:
#if NGIO > 0
		if (giofb_cnattach() != 0)
			return;
#endif
		if (sys_config.system_type == SGI_IP20) {
#if NZSKBD > 0
			extern void zskbd_cnattach(int, int);
			zskbd_cnattach(0, 0);
#endif
		} else {
#if NPCKBC > 0
			if (pckbc_cnattach(&hpc3bus_tag,
			    HPC_BASE_ADDRESS_0 + IOC_BASE + IOC_KB_REGS + 3,
			    KBCMDP - KBDATAP, 0))
				return;
#endif
		}
		break;
#endif	/* TGT_INDIGO || TGT_INDY || TGT_INDIGO2 */

#if defined(TGT_O2)
	case SGI_O2:
#if NGBE > 0
		if (gbe_cnattach(&crimebus_tag, GBE_BASE) != 0)
			return;
#endif

#if NMKBC > 0
		if (mkbc_cnattach(&macebus_tag, MACE_IO_KBC_OFFS) == 0)
			return;	/* console keyboard found */
#endif
#if NUKBD > 0
		/* fallback keyboard console attachment if the others failed */
		ukbd_cnattach();
#endif
		break;
#endif	/* TGT_O2 */

#if defined(TGT_OCTANE) || defined(TGT_ORIGIN)
	case SGI_IP27:
	case SGI_IP35:
	case SGI_OCTANE:
		widget_cnattach();
		break;
#endif	/* TGT_OCTANE || TGT_ORIGIN */

	default:
		break;
	}
}

void
wscnputc(dev_t dev, int i)
{
	wsdisplay_cnputc(dev, i);
}

int
wscngetc(dev_t dev)
{
	int c;

	wskbd_cnpollc(dev, 1);
	c = wskbd_cngetc(dev);
	wskbd_cnpollc(dev, 0);

	return c;
}

void
wscnpollc(dev_t dev, int on)
{
	wskbd_cnpollc(dev, on);
}

/*
 * Try to figure out if we have a glass console widget, and if we have
 * a driver for it.
 */

#if defined(TGT_OCTANE) || defined(TGT_ORIGIN)
int
widget_cnprobe()
{
#ifdef TGT_ORIGIN
	console_t *cons;
#endif

        switch (sys_config.system_type) {
#ifdef TGT_ORIGIN
	case SGI_IP27:
	case SGI_IP35:
		/*
		 * Our first pass over the KL configuration data has figured
		 * out which component is the glass console, if any.
		 */
		if (kl_glass_console == NULL)
			return 0;
		kl_get_location(kl_glass_console, &console_output);

		cons = kl_get_console();
		kl_get_console_location(cons, &console_input);
		break;
#endif
#ifdef TGT_OCTANE
	case SGI_OCTANE:
		console_output.nasid = masternasid;
		console_output.widget = ip30_find_video();
		if (console_output.widget == 0)
			return 0;

		console_input.nasid = masternasid;
		console_input.widget = IP30_BRIDGE_WIDGET;
		console_input.bus = 0;
		console_input.device = IP30_IOC_SLOTNO;
		console_input.fn = -1;
		console_input.specific =
		    PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3);
		break;
#endif
	default:
		return 0;
	}

	/*
	 * Try supported frame buffers in no particular order.
	 */

#if NIMPACT_XBOW > 0
	if (impact_xbow_cnprobe() != 0) {
		output_widget_cninit = impact_xbow_cnattach;
		goto success;
	}
#endif
#if NODYSSEY > 0
	if (odyssey_cnprobe() != 0) {
		output_widget_cninit = odyssey_cnattach;
		goto success;
	}
#endif

	return 0;

success:
	/*
	 * At this point, we are commited to setup a glass console,
	 * so prevent serial console from winning over.
	 */
	comconsaddr = 0;

	return 1;
}

void
widget_cnattach()
{
	/* should not happen */
	if (output_widget_cninit == NULL)
		return;

	if ((*output_widget_cninit)() != 0)
		return;

#if NIOCKBC > 0
	if (iockbc_cnattach() == 0)
		return;	/* console keyboard found */
#endif
#if NUKBD > 0
	/* fallback keyboard console attachment if the others failed */
	ukbd_cnattach();
#endif
}
#endif	/* TGT_ORIGIN || TGT_OCTANE */
