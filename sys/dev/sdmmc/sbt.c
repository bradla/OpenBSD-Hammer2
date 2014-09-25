/*	$OpenBSD: sbt.c,v 1.17 2010/08/24 14:52:23 blambert Exp $	*/

/*
 * Copyright (c) 2007 Uwe Stuehler <uwe@openbsd.org>
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

/* Driver for Type-A/B SDIO Bluetooth cards */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <netbt/hci.h>

#include <dev/sdmmc/sdmmcdevs.h>
#include <dev/sdmmc/sdmmcvar.h>

#define CSR_READ_1(sc, reg)       sdmmc_io_read_1((sc)->sc_sf, (reg))
#define CSR_WRITE_1(sc, reg, val) sdmmc_io_write_1((sc)->sc_sf, (reg), (val))

#define SBT_REG_DAT	0x00		/* receiver/transmitter data */
#define SBT_REG_RPC	0x10		/* read packet control */
#define  RPC_PCRRT	(1<<0)		/* packet read retry */
#define SBT_REG_WPC	0x11		/* write packet control */
#define  WPC_PCWRT	(1<<0)		/* packet write retry */
#define SBT_REG_RC	0x12		/* retry control status/set */
#define SBT_REG_ISTAT	0x13		/* interrupt status */
#define  ISTAT_INTRD	(1<<0)		/* packet available for read */
#define SBT_REG_ICLR	0x13		/* interrupt clear */
#define SBT_REG_IENA	0x14		/* interrupt enable */
#define SBT_REG_BTMODE	0x20		/* SDIO Bluetooth card mode */
#define  BTMODE_TYPEB	(1<<0)		/* 1=Type-B, 0=Type-A */

#define SBT_PKT_BUFSIZ	65540
#define SBT_RXTRY_MAX	5

struct sbt_softc {
	struct device sc_dev;		/* base device */
	struct sdmmc_function *sc_sf;	/* SDIO function */
	struct workq *sc_workq;		/* transfer deferred packets */
	int sc_enabled;			/* HCI enabled */
	int sc_dying;			/* shutdown in progress */
	int sc_busy;			/* transmitting or receiving */
	void *sc_ih;
	u_char *sc_buf;
	int sc_rxtry;

	struct hci_unit *sc_unit;	/* MI host controller */
	struct bt_stats sc_stats;	/* MI bluetooth stats */
	struct ifqueue sc_cmdq;
	struct ifqueue sc_acltxq;
	struct ifqueue sc_scotxq;
};

int	sbt_match(struct device *, void *, void *);
void	sbt_attach(struct device *, struct device *, void *);
int	sbt_detach(struct device *, int);

int	sbt_write_packet(struct sbt_softc *, u_char *, size_t);
int	sbt_read_packet(struct sbt_softc *, u_char *, size_t *);

int	sbt_intr(void *);

int	sbt_enable(struct device *);
void	sbt_disable(struct device *);
void	sbt_start(struct sbt_softc *, struct mbuf *, struct ifqueue *, int);
void	sbt_xmit_cmd(struct device *, struct mbuf *);
void	sbt_xmit_acl(struct device *, struct mbuf *);
void	sbt_xmit_sco(struct device *, struct mbuf *);
void	sbt_start_task(void *, void *);

void	sbt_stats(struct device *, struct bt_stats *, int);

#undef DPRINTF
#define SBT_DEBUG
#ifdef SBT_DEBUG
int sbt_debug = 0;
#define DPRINTF(s)	printf s
#define DNPRINTF(n, s)	do { if ((n) <= sbt_debug) printf s; } while (0)
#else
#define DPRINTF(s)	do {} while (0)
#define DNPRINTF(n, s)	do {} while (0)
#endif

struct cfattach sbt_ca = {
	sizeof(struct sbt_softc), sbt_match, sbt_attach, sbt_detach
};

struct cfdriver sbt_cd = {
	NULL, "sbt", DV_DULL
};


/*
 * Autoconf glue
 */

static const struct sbt_product {
	u_int16_t	sp_vendor;
	u_int16_t	sp_product;
	const char	*sp_cisinfo[4];
} sbt_products[] = {
	{ SDMMC_VENDOR_SOCKETCOM,
	  SDMMC_PRODUCT_SOCKETCOM_BTCARD,
	  SDMMC_CIS_SOCKETCOM_BTCARD }
};

const struct hci_if sbt_hci = {
	.enable = sbt_enable,
	.disable = sbt_disable,
	.output_cmd = sbt_xmit_cmd,
	.output_acl = sbt_xmit_acl,
	.output_sco = sbt_xmit_sco,
	.get_stats = sbt_stats,
	.ipl = IPL_SDMMC
};

