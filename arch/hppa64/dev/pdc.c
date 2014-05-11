/*	$OpenBSD: pdc.c,v 1.11 2011/08/16 17:26:46 kettenis Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "com.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>
#include <sys/timeout.h>

#include <dev/cons.h>

#include <machine/conf.h>
#include <machine/pdc.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

typedef
struct pdc_softc {
	struct device sc_dv;
	struct tty *sc_tty;
	struct timeout sc_to;
} pdcsoftc_t;

pdcio_t pdc;
int pdcret[32] PDC_ALIGNMENT;
char pdc_consbuf[IODC_MINIOSIZ] PDC_ALIGNMENT;
iodcio_t pdc_cniodc, pdc_kbdiodc;
pz_device_t *pz_kbd, *pz_cons;

int pdcngetc(dev_t);
void pdcnputc(dev_t, char *);

struct consdev pdccons = { NULL, NULL, pdccngetc, pdccnputc,
     nullcnpollc, NULL, makedev(22, 0), CN_LOWPRI };

int pdcmatch(struct device *, void *, void *);
void pdcattach(struct device *, struct device *, void *);

struct cfattach pdc_ca = {
	sizeof(pdcsoftc_t), pdcmatch, pdcattach
};

struct cfdriver pdc_cd = {
	NULL, "pdc", DV_DULL
};

void pdcstart(struct tty *tp);
void pdctimeout(void *v);
int pdcparam(struct tty *tp, struct termios *);
int pdccnlookc(dev_t dev, int *cp);

#if NCOM > 0
/* serial console speed table */
static int pdc_speeds[] = {
	B50,
	B75,
	B110,
	B150,
	B300,
	B600,
	B1200,
	B2400,
	B4800,
	B7200,
	B9600,
	B19200,
	B38400,
	B57600,
	B115200,
	B230400,
};
#endif

void
pdc_init()
{
	static int kbd_iodc[IODC_MAXSIZE/sizeof(int)];
	static int cn_iodc[IODC_MAXSIZE/sizeof(int)];
	int err;

	/* pdc = (pdcio_t)(long)PAGE0->mem_pdc; */
	pz_kbd = &PAGE0->mem_kbd;
	pz_cons = &PAGE0->mem_cons;

	/* XXX should we reset the console/kbd here?
	   well, /boot did that for us anyway */
	if ((err = pdc_call((iodcio_t)pdc, 0, PDC_IODC, PDC_IODC_READ, pdcret,
	    pz_cons->pz_hpa, IODC_IO, cn_iodc, (long)IODC_MAXSIZE)) < 0 ||
	    (err = pdc_call((iodcio_t)pdc, 0, PDC_IODC, PDC_IODC_READ, pdcret,
	    pz_kbd->pz_hpa, IODC_IO, kbd_iodc, (long)IODC_MAXSIZE)) < 0) {
#ifdef DEBUG
		printf("pdc_init: failed reading IODC (%d)\n", err);
#endif
	}

	pdc_cniodc = (iodcio_t)cn_iodc;
	pdc_kbdiodc = (iodcio_t)kbd_iodc;

	/* Start out with pdc as the console. */
	cn_tab = &pdccons;

	/* Figure out console settings. */
#if NCOM > 0
	if (PAGE0->mem_cons.pz_class == PCL_DUPLEX) {
		struct pz_device *pzd = &PAGE0->mem_cons;
		extern int comdefaultrate;
#ifdef DEBUG
		printf("console: class %d flags %b ",
		    pzd->pz_class, pzd->pz_flags, PZF_BITS);
		printf("bc %d/%d/%d/%d/%d/%d ",
		    pzd->pz_bc[0], pzd->pz_bc[1], pzd->pz_bc[2],
		    pzd->pz_bc[3], pzd->pz_bc[4], pzd->pz_bc[5]);
		printf("mod %x layers %x/%x/%x/%x/%x/%x hpa %x\n", pzd->pz_mod,
		    pzd->pz_layers[0], pzd->pz_layers[1], pzd->pz_layers[2],
		    pzd->pz_layers[3], pzd->pz_layers[4], pzd->pz_layers[5],
		    pzd->pz_hpa);
#endif

		/* compute correct baud rate */
		if (PZL_SPEED(pzd->pz_layers[0]) <
		    sizeof(pdc_speeds) / sizeof(int))
			comdefaultrate =
			    pdc_speeds[PZL_SPEED(pzd->pz_layers[0])];
		else
			comdefaultrate = B9600;	/* XXX */
	}
#endif
}

int
pdcmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;
	struct confargs *ca = aux;

	/* there could be only one */
	if (cf->cf_unit > 0 && !strcmp(ca->ca_name, "pdc"))
		return 0;

	return 1;
}

void
pdcattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct pdc_softc *sc = (struct pdc_softc *)self;

	if (!pdc)
		pdc_init();

	printf("\n");

	timeout_set(&sc->sc_to, pdctimeout, sc);
}

