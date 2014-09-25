/*	$OpenBSD: dma.c,v 1.28 2010/07/10 19:32:24 miod Exp $	*/
/*	$NetBSD: dma.c,v 1.46 1997/08/27 11:24:16 bouyer Exp $ */

/*
 * Copyright (c) 1994 Paul Kranenburg.  All rights reserved.
 * Copyright (c) 1994 Peter Galbavy.  All rights reserved.
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <sparc/autoconf.h>
#include <sparc/cpu.h>

#include <sparc/sparc/cpuvar.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <uvm/uvm_extern.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>
#include <sparc/dev/dmavar.h>
#include <sparc/dev/espvar.h>

int dmaprint(void *, const char *);
void dmaattach(struct device *, struct device *, void *);
int dmamatch(struct device *, void *, void *);
void dma_reset(struct dma_softc *, int);
void espdma_reset(struct dma_softc *);
int ledmamatch(struct device *, void *, void *);
void ledma_reset(struct dma_softc *);
void dma_enintr(struct dma_softc *);
int dma_isintr(struct dma_softc *);
int espdmaintr(struct dma_softc *);
int ledmaintr(struct dma_softc *);
int dma_setup(struct dma_softc *, caddr_t *, size_t *,
			     int, size_t *);
void dma_go(struct dma_softc *);

struct cfattach dma_ca = {
	sizeof(struct dma_softc), dmamatch, dmaattach
};

struct cfdriver dma_cd = {
	NULL, "dma", DV_DULL
};

struct cfattach ledma_ca = {
	sizeof(struct dma_softc), ledmamatch, dmaattach
};

struct cfdriver ledma_cd = {
	NULL, "ledma", DV_DULL
};

int
dmaprint(aux, name)
	void *aux;
	const char *name;
{
	struct confargs *ca = aux;

	if (name)
		printf("%s at %s", ca->ca_ra.ra_name, name);
	printf(" offset 0x%x", ca->ca_offset);
	return (UNCONF);
}

int
dmamatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name) &&
	    strcmp("espdma", ra->ra_name))
		return (0);
#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	if (ca->ca_bustype == BUS_SBUS) {
		if (!sbus_testdma((struct sbus_softc *)parent, ca))
			return (0);
		return (1);
	}
#endif
	ra->ra_len = NBPG;
	return (probeget(ra->ra_vaddr, 4) != -1);
}

/*
 * Attach all the sub-devices we can find
 */
void
dmaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct dma_softc *sc = (void *)self;
	int devnode;
#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	int node;
	struct confargs oca;
	char *name;
#endif

	if (ca->ca_ra.ra_vaddr == NULL || ca->ca_ra.ra_nvaddrs == 0)
		ca->ca_ra.ra_vaddr =
		    mapiodev(ca->ca_ra.ra_reg, 0, ca->ca_ra.ra_len);

	sc->sc_regs = (struct dma_regs *) ca->ca_ra.ra_vaddr;
	devnode = ca->ca_ra.ra_node;

	/*
	 * If we're a ledma, check to see what cable type is currently 
	 * active and set the appropriate bit in the ledma csr so that
	 * it gets used. If we didn't netboot, the PROM won't have the
	 * "cable-selection" property; default to TP and then the user
	 * can change it via a "link0" option to ifconfig.
	 */
	if (strcmp(ca->ca_ra.ra_name, "ledma") == 0) {
		char *cabletype = getpropstring(devnode, "cable-selection");
		if (strcmp(cabletype, "tpe") == 0) {
			sc->sc_regs->csr |= E_TP_AUI;
		} else if (strcmp(cabletype, "aui") == 0) {
			sc->sc_regs->csr &= ~E_TP_AUI;
		} else {
			/* assume TP if nothing there */
			sc->sc_regs->csr |= E_TP_AUI;
		}
		delay(20000);	/* manual says we need 20ms delay */
	}

	/*
	 * Get transfer burst size from PROM and plug it into the
	 * controller registers. This is needed on the Sun4m; do
	 * others need it too?
	 */
	if (CPU_ISSUN4M) {
		int sbusburst = ((struct sbus_softc *)parent)->sc_burst;
		if (sbusburst == 0)
			sbusburst = SBUS_BURST_32 - 1; /* 1->16 */

		sc->sc_burst = getpropint(devnode, "burst-sizes", -1);
		if (sc->sc_burst == -1)
			/* take SBus burst sizes */
			sc->sc_burst = sbusburst;

		/* Clamp at parent's burst sizes */
		sc->sc_burst &= sbusburst;
	}

	printf(": rev ");
	sc->sc_rev = sc->sc_regs->csr & D_DEV_ID;
	switch (sc->sc_rev) {
	case DMAREV_0:
		printf("0");
		break;
	case DMAREV_ESC:
		printf("esc");
		break;
	case DMAREV_1:
		printf("1");
		break;
	case DMAREV_PLUS:
		printf("1+");
		break;
	case DMAREV_2:
		printf("2");
		break;
	case DMAREV_HME:
		printf("fas");
		break;
	default:
		printf("unknown (0x%x)", sc->sc_rev);
	}
	printf("\n");

	/* indirect functions */
	dma_setuphandlers(sc);

	if (CPU_ISSUN4)
		goto espsearch;

