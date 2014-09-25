/* $OpenBSD: xhci.c,v 1.9 2014/04/07 15:34:27 mpi Exp $ */

/*
 * Copyright (c) 2014 Martin Pieuchot
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/timeout.h>

#include <machine/bus.h>
#include <machine/endian.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/xhcireg.h>
#include <dev/usb/xhcivar.h>

struct cfdriver xhci_cd = {
	NULL, "xhci", DV_DULL
};

#ifdef XHCI_DEBUG
#define DPRINTF(x)	do { if (xhcidebug) printf x; } while(0)
#define DPRINTFN(n,x)	do { if (xhcidebug>(n)) printf x; } while (0)
int xhcidebug = 3;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define DEVNAME(sc)		((sc)->sc_bus.bdev.dv_xname)

#define TRBOFF(ring, trb)	((void *)(trb) - (void *)((ring).trbs))
#define TRBADDR(ring, trb)	DMAADDR(&(ring).dma, TRBOFF(ring, trb))

struct xhci_pipe {
	struct usbd_pipe	pipe;

	uint8_t			dci;
	uint8_t			slot;	/* Device slot ID */
	struct xhci_ring	ring;

	/*
	 * XXX used to pass the xfer pointer back to the
	 * interrupt routine, better way?
	 */
	struct usbd_xfer	*pending_xfers[XHCI_MAX_TRANSFERS];
	int			 halted;
	size_t			 free_trbs;
};

int	xhci_reset(struct xhci_softc *);
void	xhci_config(struct xhci_softc *);
int	xhci_intr1(struct xhci_softc *);
void	xhci_waitintr(struct xhci_softc *, struct usbd_xfer *);
void	xhci_event_dequeue(struct xhci_softc *);
void	xhci_event_xfer(struct xhci_softc *, uint64_t, uint32_t, uint32_t);
void	xhci_event_command(struct xhci_softc *, uint64_t);
void	xhci_event_port_change(struct xhci_softc *, uint64_t, uint32_t);
int	xhci_pipe_init(struct xhci_softc *, struct usbd_pipe *, uint32_t);
int	xhci_scratchpad_alloc(struct xhci_softc *, int);
void	xhci_scratchpad_free(struct xhci_softc *);
int	xhci_softdev_alloc(struct xhci_softc *, uint8_t);
void	xhci_softdev_free(struct xhci_softc *, uint8_t);
int	xhci_ring_alloc(struct xhci_softc *, struct xhci_ring *, size_t);
void	xhci_ring_free(struct xhci_softc *, struct xhci_ring *);
void	xhci_ring_reset(struct xhci_softc *, struct xhci_ring *);
struct	xhci_trb *xhci_ring_dequeue(struct xhci_softc *, struct xhci_ring *,
	    int);

struct	xhci_trb *xhci_xfer_get_trb(struct xhci_softc *, struct usbd_xfer*,
	    uint8_t *, int);
void	xhci_xfer_done(struct usbd_xfer *xfer);
/* xHCI command helpers. */
int	xhci_command_submit(struct xhci_softc *, struct xhci_trb *, int);
int	xhci_command_abort(struct xhci_softc *);

void	xhci_cmd_reset_endpoint_async(struct xhci_softc *, uint8_t, uint8_t);
void	xhci_cmd_set_tr_deq_async(struct xhci_softc *, uint8_t, uint8_t, uint64_t);
int	xhci_cmd_configure_ep(struct xhci_softc *, uint8_t, uint64_t);
int	xhci_cmd_stop_ep(struct xhci_softc *, uint8_t, uint8_t);
int	xhci_cmd_slot_control(struct xhci_softc *, uint8_t *, int);
int	xhci_cmd_address_device(struct xhci_softc *,uint8_t,  uint64_t, int);
int	xhci_cmd_evaluate_ctx(struct xhci_softc *, uint8_t, uint64_t);
#ifdef XHCI_DEBUG
int	xhci_cmd_noop(struct xhci_softc *);
#endif

/* XXX should be part of the Bus interface. */
void	xhci_abort_xfer(struct usbd_xfer *, usbd_status);
void	xhci_pipe_close(struct usbd_pipe *);
void	xhci_noop(struct usbd_xfer *);

/* XXX these are common to all HC drivers and should be merged. */
void 	xhci_timeout(void *);
void 	xhci_timeout_task(void *);

/* USBD Bus Interface. */
usbd_status	  xhci_pipe_open(struct usbd_pipe *);
void		  xhci_softintr(void *);
void		  xhci_poll(struct usbd_bus *);
struct usbd_xfer *xhci_allocx(struct usbd_bus *);
void		  xhci_freex(struct usbd_bus *, struct usbd_xfer *);

usbd_status	  xhci_root_ctrl_transfer(struct usbd_xfer *);
usbd_status	  xhci_root_ctrl_start(struct usbd_xfer *);

usbd_status	  xhci_root_intr_transfer(struct usbd_xfer *);
usbd_status	  xhci_root_intr_start(struct usbd_xfer *);
void		  xhci_root_intr_abort(struct usbd_xfer *);
void		  xhci_root_intr_done(struct usbd_xfer *);

usbd_status	  xhci_device_ctrl_transfer(struct usbd_xfer *);
usbd_status	  xhci_device_ctrl_start(struct usbd_xfer *);
void		  xhci_device_ctrl_abort(struct usbd_xfer *);

usbd_status	  xhci_device_generic_transfer(struct usbd_xfer *);
usbd_status	  xhci_device_generic_start(struct usbd_xfer *);
void		  xhci_device_generic_abort(struct usbd_xfer *);
void		  xhci_device_generic_done(struct usbd_xfer *);

#define XHCI_INTR_ENDPT 1

struct usbd_bus_methods xhci_bus_methods = {
	.open_pipe = xhci_pipe_open,
	.soft_intr = xhci_softintr,
	.do_poll = xhci_poll,
	.allocx = xhci_allocx,
	.freex = xhci_freex,
};

struct usbd_pipe_methods xhci_root_ctrl_methods = {
	.transfer = xhci_root_ctrl_transfer,
	.start = xhci_root_ctrl_start,
	.abort = xhci_noop,
	.close = xhci_pipe_close,
	.done = xhci_noop,
};

struct usbd_pipe_methods xhci_root_intr_methods = {
	.transfer = xhci_root_intr_transfer,
	.start = xhci_root_intr_start,
	.abort = xhci_root_intr_abort,
	.close = xhci_pipe_close,
	.done = xhci_root_intr_done,
};

struct usbd_pipe_methods xhci_device_ctrl_methods = {
	.transfer = xhci_device_ctrl_transfer,
	.start = xhci_device_ctrl_start,
	.abort = xhci_device_ctrl_abort,
	.close = xhci_pipe_close,
	.done = xhci_noop,
};

#if notyet
struct usbd_pipe_methods xhci_device_isoc_methods = {
};
#endif

struct usbd_pipe_methods xhci_device_bulk_methods = {
	.transfer = xhci_device_generic_transfer,
	.start = xhci_device_generic_start,
	.abort = xhci_device_generic_abort,
	.close = xhci_pipe_close,
	.done = xhci_device_generic_done,
};

struct usbd_pipe_methods xhci_device_generic_methods = {
	.transfer = xhci_device_generic_transfer,
	.start = xhci_device_generic_start,
	.abort = xhci_device_generic_abort,
	.close = xhci_pipe_close,
	.done = xhci_device_generic_done,
};

#ifdef XHCI_DEBUG
static void
xhci_dump_trb(struct xhci_trb *trb)
{
	printf("trb=%p (0x%016llx 0x%08x 0x%08x)\n", trb,
	   (long long)trb->trb_paddr, trb->trb_status, trb->trb_flags);
}
#endif