int
sbt_match(struct device *parent, void *match, void *aux)
{
	struct sdmmc_attach_args *sa = aux;
	const struct sbt_product *sp;
	struct sdmmc_function *sf;
	int i;

	if (sa->sf == NULL)
		return 0;	/* not SDIO */

	sf = sa->sf->sc->sc_fn0;
	sp = &sbt_products[0];

	for (i = 0; i < sizeof(sbt_products) / sizeof(sbt_products[0]);
	     i++, sp = &sbt_products[i])
		if (sp->sp_vendor == sf->cis.manufacturer &&
		    sp->sp_product == sf->cis.product)
			return 1;
	return 0;
}

void
sbt_attach(struct device *parent, struct device *self, void *aux)
{
	struct sbt_softc *sc = (struct sbt_softc *)self;
	struct sdmmc_attach_args *sa = aux;

	SDMMC_ASSERT_LOCKED(sc->sc_sf->sc);

	printf("\n");

	sc->sc_sf = sa->sf;

	(void)sdmmc_io_function_disable(sc->sc_sf);
	if (sdmmc_io_function_enable(sc->sc_sf)) {
		printf("%s: function not ready\n", DEVNAME(sc));
		return;
	}

	/* It may be Type-B, but we use it only in Type-A mode. */
	printf("%s: SDIO Bluetooth Type-A\n", DEVNAME(sc));

	/* Create a shared buffer for receive and transmit. */
	sc->sc_buf = malloc(SBT_PKT_BUFSIZ, M_DEVBUF, M_NOWAIT);
	if (sc->sc_buf == NULL) {
		printf("%s: can't allocate cmd buffer\n", DEVNAME(sc));
		return;
	}

	/* Create a work thread to transmit deferred packets. */
	sc->sc_workq = workq_create(DEVNAME(sc), 1, IPL_SDMMC);
	if (sc->sc_workq == NULL) {
		printf("%s: can't allocate workq\n", DEVNAME(sc));
		return;
	}

	/* Enable the HCI packet transport read interrupt. */
	CSR_WRITE_1(sc, SBT_REG_IENA, ISTAT_INTRD);

	/* Enable the card interrupt for this function. */
	sc->sc_ih = sdmmc_intr_establish(parent, sbt_intr, sc, DEVNAME(sc));
	if (sc->sc_ih == NULL) {
		printf("%s: can't establish interrupt\n", DEVNAME(sc));
		return;
	}
	sdmmc_intr_enable(sc->sc_sf);

	/*
	 * Attach Bluetooth unit (machine-independent HCI).
	 */
	sc->sc_unit = hci_attach(&sbt_hci, &sc->sc_dev, 0);
}

int
sbt_detach(struct device *self, int flags)
{
	struct sbt_softc *sc = (struct sbt_softc *)self;

	sc->sc_dying = 1;

	while (sc->sc_busy)
		tsleep(&sc->sc_busy, PWAIT, "sbtdie", 0);

	/* Detach HCI interface */
	if (sc->sc_unit) {
		hci_detach(sc->sc_unit);
		sc->sc_unit = NULL;
	}

	if (sc->sc_ih != NULL)
		sdmmc_intr_disestablish(sc->sc_ih);

	if (sc->sc_workq != NULL)
		workq_destroy(sc->sc_workq);

	if (sc->sc_buf != NULL)
		free(sc->sc_buf, M_DEVBUF);

	return 0;
}


/*
 * Bluetooth HCI packet transport
 */

int
sbt_write_packet(struct sbt_softc *sc, u_char *buf, size_t len)
{
	u_char hdr[3];
	size_t pktlen;
	int error = EIO;
	int retry = 3;

again:
	if (retry-- == 0) {
		DPRINTF(("%s: sbt_write_cmd: giving up\n", DEVNAME(sc)));
		return error;
	}

	/* Restart the current packet. */
	sdmmc_io_write_1(sc->sc_sf, SBT_REG_WPC, WPC_PCWRT);

	/* Write the packet length. */
	pktlen = len + 3;
	hdr[0] = pktlen & 0xff;
	hdr[1] = (pktlen >> 8) & 0xff;
	hdr[2] = (pktlen >> 16) & 0xff;
	error = sdmmc_io_write_multi_1(sc->sc_sf, SBT_REG_DAT, hdr, 3);
	if (error) {
		DPRINTF(("%s: sbt_write_packet: failed to send length\n",
		    DEVNAME(sc)));
		goto again;
	}

	error = sdmmc_io_write_multi_1(sc->sc_sf, SBT_REG_DAT, buf, len);
	if (error) {
		DPRINTF(("%s: sbt_write_packet: failed to send packet data\n",
		    DEVNAME(sc)));
		goto again;
	}
	return 0;
}