#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	/* Propagate bootpath */
	if (ca->ca_ra.ra_bp != NULL &&
	    (strcmp(ca->ca_ra.ra_bp->name, "espdma") == 0 ||
	     strcmp(ca->ca_ra.ra_bp->name, "dma") == 0 ||
	     strcmp(ca->ca_ra.ra_bp->name, "ledma") == 0))
		oca.ca_ra.ra_bp = ca->ca_ra.ra_bp + 1;
	else
		oca.ca_ra.ra_bp = NULL;

	/* search through children */
	node = firstchild(devnode);
	if (node != 0) do {
		name = getpropstring(node, "name");
		if (!romprop(&oca.ca_ra, name, node))
			continue;

		sbus_translate(parent, &oca);
		oca.ca_bustype = BUS_SBUS;
		(void) config_found(&sc->sc_dev, (void *)&oca, dmaprint);
	} while ((node = nextsibling(node)) != 0); else
#endif /* SUN4C || SUN4D || SUN4E || SUN4M */

	if (strcmp(ca->ca_ra.ra_name, "dma") == 0) {
espsearch:
		/*
		 * find the ESP by poking around the esp device structures
		 *
		 * What happens here is that if the esp driver has not been
		 * configured, then this returns a NULL pointer. Then when the
		 * esp actually gets configured, it does the opposing test, and
		 * if the sc->sc_dma field in its softc is NULL, then tries to
		 * find the matching dma driver.
		 *
		 */
		sc->sc_esp = (struct esp_softc *)
			     getdevunit("esp", sc->sc_dev.dv_unit);

		/*
		 * and a back pointer to us, for DMA
		 */
		if (sc->sc_esp)
			sc->sc_esp->sc_dma = sc;
	}
}

void
dma_setuphandlers(struct dma_softc *sc)
{
	if (sc->sc_dev.dv_cfdata == NULL ||	/* happens on SUNW,fas */
	    sc->sc_dev.dv_cfdata->cf_attach == &dma_ca) {
		sc->reset = espdma_reset;
		sc->intr = espdmaintr;
	} else {
		sc->reset = ledma_reset;
		sc->intr = ledmaintr;
	}
	sc->enintr = dma_enintr;
	sc->isintr = dma_isintr;
	sc->setup = dma_setup;
	sc->go = dma_go;
}

#define DMAWAIT(SC, COND, MSG, DONTPANIC) do if (COND) {		\
	int count = 500000;						\
	while ((COND) && --count > 0) DELAY(1);				\
	if (count == 0) {						\
		printf("%s: line %d: CSR = 0x%lx\n", __FILE__, __LINE__, \
			(SC)->sc_regs->csr);				\
		if (DONTPANIC)						\
			printf(MSG);					\
		else							\
			panic(MSG);					\
	}								\
} while (0)