int
xhci_init(struct xhci_softc *sc)
{
	uint32_t hcr;
	int npage, error;

#ifdef XHCI_DEBUG
	uint16_t vers;

	vers = XREAD2(sc, XHCI_HCIVERSION);
	printf("%s: xHCI version %x.%x\n", DEVNAME(sc), vers >> 8, vers & 0xff);
#endif
	sc->sc_bus.usbrev = USBREV_3_0;
	sc->sc_bus.methods = &xhci_bus_methods;
	sc->sc_bus.pipe_size = sizeof(struct xhci_pipe);

	sc->sc_oper_off = XREAD1(sc, XHCI_CAPLENGTH);
	sc->sc_door_off = XREAD4(sc, XHCI_DBOFF);
	sc->sc_runt_off = XREAD4(sc, XHCI_RTSOFF);

#ifdef XHCI_DEBUG
        printf("%s: CAPLENGTH=0x%x\n", DEVNAME(sc), sc->sc_oper_off);
	printf("%s: DOORBELL=0x%x\n", DEVNAME(sc), sc->sc_door_off);
	printf("%s: RUNTIME=0x%x\n", DEVNAME(sc), sc->sc_runt_off);
#endif

	error = xhci_reset(sc);
	if (error)
		return (error);

	hcr = XREAD4(sc, XHCI_HCCPARAMS);
	sc->sc_ctxsize = XHCI_HCC_CSZ(hcr) ? 64 : 32;
	DPRINTF(("%s: %d bytes context\n", DEVNAME(sc), sc->sc_ctxsize));

#ifdef XHCI_DEBUG
	hcr = XOREAD4(sc, XHCI_PAGESIZE);
	printf("%s: supported page size 0x%08x\n", DEVNAME(sc), hcr);
#endif
	/* Use 4K for the moment since it's easier. */
	sc->sc_pagesize = 4096;

	/* Get port and device slot numbers. */
	hcr = XREAD4(sc, XHCI_HCSPARAMS1);
	sc->sc_noport = XHCI_HCS1_N_PORTS(hcr);
	sc->sc_noslot = XHCI_HCS1_DEVSLOT_MAX(hcr);
	DPRINTF(("%s: %d ports and %d slots\n", DEVNAME(sc), sc->sc_noport,
	    sc->sc_noslot));

	/*
	 * Section 6.1 - Device Context Base Address Array
	 * shall be aligned to a 64 byte boundary.
	 */
	sc->sc_dcbaa.size = (sc->sc_noslot + 1) * sizeof(uint64_t);
	error = usb_allocmem(&sc->sc_bus, sc->sc_dcbaa.size, 64,
	    &sc->sc_dcbaa.dma);
	if (error)
		return (ENOMEM);
	sc->sc_dcbaa.segs = KERNADDR(&sc->sc_dcbaa.dma, 0);
	memset(sc->sc_dcbaa.segs, 0, sc->sc_dcbaa.size);
	usb_syncmem(&sc->sc_dcbaa.dma, 0, sc->sc_dcbaa.size,
	    BUS_DMASYNC_PREWRITE);

	/* Setup command ring. */
	error = xhci_ring_alloc(sc, &sc->sc_cmd_ring, XHCI_MAX_COMMANDS);
	if (error) {
		printf("%s: could not allocate command ring.\n", DEVNAME(sc));
		usb_freemem(&sc->sc_bus, &sc->sc_dcbaa.dma);
		return (error);
	}

	/* Setup one event ring and its segment table (ERST). */
	error = xhci_ring_alloc(sc, &sc->sc_evt_ring, XHCI_MAX_EVENTS);
	if (error) {
		printf("%s: could not allocate event ring.\n", DEVNAME(sc));
		xhci_ring_free(sc, &sc->sc_cmd_ring);
		usb_freemem(&sc->sc_bus, &sc->sc_dcbaa.dma);
		return (error);
	}

	/* Allocate the required entry for the segment table. */
	sc->sc_erst.size = 1 * sizeof(struct xhci_erseg);
	error = usb_allocmem(&sc->sc_bus, sc->sc_erst.size, 64,
	    &sc->sc_erst.dma);
	if (error) {
		printf("%s: could not allocate segment table.\n", DEVNAME(sc));
		xhci_ring_free(sc, &sc->sc_evt_ring);
		xhci_ring_free(sc, &sc->sc_cmd_ring);
		usb_freemem(&sc->sc_bus, &sc->sc_dcbaa.dma);
		return (ENOMEM);
	}
	sc->sc_erst.segs = KERNADDR(&sc->sc_erst.dma, 0);

	/* Set our ring address and size in its corresponding segment. */
	sc->sc_erst.segs[0].er_addr = htole64(DMAADDR(&sc->sc_evt_ring.dma, 0));
	sc->sc_erst.segs[0].er_size = htole32(XHCI_MAX_EVENTS);
	sc->sc_erst.segs[0].er_rsvd = 0;
	usb_syncmem(&sc->sc_erst.dma, 0, sc->sc_erst.size,
	   BUS_DMASYNC_PREWRITE);

	/* Get the number of scratch pages and configure them if necessary. */
	hcr = XREAD4(sc, XHCI_HCSPARAMS2);
	npage = XHCI_HCS2_SPB_MAX(hcr);
	DPRINTF(("%s: %d scratch pages\n", DEVNAME(sc), npage));

	if (npage > 0 && xhci_scratchpad_alloc(sc, npage)) {
		printf("%s: could not allocate scratchpad.\n", DEVNAME(sc));
		usb_freemem(&sc->sc_bus, &sc->sc_erst.dma);
		xhci_ring_free(sc, &sc->sc_evt_ring);
		xhci_ring_free(sc, &sc->sc_cmd_ring);
		usb_freemem(&sc->sc_bus, &sc->sc_dcbaa.dma);
		return (ENOMEM);
	}


	xhci_config(sc);

	return (0);
}

void
xhci_config(struct xhci_softc *sc)
{
	uint64_t paddr;
	uint32_t hcr;

	/* Make sure to program a number of device slots we can handle. */
	if (sc->sc_noslot > USB_MAX_DEVICES)
		sc->sc_noslot = USB_MAX_DEVICES;
	hcr = XOREAD4(sc, XHCI_CONFIG) & ~XHCI_CONFIG_SLOTS_MASK;
	XOWRITE4(sc, XHCI_CONFIG, hcr | sc->sc_noslot);

	/* Set the device context base array address. */
	paddr = (uint64_t)DMAADDR(&sc->sc_dcbaa.dma, 0);
	XOWRITE4(sc, XHCI_DCBAAP_LO, (uint32_t)paddr);
	XOWRITE4(sc, XHCI_DCBAAP_HI, (uint32_t)(paddr >> 32));

	DPRINTF(("%s: DCBAAP=%08lx%08lx\n", DEVNAME(sc),
	    XOREAD4(sc, XHCI_DCBAAP_HI), XOREAD4(sc, XHCI_DCBAAP_LO)));

	/* Set the command ring address. */
	paddr = (uint64_t)DMAADDR(&sc->sc_cmd_ring.dma, 0);
	XOWRITE4(sc, XHCI_CRCR_LO, ((uint32_t)paddr) | XHCI_CRCR_LO_RCS);
	XOWRITE4(sc, XHCI_CRCR_HI, (uint32_t)(paddr >> 32));

	DPRINTF(("%s: CRCR=%08lx%08lx (%016llx)\n", DEVNAME(sc),
	    XOREAD4(sc, XHCI_CRCR_HI), XOREAD4(sc, XHCI_CRCR_LO), paddr));

	/* Set the ERST count number to 1, since we use only one event ring. */
	XRWRITE4(sc, XHCI_ERSTSZ(0), XHCI_ERSTS_SET(1));

	/* Set the segment table address. */
	paddr = (uint64_t)DMAADDR(&sc->sc_erst.dma, 0);
	XRWRITE4(sc, XHCI_ERSTBA_LO(0), (uint32_t)paddr);
	XRWRITE4(sc, XHCI_ERSTBA_HI(0), (uint32_t)(paddr >> 32));

	DPRINTF(("%s: ERSTBA=%08lx%08lx\n", DEVNAME(sc),
	    XRREAD4(sc, XHCI_ERSTBA_HI(0)), XRREAD4(sc, XHCI_ERSTBA_LO(0))));

	/* Set the ring dequeue address. */
	paddr = (uint64_t)DMAADDR(&sc->sc_evt_ring.dma, 0);
	XRWRITE4(sc, XHCI_ERDP_LO(0), (uint32_t)paddr);
	XRWRITE4(sc, XHCI_ERDP_HI(0), (uint32_t)(paddr >> 32));

	DPRINTF(("%s: ERDP=%08lx%08lx\n", DEVNAME(sc),
	    XRREAD4(sc, XHCI_ERDP_HI(0)), XRREAD4(sc, XHCI_ERDP_LO(0))));

	/* Enable interrupts. */
	hcr = XRREAD4(sc, XHCI_IMAN(0));
	XRWRITE4(sc, XHCI_IMAN(0), hcr | XHCI_IMAN_INTR_ENA);

	/* Set default interrupt moderation. */
	XRWRITE4(sc, XHCI_IMOD(0), XHCI_IMOD_DEFAULT);

	/* Allow event interrupt and start the controller. */
	XOWRITE4(sc, XHCI_USBCMD, XHCI_CMD_INTE|XHCI_CMD_RS);

	DPRINTF(("%s: USBCMD=%08lx\n", DEVNAME(sc), XOREAD4(sc, XHCI_USBCMD)));
	DPRINTF(("%s: IMAN=%08lx\n", DEVNAME(sc), XRREAD4(sc, XHCI_IMAN(0))));
}

int
xhci_detach(struct device *self, int flags)
{
	struct xhci_softc *sc = (struct xhci_softc *)self;
	int rv;

	rv = config_detach_children(self, flags);
	if (rv != 0) {
		printf("%s: error while detaching %d\n", DEVNAME(sc), rv);
		return (rv);
	}

	/* Since the hardware might already be gone, ignore the errors. */
	xhci_command_abort(sc);

	xhci_reset(sc);

	/* Disable interrupts. */
	XRWRITE4(sc, XHCI_IMOD(0), 0);
	XRWRITE4(sc, XHCI_IMAN(0), 0);

	/* Clear the event ring address. */
	XRWRITE4(sc, XHCI_ERDP_LO(0), 0);
	XRWRITE4(sc, XHCI_ERDP_HI(0), 0);

	XRWRITE4(sc, XHCI_ERSTBA_LO(0), 0);
	XRWRITE4(sc, XHCI_ERSTBA_HI(0), 0);

	XRWRITE4(sc, XHCI_ERSTSZ(0), 0);

	/* Clear the command ring address. */
	XOWRITE4(sc, XHCI_CRCR_LO, 0);
	XOWRITE4(sc, XHCI_CRCR_HI, 0);

	XOWRITE4(sc, XHCI_DCBAAP_LO, 0);
	XOWRITE4(sc, XHCI_DCBAAP_HI, 0);

	if (sc->sc_spad.npage > 0)
		xhci_scratchpad_free(sc);

	usb_freemem(&sc->sc_bus, &sc->sc_erst.dma);
	xhci_ring_free(sc, &sc->sc_evt_ring);
	xhci_ring_free(sc, &sc->sc_cmd_ring);
	usb_freemem(&sc->sc_bus, &sc->sc_dcbaa.dma);

	return (0);
}

int
xhci_activate(struct device *self, int act)
{
	struct xhci_softc *sc = (struct xhci_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_RESUME:
		sc->sc_bus.use_polling++;

		xhci_reset(sc);
		xhci_ring_reset(sc, &sc->sc_cmd_ring);
		xhci_ring_reset(sc, &sc->sc_evt_ring);
		xhci_config(sc);

		sc->sc_bus.use_polling--;
		rv = config_activate_children(self, act);
		break;
	case DVACT_POWERDOWN:
		rv = config_activate_children(self, act);
		xhci_reset(sc);
		break;
	default:
		rv = config_activate_children(self, act);
		break;
	}

	return (rv);
}