int
sbt_read_packet(struct sbt_softc *sc, u_char *buf, size_t *lenp)
{
	u_char hdr[3];
	size_t len;
	int error;

	error = sdmmc_io_read_multi_1(sc->sc_sf, SBT_REG_DAT, hdr, 3);
	if (error) {
		DPRINTF(("%s: sbt_read_packet: failed to read length\n",
		    DEVNAME(sc)));
		goto out;
	}
	len = (hdr[0] | (hdr[1] << 8) | (hdr[2] << 16)) - 3;
	if (len > *lenp) {
		DPRINTF(("%s: sbt_read_packet: len %u > %u\n",
		    DEVNAME(sc), len, *lenp));
		error = ENOBUFS;
		goto out;
	}

	DNPRINTF(2,("%s: sbt_read_packet: reading len %u bytes\n",
	    DEVNAME(sc), len));
	error = sdmmc_io_read_multi_1(sc->sc_sf, SBT_REG_DAT, buf, len);
	if (error) {
		DPRINTF(("%s: sbt_read_packet: failed to read packet data\n",
		    DEVNAME(sc)));
		goto out;
	}

out:
	rw_enter_write(&sc->sc_sf->sc->sc_lock);
	if (error) {
		if (sc->sc_rxtry >= SBT_RXTRY_MAX) {
			/* Drop and request the next packet. */
			sc->sc_rxtry = 0;
			CSR_WRITE_1(sc, SBT_REG_RPC, 0);
		} else {
			/* Request the current packet again. */
			sc->sc_rxtry++;
			CSR_WRITE_1(sc, SBT_REG_RPC, RPC_PCRRT);
		}
		rw_exit(&sc->sc_sf->sc->sc_lock);
		return error;
	}

	/* acknowledge read packet */
	CSR_WRITE_1(sc, SBT_REG_RPC, 0);

	rw_exit(&sc->sc_sf->sc->sc_lock);

	*lenp = len;
	return 0;
}

/*
 * Interrupt handling
 */

/* This function is called from the SDIO interrupt thread. */
int
sbt_intr(void *arg)
{
	struct sbt_softc *sc = arg;
	struct mbuf *m = NULL;
	u_int8_t status;
	size_t len;
	int s;

	/* Block further SDIO interrupts; XXX not really needed? */
	s = splsdmmc();

	rw_enter_write(&sc->sc_sf->sc->sc_lock);
	status = CSR_READ_1(sc, SBT_REG_ISTAT);
	CSR_WRITE_1(sc, SBT_REG_ICLR, status);
	rw_exit(&sc->sc_sf->sc->sc_lock);

	if ((status & ISTAT_INTRD) == 0)
		return 0;	/* shared SDIO card interrupt? */

	len = SBT_PKT_BUFSIZ;
	if (sbt_read_packet(sc, sc->sc_buf, &len) != 0 || len == 0) {
		DPRINTF(("%s: sbt_intr: read failed\n", DEVNAME(sc)));
		goto eoi;
	}

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		DPRINTF(("%s: sbt_intr: MGETHDR failed\n", DEVNAME(sc)));
		goto eoi;
	}

	m->m_pkthdr.len = m->m_len = MHLEN;
	m_copyback(m, 0, len, sc->sc_buf, M_NOWAIT);
	if (m->m_pkthdr.len == MAX(MHLEN, len)) {
		m->m_pkthdr.len = len;
		m->m_len = MIN(MHLEN, m->m_pkthdr.len);
	} else {
		DPRINTF(("%s: sbt_intr: m_copyback failed\n", DEVNAME(sc)));
		m_free(m);
		m = NULL;
	}

eoi:
	if (m != NULL) {
		switch (sc->sc_buf[0]) {
		case HCI_ACL_DATA_PKT:
			DNPRINTF(1,("%s: recv ACL packet (%d bytes)\n",
			    DEVNAME(sc), m->m_pkthdr.len));
			hci_input_acl(sc->sc_unit, m);
			break;
		case HCI_SCO_DATA_PKT:
			DNPRINTF(1,("%s: recv SCO packet (%d bytes)\n",
			    DEVNAME(sc), m->m_pkthdr.len));
			hci_input_sco(sc->sc_unit, m);
			break;
		case HCI_EVENT_PKT:
			DNPRINTF(1,("%s: recv EVENT packet (%d bytes)\n",
			    DEVNAME(sc), m->m_pkthdr.len));
			hci_input_event(sc->sc_unit, m);
			break;
		default:
			DPRINTF(("%s: recv 0x%x packet (%d bytes)\n",
			    DEVNAME(sc), sc->sc_buf[0], m->m_pkthdr.len));
			sc->sc_stats.err_rx++;
			m_free(m);
			break;
		}
	} else
		sc->sc_stats.err_rx++;

	splx(s);

	/* Claim this interrupt. */
	return 1;
}


