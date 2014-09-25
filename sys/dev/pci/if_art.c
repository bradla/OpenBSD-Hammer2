/*	$OpenBSD: if_art.c,v 1.20 2013/06/20 12:03:40 mpi Exp $ */

/*
 * Copyright (c) 2004,2005  Internet Business Solutions AG, Zurich, Switzerland
 * Written by: Claudio Jeker <jeker@accoom.net>
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
#include <sys/types.h>

#include <sys/device.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_sppp.h>

#include <dev/pci/musyccvar.h>
#include <dev/pci/if_art.h>

#define ART_E1_MASK 0xffffffff
#define ART_T1_MASK 0x01fffffe

int	art_match(struct device *, void *, void *);
void	art_softc_attach(struct device *, struct device *, void *);

int	art_ioctl(struct ifnet *, u_long, caddr_t);
int	art_ifm_change(struct ifnet *);
void	art_ifm_status(struct ifnet *, struct ifmediareq *);
int	art_ifm_options(struct ifnet *, struct channel_softc *, u_int);
void	art_onesec(void *);
void	art_linkstate(void *);
u_int32_t art_mask_tsmap(u_int, u_int32_t);

struct cfattach art_ca = {
	sizeof(struct art_softc), art_match, art_softc_attach
};

struct cfdriver art_cd = {
	NULL, "art", DV_IFNET
};

int
art_match(struct device *parent, void *match, void *aux)
{
	struct musycc_attach_args	*ma = aux;

	if (ma->ma_type == MUSYCC_FRAMER_BT8370)
		return (1);
	return (0);
}

/*
 * used for the one second timer
 */
extern int hz;

void
art_softc_attach(struct device *parent, struct device *self, void *aux)
{
	struct art_softc		*sc = (struct art_softc *)self;
	struct musycc_softc		*psc = (struct musycc_softc *)parent;
	struct musycc_attach_args	*ma = aux;

	printf(" \"%s\"", ma->ma_product);

	if (ebus_attach_device(&sc->art_ebus, psc, ma->ma_base,
	    ma->ma_size) != 0) {
		printf(": can't map framer\n");
		return;
	}

	/* set basic values */
	sc->art_port = ma->ma_port;
	sc->art_slot = ma->ma_slot;
	sc->art_gnum = ma->ma_gnum;
	sc->art_type = ma->ma_flags & 0x03;

	sc->art_channel = musycc_channel_create(self->dv_xname, 1);
	if (sc->art_channel == NULL) {
		printf(": could not alloc channel descriptor\n");
		return;
	}

	if (musycc_channel_attach(psc, sc->art_channel, self, sc->art_gnum) ==
	    -1) {
		printf(": unable to attach to hdlc controller\n");
		return;
	}

	ifmedia_init(&sc->art_ifm, 0, art_ifm_change, art_ifm_status);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_T1, 0, 0), 0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_T1_AMI, 0, 0), 0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_E1, 0, 0), 0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_E1_G704, 0, 0), 0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_E1_G704_CRC4, 0, 0), 0, NULL);

	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_T1, IFM_TDM_MASTER, 0), 0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_T1_AMI, IFM_TDM_MASTER, 0), 0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_E1, IFM_TDM_MASTER, 0), 0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_E1_G704, IFM_TDM_MASTER, 0), 0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_E1_G704_CRC4, IFM_TDM_MASTER, 0),
	    0, NULL);

	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_T1, IFM_TDM_PPP, 0), 0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_E1, IFM_TDM_PPP, 0), 0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_T1_AMI, IFM_TDM_PPP, 0), 0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_E1_G704, IFM_TDM_PPP, 0), 0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_E1_G704_CRC4, IFM_TDM_PPP, 0), 0,
	    NULL);

	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_T1, IFM_TDM_PPP | IFM_TDM_MASTER, 0),
	    0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_E1, IFM_TDM_PPP | IFM_TDM_MASTER, 0),
	    0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_T1_AMI, IFM_TDM_PPP | IFM_TDM_MASTER,
	    0), 0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_E1_G704, IFM_TDM_PPP |
	    IFM_TDM_MASTER, 0), 0, NULL);
	ifmedia_add(&sc->art_ifm,
	    IFM_MAKEWORD(IFM_TDM, IFM_TDM_E1_G704_CRC4, IFM_TDM_PPP |
	    IFM_TDM_MASTER, 0), 0, NULL);

	printf("\n");

	if (bt8370_reset(sc) != 0)
		return;

	/* Initialize timeout for statistics update. */
	timeout_set(&sc->art_onesec, art_onesec, sc);

	ifmedia_set(&sc->art_ifm, IFM_TDM|IFM_TDM_E1_G704_CRC4);
	sc->art_media = sc->art_ifm.ifm_media;

	bt8370_set_frame_mode(sc, sc->art_type, IFM_TDM_E1_G704_CRC4, 0);
	musycc_attach_sppp(sc->art_channel, art_ioctl);

	/* Set linkstate hook to track link state changes done by sppp. */
	sc->art_linkstatehook = hook_establish(
	    sc->art_channel->cc_ifp->if_linkstatehooks, 0, art_linkstate, sc);

	/* Schedule the timeout one second from now. */
	timeout_add_sec(&sc->art_onesec, 1);
}