int
xhci_reset(struct xhci_softc *sc)
{
	uint32_t hcr;
	int i;

	XOWRITE4(sc, XHCI_USBCMD, 0);	/* Halt controller */
	for (i = 0; i < 100; i++) {
		usb_delay_ms(&sc->sc_bus, 1);
		hcr = XOREAD4(sc, XHCI_USBSTS) & XHCI_STS_HCH;
		if (hcr)
			break;
	}

	if (!hcr)
		printf("%s: halt timeout\n", DEVNAME(sc));

	XOWRITE4(sc, XHCI_USBCMD, XHCI_CMD_HCRST);
	for (i = 0; i < 100; i++) {
		usb_delay_ms(&sc->sc_bus, 1);
		hcr = XOREAD4(sc, XHCI_USBCMD) & XHCI_STS_CNR;
		if (!hcr)
			break;
	}

	if (hcr) {
		printf("%s: reset timeout\n", DEVNAME(sc));
		return (EIO);
	}

	return (0);
}


int
xhci_intr(void *v)
{
	struct xhci_softc *sc = v;

	if (sc == NULL || sc->sc_bus.dying)
		return (0);

	/* If we get an interrupt while polling, then just ignore it. */
	if (sc->sc_bus.use_polling) {
		DPRINTFN(16, ("xhci_intr: ignored interrupt while polling\n"));
		return (0);
	}

	return (xhci_intr1(sc));
}

int
xhci_intr1(struct xhci_softc *sc)
{
	uint32_t intrs;

	intrs = XOREAD4(sc, XHCI_USBSTS);
	if (intrs == 0xffffffff) {
		sc->sc_bus.dying = 1;
		return (0);
	}

	if ((intrs & XHCI_STS_EINT) == 0)
		return (0);

	sc->sc_bus.intr_context++;
	sc->sc_bus.no_intrs++;

	if (intrs & XHCI_STS_HSE) {
		printf("%s: host system error\n", DEVNAME(sc));
		sc->sc_bus.dying = 1;
		sc->sc_bus.intr_context--;
		return (1);
	}

	XOWRITE4(sc, XHCI_USBSTS, intrs); /* Acknowledge */
	if (intrs & XHCI_STS_EINT)
		usb_schedsoftintr(&sc->sc_bus);

	/* Acknowledge PCI interrupt */
	intrs = XRREAD4(sc, XHCI_IMAN(0));
	XRWRITE4(sc, XHCI_IMAN(0), intrs | XHCI_IMAN_INTR_PEND);

	sc->sc_bus.intr_context--;

	return (1);
}

void
xhci_poll(struct usbd_bus *bus)
{
	struct xhci_softc *sc = (struct xhci_softc *)bus;

	if (XOREAD4(sc, XHCI_USBSTS))
		xhci_intr1(sc);
}

void
xhci_waitintr(struct xhci_softc *sc, struct usbd_xfer *xfer)
{
	DPRINTF(("%s: stub\n", __func__));
}

void
xhci_softintr(void *v)
{
	struct xhci_softc *sc = v;

	if (sc->sc_bus.dying)
		return;

	sc->sc_bus.intr_context++;
	xhci_event_dequeue(sc);
	sc->sc_bus.intr_context--;
}

void
xhci_event_dequeue(struct xhci_softc *sc)
{
	struct xhci_trb *trb;
	uint64_t paddr;
	uint32_t status, flags;

	while ((trb = xhci_ring_dequeue(sc, &sc->sc_evt_ring, 1)) != NULL) {
		paddr = letoh64(trb->trb_paddr);
		status = letoh32(trb->trb_status);
		flags = letoh32(trb->trb_flags);

		switch (flags & XHCI_TRB_TYPE_MASK) {
		case XHCI_EVT_XFER:
			xhci_event_xfer(sc, paddr, status, flags);
			break;
		case XHCI_EVT_CMD_COMPLETE:
			memcpy(&sc->sc_result_trb, trb, sizeof(*trb));
			xhci_event_command(sc, paddr);
			break;
		case XHCI_EVT_PORT_CHANGE:
			xhci_event_port_change(sc, paddr, status);
			break;
		default:
#ifdef XHCI_DEBUG
			printf("event (%d): ", XHCI_TRB_TYPE(flags));
			xhci_dump_trb(trb);
#endif
			break;
		}

	}

	paddr = (uint64_t)DMAADDR(&sc->sc_evt_ring.dma,
	    sizeof(struct xhci_trb) * sc->sc_evt_ring.index);
	XRWRITE4(sc, XHCI_ERDP_LO(0), ((uint32_t)paddr) | XHCI_ERDP_LO_BUSY);
	XRWRITE4(sc, XHCI_ERDP_HI(0), (uint32_t)(paddr >> 32));
}

void
xhci_event_xfer(struct xhci_softc *sc, uint64_t paddr, uint32_t status,
    uint32_t flags)
{
	struct xhci_pipe *xp;
	struct usbd_xfer *xfer;
	uint8_t dci, slot, code, remain;
	int trb_idx;

	slot = XHCI_TRB_GET_SLOT(flags);
	dci = XHCI_TRB_GET_EP(flags);
	if (slot > sc->sc_noslot)
		return; /* XXX */

	xp = sc->sc_sdevs[slot].pipes[dci - 1];

	code = XHCI_TRB_GET_CODE(status);
	remain = XHCI_TRB_REMAIN(status);

	trb_idx = (paddr - DMAADDR(&xp->ring.dma, 0)) / sizeof(struct xhci_trb);
	if (trb_idx < 0 || trb_idx >= xp->ring.ntrb) {
		printf("%s: wrong trb index (%d) max is %d\n", DEVNAME(sc),
		    trb_idx, xp->ring.ntrb - 1);
		return;
	}

	xfer = xp->pending_xfers[trb_idx];
	if (xfer == NULL) {
#if 1
		DPRINTF(("%s: dev %d dci=%d paddr=0x%016llx idx=%d remain=%u"
		    " code=%u\n", DEVNAME(sc), slot, dci, (long long)paddr,
		    trb_idx, remain, code));
#endif
		printf("%s: NULL xfer pointer\n", DEVNAME(sc));
		return;
	}

	switch (code) {
	case XHCI_CODE_SUCCESS:
	case XHCI_CODE_SHORT_XFER:
		xfer->actlen = xfer->length - remain;
		xfer->status = USBD_NORMAL_COMPLETION;
		break;
#if 0
	case XHCI_CODE_STALL:
		xfer->status = USBD_STALLED;
		xp->halted = 1;
		break;
#endif
	case XHCI_CODE_BABBLE:
		/*
		 * Since the stack might try to start a new transfer as
		 * soon as a pending one finishes, make sure the endpoint
		 * is fully reset before calling usb_transfer_complete().
		 */
		xp->halted = 1;
		xhci_cmd_reset_endpoint_async(sc, slot, dci);
		return;
	default:
#if 1
		DPRINTF(("%s: dev %d dci=%d paddr=0x%016llx idx=%d remain=%u"
		    " code=%u\n", DEVNAME(sc), slot, dci, (long long)paddr,
		    trb_idx, remain, code));
#endif
		DPRINTF(("%s: unhandled code %d\n", DEVNAME(sc), code));
		xfer->status = USBD_IOERROR;
		xp->halted = 1;
		break;
	}

	xhci_xfer_done(xfer);
	usb_transfer_complete(xfer);
}

void
xhci_event_command(struct xhci_softc *sc, uint64_t paddr)
{
	struct usbd_xfer *xfer;
	struct xhci_pipe *xp;
	uint32_t flags;
	uint8_t dci, slot;
	int i;

	KASSERT(paddr == TRBADDR(sc->sc_cmd_ring, sc->sc_cmd_trb));

	flags = letoh32(sc->sc_cmd_trb->trb_flags);

	slot = XHCI_TRB_GET_SLOT(flags);
	dci = XHCI_TRB_GET_EP(flags);
	xp = sc->sc_sdevs[slot].pipes[dci - 1];

	sc->sc_cmd_trb = NULL;

	switch (flags & XHCI_TRB_TYPE_MASK) {
	case XHCI_CMD_RESET_EP:
		/*
		 * Clear the TRBs and reconfigure the dequeue pointer
		 * before declaring the endpoint ready.
		 */
		xhci_ring_reset(sc, &xp->ring);
		xp->free_trbs = xp->ring.ntrb;
		xhci_cmd_set_tr_deq_async(sc, xp->slot, xp->dci,
		    DMAADDR(&xp->ring.dma, 0) | XHCI_EPCTX_DCS);
		break;
	case XHCI_CMD_SET_TR_DEQ:
		/*
		 * Now that the endpoint is in its initial state, we
		 * can finish all its pending transfers and let the
		 * stack play with it again.
		 */
		xp->halted = 0;
		for (i = 0; i < XHCI_MAX_TRANSFERS; i++) {
			xfer = xp->pending_xfers[i];
			if (xfer != NULL && xfer->done == 0) {
				xfer->status = USBD_IOERROR;
				usb_transfer_complete(xfer);
			}
			xp->pending_xfers[i] = NULL;
		}
		break;
	default:
		/* All other commands are synchronous. */
		wakeup(&sc->sc_cmd_trb);
		break;
	}
}

void
xhci_event_port_change(struct xhci_softc *sc, uint64_t paddr, uint32_t status)
{
	struct usbd_xfer *xfer = sc->sc_intrxfer;
	uint32_t port = XHCI_TRB_PORTID(paddr);
	uint8_t *p;

	if (XHCI_TRB_GET_CODE(status) != XHCI_CODE_SUCCESS) {
		DPRINTF(("failed port status event\n"));/* XXX can it happen? */
		return;
	}

	if (xfer == NULL)
		return;

	p = KERNADDR(&xfer->dmabuf, 0);
	memset(p, 0, xfer->length);

	p[port/8] |= 1 << (port%8);
	DPRINTF(("%s: port=%d change=0x%02x\n", DEVNAME(sc), port, *p));

	xfer->actlen = xfer->length;
	xfer->status = USBD_NORMAL_COMPLETION;

	usb_transfer_complete(xfer);
}

void
xhci_xfer_done(struct usbd_xfer *xfer)
{
	struct xhci_pipe *xp = (struct xhci_pipe *)xfer->pipe;
	struct xhci_xfer *xx = (struct xhci_xfer *)xfer;
	int ntrb, i;

#ifdef XHCI_DEBUG
	if (xp->pending_xfers[xx->index] == NULL) {
		printf("%s: xfer=%p already done (index=%d)\n", __func__,
		    xfer, xx->index);
		return;
	}
#endif

	for (ntrb = 0, i = xx->index; ntrb < xx->ntrb; ntrb++, i--) {
		xp->pending_xfers[i] = NULL;
		if (i == 0)
			i = (xp->ring.ntrb - 1);
	}
	xp->free_trbs += xx->ntrb;
	xx->index = -1;
	xx->ntrb = 0;
}