/*
 * Bluetooth HCI unit functions
 */

int
sbt_enable(struct device *self)
{
	struct sbt_softc *sc = (struct sbt_softc *)self;

	if (sc->sc_enabled)
		return 0;

	sc->sc_enabled = 1;
	return 0;
}

void
sbt_disable(struct device *self)
{
	struct sbt_softc *sc = (struct sbt_softc *)self;
	int s;

	if (!sc->sc_enabled)
		return;

	s = splsdmmc();
#ifdef notyet			/* XXX */
	if (sc->sc_rxp) {
		m_freem(sc->sc_rxp);
		sc->sc_rxp = NULL;
	}

	if (sc->sc_txp) {
		m_freem(sc->sc_txp);
		sc->sc_txp = NULL;
	}
#endif
	sc->sc_enabled = 0;
	splx(s);
}

void
sbt_start(struct sbt_softc *sc, struct mbuf *m, struct ifqueue *q, int xmit)
{
	int s;
	int len;
#ifdef SBT_DEBUG
	const char *what;
#endif

	s = splsdmmc();
	if (m != NULL)
		IF_ENQUEUE(q, m);

	if (sc->sc_dying || IF_IS_EMPTY(q)) {
		splx(s);
		return;
	}

	if (curproc == NULL || sc->sc_busy) {
		(void)workq_add_task(sc->sc_workq, 0, sbt_start_task,
		    sc, (void *)(long)xmit);
		splx(s);
		return;
	}

	/* Defer additional transfers and reception of packets. */
	sdmmc_intr_disable(sc->sc_sf);
	sc->sc_busy++;

	IF_DEQUEUE(q, m);

#ifdef SBT_DEBUG
	switch (xmit) {
	case BTF_XMIT_CMD:
		what = "CMD";
		break;
	case BTF_XMIT_ACL:
		what = "ACL";
		break;
	case BTF_XMIT_SCO:
		what = "SCO";
		break;
	}
	DNPRINTF(1,("%s: xmit %s packet (%d bytes)\n", DEVNAME(sc),
	    what, m->m_pkthdr.len));
#endif

	sc->sc_unit->hci_flags |= xmit;

	len = m->m_pkthdr.len;
	m_copydata(m, 0, len, sc->sc_buf);
	m_freem(m);

	if (sbt_write_packet(sc, sc->sc_buf, len))
		DPRINTF(("%s: sbt_write_packet failed\n", DEVNAME(sc)));

	sc->sc_unit->hci_flags &= ~xmit;

	sc->sc_busy--;
	sdmmc_intr_enable(sc->sc_sf);

	if (sc->sc_dying)
		wakeup(&sc->sc_busy);

	splx(s);
}

void
sbt_xmit_cmd(struct device *self, struct mbuf *m)
{
	struct sbt_softc *sc = (struct sbt_softc *)self;

	sbt_start(sc, m, &sc->sc_cmdq, BTF_XMIT_CMD);
}

void
sbt_xmit_acl(struct device *self, struct mbuf *m)
{
	struct sbt_softc *sc = (struct sbt_softc *)self;

	sbt_start(sc, m, &sc->sc_acltxq, BTF_XMIT_ACL);
}

void
sbt_xmit_sco(struct device *self, struct mbuf *m)
{
	struct sbt_softc *sc = (struct sbt_softc *)self;

	sbt_start(sc, m, &sc->sc_scotxq, BTF_XMIT_SCO);
}

void
sbt_start_task(void *arg1, void *arg2)
{
	struct sbt_softc *sc = arg1;
	int xmit = (long)arg2;

	switch (xmit) {
	case BTF_XMIT_CMD:
		sbt_xmit_cmd(&sc->sc_dev, NULL);
		break;
	case BTF_XMIT_ACL:
		sbt_xmit_acl(&sc->sc_dev, NULL);
		break;
	case BTF_XMIT_SCO:
		sbt_xmit_sco(&sc->sc_dev, NULL);
		break;
	}
}

void
sbt_stats(struct device *self, struct bt_stats *dest, int flush)
{
	struct sbt_softc *sc = (struct sbt_softc *)self;
	int s;

	s = splsdmmc();
	memcpy(dest, &sc->sc_stats, sizeof(struct bt_stats));

	if (flush)
		memset(&sc->sc_stats, 0, sizeof(struct bt_stats));

	splx(s);
}