#define DMA_DRAIN(sc, dontpanic) do {					\
	/*								\
	 * DMA rev0 & rev1: we are not allowed to touch the DMA "flush"	\
	 *     and "drain" bits while it is still thinking about a	\
	 *     request.							\
	 * other revs: D_ESC_R_PEND bit reads as 0			\
	 */								\
	DMAWAIT(sc, sc->sc_regs->csr & D_ESC_R_PEND, "R_PEND", dontpanic);\
	if (sc->sc_rev != DMAREV_HME) {					\
		/*							\
		 * Select drain bit based on revision			\
		 * also clears errors and D_TC flag			\
		 */							\
		if (sc->sc_rev == DMAREV_1 || sc->sc_rev == DMAREV_0)	\
			DMACSR(sc) |= D_ESC_DRAIN;			\
		else							\
			DMACSR(sc) |= D_INVALIDATE;			\
	}								\
	/*								\
	 * Wait for draining to finish					\
	 *  rev0 & rev1 call this PACKCNT				\
	 */								\
	DMAWAIT(sc, sc->sc_regs->csr & D_DRAINING, "DRAINING", dontpanic);\
} while(0)

#define DMA_FLUSH(sc, dontpanic) do {					\
	int csr;							\
	/*								\
	 * DMA rev0 & rev1: we are not allowed to touch the DMA "flush"	\
	 *     and "drain" bits while it is still thinking about a	\
	 *     request.							\
	 * other revs: D_ESC_R_PEND bit reads as 0			\
	 */								\
	DMAWAIT(sc, sc->sc_regs->csr & D_ESC_R_PEND, "R_PEND", dontpanic);\
	csr = DMACSR(sc);						\
	csr &= ~(D_WRITE|D_EN_DMA);	/* no-ops on ENET */		\
	csr |= D_INVALIDATE;		/* XXX FAS ? */			\
	DMACSR(sc) = csr;						\
} while(0)

void
dma_reset(sc, isledma)
	struct dma_softc *sc;
	int isledma;
{
	int csr;

	DMA_FLUSH(sc, 1);
	csr = DMACSR(sc);

	if (sc->sc_rev == DMAREV_HME)
		DMACSR(sc) = csr | D_HW_RESET_FAS366;

	csr |= D_RESET;				/* reset DMA */
	DMACSR(sc) = csr;
	DELAY(200);				/* > 10 SBus clocks(?) */

	/*DMAWAIT1(sc); why was this here? */
	DMACSR(sc) &= ~D_RESET;			/* de-assert reset line */
	DELAY(5);

	csr = DMACSR(sc);
	csr |= D_INT_EN;			/* enable interrupts */
	if (sc->sc_rev > DMAREV_1 && isledma == 0) {
		if (sc->sc_rev == DMAREV_HME)
			csr |= D_TWO_CYCLE;
		else
			csr |= D_FASTER;
	}

	switch (sc->sc_rev) {
	case DMAREV_HME:
	case DMAREV_2:
		csr &= ~L64854_BURST_SIZE; /* must clear first */
		if (sc->sc_burst & SBUS_BURST_32) {
			csr |= L64854_BURST_32;
		} else if (sc->sc_burst & SBUS_BURST_16) {
			csr |= L64854_BURST_16;
		} else {
			csr |= L64854_BURST_0;
		}
		break;
	case DMAREV_ESC:
		csr |= D_ESC_AUTODRAIN;	/* Auto-drain */
		if (sc->sc_burst & SBUS_BURST_32) {
			csr &= ~D_ESC_BURST;
		} else
			csr |= D_ESC_BURST;
		break;
	default:
		break;
	}
	DMACSR(sc) = csr;

	if (sc->sc_rev == DMAREV_HME) {
		DMADDR(sc) = 0;
		sc->sc_dmactl = csr;
	}

	sc->sc_active = 0;			/* and of course we aren't */
}

void
espdma_reset(sc)
	struct dma_softc *sc;
{
	dma_reset(sc, 0);
}

int
ledmamatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

        if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);
#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	if (!sbus_testdma((struct sbus_softc *)parent, ca))
		return(0);
#endif
	return (1);
}

void
ledma_reset(sc)
	struct dma_softc *sc;
{
	dma_reset(sc, 1);
}

void
dma_enintr(sc)
	struct dma_softc *sc;
{
	sc->sc_regs->csr |= D_INT_EN;
}

int
dma_isintr(sc)
	struct dma_softc *sc;
{
	return (sc->sc_regs->csr & (D_INT_PEND|D_ERR_PEND));
}

#define DMAMAX(a)	(0x01000000 - ((a) & 0x00ffffff))


/*
 * setup a dma transfer
 */