static inline uint8_t
xhci_ed2dci(usb_endpoint_descriptor_t *ed)
{
	uint8_t dir;

	if (UE_GET_XFERTYPE(ed->bmAttributes) == UE_CONTROL)
		return (UE_GET_ADDR(ed->bEndpointAddress) * 2 + 1);

	if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN)
		dir = 1;
	else
		dir = 0;

	return (UE_GET_ADDR(ed->bEndpointAddress) * 2 + dir);
}

usbd_status
xhci_pipe_open(struct usbd_pipe *pipe)
{
	struct xhci_softc *sc = (struct xhci_softc *)pipe->device->bus;
	struct xhci_pipe *xp = (struct xhci_pipe *)pipe;
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	uint8_t slot = 0, xfertype = UE_GET_XFERTYPE(ed->bmAttributes);
	struct usbd_device *hub;
	uint32_t rhport = 0;
	int error;

	KASSERT(xp->slot == 0);

#ifdef XHCI_DEBUG
	struct usbd_device *dev = pipe->device;
	printf("%s: pipe=%p addr=%d depth=%d port=%d speed=%d\n", __func__,
	    pipe, dev->address, dev->depth, dev->powersrc->portno, dev->speed);
#endif

	if (sc->sc_bus.dying)
		return (USBD_IOERROR);

	/* Root Hub */
	if (pipe->device->depth == 0) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			pipe->methods = &xhci_root_ctrl_methods;
			break;
		case UE_DIR_IN | XHCI_INTR_ENDPT:
			pipe->methods = &xhci_root_intr_methods;
			break;
		default:
			pipe->methods = NULL;
			DPRINTF(("%s: bad bEndpointAddress 0x%02x\n", __func__,
			    ed->bEndpointAddress));
			return (USBD_INVAL);
		}
		return (USBD_NORMAL_COMPLETION);
	}

#if 0
	/* Issue a noop to check if the command ring is correctly configured. */
	xhci_cmd_noop(sc);
#endif

	switch (xfertype) {
	case UE_CONTROL:
		pipe->methods = &xhci_device_ctrl_methods;

		/* Get a slot and init the device's contexts. */
		error = xhci_cmd_slot_control(sc, &slot, 1);
		if (error || slot == 0 || slot > sc->sc_noslot)
			return (USBD_INVAL);

		if (xhci_softdev_alloc(sc, slot))
			return (USBD_NOMEM);

		/* Get root hub port */
		for (hub = pipe->device; hub->myhub->depth; hub = hub->myhub)
			;
		rhport = hub->powersrc->portno;
		break;
	case UE_ISOCHRONOUS:
#if notyet
		pipe->methods = &xhci_device_isoc_methods;
		break;
#else
		DPRINTF(("%s: isochronous xfer not supported \n", __func__));
		return (USBD_INVAL);
#endif
	case UE_BULK:
		pipe->methods = &xhci_device_bulk_methods;
		break;
	case UE_INTERRUPT:
		pipe->methods = &xhci_device_generic_methods;
		break;
	default:
		DPRINTF(("%s: bad xfer type %d\n", __func__, xfertype));
		return (USBD_INVAL);
	}

	/* XXX Section nb? */
	xp->dci = xhci_ed2dci(ed);

	if (slot != 0)
		xp->slot = slot;
	else
		xp->slot = ((struct xhci_pipe *)pipe->device->default_pipe)->slot;

	if (xhci_pipe_init(sc, pipe, rhport))
		return (USBD_IOERROR);

	return (USBD_NORMAL_COMPLETION);
}

static inline uint32_t
xhci_endpoint_txinfo(struct xhci_softc *sc, usb_endpoint_descriptor_t *ed)
{
	switch (ed->bmAttributes & UE_XFERTYPE) {
	case UE_CONTROL:
		return (XHCI_EPCTX_AVG_TRB_LEN(8));
	case UE_BULK:
		return (0);
	case UE_INTERRUPT:
	case UE_ISOCHRONOUS:
	default:
		break;
	}

	DPRINTF(("%s: partial stub\n", __func__));

	return (XHCI_EPCTX_MAX_ESIT_PAYLOAD(0) | XHCI_EPCTX_AVG_TRB_LEN(0));
}

int
xhci_pipe_init(struct xhci_softc *sc, struct usbd_pipe *pipe, uint32_t port)
{
	struct xhci_pipe *xp = (struct xhci_pipe *)pipe;
	struct xhci_soft_dev *sdev = &sc->sc_sdevs[xp->slot];
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	uint8_t xfertype = UE_GET_XFERTYPE(ed->bmAttributes);
	uint8_t ival, speed, cerr = 0;
	uint32_t mps;
	int error;

	DPRINTF(("%s: dev %d dci %u (epAddr=0x%x)\n", DEVNAME(sc), xp->slot,
	    xp->dci, pipe->endpoint->edesc->bEndpointAddress));

	if (xhci_ring_alloc(sc, &xp->ring, XHCI_MAX_TRANSFERS))
		return (ENOMEM);

	xp->free_trbs = xp->ring.ntrb;
	xp->halted = 0;

	sdev->pipes[xp->dci - 1] = xp;

	switch (pipe->device->speed) {
	case USB_SPEED_LOW:
		ival= 3;
		speed = XHCI_SPEED_LOW;
		mps = USB_MAX_IPACKET;
		break;
	case USB_SPEED_FULL:
		ival = 3;
		speed = XHCI_SPEED_FULL;
		mps = 64;
		break;
	case USB_SPEED_HIGH:
		ival = min(3, ed->bInterval);
		speed = XHCI_SPEED_HIGH;
		mps = 64;
		break;
	case USB_SPEED_SUPER:
		ival = min(3, ed->bInterval);
		speed = XHCI_SPEED_SUPER;
		mps = 512;
		break;
	default:
		return (EINVAL);
	}

	/* XXX Until we fix wMaxPacketSize for ctrl ep depending on the speed */
	mps = max(mps, UGETW(ed->wMaxPacketSize));

	if (pipe->interval != USBD_DEFAULT_INTERVAL)
		ival = min(ival, pipe->interval);

	DPRINTF(("%s: speed %d mps %d rhport %d\n", DEVNAME(sc), speed, mps,
	    port));

	/* Setup the endpoint context */
	if (xfertype != UE_ISOCHRONOUS)
		cerr = 3;

	if (xfertype == UE_CONTROL || xfertype == UE_BULK)
		ival = 0;

	if ((ed->bEndpointAddress & UE_DIR_IN) || (xfertype == UE_CONTROL))
		xfertype |= 0x4;

	sdev->ep_ctx[xp->dci-1]->info_lo = htole32(XHCI_EPCTX_SET_IVAL(ival));
	sdev->ep_ctx[xp->dci-1]->info_hi = htole32(
	    XHCI_EPCTX_SET_MPS(mps) | XHCI_EPCTX_SET_EPTYPE(xfertype) |
	    XHCI_EPCTX_SET_CERR(cerr) | XHCI_EPCTX_SET_MAXB(0)
	);
	sdev->ep_ctx[xp->dci-1]->txinfo = htole32(xhci_endpoint_txinfo(sc, ed));
	sdev->ep_ctx[xp->dci-1]->deqp = htole64(
	    DMAADDR(&xp->ring.dma, 0) | XHCI_EPCTX_DCS
	);

	/* Unmask the new endoint */
	sdev->input_ctx->drop_flags = 0;
	sdev->input_ctx->add_flags = htole32(XHCI_INCTX_MASK_DCI(xp->dci));

	/* Setup the slot context */
	sdev->slot_ctx->info_lo = htole32(XHCI_SCTX_SET_DCI(xp->dci));
	sdev->slot_ctx->info_hi = 0;
	sdev->slot_ctx->tt = 0;
	sdev->slot_ctx->state = 0;

	if (UE_GET_XFERTYPE(ed->bmAttributes) == UE_CONTROL) {
		sdev->slot_ctx->info_lo |= htole32(XHCI_SCTX_SET_SPEED(speed));
		sdev->slot_ctx->info_hi |= htole32(XHCI_SCTX_SET_RHPORT(port));
	}

	usb_syncmem(&sdev->ictx_dma, 0, sc->sc_pagesize, BUS_DMASYNC_PREWRITE);

	if (xp->dci == 1) {
		error = xhci_cmd_address_device(sc, xp->slot,
		    DMAADDR(&sdev->ictx_dma, 0), 1 /* XXX see below */);
		if (error)
			return (error);
		/*
		 * XXX Set the address.  This is ugly and is not
		 * adapted to our stack.  But the whole idea of
		 * reopening the default pipes should be revisited
		 * anyway...
		 */
#if 1
		struct usbd_device *dev = pipe->device;
		struct xhci_sctx *slot;
		uint8_t addr;

		usb_syncmem(&sdev->octx_dma, 0, sc->sc_pagesize,
		    BUS_DMASYNC_POSTREAD);

		/* Get output slot context. */
		slot = KERNADDR(&sdev->octx_dma, 0);
		addr = XHCI_SCTX_DEV_ADDR(letoh32(slot->state));
		if (addr == 0)
			return (EINVAL);

		DPRINTF(("%s: dev %d new addr %d (old %d)\n", DEVNAME(sc),
		    xp->slot, addr, dev->address));

		dev->bus->devices[dev->address] = 0;
		dev->bus->devices[addr] = dev;
		dev->address = addr;
#endif
	} else {
		error = xhci_cmd_configure_ep(sc, xp->slot,
		    DMAADDR(&sdev->ictx_dma, 0));
		if (error) {
			xhci_ring_free(sc, &xp->ring);
			return (EIO);
		}
	}

	usb_syncmem(&sdev->octx_dma, 0, sc->sc_pagesize, BUS_DMASYNC_POSTREAD);

	return (0);
}