/* interface ioctl */
int
art_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ifreq		*ifr = (struct ifreq*) data;
	struct channel_softc	*cc = ifp->if_softc;
	struct art_softc	*ac = (struct art_softc *)cc->cc_parent;
	u_int32_t		 tsmap;
	int			 s, rv = 0;

	s = splnet();
	switch (command) {
	case SIOCSIFADDR:
		if ((rv = musycc_init_channel(cc, ac->art_slot)))
			break;
		rv = sppp_ioctl(ifp, command, data);
		break;
	case SIOCSIFTIMESLOT:
		if ((rv = suser(curproc, 0)) != 0)
			break;
		rv = copyin(ifr->ifr_data, &tsmap, sizeof(tsmap));
		if (rv)
			break;
		if (art_mask_tsmap(IFM_SUBTYPE(ac->art_media), tsmap) !=
		    tsmap) {
			rv = EINVAL;
			break;
		}
		if (ac->art_type == ART_SBI_SINGLE &&
		    (IFM_SUBTYPE(ac->art_media) == IFM_TDM_T1 ||
		    IFM_SUBTYPE(ac->art_media) == IFM_TDM_T1_AMI))
			/*
			 * need to adjust timeslot mask for T1 on single port
			 * cards. There timeslot 0-23 are usable not 1-24
			 */
			tsmap >>= 1;

		cc->cc_tslots = tsmap;
		rv = musycc_init_channel(cc, ac->art_slot);
		break;
	case SIOCGIFTIMESLOT:
		tsmap = cc->cc_tslots;
		if (ac->art_type == ART_SBI_SINGLE &&
		    (IFM_SUBTYPE(ac->art_media) == IFM_TDM_T1 ||
		    IFM_SUBTYPE(ac->art_media) == IFM_TDM_T1_AMI))
			tsmap <<= 1;
		rv = copyout(&tsmap, ifr->ifr_data, sizeof(tsmap));
		break;
	case SIOCSIFFLAGS:
		/*
		 * If interface is marked up and not running, then start it.
		 * If it is marked down and running, stop it.
		 */
		if (ifr->ifr_flags & IFF_UP && cc->cc_state != CHAN_RUNNING) {
			if ((rv = musycc_init_channel(cc, ac->art_slot)))
				break;
		} else if ((ifr->ifr_flags & IFF_UP) == 0 &&
		    cc->cc_state == CHAN_RUNNING)
			musycc_stop_channel(cc);
		rv = sppp_ioctl(ifp, command, data);
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		if (ac != NULL)
			rv = ifmedia_ioctl(ifp, ifr, &ac->art_ifm, command);
		else
			rv = EINVAL;
		break;
	default:
		rv = sppp_ioctl(ifp, command, data);
		break;
	}
	splx(s);
	return (rv);
}

int
art_ifm_change(struct ifnet *ifp)
{
	struct channel_softc	*cc = ifp->if_softc;
	struct art_softc	*ac = (struct art_softc *)cc->cc_parent;
	struct ifmedia		*ifm = &ac->art_ifm;
	u_int64_t		baudrate;
	int			rv, s;

	ACCOOM_PRINTF(2, ("%s: art_ifm_change %08x\n", ifp->if_xname,
	    ifm->ifm_media));

	if (IFM_TYPE(ifm->ifm_media) != IFM_TDM)
		return (EINVAL);

	/* OPTIONS (controller mode hdlc, ppp, eoe) */
	if ((rv = art_ifm_options(ifp, cc, IFM_OPTIONS(ifm->ifm_media))) != 0)
		return (rv);

	/* SUBTYPE (framing mode T1/E1) + MODE (clocking master/slave) */
	if (IFM_SUBTYPE(ifm->ifm_media) != IFM_SUBTYPE(ac->art_media) ||
	    IFM_MODE(ifm->ifm_media) != IFM_MODE(ac->art_media)) {
		ACCOOM_PRINTF(0, ("%s: art_ifm_change type %d mode %x\n",
		    ifp->if_xname, IFM_SUBTYPE(ifm->ifm_media),
		    IFM_MODE(ifm->ifm_media)));

		bt8370_set_frame_mode(ac, ac->art_type,
		    IFM_SUBTYPE(ifm->ifm_media), IFM_MODE(ifm->ifm_media));

		if (IFM_SUBTYPE(ifm->ifm_media) != IFM_SUBTYPE(ac->art_media)) {
			/* adjust timeslot map on media change */
			cc->cc_tslots = art_mask_tsmap(
			    IFM_SUBTYPE(ifm->ifm_media), cc->cc_tslots);

			if (ac->art_type == ART_SBI_SINGLE &&
			    (IFM_SUBTYPE(ifm->ifm_media) == IFM_TDM_T1 ||
			     IFM_SUBTYPE(ifm->ifm_media) == IFM_TDM_T1_AMI) &&
			    (IFM_SUBTYPE(ac->art_media) != IFM_TDM_T1 &&
			     IFM_SUBTYPE(ac->art_media) != IFM_TDM_T1_AMI))
				/*
				 * need to adjust timeslot mask for T1 on
				 * single port cards. There timeslot 0-23 are
				 * usable not 1-24
				 */
				cc->cc_tslots >>= 1;
			else if (ac->art_type == ART_SBI_SINGLE &&
			    (IFM_SUBTYPE(ifm->ifm_media) != IFM_TDM_T1 &&
			     IFM_SUBTYPE(ifm->ifm_media) != IFM_TDM_T1_AMI) &&
			    (IFM_SUBTYPE(ac->art_media) == IFM_TDM_T1 ||
			     IFM_SUBTYPE(ac->art_media) == IFM_TDM_T1_AMI))
				/* undo the last adjustment */
				cc->cc_tslots <<= 1;
		}

		/* re-init the card */
		if ((rv = musycc_init_channel(cc, ac->art_slot)))
			return (rv);
	}

	baudrate = ifmedia_baudrate(ac->art_media);
	if (baudrate != ifp->if_baudrate) {
		ifp->if_baudrate = baudrate;
		s = splsoftnet();
		if_link_state_change(ifp);
		splx(s);
	}

	ac->art_media = ifm->ifm_media;

	return (0);
}