int
dma_setup(sc, addr, len, datain, dmasize)
	struct dma_softc *sc;
	caddr_t *addr;
	size_t *len;
	int datain;
	size_t *dmasize;	/* IN-OUT */
{
	u_long csr;

	DMA_FLUSH(sc, 0);

#if 0
	DMACSR(sc) &= ~D_INT_EN;
#endif
	sc->sc_dmaaddr = addr;
	sc->sc_dmalen = len;

	NCR_DMA(("%s: start %d@%p,%d\n", sc->sc_dev.dv_xname,
		*sc->sc_dmalen, *sc->sc_dmaaddr, datain ? 1 : 0));

	/*
	 * the rules say we cannot transfer more than the limit
	 * of this DMA chip (64k for old and 16Mb for new),
	 * and we cannot cross a 16Mb boundary.
	 */
	*dmasize = sc->sc_dmasize =
		min(*dmasize, DMAMAX((size_t) *sc->sc_dmaaddr));

	NCR_DMA(("dma_setup: dmasize = %d\n", sc->sc_dmasize));

	/*
	 * XXX what length?
	 */
	if (sc->sc_rev == DMAREV_HME) {
		DMACSR(sc) = sc->sc_dmactl | L64854_RESET;
		DMACSR(sc) = sc->sc_dmactl;
		DMACNT(sc) = *dmasize;
	}

	/* Program the DMA address */
	if (CPU_ISSUN4M && sc->sc_dmasize) {
		/*
		 * Use dvma mapin routines to map the buffer into DVMA space.
		 */
		sc->sc_dvmaaddr = *sc->sc_dmaaddr;
		sc->sc_dvmakaddr = kdvma_mapin(sc->sc_dvmaaddr,
					       sc->sc_dmasize, 0);
		if (sc->sc_dvmakaddr == NULL)
			panic("dma: cannot allocate DVMA address");
		DMADDR(sc) = sc->sc_dvmakaddr;
	} else
		DMADDR(sc) = *sc->sc_dmaaddr;

	if (sc->sc_rev == DMAREV_ESC) {
		/* DMA ESC chip bug work-around */
		long bcnt = sc->sc_dmasize;
		long eaddr = bcnt + (long)*sc->sc_dmaaddr;
		if ((eaddr & PGOFSET) != 0)
			bcnt = round_page(bcnt);
		DMACNT(sc) = bcnt;
	}
	/* Setup DMA control register */
	csr = DMACSR(sc);
	if (datain)
		csr |= D_WRITE;
	else
		csr &= ~D_WRITE;
	csr |= D_INT_EN;

	if (sc->sc_rev == DMAREV_HME) {
		csr |= (D_DSBL_SCSI_DRN | D_EN_DMA);
	}

	DMACSR(sc) = csr;

	return 0;
}

void
dma_go(sc)
	struct dma_softc *sc;
{

	/* Start DMA */
	DMACSR(sc) |= D_EN_DMA;
	sc->sc_active = 1;
}

/*
 * Pseudo (chained) interrupt from the esp driver to kick the
 * current running DMA transfer. I am replying on espintr() to
 * pickup and clean errors for now
 *
 * return 1 if it was a DMA continue.
 */
int
espdmaintr(sc)
	struct dma_softc *sc;
{
	struct ncr53c9x_softc *nsc = &sc->sc_esp->sc_ncr53c9x;
	int trans, resid;
	u_long csr;

	csr = DMACSR(sc);

	NCR_DMA(("%s: intr: addr %p, csr %b\n", sc->sc_dev.dv_xname,
		 DMADDR(sc), csr, DDMACSR_BITS));

	if (csr & (D_ERR_PEND|D_SLAVE_ERR)) {
		printf("%s: error: csr=%b\n", sc->sc_dev.dv_xname,
			csr, DDMACSR_BITS);
		DMACSR(sc) &= ~D_EN_DMA;	/* Stop DMA */
		/* Invalidate the queue; SLAVE_ERR bit is write-to-clear */
		DMACSR(sc) |= D_INVALIDATE|D_SLAVE_ERR;
		return (-1);
	}

	/* This is an "assertion" :) */
	if (sc->sc_active == 0)
		panic("dmaintr: DMA wasn't active");

	DMA_DRAIN(sc, 0);