void
xhci_pipe_close(struct usbd_pipe *pipe)
{
	struct xhci_softc *sc = (struct xhci_softc *)pipe->device->bus;
	struct xhci_pipe *lxp, *xp = (struct xhci_pipe *)pipe;
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	struct xhci_soft_dev *sdev = &sc->sc_sdevs[xp->slot];
	int i;

	/* Root Hub */
	if (pipe->device->depth == 0)
		return;

	if (!xp->halted || xhci_cmd_stop_ep(sc, xp->slot, xp->dci))
		DPRINTF(("%s: error stopping ep (%d)\n", DEVNAME(sc), xp->dci));

	/* Mask the endpoint */
	sdev->input_ctx->drop_flags = htole32(XHCI_INCTX_MASK_DCI(xp->dci));
	sdev->input_ctx->add_flags = 0;

	/* Update last valid Endpoint Context */
	for (i = 30; i >= 0; i--) {
		lxp = sdev->pipes[i];
		if (lxp != NULL && lxp != xp)
			break;
	}
	sdev->slot_ctx->info_lo = htole32(XHCI_SCTX_SET_DCI(lxp->dci));

	/* Clear the Endpoint Context */
	memset(&sdev->ep_ctx[xp->dci - 1], 0, sizeof(struct xhci_epctx));

	usb_syncmem(&sdev->ictx_dma, 0, sc->sc_pagesize, BUS_DMASYNC_PREWRITE);

	if (xhci_cmd_configure_ep(sc, xp->slot, DMAADDR(&sdev->ictx_dma, 0)))
		DPRINTF(("%s: error clearing ep (%d)\n", DEVNAME(sc), xp->dci));

	xhci_ring_free(sc, &xp->ring);
	sdev->pipes[xp->dci - 1] = NULL;

	if (UE_GET_XFERTYPE(ed->bmAttributes) == UE_CONTROL) {
		xhci_cmd_slot_control(sc, &xp->slot, 0);
		xhci_softdev_free(sc, xp->slot);
	}
}

struct usbd_xfer *
xhci_allocx(struct usbd_bus *bus)
{
	struct xhci_softc *sc = (struct xhci_softc *)bus;
	struct usbd_xfer *xfer;

	xfer = SIMPLEQ_FIRST(&sc->sc_free_xfers);
	if (xfer != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_free_xfers, next);
#ifdef DIAGNOSTIC
		if (xfer->busy_free != XFER_FREE)
			printf("%s: xfer=%p not free, 0x%08x\n", __func__,
			    xfer, xfer->busy_free);
#endif
	} else
		xfer = malloc(sizeof(struct xhci_xfer), M_USB, M_NOWAIT);

	if (xfer != NULL) {
		memset(xfer, 0, sizeof(struct xhci_xfer));
#ifdef DIAGNOSTIC
		xfer->busy_free = XFER_BUSY;
#endif
	}
	return (xfer);
}

void
xhci_freex(struct usbd_bus *bus, struct usbd_xfer *xfer)
{
	struct xhci_softc *sc = (struct xhci_softc *)bus;

#ifdef DIAGNOSTIC
	if (xfer->busy_free != XFER_BUSY) {
		printf("xhci_freex: xfer=%p not busy, 0x%08x\n", xfer,
		    xfer->busy_free);
		return;
	}
	xfer->busy_free = XFER_FREE;
#endif

	SIMPLEQ_INSERT_HEAD(&sc->sc_free_xfers, xfer, next);
}

int
xhci_scratchpad_alloc(struct xhci_softc *sc, int npage)
{
	uint64_t *pte;
	int error, i;

	/* Allocate the required entry for the table. */
	error = usb_allocmem(&sc->sc_bus, npage * sizeof(uint64_t), 64,
	    &sc->sc_spad.table_dma);
	if (error)
		return (ENOMEM);
	pte = KERNADDR(&sc->sc_spad.table_dma, 0);

	/* Alloccate space for the pages. */
	error = usb_allocmem(&sc->sc_bus, npage * sc->sc_pagesize,
	    sc->sc_pagesize, &sc->sc_spad.pages_dma);
	if (error) {
		usb_freemem(&sc->sc_bus, &sc->sc_spad.table_dma);
		return (ENOMEM);
	}
	memset(KERNADDR(&sc->sc_spad.pages_dma, 0), 0, npage * sc->sc_pagesize);
	usb_syncmem(&sc->sc_spad.pages_dma, 0, npage * sc->sc_pagesize,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	for (i = 0; i < npage; i++) {
		pte[i] = htole64(
		    DMAADDR(&sc->sc_spad.pages_dma, i * sc->sc_pagesize)
		);
	}
	usb_syncmem(&sc->sc_spad.table_dma, 0, npage * sizeof(uint64_t),
	    BUS_DMASYNC_PREWRITE);

	/*  Entry 0 points to the table of scratchpad pointers. */
	sc->sc_dcbaa.segs[0] = htole64(DMAADDR(&sc->sc_spad.table_dma, 0));
	usb_syncmem(&sc->sc_dcbaa.dma, 0, sizeof(uint64_t),
	    BUS_DMASYNC_PREWRITE);

	sc->sc_spad.npage = npage;

	return (0);
}

void
xhci_scratchpad_free(struct xhci_softc *sc)
{
	sc->sc_dcbaa.segs[0] = 0;
	usb_syncmem(&sc->sc_dcbaa.dma, 0, sizeof(uint64_t),
	    BUS_DMASYNC_PREWRITE);

	usb_freemem(&sc->sc_bus, &sc->sc_spad.pages_dma);
	usb_freemem(&sc->sc_bus, &sc->sc_spad.table_dma);
}


int
xhci_ring_alloc(struct xhci_softc *sc, struct xhci_ring *ring, size_t ntrb)
{
	size_t size;

	size = ntrb * sizeof(struct xhci_trb);

	if (usb_allocmem(&sc->sc_bus, size, 16, &ring->dma) != 0)
		return (ENOMEM);

	ring->trbs = KERNADDR(&ring->dma, 0);
	ring->ntrb = ntrb;

	xhci_ring_reset(sc, ring);

	return (0);
}

void
xhci_ring_free(struct xhci_softc *sc, struct xhci_ring *ring)
{
	usb_freemem(&sc->sc_bus, &ring->dma);
}

void
xhci_ring_reset(struct xhci_softc *sc, struct xhci_ring *ring)
{
	size_t size;

	size = ring->ntrb * sizeof(struct xhci_trb);

	memset(ring->trbs, 0, size);

	ring->index = 0;
	ring->toggle = XHCI_TRB_CYCLE;

	/*
	 * Since all our rings use only one segment, at least for
	 * the moment, link their tail to their head.
	 */
	if (ring != &sc->sc_evt_ring) {
		struct xhci_trb *trb = &ring->trbs[ring->ntrb - 1];

		trb->trb_paddr = htole64(DMAADDR(&ring->dma, 0));
		trb->trb_flags = htole32(XHCI_TRB_TYPE_LINK | XHCI_TRB_LINKSEG);
	}
	usb_syncmem(&ring->dma, 0, size, BUS_DMASYNC_PREWRITE);
}

struct xhci_trb*
xhci_ring_dequeue(struct xhci_softc *sc, struct xhci_ring *ring, int cons)
{
	struct xhci_trb *trb;
	uint32_t idx = ring->index;

	KASSERT(idx < ring->ntrb);

	usb_syncmem(&ring->dma, idx * sizeof(struct xhci_trb),
	    sizeof(struct xhci_trb), BUS_DMASYNC_POSTREAD);

	trb = &ring->trbs[idx];

	/* Make sure this TRB can be consumed. */
	if (cons && ring->toggle != (letoh32(trb->trb_flags) & XHCI_TRB_CYCLE))
		return (NULL);
	idx++;

	if (idx < (ring->ntrb - 1)) {
		ring->index = idx;
	} else {
		if (ring->toggle)
			ring->trbs[idx].trb_flags |= htole32(XHCI_TRB_CYCLE);
		else
			ring->trbs[idx].trb_flags &= ~htole32(XHCI_TRB_CYCLE);

		usb_syncmem(&ring->dma, sizeof(struct xhci_trb) * idx,
		    sizeof(struct xhci_trb), BUS_DMASYNC_PREWRITE);

		ring->index = 0;
		ring->toggle ^= 1;
	}

	return (trb);
}

struct xhci_trb *
xhci_xfer_get_trb(struct xhci_softc *sc, struct usbd_xfer* xfer,
    uint8_t *togglep, int last)
{
	struct xhci_pipe *xp = (struct xhci_pipe *)xfer->pipe;
	struct xhci_xfer *xx = (struct xhci_xfer *)xfer;

	KASSERT(xp->free_trbs >= 1);

	/* Associate this TRB to our xfer. */
	xp->pending_xfers[xp->ring.index] = xfer;
	xp->free_trbs--;

	xx->index = (last) ? xp->ring.index : -1;
	xx->ntrb += 1;

	*togglep = xp->ring.toggle;
	return (xhci_ring_dequeue(sc, &xp->ring, 0));
}

int
xhci_command_submit(struct xhci_softc *sc, struct xhci_trb *trb0, int timeout)
{
	struct xhci_trb *trb;
	int error = 0;

	KASSERT(sc->sc_cmd_trb == NULL);

	trb0->trb_flags |= htole32(sc->sc_cmd_ring.toggle);

	trb = xhci_ring_dequeue(sc, &sc->sc_cmd_ring, 0);
	memcpy(trb, trb0, sizeof(struct xhci_trb));
	usb_syncmem(&sc->sc_cmd_ring.dma, TRBOFF(sc->sc_cmd_ring, trb),
	    sizeof(struct xhci_trb), BUS_DMASYNC_PREWRITE);

	sc->sc_cmd_trb = trb;
	XDWRITE4(sc, XHCI_DOORBELL(0), 0);

	if (timeout == 0)
		return (0);

	assertwaitok();

	error = tsleep(&sc->sc_cmd_trb, PZERO, "xhcicmd",
	    (timeout*hz+999)/ 1000 + 1);
	if (error) {
#ifdef XHCI_DEBUG
		printf("%s: tsleep() = %d\n", __func__, error);
		printf("cmd = %d " ,XHCI_TRB_TYPE(letoh32(trb->trb_flags)));
		xhci_dump_trb(trb);
#endif
		sc->sc_cmd_trb = NULL;
		return (error);
	}

	memcpy(trb0, &sc->sc_result_trb, sizeof(struct xhci_trb));

	if (XHCI_TRB_GET_CODE(letoh32(trb0->trb_status)) != XHCI_CODE_SUCCESS) {
		printf("%s: event error code=%d\n", DEVNAME(sc),
		    XHCI_TRB_GET_CODE(letoh32(trb0->trb_status)));
		error = EIO;
	}

#ifdef XHCI_DEBUG
	if (error) {
		printf("result = %d ", XHCI_TRB_TYPE(letoh32(trb0->trb_flags)));
		xhci_dump_trb(trb0);
	}
#endif
	return (error);
}

int
xhci_command_abort(struct xhci_softc *sc)
{
	uint32_t reg;
	int i;

	reg = XOREAD4(sc, XHCI_CRCR_LO);
	if ((reg & XHCI_CRCR_LO_CRR) == 0)
		return (0);

	XOWRITE4(sc, XHCI_CRCR_LO, reg | XHCI_CRCR_LO_CA);
	XOWRITE4(sc, XHCI_CRCR_HI, 0);

	for (i = 0; i < 250; i++) {
		usb_delay_ms(&sc->sc_bus, 1);
		reg = XOREAD4(sc, XHCI_CRCR_LO) & XHCI_CRCR_LO_CRR;
		if (!reg)
			break;
	}

	if (reg) {
		printf("%s: command ring abort timeout\n", DEVNAME(sc));
		return (1);
	}

	return (0);
}

int
xhci_cmd_configure_ep(struct xhci_softc *sc, uint8_t slot, uint64_t addr)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s\n", DEVNAME(sc), __func__));

	trb.trb_paddr = htole64(addr);
	trb.trb_status = 0;
	trb.trb_flags = htole32(
	    XHCI_TRB_SET_SLOT(slot) | XHCI_CMD_CONFIG_EP
	);

	return (xhci_command_submit(sc, &trb, XHCI_COMMAND_TIMEOUT));
}