int
pdcopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = minor(dev);
	struct pdc_softc *sc;
	struct tty *tp;
	int s;
	int error = 0, setuptimeout = 0;

	if (unit >= pdc_cd.cd_ndevs || (sc = pdc_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	s = spltty();

	if (sc->sc_tty)
		tp = sc->sc_tty;
	else {
		tp = sc->sc_tty = ttymalloc(0);
	}

	tp->t_oproc = pdcstart;
	tp->t_param = pdcparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG|CLOCAL;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = B9600;
		ttsetwater(tp);

		setuptimeout = 1;
	} else if (tp->t_state&TS_XCLUDE && suser(p, 0) != 0) {
		splx(s);
		return (EBUSY);
	}
	tp->t_state |= TS_CARR_ON;
	splx(s);

	error = (*linesw[tp->t_line].l_open)(dev, tp, p);
	if (error == 0 && setuptimeout)
		pdctimeout(sc);

	return error;
}

int
pdcclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = minor(dev);
	struct tty *tp;
	struct pdc_softc *sc;

	if (unit >= pdc_cd.cd_ndevs || (sc = pdc_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	tp = sc->sc_tty;
	timeout_del(&sc->sc_to);
	(*linesw[tp->t_line].l_close)(tp, flag, p);
	ttyclose(tp);
	return 0;
}

int
pdcread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit = minor(dev);
	struct tty *tp;
	struct pdc_softc *sc;

	if (unit >= pdc_cd.cd_ndevs || (sc = pdc_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	tp = sc->sc_tty;
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
pdcwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit = minor(dev);
	struct tty *tp;
	struct pdc_softc *sc;

	if (unit >= pdc_cd.cd_ndevs || (sc = pdc_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	tp = sc->sc_tty;
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

int
pdcioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = minor(dev);
	int error;
	struct tty *tp;
	struct pdc_softc *sc;

	if (unit >= pdc_cd.cd_ndevs || (sc = pdc_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	tp = sc->sc_tty;
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	return ENOTTY;
}

int
pdcparam(tp, t)
	struct tty *tp;
	struct termios *t;
{

	return 0;
}

void
pdcstart(tp)
	struct tty *tp;
{
	int s;

	s = spltty();
	if (tp->t_state & (TS_TTSTOP | TS_BUSY)) {
		splx(s);
		return;
	}
	ttwakeupwr(tp);
	tp->t_state |= TS_BUSY;
	while (tp->t_outq.c_cc != 0)
		pdccnputc(tp->t_dev, getc(&tp->t_outq));
	tp->t_state &= ~TS_BUSY;
	splx(s);
}

int
pdcstop(tp, flag)
	struct tty *tp;
	int flag;
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY)
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	splx(s);
	return 0;
}

void
pdctimeout(v)
	void *v;
{
	struct pdc_softc *sc = v;
	struct tty *tp = sc->sc_tty;
	int c;

	while (pdccnlookc(tp->t_dev, &c)) {
		if (tp->t_state & TS_ISOPEN)
			(*linesw[tp->t_line].l_rint)(c, tp);
	}
	timeout_add(&sc->sc_to, 1);
}

struct tty *
pdctty(dev)
	dev_t dev;
{
	int unit = minor(dev);
	struct pdc_softc *sc;

	if (unit >= pdc_cd.cd_ndevs || (sc = pdc_cd.cd_devs[unit]) == NULL)
		return NULL;

	return sc->sc_tty;
}

int
pdccnlookc(dev, cp)
	dev_t dev;
	int *cp;
{
	int err, l;
	int s = splhigh();

	err = pdc_call(pdc_kbdiodc, 0, pz_kbd->pz_hpa, IODC_IO_CONSIN,
	    pz_kbd->pz_spa, pz_kbd->pz_layers, pdcret, 0, pdc_consbuf,
	    (long)1, (long)0);

	l = pdcret[0];
	*cp = pdc_consbuf[0];
	splx(s);
#ifdef DEBUG
	if (err < 0)
		printf("pdccnlookc: input error: %d\n", err);
#endif

	return l;
}

int
pdccngetc(dev)
	dev_t dev;
{
	int c;

	if (!pdc)
		return 0;

	while(!pdccnlookc(dev, &c))
		;

	return (c);
}

void
pdccnputc(dev, c)
	dev_t dev;
	int c;
{
	register int err;
	int s = splhigh();

	*pdc_consbuf = c;
	err = pdc_call(pdc_cniodc, 0, pz_cons->pz_hpa, IODC_IO_CONSOUT,
	    pz_cons->pz_spa, pz_cons->pz_layers, pdcret, 0, pdc_consbuf,
	    (long)1, (long)0);
	splx(s);

	if (err < 0) {
#ifdef DEBUG
		printf("pdccnputc: output error: %d\n", err);
#endif
	}
}