	/* DMA has stopped */
	DMACSR(sc) &= ~D_EN_DMA;
	sc->sc_active = 0;

	if (sc->sc_dmasize == 0) {
		/* A "Transfer Pad" operation completed */
		NCR_DMA(("dmaintr: discarded %d bytes (tcl=%d, tcm=%d)\n",
			NCR_READ_REG(nsc, NCR_TCL) |
				(NCR_READ_REG(nsc, NCR_TCM) << 8),
			NCR_READ_REG(nsc, NCR_TCL),
			NCR_READ_REG(nsc, NCR_TCM)));
		return 0;
	}

	resid = 0;
	/*
	 * If a transfer onto the SCSI bus gets interrupted by the device
	 * (e.g. for a SAVEPOINTER message), the data in the FIFO counts
	 * as residual since the ESP counter registers get decremented as
	 * bytes are clocked into the FIFO.
	 */
	if (!(csr & D_WRITE) &&
	    (resid = (NCR_READ_REG(nsc, NCR_FFLAG) & NCRFIFO_FF)) != 0) {
		NCR_DMA(("dmaintr: empty esp FIFO of %d ", resid));
	}

	if ((nsc->sc_espstat & NCRSTAT_TC) == 0) {
		/*
		 * `Terminal count' is off, so read the residue
		 * out of the ESP counter registers.
		 */
		resid += (NCR_READ_REG(nsc, NCR_TCL) |
			  (NCR_READ_REG(nsc, NCR_TCM) << 8) |
			   ((nsc->sc_cfg2 & NCRCFG2_FE)
				? (NCR_READ_REG(nsc, NCR_TCH) << 16)
				: 0));

		if (resid == 0 && sc->sc_dmasize == 65536 &&
		    (nsc->sc_cfg2 & NCRCFG2_FE) == 0)
			/* A transfer of 64K is encoded as `TCL=TCM=0' */
			resid = 65536;
	}

	trans = sc->sc_dmasize - resid;
	if (trans < 0) {			/* transferred < 0 ? */
#if 0
		/*
		 * This situation can happen in perfectly normal operation
		 * if the ESP is reselected while using DMA to select
		 * another target.  As such, don't print the warning.
		 */
		printf("%s: xfer (%d) > req (%d)\n",
		    sc->sc_dev.dv_xname, trans, sc->sc_dmasize);
#endif
		trans = sc->sc_dmasize;
	}

	NCR_DMA(("dmaintr: tcl=%d, tcm=%d, tch=%d; trans=%d, resid=%d\n",
		NCR_READ_REG(nsc, NCR_TCL),
		NCR_READ_REG(nsc, NCR_TCM),
		(nsc->sc_cfg2 & NCRCFG2_FE)
			? NCR_READ_REG(nsc, NCR_TCH) : 0,
		trans, resid));

	if (csr & D_WRITE)
		cpuinfo.cache_flush(*sc->sc_dmaaddr, trans);

	if (CPU_ISSUN4M && sc->sc_dvmakaddr)
		dvma_mapout((vaddr_t)sc->sc_dvmakaddr,
			    (vaddr_t)sc->sc_dvmaaddr, sc->sc_dmasize);

	*sc->sc_dmalen -= trans;
	*sc->sc_dmaaddr += trans;

#if 0	/* this is not normal operation just yet */
	if (*sc->sc_dmalen == 0 ||
	    nsc->sc_phase != nsc->sc_prevphase)
		return 0;

	/* and again */
	dma_start(sc, sc->sc_dmaaddr, sc->sc_dmalen, DMACSR(sc) & D_WRITE);
	return 1;
#endif
	return 0;
}

/*
 * Pseudo (chained) interrupt from the le driver to handle DMA
 * errors.
 *
 * XXX: untested
 */
int
ledmaintr(sc)
	struct dma_softc *sc;
{
	u_long csr;

	csr = DMACSR(sc);

	if (csr & D_ERR_PEND) {
		DMACSR(sc) &= ~D_EN_DMA;	/* Stop DMA */
		DMACSR(sc) |= D_INVALIDATE;
		printf("%s: error: csr=%b\n", sc->sc_dev.dv_xname,
			csr, DDMACSR_BITS);
		DMA_RESET(sc);
	}
	return 1;
}