int
xhci_cmd_stop_ep(struct xhci_softc *sc, uint8_t slot, uint8_t dci)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s\n", DEVNAME(sc), __func__));

	trb.trb_paddr = 0;
	trb.trb_status = 0;
	trb.trb_flags = htole32(
	    XHCI_TRB_SET_SLOT(slot) | XHCI_TRB_SET_EP(dci) | XHCI_CMD_STOP_EP
	);

	return (xhci_command_submit(sc, &trb, XHCI_COMMAND_TIMEOUT));
}

void
xhci_cmd_reset_endpoint_async(struct xhci_softc *sc, uint8_t slot, uint8_t dci)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s\n", DEVNAME(sc), __func__));

	trb.trb_paddr = 0;
	trb.trb_status = 0;
	trb.trb_flags = htole32(
	    XHCI_TRB_SET_SLOT(slot) | XHCI_TRB_SET_EP(dci) | XHCI_CMD_RESET_EP
	);

	xhci_command_submit(sc, &trb, 0);
}

void
xhci_cmd_set_tr_deq_async(struct xhci_softc *sc, uint8_t slot, uint8_t dci,
   uint64_t addr)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s\n", DEVNAME(sc), __func__));

	trb.trb_paddr = htole64(addr);
	trb.trb_status = 0;
	trb.trb_flags = htole32(
	    XHCI_TRB_SET_SLOT(slot) | XHCI_TRB_SET_EP(dci) | XHCI_CMD_SET_TR_DEQ
	);

	xhci_command_submit(sc, &trb, 0);
}

int
xhci_cmd_slot_control(struct xhci_softc *sc, uint8_t *slotp, int enable)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s\n", DEVNAME(sc), __func__));

	trb.trb_paddr = 0;
	trb.trb_status = 0;
	if (enable)
		trb.trb_flags = htole32(XHCI_CMD_ENABLE_SLOT);
	else
		trb.trb_flags = htole32(
			XHCI_TRB_SET_SLOT(*slotp) | XHCI_CMD_DISABLE_SLOT
		);

	if (xhci_command_submit(sc, &trb, XHCI_COMMAND_TIMEOUT))
		return (EIO);

	if (enable)
		*slotp = XHCI_TRB_GET_SLOT(letoh32(trb.trb_flags));

	return (0);
}

int
xhci_cmd_address_device(struct xhci_softc *sc, uint8_t slot, uint64_t addr,
    int set_address)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s\n", DEVNAME(sc), __func__));

	trb.trb_paddr = htole64(addr);
	trb.trb_status = 0;
	trb.trb_flags = htole32(
	    XHCI_TRB_SET_SLOT(slot) | (set_address ? 0 : XHCI_TRB_BSR) |
	    XHCI_CMD_ADDRESS_DEVICE
	);

	return (xhci_command_submit(sc, &trb, XHCI_COMMAND_TIMEOUT));
}

int
xhci_cmd_evaluate_ctx(struct xhci_softc *sc, uint8_t slot, uint64_t addr)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s\n", DEVNAME(sc), __func__));

	trb.trb_paddr = htole64(addr);
	trb.trb_status = 0;
	trb.trb_flags = htole32(
	    XHCI_TRB_SET_SLOT(slot) | XHCI_CMD_EVAL_CTX
	);

	return (xhci_command_submit(sc, &trb, XHCI_COMMAND_TIMEOUT));
}

#ifdef XHCI_DEBUG
int
xhci_cmd_noop(struct xhci_softc *sc)
{
	struct xhci_trb trb;

	DPRINTF(("%s: %s\n", DEVNAME(sc), __func__));

	trb.trb_paddr = 0;
	trb.trb_status = 0;
	trb.trb_flags = htole32(XHCI_CMD_NOOP);

	return (xhci_command_submit(sc, &trb, XHCI_COMMAND_TIMEOUT));
}
#endif

int
xhci_softdev_alloc(struct xhci_softc *sc, uint8_t slot)
{
	struct xhci_soft_dev *sdev = &sc->sc_sdevs[slot];
	int i, error;

	/*
	 * Setup input context.  Even with 64 byte context size, it
	 * fits into the smallest supported page size, so use that.
	 */
	error = usb_allocmem(&sc->sc_bus, sc->sc_pagesize, sc->sc_pagesize,
	    &sdev->ictx_dma);
	if (error)
		return (ENOMEM);
	memset(KERNADDR(&sdev->ictx_dma, 0), 0, sc->sc_pagesize);

	sdev->input_ctx = KERNADDR(&sdev->ictx_dma, 0);
	sdev->slot_ctx = KERNADDR(&sdev->ictx_dma, sc->sc_ctxsize);
	for (i = 0; i < 31; i++)
		sdev->ep_ctx[i] =
		   KERNADDR(&sdev->ictx_dma, (i + 2) * sc->sc_ctxsize);

	DPRINTF(("%s: dev %d, input=%p slot=%p ep0=%p\n", DEVNAME(sc),
	 slot, sdev->input_ctx, sdev->slot_ctx, sdev->ep_ctx[0]));

	/* Setup output context */
	error = usb_allocmem(&sc->sc_bus, sc->sc_pagesize, sc->sc_pagesize,
	    &sdev->octx_dma);
	if (error) {
		usb_freemem(&sc->sc_bus, &sdev->ictx_dma);
		return (ENOMEM);
	}
	memset(KERNADDR(&sdev->octx_dma, 0), 0, sc->sc_pagesize);

	memset(&sdev->pipes, 0, sizeof(sdev->pipes));

	DPRINTF(("%s: dev %d, setting DCBAA to 0x%016llx\n", DEVNAME(sc),
	    slot, (long long)DMAADDR(&sdev->octx_dma, 0)));

	sc->sc_dcbaa.segs[slot] = htole64(DMAADDR(&sdev->octx_dma, 0));
	usb_syncmem(&sc->sc_dcbaa.dma, slot * sizeof(uint64_t),
	    sizeof(uint64_t), BUS_DMASYNC_PREWRITE);

	return (0);
}

void
xhci_softdev_free(struct xhci_softc *sc, uint8_t slot)
{
	struct xhci_soft_dev *sdev = &sc->sc_sdevs[slot];

	sc->sc_dcbaa.segs[slot] = 0;
	usb_syncmem(&sc->sc_dcbaa.dma, slot * sizeof(uint64_t),
	    sizeof(uint64_t), BUS_DMASYNC_PREWRITE);

	usb_freemem(&sc->sc_bus, &sdev->octx_dma);
	usb_freemem(&sc->sc_bus, &sdev->ictx_dma);

	memset(sdev, 0, sizeof(struct xhci_soft_dev));
}

/* Root hub descriptors. */
usb_device_descriptor_t xhci_devd = {
	USB_DEVICE_DESCRIPTOR_SIZE,
	UDESC_DEVICE,		/* type */
	{0x00, 0x03},		/* USB version */
	UDCLASS_HUB,		/* class */
	UDSUBCLASS_HUB,		/* subclass */
	UDPROTO_HSHUBSTT,	/* protocol */
	9,			/* max packet */
	{0},{0},{0x00,0x01},	/* device id */
	1,2,0,			/* string indexes */
	1			/* # of configurations */
};

const usb_config_descriptor_t xhci_confd = {
	USB_CONFIG_DESCRIPTOR_SIZE,
	UDESC_CONFIG,
	{USB_CONFIG_DESCRIPTOR_SIZE +
	 USB_INTERFACE_DESCRIPTOR_SIZE +
	 USB_ENDPOINT_DESCRIPTOR_SIZE},
	1,
	1,
	0,
	UC_SELF_POWERED,
	0                      /* max power */
};

const usb_interface_descriptor_t xhci_ifcd = {
	USB_INTERFACE_DESCRIPTOR_SIZE,
	UDESC_INTERFACE,
	0,
	0,
	1,
	UICLASS_HUB,
	UISUBCLASS_HUB,
	UIPROTO_HSHUBSTT,	/* XXX */
	0
};