void
art_ifm_status(struct ifnet *ifp, struct ifmediareq *ifmreq)
{
	struct art_softc	*ac;

	ac = (struct art_softc *)
	    ((struct channel_softc *)ifp->if_softc)->cc_parent;
	ifmreq->ifm_status = IFM_AVALID;
	if (LINK_STATE_IS_UP(ifp->if_link_state))
		ifmreq->ifm_status |= IFM_ACTIVE;
	ifmreq->ifm_active = ac->art_media;

	return;
}

int
art_ifm_options(struct ifnet *ifp, struct channel_softc *cc, u_int options)
{
	struct art_softc	*ac = (struct art_softc *)cc->cc_parent;
	u_int			 flags = cc->cc_ppp.pp_flags;
	int			 rv;

	if (options == IFM_TDM_PPP) {
		flags &= ~PP_CISCO;
		flags |= PP_KEEPALIVE;
	} else if (options == 0) {
		flags |= PP_CISCO;
		flags |= PP_KEEPALIVE;
	} else {
		ACCOOM_PRINTF(0, ("%s: Unsupported ifmedia options\n",
		    ifp->if_xname));
		return (EINVAL);
	}
	if (flags != cc->cc_ppp.pp_flags) {
		musycc_stop_channel(cc);
		cc->cc_ppp.pp_flags = flags;
		if ((rv = musycc_init_channel(cc, ac->art_slot)))
			return (rv);
		return (sppp_ioctl(ifp, SIOCSIFFLAGS, NULL));
	}
	return (0);
}

void
art_onesec(void *arg)
{
	struct art_softc	*ac = arg;
	struct ifnet		*ifp = ac->art_channel->cc_ifp;
	struct sppp		*ppp = &ac->art_channel->cc_ppp;
	int			 s, rv, link_state;

	rv = bt8370_link_status(ac);
	switch (rv) {
	case 1:
		link_state = LINK_STATE_UP;
		/* set green led */
		ebus_set_led(ac->art_channel, 1, MUSYCC_LED_GREEN);
		break;
	case 0:
		link_state = LINK_STATE_DOWN;
		/* set green led and red led as well */
		ebus_set_led(ac->art_channel, 1,
		    MUSYCC_LED_GREEN | MUSYCC_LED_RED);
		break;
	default:
		link_state = LINK_STATE_DOWN;
		/* turn green led off */
		ebus_set_led(ac->art_channel, 0, MUSYCC_LED_GREEN);
		break;
	}

	if (link_state != ifp->if_link_state) {
		s = splsoftnet();
		if (LINK_STATE_IS_UP(link_state))
			ppp->pp_up(ppp);
		else
			ppp->pp_down(ppp);
		splx(s);
	}

	/*
	 * run musycc onesec job
	 */
	musycc_tick(ac->art_channel);

	/*
	 * Schedule another timeout one second from now.
	 */
	timeout_add_sec(&ac->art_onesec, 1);
}

void
art_linkstate(void *arg)
{
	struct art_softc	*ac = arg;
	struct ifnet		*ifp = ac->art_channel->cc_ifp;

	if (LINK_STATE_IS_UP(ifp->if_link_state))
		/* turn red led off */
		ebus_set_led(ac->art_channel, 0, MUSYCC_LED_RED);
	else
		/* turn red led on */
		ebus_set_led(ac->art_channel, 1, MUSYCC_LED_RED);
}

u_int32_t
art_mask_tsmap(u_int mode, u_int32_t tsmap)
{
	switch (mode) {
	case IFM_TDM_E1:
	case IFM_TDM_E1_G704:
	case IFM_TDM_E1_G704_CRC4:
		return (tsmap & ART_E1_MASK);
	case IFM_TDM_T1_AMI:
	case IFM_TDM_T1:
		return (tsmap & ART_T1_MASK);
	default:
		return (tsmap);
	}
}