const usb_endpoint_descriptor_t xhci_endpd = {
	USB_ENDPOINT_DESCRIPTOR_SIZE,
	UDESC_ENDPOINT,
	UE_DIR_IN | XHCI_INTR_ENDPT,
	UE_INTERRUPT,
	{2, 0},                 /* max 15 ports */
	255
};

const usb_endpoint_ss_comp_descriptor_t xhci_endpcd = {
	USB_ENDPOINT_SS_COMP_DESCRIPTOR_SIZE,
	UDESC_ENDPOINT_SS_COMP,
	0,
	0,
	{0, 0}			/* XXX */
};

const usb_hub_descriptor_t xhci_hubd = {
	USB_HUB_DESCRIPTOR_SIZE,
	UDESC_SS_HUB,
	0,
	{0,0},
	0,
	0,
	{0},
};

void
xhci_abort_xfer(struct usbd_xfer *xfer, usbd_status status)
{
	int s;

	DPRINTF(("%s: partial stub\n", __func__));

	xhci_xfer_done(xfer);

	xfer->status = status;
	timeout_del(&xfer->timeout_handle);
	usb_rem_task(xfer->device, &xfer->abort_task);

	s = splusb();
	usb_transfer_complete(xfer);
	splx(s);
}

void
xhci_timeout(void *addr)
{
	struct usbd_xfer *xfer = addr;
	struct xhci_softc *sc = (struct xhci_softc *)xfer->device->bus;

	if (sc->sc_bus.dying) {
		xhci_timeout_task(addr);
		return;
	}

	usb_init_task(&xfer->abort_task, xhci_timeout_task, addr,
	    USB_TASK_TYPE_ABORT);
	usb_add_task(xfer->device, &xfer->abort_task);
}

void
xhci_timeout_task(void *addr)
{
	struct usbd_xfer *xfer = addr;

	DPRINTF(("%s: xfer=%p\n", __func__, xfer));

	xhci_abort_xfer(xfer, USBD_TIMEOUT);
}

usbd_status
xhci_root_ctrl_transfer(struct usbd_xfer *xfer)
{
	usbd_status err;

	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	return (xhci_root_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

usbd_status
xhci_root_ctrl_start(struct usbd_xfer *xfer)
{
	struct xhci_softc *sc = (struct xhci_softc *)xfer->device->bus;
	usb_port_status_t ps;
	usb_device_request_t *req;
	void *buf = NULL;
	usb_hub_descriptor_t hubd;
	usbd_status err;
	int s, len, value, index;
	int l, totlen = 0;
	int port, i;
	uint32_t v;

	KASSERT(xfer->rqflags & URQ_REQUEST);

	if (sc->sc_bus.dying)
		return (USBD_IOERROR);

	req = &xfer->request;

	DPRINTFN(4,("%s: type=0x%02x request=%02x\n", __func__,
	    req->bmRequestType, req->bRequest));

	len = UGETW(req->wLength);
	value = UGETW(req->wValue);
	index = UGETW(req->wIndex);

	if (len != 0)
		buf = KERNADDR(&xfer->dmabuf, 0);

#define C(x,y) ((x) | ((y) << 8))
	switch(C(req->bRequest, req->bmRequestType)) {
	case C(UR_CLEAR_FEATURE, UT_WRITE_DEVICE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_ENDPOINT):
		/*
		 * DEVICE_REMOTE_WAKEUP and ENDPOINT_HALT are no-ops
		 * for the integrated root hub.
		 */
		break;
	case C(UR_GET_CONFIG, UT_READ_DEVICE):
		if (len > 0) {
			*(uint8_t *)buf = sc->sc_conf;
			totlen = 1;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_DEVICE):
		DPRINTFN(8,("xhci_root_ctrl_start: wValue=0x%04x\n", value));
		switch(value >> 8) {
		case UDESC_DEVICE:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			USETW(xhci_devd.idVendor, sc->sc_id_vendor);
			memcpy(buf, &xhci_devd, l);
			break;
		/*
		 * We can't really operate at another speed, but the spec says
		 * we need this descriptor.
		 */
		case UDESC_OTHER_SPEED_CONFIGURATION:
		case UDESC_CONFIG:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_CONFIG_DESCRIPTOR_SIZE);
			memcpy(buf, &xhci_confd, l);
			((usb_config_descriptor_t *)buf)->bDescriptorType =
			    value >> 8;
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_INTERFACE_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &xhci_ifcd, l);
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_ENDPOINT_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &xhci_endpd, l);
			break;
		case UDESC_STRING:
			if (len == 0)
				break;
			*(u_int8_t *)buf = 0;
			totlen = 1;
			switch (value & 0xff) {
			case 0: /* Language table */
				totlen = usbd_str(buf, len, "\001");
				break;
			case 1: /* Vendor */
				totlen = usbd_str(buf, len, sc->sc_vendor);
				break;
			case 2: /* Product */
				totlen = usbd_str(buf, len, "xHCI root hub");
				break;
			}
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_GET_INTERFACE, UT_READ_INTERFACE):
		if (len > 0) {
			*(uint8_t *)buf = 0;
			totlen = 1;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_DEVICE):
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus,UDS_SELF_POWERED);
			totlen = 2;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_INTERFACE):
	case C(UR_GET_STATUS, UT_READ_ENDPOINT):
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus, 0);
			totlen = 2;
		}
		break;
	case C(UR_SET_ADDRESS, UT_WRITE_DEVICE):
		if (value >= USB_MAX_DEVICES) {
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_SET_CONFIG, UT_WRITE_DEVICE):
		if (value != 0 && value != 1) {
			err = USBD_IOERROR;
			goto ret;
		}
		sc->sc_conf = value;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_DEVICE):
	case C(UR_SET_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_SET_FEATURE, UT_WRITE_ENDPOINT):
		err = USBD_IOERROR;
		goto ret;
	case C(UR_SET_INTERFACE, UT_WRITE_INTERFACE):
		break;
	case C(UR_SYNCH_FRAME, UT_WRITE_ENDPOINT):
		break;
	/* Hub requests */
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_OTHER):
		DPRINTFN(8, ("xhci_root_ctrl_start: UR_CLEAR_PORT_FEATURE "
		    "port=%d feature=%d\n", index, value));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		port = XHCI_PORTSC(index);
		v = XOREAD4(sc, port) & ~XHCI_PS_CLEAR;
		switch (value) {
		case UHF_PORT_ENABLE:
			XOWRITE4(sc, port, v | XHCI_PS_PED);
			break;
		case UHF_PORT_SUSPEND:
			/* TODO */
			break;
		case UHF_PORT_POWER:
			XOWRITE4(sc, port, v & ~XHCI_PS_PP);
			break;
		case UHF_PORT_INDICATOR:
			XOWRITE4(sc, port, v & ~XHCI_PS_SET_PIC(3));
			break;
		case UHF_C_PORT_CONNECTION:
			XOWRITE4(sc, port, v | XHCI_PS_CSC);
			break;
		case UHF_C_PORT_ENABLE:
			XOWRITE4(sc, port, v | XHCI_PS_PEC);
			break;
		case UHF_C_PORT_SUSPEND:
			XOWRITE4(sc, port, v | XHCI_PS_PLC);
			break;
		case UHF_C_PORT_OVER_CURRENT:
			XOWRITE4(sc, port, v | XHCI_PS_OCC);
			break;
		case UHF_C_PORT_RESET:
			XOWRITE4(sc, port, v | XHCI_PS_PRC);
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;

	case C(UR_GET_DESCRIPTOR, UT_READ_CLASS_DEVICE):
		if (len == 0)
			break;
		if ((value & 0xff) != 0) {
			err = USBD_IOERROR;
			goto ret;
		}
		v = XREAD4(sc, XHCI_HCCPARAMS);
		hubd = xhci_hubd;
		hubd.bNbrPorts = sc->sc_noport;
		USETW(hubd.wHubCharacteristics,
		    (XHCI_HCC_PPC(v) ? UHD_PWR_INDIVIDUAL : UHD_PWR_GANGED) |
		    (XHCI_HCC_PIND(v) ? UHD_PORT_IND : 0));
		hubd.bPwrOn2PwrGood = 10; /* xHCI section 5.4.9 */
		for (i = 1; i <= sc->sc_noport; i++) {
			v = XOREAD4(sc, XHCI_PORTSC(i));
			if (v & XHCI_PS_DR)
				hubd.DeviceRemovable[i / 8] |= 1U << (i % 8);
		}
		hubd.bDescLength = USB_HUB_DESCRIPTOR_SIZE + i;
		l = min(len, hubd.bDescLength);
		totlen = l;
		memcpy(buf, &hubd, l);
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_DEVICE):
		if (len != 16) {
			err = USBD_IOERROR;
			goto ret;
		}
		memset(buf, 0, len);
		totlen = len;
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_OTHER):
		DPRINTFN(8,("xhci_root_ctrl_start: get port status i=%d\n",
		    index));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		if (len != 4) {
			err = USBD_IOERROR;
			goto ret;
		}
		v = XOREAD4(sc, XHCI_PORTSC(index));
		DPRINTFN(8,("xhci_root_ctrl_start: port status=0x%04x\n", v));
		switch (XHCI_PS_SPEED(v)) {
		case XHCI_SPEED_FULL:
			i = UPS_FULL_SPEED;
			break;
		case XHCI_SPEED_LOW:
			i = UPS_LOW_SPEED;
			break;
		case XHCI_SPEED_HIGH:
			i = UPS_HIGH_SPEED;
			break;
		case XHCI_SPEED_SUPER:
		default:
			i = UPS_SUPER_SPEED;
			break;
		}
		if (v & XHCI_PS_CCS)	i |= UPS_CURRENT_CONNECT_STATUS;
		if (v & XHCI_PS_PED)	i |= UPS_PORT_ENABLED;
		if (v & XHCI_PS_OCA)	i |= UPS_OVERCURRENT_INDICATOR;
		if (v & XHCI_PS_PR)	i |= UPS_RESET;
		if (v & XHCI_PS_PP)	i |= UPS_PORT_POWER;
		USETW(ps.wPortStatus, i);
		i = 0;
		if (v & XHCI_PS_CSC)    i |= UPS_C_CONNECT_STATUS;
		if (v & XHCI_PS_PEC)    i |= UPS_C_PORT_ENABLED;
		if (v & XHCI_PS_OCC)    i |= UPS_C_OVERCURRENT_INDICATOR;
		if (v & XHCI_PS_PRC)	i |= UPS_C_PORT_RESET;
		USETW(ps.wPortChange, i);
		l = min(len, sizeof ps);
		memcpy(buf, &ps, l);
		totlen = l;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_CLASS_DEVICE):
		err = USBD_IOERROR;
		goto ret;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_OTHER):

		i = index >> 8;
		index &= 0x00ff;

		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		port = XHCI_PORTSC(index);
		v = XOREAD4(sc, port) & ~XHCI_PS_CLEAR;

		switch (value) {
		case UHF_PORT_ENABLE:
			XOWRITE4(sc, port, v | XHCI_PS_PED);
			break;
		case UHF_PORT_SUSPEND:
			DPRINTFN(6, ("suspend port %u (LPM=%u)\n", index, i));
			if (XHCI_PS_SPEED(v) == XHCI_SPEED_SUPER) {
				err = USBD_IOERROR;
				goto ret;
			}
			XOWRITE4(sc, port, v |
			    XHCI_PS_SET_PLS(i ? 2 /* LPM */ : 3) | XHCI_PS_LWS);
			break;
		case UHF_PORT_RESET:
			DPRINTFN(6, ("reset port %d\n", index));
			XOWRITE4(sc, port, v | XHCI_PS_PR);
			break;
		case UHF_PORT_POWER:
			DPRINTFN(3, ("set port power %d\n", index));
			XOWRITE4(sc, port, v | XHCI_PS_PP);
			break;
		case UHF_PORT_INDICATOR:
			DPRINTFN(3, ("set port indicator %d\n", index));

			v &= ~XHCI_PS_SET_PIC(3);
			v |= XHCI_PS_SET_PIC(1);

			XOWRITE4(sc, port, v);
			break;
		case UHF_C_PORT_RESET:
			XOWRITE4(sc, port, v | XHCI_PS_PRC);
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_CLEAR_TT_BUFFER, UT_WRITE_CLASS_OTHER):
	case C(UR_RESET_TT, UT_WRITE_CLASS_OTHER):
	case C(UR_GET_TT_STATE, UT_READ_CLASS_OTHER):
	case C(UR_STOP_TT, UT_WRITE_CLASS_OTHER):
		break;
	default:
		err = USBD_IOERROR;
		goto ret;
	}
	xfer->actlen = totlen;
	err = USBD_NORMAL_COMPLETION;
ret:
	xfer->status = err;
	s = splusb();
	usb_transfer_complete(xfer);
	splx(s);
	return (USBD_IN_PROGRESS);
}


void
xhci_noop(struct usbd_xfer *xfer)
{
}


usbd_status
xhci_root_intr_transfer(struct usbd_xfer *xfer)
{
	usbd_status err;

	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	return (xhci_root_intr_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

usbd_status
xhci_root_intr_start(struct usbd_xfer *xfer)
{
	struct xhci_softc *sc = (struct xhci_softc *)xfer->device->bus;

	if (sc->sc_bus.dying)
		return (USBD_IOERROR);

	sc->sc_intrxfer = xfer;

	return (USBD_IN_PROGRESS);
}

void
xhci_root_intr_abort(struct usbd_xfer *xfer)
{
	int s;

	xfer->status = USBD_CANCELLED;

	s = splusb();
	usb_transfer_complete(xfer);
	splx(s);
}

void
xhci_root_intr_done(struct usbd_xfer *xfer)
{
	struct xhci_softc *sc = (struct xhci_softc *)xfer->device->bus;

	KASSERT(sc->sc_intrxfer == xfer);

	if (!xfer->pipe->repeat)
		sc->sc_intrxfer = NULL;
}

usbd_status
xhci_device_ctrl_transfer(struct usbd_xfer *xfer)
{
	usbd_status err;

	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	return (xhci_device_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

usbd_status
xhci_device_ctrl_start(struct usbd_xfer *xfer)
{
	struct xhci_softc *sc = (struct xhci_softc *)xfer->device->bus;
	struct xhci_pipe *xp = (struct xhci_pipe *)xfer->pipe;
	struct xhci_trb *trb0, *trb;
	uint32_t len = UGETW(xfer->request.wLength);
	uint8_t toggle0, toggle;

	KASSERT(xfer->rqflags & URQ_REQUEST);

	if (sc->sc_bus.dying || xp->halted)
		return (USBD_IOERROR);

	if (xp->free_trbs < 3)
		return (USBD_NOMEM);

	/* We'll do the setup TRB once we're finished with the other stages. */
	trb0 = xhci_xfer_get_trb(sc, xfer, &toggle0, 0);

	/* Data TRB */
	if (len != 0) {
		trb = xhci_xfer_get_trb(sc, xfer, &toggle, 0);
		trb->trb_paddr = htole64(DMAADDR(&xfer->dmabuf, 0));
		trb->trb_status = htole32(
		    XHCI_TRB_INTR(0) | XHCI_TRB_TDREM(1) | XHCI_TRB_LEN(len)
		);
		trb->trb_flags = htole32(XHCI_TRB_TYPE_DATA | toggle);

		if (usbd_xfer_isread(xfer))
			trb->trb_flags |= htole32(XHCI_TRB_DIR_IN|XHCI_TRB_ISP);

	}

	/* Status TRB */
	trb = xhci_xfer_get_trb(sc, xfer, &toggle, 1);
	trb->trb_paddr = 0;
	trb->trb_status = htole32(XHCI_TRB_INTR(0));
	trb->trb_flags = htole32(XHCI_TRB_TYPE_STATUS | XHCI_TRB_IOC | toggle);

	if (len == 0 || !usbd_xfer_isread(xfer))
		trb->trb_flags |= htole32(XHCI_TRB_DIR_IN);

	/* Setup TRB */
	trb0->trb_paddr = (uint64_t)*((uint64_t *)&xfer->request);
	trb0->trb_status = htole32(XHCI_TRB_INTR(0) | XHCI_TRB_LEN(8));
	trb0->trb_flags = htole32(XHCI_TRB_TYPE_SETUP | XHCI_TRB_IDT);

	if (len != 0) {
		if (usbd_xfer_isread(xfer))
			trb0->trb_flags |= htole32(XHCI_TRB_TRT_IN);
		else
			trb0->trb_flags |= htole32(XHCI_TRB_TRT_OUT);
	}

	trb0->trb_flags |= htole32(toggle0);

	usb_syncmem(&xp->ring.dma, 0, xp->ring.ntrb * sizeof(struct xhci_trb),
	    BUS_DMASYNC_PREWRITE); /* XXX too big hammer? */
	XDWRITE4(sc, XHCI_DOORBELL(xp->slot), xp->dci);

	xfer->status = USBD_IN_PROGRESS;

	if (sc->sc_bus.use_polling)
		xhci_waitintr(sc, xfer);
#if notyet
	else if (xfer->timeout) {
		timeout_del(&xfer->timeout_handle);
		timeout_set(&xfer->timeout_handle, xhci_timeout, xfer);
		timeout_add_msec(&xfer->timeout_handle, xfer->timeout);
	}
#endif

	return (USBD_IN_PROGRESS);
}

void
xhci_device_ctrl_abort(struct usbd_xfer *xfer)
{
	xhci_abort_xfer(xfer, USBD_CANCELLED);
}

usbd_status
xhci_device_generic_transfer(struct usbd_xfer *xfer)
{
	usbd_status err;

	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	return (xhci_device_generic_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

usbd_status
xhci_device_generic_start(struct usbd_xfer *xfer)
{
	struct xhci_softc *sc = (struct xhci_softc *)xfer->device->bus;
	struct xhci_pipe *xp = (struct xhci_pipe *)xfer->pipe;
	struct xhci_trb *trb;
	uint8_t toggle;

	KASSERT(!(xfer->rqflags & URQ_REQUEST));

	if (sc->sc_bus.dying || xp->halted)
		return (USBD_IOERROR);

	if (xp->free_trbs < 1)
		return (USBD_NOMEM);

	trb = xhci_xfer_get_trb(sc, xfer, &toggle, 1);
	trb->trb_paddr = htole64(DMAADDR(&xfer->dmabuf, 0));
	trb->trb_status = htole32(
	    XHCI_TRB_INTR(0) | XHCI_TRB_TDREM(1) | XHCI_TRB_LEN(xfer->length)
	);
	trb->trb_flags = htole32(
	    XHCI_TRB_TYPE_NORMAL | XHCI_TRB_ISP | XHCI_TRB_IOC | toggle
	);

	usb_syncmem(&xp->ring.dma, ((void *)trb - (void *)xp->ring.trbs),
	    sizeof(struct xhci_trb), BUS_DMASYNC_PREWRITE);
	XDWRITE4(sc, XHCI_DOORBELL(xp->slot), xp->dci);

	xfer->status = USBD_IN_PROGRESS;

	if (sc->sc_bus.use_polling)
		xhci_waitintr(sc, xfer);
#if notyet
	else if (xfer->timeout) {
		timeout_del(&xfer->timeout_handle);
		timeout_set(&xfer->timeout_handle, xhci_timeout, xfer);
		timeout_add_msec(&xfer->timeout_handle, xfer->timeout);
	}
#endif

	return (USBD_IN_PROGRESS);
}

void
xhci_device_generic_done(struct usbd_xfer *xfer)
{
	usb_syncmem(&xfer->dmabuf, 0, xfer->length, usbd_xfer_isread(xfer) ?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

	/* Only happens with interrupt transfers. */
	if (xfer->pipe->repeat)
		xfer->status = xhci_device_generic_start(xfer);
}

void
xhci_device_generic_abort(struct usbd_xfer *xfer)
{
	KASSERT(!xfer->pipe->repeat || xfer->pipe->intrxfer == xfer);

	xhci_abort_xfer(xfer, USBD_CANCELLED);
}
