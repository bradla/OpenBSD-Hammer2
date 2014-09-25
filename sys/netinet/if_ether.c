/*	$OpenBSD: if_ether.c,v 1.125 2014/04/14 09:06:42 mpi Exp $	*/
/*	$NetBSD: if_ether.c,v 1.31 1996/05/11 12:59:58 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)if_ether.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Ethernet address resolution protocol.
 * TODO:
 *	add "inuse/lock" bit (or ref. count) along with valid bit
 */

#ifdef INET
#include "carp.h"

#include "bridge.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#if NCARP > 0
#include <netinet/ip_carp.h>
#endif
#if NBRIDGE > 0
#include <net/if_bridge.h>
#endif

#define SDL(s) ((struct sockaddr_dl *)s)
#define SRP(s) ((struct sockaddr_inarp *)s)

/*
 * ARP trailer negotiation.  Trailer protocol is not IP specific,
 * but ARP request/response use IP addresses.
 */
#define ETHERTYPE_IPTRAILERS ETHERTYPE_TRAIL

/* timer values */
int	arpt_prune = (5*60*1);	/* walk list every 5 minutes */
int	arpt_keep = (20*60);	/* once resolved, good for 20 more minutes */
int	arpt_down = 20;		/* once declared down, don't send for 20 secs */
#define	rt_expire rt_rmx.rmx_expire

void arptfree(struct llinfo_arp *);
void arptimer(void *);
struct llinfo_arp *arplookup(u_int32_t, int, int, u_int);
void in_arpinput(struct mbuf *);

LIST_HEAD(, llinfo_arp) llinfo_arp;
struct	ifqueue arpintrq;
int	arp_inuse, arp_allocated;
int	arp_maxtries = 5;
int	useloopback = 1;	/* use loopback interface for local traffic */
int	arpinit_done;
int	la_hold_total;

#ifdef NFSCLIENT
/* revarp state */
struct in_addr revarp_myip, revarp_srvip;
int revarp_finished;
int revarp_in_progress;
struct ifnet *revarp_ifp;
#endif /* NFSCLIENT */

#ifdef DDB

void	db_print_sa(struct sockaddr *);
void	db_print_ifa(struct ifaddr *);
void	db_print_llinfo(caddr_t);
int	db_show_radix_node(struct radix_node *, void *, u_int);
#endif

static const struct sockaddr_dl null_sdl = { sizeof(null_sdl), AF_LINK };

/*
 * Timeout routine.  Age arp_tab entries periodically.
 */
/* ARGSUSED */
void
arptimer(void *arg)
{
	struct timeout *to = (struct timeout *)arg;
	int s;
	struct llinfo_arp *la, *nla;

	s = splsoftnet();
	timeout_add_sec(to, arpt_prune);
	for (la = LIST_FIRST(&llinfo_arp); la != NULL; la = nla) {
		struct rtentry *rt = la->la_rt;

		nla = LIST_NEXT(la, la_list);
		if (rt->rt_expire && rt->rt_expire <= time_second)
			arptfree(la); /* timer has expired; clear */
	}
	splx(s);
}

void
arp_rtrequest(int req, struct rtentry *rt)
{
	struct sockaddr *gate = rt->rt_gateway;
	struct llinfo_arp *la = (struct llinfo_arp *)rt->rt_llinfo;
	struct ifnet *ifp = rt->rt_ifp;
	struct ifaddr *ifa;
	struct mbuf *m;

	if (!arpinit_done) {
		static struct timeout arptimer_to;

		arpinit_done = 1;
		IFQ_SET_MAXLEN(&arpintrq, 50);	/* XXX hate magic numbers */
		/*
		 * We generate expiration times from time.tv_sec
		 * so avoid accidently creating permanent routes.
		 */
		if (time_second == 0) {
			time_second++;
		}

		timeout_set(&arptimer_to, arptimer, &arptimer_to);
		timeout_add_sec(&arptimer_to, 1);
	}

	if (rt->rt_flags & RTF_GATEWAY)
		return;

	switch (req) {

	case RTM_ADD:
		/*
		 * XXX: If this is a manually added route to interface
		 * such as older version of routed or gated might provide,
		 * restore cloning bit.
		 */
		if ((rt->rt_flags & RTF_HOST) == 0 && rt_mask(rt) &&
		    satosin(rt_mask(rt))->sin_addr.s_addr != 0xffffffff)
			rt->rt_flags |= RTF_CLONING;
		if (rt->rt_flags & RTF_CLONING) {
			/*
			 * Case 1: This route should come from a route to iface.
			 */
			rt_setgate(rt, rt_key(rt), (struct sockaddr *)&null_sdl,
			    ifp->if_rdomain);
			gate = rt->rt_gateway;
			SDL(gate)->sdl_type = ifp->if_type;
			SDL(gate)->sdl_index = ifp->if_index;
			/*
			 * Give this route an expiration time, even though
			 * it's a "permanent" route, so that routes cloned
			 * from it do not need their expiration time set.
			 */
			rt->rt_expire = time_second;
			break;
		}
		/* Announce a new entry if requested. */
		if (rt->rt_flags & RTF_ANNOUNCE)
			arprequest(ifp,
			    &satosin(rt_key(rt))->sin_addr.s_addr,
			    &satosin(rt_key(rt))->sin_addr.s_addr,
			    (u_char *)LLADDR(SDL(gate)));
		/*FALLTHROUGH*/
	case RTM_RESOLVE:
		if (gate->sa_family != AF_LINK ||
		    gate->sa_len < sizeof(struct sockaddr_dl)) {
			log(LOG_DEBUG, "%s: bad gateway value: %s\n", __func__,
			    ifp->if_xname);
			break;
		}
		SDL(gate)->sdl_type = ifp->if_type;
		SDL(gate)->sdl_index = ifp->if_index;
		if (la != 0)
			break; /* This happens on a route change */
		/*
		 * Case 2:  This route may come from cloning, or a manual route
		 * add with a LL address.
		 */
		la = malloc(sizeof(*la), M_RTABLE, M_NOWAIT | M_ZERO);
		rt->rt_llinfo = (caddr_t)la;
		if (la == NULL) {
			log(LOG_DEBUG, "%s: malloc failed\n", __func__);
			break;
		}
		arp_inuse++;
		arp_allocated++;
		la->la_rt = rt;
		rt->rt_flags |= RTF_LLINFO;
		LIST_INSERT_HEAD(&llinfo_arp, la, la_list);

		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if ((ifa->ifa_addr->sa_family == AF_INET) &&
			    ifatoia(ifa)->ia_addr.sin_addr.s_addr ==
			    satosin(rt_key(rt))->sin_addr.s_addr)
				break;
		}
		if (ifa) {
			/*
			 * This test used to be
			 *	if (lo0ifp->if_flags & IFF_UP)
			 * It allowed local traffic to be forced through
			 * the hardware by configuring the loopback down.
			 * However, it causes problems during network
			 * configuration for boards that can't receive
			 * packets they send.  It is now necessary to clear
			 * "useloopback" and remove the route to force
			 * traffic out to the hardware.
			 *
			 * In 4.4BSD, the above "if" statement checked
			 * rt->rt_ifa against rt_key(rt).  It was changed
			 * to the current form so that we can provide a
			 * better support for multiple IPv4 addresses on a
			 * interface.
			 */
			rt->rt_expire = 0;
			SDL(gate)->sdl_alen = ETHER_ADDR_LEN;
			memcpy(LLADDR(SDL(gate)),
			    ((struct arpcom *)ifp)->ac_enaddr, ETHER_ADDR_LEN);
			if (useloopback)
				rt->rt_ifp = lo0ifp;
			/*
			 * make sure to set rt->rt_ifa to the interface
			 * address we are using, otherwise we will have trouble
			 * with source address selection.
			 */
			if (ifa != rt->rt_ifa) {
				ifafree(rt->rt_ifa);
				ifa->ifa_refcnt++;
				rt->rt_ifa = ifa;
			}
		}
		break;

	case RTM_DELETE:
		if (la == 0)
			break;
		arp_inuse--;
		LIST_REMOVE(la, la_list);
		rt->rt_llinfo = 0;
		rt->rt_flags &= ~RTF_LLINFO;
		while ((m = la->la_hold_head) != NULL) {
			la->la_hold_head = la->la_hold_head->m_nextpkt;
			la_hold_total--;
			m_freem(m);
		}
		free(la, M_RTABLE);
	}
}

/*
 * Broadcast an ARP request. Caller specifies:
 *	- arp header source ip address
 *	- arp header target ip address
 *	- arp header source ethernet address
 */
void
arprequest(struct ifnet *ifp, u_int32_t *sip, u_int32_t *tip, u_int8_t *enaddr)
{
	struct mbuf *m;
	struct ether_header *eh;
	struct ether_arp *ea;
	struct sockaddr sa;

	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
		return;
	m->m_len = sizeof(*ea);
	m->m_pkthdr.len = sizeof(*ea);
	m->m_pkthdr.ph_rtableid = ifp->if_rdomain;
	MH_ALIGN(m, sizeof(*ea));
	ea = mtod(m, struct ether_arp *);
	eh = (struct ether_header *)sa.sa_data;
	memset(ea, 0, sizeof(*ea));
	memcpy(eh->ether_dhost, etherbroadcastaddr, sizeof(eh->ether_dhost));
	eh->ether_type = htons(ETHERTYPE_ARP);	/* if_output will not swap */
	ea->arp_hrd = htons(ARPHRD_ETHER);
	ea->arp_pro = htons(ETHERTYPE_IP);
	ea->arp_hln = sizeof(ea->arp_sha);	/* hardware address length */
	ea->arp_pln = sizeof(ea->arp_spa);	/* protocol address length */
	ea->arp_op = htons(ARPOP_REQUEST);
	memcpy(eh->ether_shost, enaddr, sizeof(eh->ether_shost));
	memcpy(ea->arp_sha, enaddr, sizeof(ea->arp_sha));
	memcpy(ea->arp_spa, sip, sizeof(ea->arp_spa));
	memcpy(ea->arp_tpa, tip, sizeof(ea->arp_tpa));
	sa.sa_family = pseudo_AF_HDRCMPLT;
	sa.sa_len = sizeof(sa);
	m->m_flags |= M_BCAST;
	(*ifp->if_output)(ifp, m, &sa, (struct rtentry *)0);
}

/*
 * Resolve an IP address into an ethernet address.  If success,
 * desten is filled in.  If there is no entry in arptab,
 * set one up and broadcast a request for the IP address.
 * Hold onto this mbuf and resend it once the address
 * is finally resolved.  A return value of 1 indicates
 * that desten has been filled in and the packet should be sent
 * normally; a 0 return indicates that the packet has been
 * taken over here, either now or for later transmission.
 */
int
arpresolve(struct arpcom *ac, struct rtentry *rt, struct mbuf *m,
    struct sockaddr *dst, u_char *desten)
{
	struct llinfo_arp *la;
	struct sockaddr_dl *sdl;
	struct mbuf *mh;
	char addr[INET_ADDRSTRLEN];

	if (m->m_flags & M_BCAST) {	/* broadcast */
		memcpy(desten, etherbroadcastaddr, sizeof(etherbroadcastaddr));
		return (1);
	}
	if (m->m_flags & M_MCAST) {	/* multicast */
		ETHER_MAP_IP_MULTICAST(&satosin(dst)->sin_addr, desten);
		return (1);
	}
	if (rt) {
		la = (struct llinfo_arp *)rt->rt_llinfo;
		if (la == NULL)
			log(LOG_DEBUG, "arpresolve: %s: route without link "
			    "local address\n", inet_ntop(AF_INET,
				&satosin(dst)->sin_addr, addr, sizeof(addr)));
	} else {
		if ((la = arplookup(satosin(dst)->sin_addr.s_addr, RT_REPORT, 0,
		    ac->ac_if.if_rdomain)) != NULL)
			rt = la->la_rt;
		else
			log(LOG_DEBUG,
			    "arpresolve: %s: can't allocate llinfo\n",
			    inet_ntop(AF_INET, &satosin(dst)->sin_addr,
				addr, sizeof(addr)));
	}
	if (la == 0 || rt == 0) {
		m_freem(m);
		return (0);
	}
	sdl = SDL(rt->rt_gateway);
	/*
	 * Check the address family and length is valid, the address
	 * is resolved; otherwise, try to resolve.
	 */
	if ((rt->rt_expire == 0 || rt->rt_expire > time_second) &&
	    sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0) {
		memcpy(desten, LLADDR(sdl), sdl->sdl_alen);
		return 1;
	}
	if (((struct ifnet *)ac)->if_flags & IFF_NOARP) {
		m_freem(m);
		return 0;
	}

	/*
	 * There is an arptab entry, but no ethernet address
	 * response yet. Insert mbuf in hold queue if below limit
	 * if above the limit free the queue without queuing the new packet.
	 */
	if (la_hold_total < MAX_HOLD_TOTAL && la_hold_total < nmbclust / 64) {
		if (la->la_hold_count >= MAX_HOLD_QUEUE) {
			mh = la->la_hold_head;
			la->la_hold_head = la->la_hold_head->m_nextpkt;
			if (mh == la->la_hold_tail)
				la->la_hold_tail = NULL;
			la->la_hold_count--;
			la_hold_total--;
			m_freem(mh);
		}
		if (la->la_hold_tail == NULL)
			la->la_hold_head = m;
		else
			la->la_hold_tail->m_nextpkt = m;
		la->la_hold_tail = m;
		la->la_hold_count++;
		la_hold_total++;
	} else {
		while ((mh = la->la_hold_head) != NULL) {
			la->la_hold_head =
			    la->la_hold_head->m_nextpkt;
			la_hold_total--;
			m_freem(mh);
		}
		la->la_hold_tail = NULL;
		la->la_hold_count = 0;
		m_freem(m);
	}

	/*
	 * Re-send the ARP request when appropriate.
	 */
#ifdef	DIAGNOSTIC
	if (rt->rt_expire == 0) {
		/* This should never happen. (Should it? -gwr) */
		printf("arpresolve: unresolved and rt_expire == 0\n");
		/* Set expiration time to now (expired). */
		rt->rt_expire = time_second;
	}
#endif
	if (rt->rt_expire) {
		rt->rt_flags &= ~RTF_REJECT;
		if (la->la_asked == 0 || rt->rt_expire != time_second) {
			rt->rt_expire = time_second;
			if (la->la_asked++ < arp_maxtries)
				arprequest(&ac->ac_if,
				    &satosin(rt->rt_ifa->ifa_addr)->sin_addr.s_addr,
				    &satosin(dst)->sin_addr.s_addr,
#if NCARP > 0
				    (rt->rt_ifp->if_type == IFT_CARP) ?
					((struct arpcom *) rt->rt_ifp->if_softc
					)->ac_enaddr :
#endif
				    ac->ac_enaddr);
			else {
				rt->rt_flags |= RTF_REJECT;
				rt->rt_expire += arpt_down;
				la->la_asked = 0;
				while ((mh = la->la_hold_head) != NULL) {
					la->la_hold_head =
					    la->la_hold_head->m_nextpkt;
					la_hold_total--;
					m_freem(mh);
				}
				la->la_hold_tail = NULL;
				la->la_hold_count = 0;
			}
		}
	}
	return (0);
}

/*
 * Common length and type checks are done here,
 * then the protocol-specific routine is called.
 */
void
arpintr(void)
{
	struct mbuf *m;
	struct arphdr *ar;
	int s, len;

	for (;;) {
		s = splnet();
		IF_DEQUEUE(&arpintrq, m);
		splx(s);
		if (m == NULL)
			break;
#ifdef DIAGNOSTIC
		if ((m->m_flags & M_PKTHDR) == 0)
			panic("arpintr");
#endif

		len = sizeof(struct arphdr);
		if (m->m_len < len && (m = m_pullup(m, len)) == NULL)
			continue;

		ar = mtod(m, struct arphdr *);
		if (ntohs(ar->ar_hrd) != ARPHRD_ETHER) {
			m_freem(m);
			continue;
		}

		len += 2 * (ar->ar_hln + ar->ar_pln);
		if (m->m_len < len && (m = m_pullup(m, len)) == NULL)
			continue;

		switch (ntohs(ar->ar_pro)) {
		case ETHERTYPE_IP:
		case ETHERTYPE_IPTRAILERS:
			in_arpinput(m);
			continue;
		}
		m_freem(m);
	}
}

/*
 * ARP for Internet protocols on Ethernet.
 * Algorithm is that given in RFC 826.
 * In addition, a sanity check is performed on the sender
 * protocol address, to catch impersonators.
 * We no longer handle negotiations for use of trailer protocol:
 * Formerly, ARP replied for protocol type ETHERTYPE_TRAIL sent
 * along with IP replies if we wanted trailers sent to us,
 * and also sent them in response to IP replies.
 * This allowed either end to announce the desire to receive
 * trailer packets.
 * We no longer reply to requests for ETHERTYPE_TRAIL protocol either,
 * but formerly didn't normally send requests.
 */
void
in_arpinput(struct mbuf *m)
{
	struct ether_arp *ea;
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct arpcom *ac = (struct arpcom *)ifp;
	struct ether_header *eh;
	struct llinfo_arp *la = 0;
	struct rtentry *rt;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
	struct sockaddr sa;
	struct in_addr isaddr, itaddr, myaddr;
	struct mbuf *mh, *mt;
	u_int8_t *enaddr = NULL;
#if NCARP > 0
	u_int8_t *ether_shost = NULL;
#endif
	char addr[INET_ADDRSTRLEN];
	int op;

	ea = mtod(m, struct ether_arp *);
	op = ntohs(ea->arp_op);
	if ((op != ARPOP_REQUEST) && (op != ARPOP_REPLY))
		goto out;
#if notyet
	if ((op == ARPOP_REPLY) && (m->m_flags & (M_BCAST|M_MCAST))) {
		log(LOG_ERR,
		    "arp: received reply to broadcast or multicast address\n");
		goto out;
	}
#endif

	memcpy(&itaddr, ea->arp_tpa, sizeof(itaddr));
	memcpy(&isaddr, ea->arp_spa, sizeof(isaddr));

	/* First try: check target against our addresses */
	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		if (itaddr.s_addr != ifatoia(ifa)->ia_addr.sin_addr.s_addr)
			continue;

#if NCARP > 0
		if (ifp->if_type == IFT_CARP &&
		    ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) ==
		    (IFF_UP|IFF_RUNNING))) {
			if (op == ARPOP_REPLY)
				break;
			if (carp_iamatch(ifatoia(ifa), ea->arp_sha,
			    &enaddr, &ether_shost))
				break;
			else
				goto out;
		}
#endif
		break;
	}

	/* Second try: check source against our addresses */
	if (ifa == NULL) {
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;

			if (isaddr.s_addr ==
			    ifatoia(ifa)->ia_addr.sin_addr.s_addr)
				break;
		}
	}

	/* Third try: not one of our addresses, just find an usable ia */
	if (ifa == NULL) {
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family == AF_INET)
				break;
		}
	}

	if (ifa == NULL)
		goto out;

	if (!enaddr)
		enaddr = ac->ac_enaddr;
	myaddr = ifatoia(ifa)->ia_addr.sin_addr;

	if (!memcmp(ea->arp_sha, enaddr, sizeof(ea->arp_sha)))
		goto out;	/* it's from me, ignore it. */
	if (ETHER_IS_MULTICAST(&ea->arp_sha[0]))
		if (!memcmp(ea->arp_sha, etherbroadcastaddr,
		    sizeof (ea->arp_sha))) {
			inet_ntop(AF_INET, &isaddr, addr, sizeof(addr));
			log(LOG_ERR, "arp: ether address is broadcast for "
			    "IP address %s!\n", addr);
			goto out;
		}
	if (myaddr.s_addr && isaddr.s_addr == myaddr.s_addr) {
		inet_ntop(AF_INET, &isaddr, addr, sizeof(addr));
		log(LOG_ERR,
		   "duplicate IP address %s sent from ethernet address %s\n",
		   addr, ether_sprintf(ea->arp_sha));
		itaddr = myaddr;
		goto reply;
	}
	la = arplookup(isaddr.s_addr, itaddr.s_addr == myaddr.s_addr, 0,
	    rtable_l2(m->m_pkthdr.ph_rtableid));
	if (la && (rt = la->la_rt) && (sdl = SDL(rt->rt_gateway))) {
		if (sdl->sdl_alen) {
		    if (memcmp(ea->arp_sha, LLADDR(sdl), sdl->sdl_alen)) {
			if (rt->rt_flags & RTF_PERMANENT_ARP) {
				inet_ntop(AF_INET, &isaddr, addr, sizeof(addr));
				log(LOG_WARNING,
				   "arp: attempt to overwrite permanent "
				   "entry for %s by %s on %s\n", addr,
				   ether_sprintf(ea->arp_sha),
				   ac->ac_if.if_xname);
				goto out;
			} else if (rt->rt_ifp != &ac->ac_if) {
#if NCARP > 0
				if (ac->ac_if.if_type != IFT_CARP)
#endif
				{
					inet_ntop(AF_INET, &isaddr,
					    addr, sizeof(addr));
					log(LOG_WARNING,
					   "arp: attempt to overwrite entry for"
					   " %s on %s by %s on %s\n", addr,
					   rt->rt_ifp->if_xname,
					   ether_sprintf(ea->arp_sha),
					   ac->ac_if.if_xname);
				}
				goto out;
			} else {
				inet_ntop(AF_INET, &isaddr, addr, sizeof(addr));
				log(LOG_INFO,
				   "arp info overwritten for %s by %s on %s\n",
				   addr,
				   ether_sprintf(ea->arp_sha),
				   ac->ac_if.if_xname);
				rt->rt_expire = 1; /* no longer static */
			}
		    }
		} else if (rt->rt_ifp != &ac->ac_if &&
#if NBRIDGE > 0
		    !SAME_BRIDGE(ac->ac_if.if_bridgeport,
		    rt->rt_ifp->if_bridgeport) &&
#endif
#if NCARP > 0
		    !(rt->rt_ifp->if_type == IFT_CARP &&
		    rt->rt_ifp->if_carpdev == &ac->ac_if) &&
		    !(ac->ac_if.if_type == IFT_CARP &&
		    ac->ac_if.if_carpdev == rt->rt_ifp) &&
#endif
		    1) {
			inet_ntop(AF_INET, &isaddr, addr, sizeof(addr));
			log(LOG_WARNING,
			    "arp: attempt to add entry for %s "
			    "on %s by %s on %s\n", addr,
			    rt->rt_ifp->if_xname,
			    ether_sprintf(ea->arp_sha),
			    ac->ac_if.if_xname);
			goto out;
		}
 		sdl->sdl_alen = sizeof(ea->arp_sha);
		memcpy(LLADDR(sdl), ea->arp_sha, sizeof(ea->arp_sha));
		if (rt->rt_expire)
			rt->rt_expire = time_second + arpt_keep;
		rt->rt_flags &= ~RTF_REJECT;
		la->la_asked = 0;
		rt_sendmsg(rt, RTM_RESOLVE, rt->rt_ifp->if_rdomain);
		while ((mh = la->la_hold_head) != NULL) {
			if ((la->la_hold_head = mh->m_nextpkt) == NULL)
				la->la_hold_tail = NULL;
			la->la_hold_count--;
			la_hold_total--;
			mt = la->la_hold_tail;

			(*ac->ac_if.if_output)(&ac->ac_if, mh, rt_key(rt), rt);

			if (la->la_hold_tail == mh) {
				/* mbuf is back in queue. Discard. */
				la->la_hold_tail = mt;
				if (la->la_hold_tail)
					la->la_hold_tail->m_nextpkt = NULL;
				else
					la->la_hold_head = NULL;
				la->la_hold_count--;
				la_hold_total--;
				m_freem(mh);
			}
		}
	}
reply:
	if (op != ARPOP_REQUEST) {
out:
		m_freem(m);
		return;
	}
	if (itaddr.s_addr == myaddr.s_addr) {
		/* I am the target */
		memcpy(ea->arp_tha, ea->arp_sha, sizeof(ea->arp_sha));
		memcpy(ea->arp_sha, enaddr, sizeof(ea->arp_sha));
	} else {
		la = arplookup(itaddr.s_addr, 0, SIN_PROXY,
		    rtable_l2(m->m_pkthdr.ph_rtableid));
		if (la == 0)
			goto out;
		rt = la->la_rt;
		if (rt->rt_ifp->if_type == IFT_CARP && ifp->if_type != IFT_CARP)
			goto out;
		memcpy(ea->arp_tha, ea->arp_sha, sizeof(ea->arp_sha));
		sdl = SDL(rt->rt_gateway);
		memcpy(ea->arp_sha, LLADDR(sdl), sizeof(ea->arp_sha));
	}

	memcpy(ea->arp_tpa, ea->arp_spa, sizeof(ea->arp_spa));
	memcpy(ea->arp_spa, &itaddr, sizeof(ea->arp_spa));
	ea->arp_op = htons(ARPOP_REPLY);
	ea->arp_pro = htons(ETHERTYPE_IP); /* let's be sure! */
	eh = (struct ether_header *)sa.sa_data;
	memcpy(eh->ether_dhost, ea->arp_tha, sizeof(eh->ether_dhost));
#if NCARP > 0
	if (ether_shost)
		enaddr = ether_shost;
#endif
	memcpy(eh->ether_shost, enaddr, sizeof(eh->ether_shost));

	eh->ether_type = htons(ETHERTYPE_ARP);
	sa.sa_family = pseudo_AF_HDRCMPLT;
	sa.sa_len = sizeof(sa);
	(*ac->ac_if.if_output)(&ac->ac_if, m, &sa, (struct rtentry *)0);
	return;
}

/*
 * Free an arp entry.
 */
void
arptfree(struct llinfo_arp *la)
{
	struct rtentry *rt = la->la_rt;
	struct sockaddr_dl *sdl;
	u_int tid = 0;

	if (rt == NULL)
		panic("arptfree");
	if (rt->rt_refcnt > 0 && (sdl = SDL(rt->rt_gateway)) &&
	    sdl->sdl_family == AF_LINK) {
		sdl->sdl_alen = 0;
		la->la_asked = 0;
		rt->rt_flags &= ~RTF_REJECT;
		return;
	}

	if (rt->rt_ifp)
		tid = rt->rt_ifp->if_rdomain;

	rtdeletemsg(rt, tid);
}

/*
 * Lookup or enter a new address in arptab.
 */
struct llinfo_arp *
arplookup(u_int32_t addr, int create, int proxy, u_int tableid)
{
	struct rtentry *rt;
	struct sockaddr_inarp sin;

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	sin.sin_other = proxy ? SIN_PROXY : 0;
	rt = rtalloc1((struct sockaddr *)&sin, create, tableid);
	if (rt == 0)
		return (0);
	rt->rt_refcnt--;
	if ((rt->rt_flags & RTF_GATEWAY) || (rt->rt_flags & RTF_LLINFO) == 0 ||
	    rt->rt_gateway->sa_family != AF_LINK) {
		if (create) {
			if (rt->rt_refcnt <= 0 &&
			    (rt->rt_flags & RTF_CLONED) != 0) {
				struct rt_addrinfo info;

				memset(&info, 0, sizeof(info));
				info.rti_info[RTAX_DST] = rt_key(rt);
				info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
				info.rti_info[RTAX_NETMASK] = rt_mask(rt);

				rtrequest1(RTM_DELETE, &info, rt->rt_priority,
				    NULL, tableid);
			}
		}
		return (0);
	}
	return ((struct llinfo_arp *)rt->rt_llinfo);
}

/*
 * Check whether we do proxy ARP for this address and we point to ourselves.
 */
int
arpproxy(struct in_addr in, u_int rdomain)
{
	struct llinfo_arp *la;
	struct ifnet *ifp;
	int found = 0;

	la = arplookup(in.s_addr, 0, SIN_PROXY, rdomain);
	if (la == NULL)
		return (0);

	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_rdomain != rdomain)
			continue;

		if (!memcmp(LLADDR((struct sockaddr_dl *)la->la_rt->rt_gateway),
		    LLADDR(ifp->if_sadl),
		    ETHER_ADDR_LEN)) {
		    	found = 1;
		    	break;
		}
	}

	return (found);
}

void
arp_ifinit(struct arpcom *ac, struct ifaddr *ifa)
{

	/* Warn the user if another station has this IP address. */
	arprequest(&ac->ac_if,
	    &satosin(ifa->ifa_addr)->sin_addr.s_addr,
	    &satosin(ifa->ifa_addr)->sin_addr.s_addr,
	    ac->ac_enaddr);
	ifa->ifa_rtrequest = arp_rtrequest;
}

/*
 * Called from Ethernet interrupt handlers
 * when ether packet type ETHERTYPE_REVARP
 * is received.  Common length and type checks are done here,
 * then the protocol-specific routine is called.
 */
void
revarpinput(struct mbuf *m)
{
	struct arphdr *ar;

	if (m->m_len < sizeof(struct arphdr))
		goto out;
	ar = mtod(m, struct arphdr *);
	if (ntohs(ar->ar_hrd) != ARPHRD_ETHER)
		goto out;
	if (m->m_len < sizeof(struct arphdr) + 2 * (ar->ar_hln + ar->ar_pln))
		goto out;
	switch (ntohs(ar->ar_pro)) {

	case ETHERTYPE_IP:
	case ETHERTYPE_IPTRAILERS:
		in_revarpinput(m);
		return;

	default:
		break;
	}
out:
	m_freem(m);
}

/*
 * RARP for Internet protocols on Ethernet.
 * Algorithm is that given in RFC 903.
 * We are only using for bootstrap purposes to get an ip address for one of
 * our interfaces.  Thus we support no user-interface.
 *
 * Since the contents of the RARP reply are specific to the interface that
 * sent the request, this code must ensure that they are properly associated.
 *
 * Note: also supports ARP via RARP packets, per the RFC.
 */
void
in_revarpinput(struct mbuf *m)
{
#ifdef NFSCLIENT
	struct ifnet *ifp;
#endif /* NFSCLIENT */
	struct ether_arp *ar;
	int op;

	ar = mtod(m, struct ether_arp *);
	op = ntohs(ar->arp_op);
	switch (op) {
	case ARPOP_REQUEST:
	case ARPOP_REPLY:	/* per RFC */
		in_arpinput(m);
		return;
	case ARPOP_REVREPLY:
		break;
	case ARPOP_REVREQUEST:	/* handled by rarpd(8) */
	default:
		goto out;
	}
#ifdef NFSCLIENT
	if (!revarp_in_progress)
		goto out;
	ifp = m->m_pkthdr.rcvif;
	if (ifp != revarp_ifp) /* !same interface */
		goto out;
	if (revarp_finished)
		goto wake;
	if (memcmp(ar->arp_tha, ((struct arpcom *)ifp)->ac_enaddr,
	    sizeof(ar->arp_tha)))
		goto out;
	memcpy(&revarp_srvip, ar->arp_spa, sizeof(revarp_srvip));
	memcpy(&revarp_myip, ar->arp_tpa, sizeof(revarp_myip));
	revarp_finished = 1;
wake:	/* Do wakeup every time in case it was missed. */
	wakeup((caddr_t)&revarp_myip);
#endif

out:
	m_freem(m);
}

/*
 * Send a RARP request for the ip address of the specified interface.
 * The request should be RFC 903-compliant.
 */
void
revarprequest(struct ifnet *ifp)
{
	struct sockaddr sa;
	struct mbuf *m;
	struct ether_header *eh;
	struct ether_arp *ea;
	struct arpcom *ac = (struct arpcom *)ifp;

	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
		return;
	m->m_len = sizeof(*ea);
	m->m_pkthdr.len = sizeof(*ea);
	MH_ALIGN(m, sizeof(*ea));
	ea = mtod(m, struct ether_arp *);
	eh = (struct ether_header *)sa.sa_data;
	memset(ea, 0, sizeof(*ea));
	memcpy(eh->ether_dhost, etherbroadcastaddr, sizeof(eh->ether_dhost));
	eh->ether_type = htons(ETHERTYPE_REVARP);
	ea->arp_hrd = htons(ARPHRD_ETHER);
	ea->arp_pro = htons(ETHERTYPE_IP);
	ea->arp_hln = sizeof(ea->arp_sha);	/* hardware address length */
	ea->arp_pln = sizeof(ea->arp_spa);	/* protocol address length */
	ea->arp_op = htons(ARPOP_REVREQUEST);
	memcpy(eh->ether_shost, ac->ac_enaddr, sizeof(ea->arp_tha));
	memcpy(ea->arp_sha, ac->ac_enaddr, sizeof(ea->arp_sha));
	memcpy(ea->arp_tha, ac->ac_enaddr, sizeof(ea->arp_tha));
	sa.sa_family = pseudo_AF_HDRCMPLT;
	sa.sa_len = sizeof(sa);
	m->m_flags |= M_BCAST;
	ifp->if_output(ifp, m, &sa, (struct rtentry *)0);
}

#ifdef NFSCLIENT
/*
 * RARP for the ip address of the specified interface, but also
 * save the ip address of the server that sent the answer.
 * Timeout if no response is received.
 */
int
revarpwhoarewe(struct ifnet *ifp, struct in_addr *serv_in,
    struct in_addr *clnt_in)
{
	int result, count = 20;

	if (revarp_finished)
		return EIO;

	revarp_ifp = ifp;
	revarp_in_progress = 1;
	while (count--) {
		revarprequest(ifp);
		result = tsleep((caddr_t)&revarp_myip, PSOCK, "revarp", hz/2);
		if (result != EWOULDBLOCK)
			break;
	}
	revarp_in_progress = 0;
	if (!revarp_finished)
		return ENETUNREACH;

	memcpy(serv_in, &revarp_srvip, sizeof(*serv_in));
	memcpy(clnt_in, &revarp_myip, sizeof(*clnt_in));
	return 0;
}

/* For compatibility: only saves interface address. */
int
revarpwhoami(struct in_addr *in, struct ifnet *ifp)
{
	struct in_addr server;
	return (revarpwhoarewe(ifp, &server, in));
}
#endif /* NFSCLIENT */

#ifdef DDB

#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>

void
db_print_sa(struct sockaddr *sa)
{
	int len;
	u_char *p;

	if (sa == 0) {
		db_printf("[NULL]");
		return;
	}

	p = (u_char *)sa;
	len = sa->sa_len;
	db_printf("[");
	while (len > 0) {
		db_printf("%d", *p);
		p++;
		len--;
		if (len)
			db_printf(",");
	}
	db_printf("]\n");
}

void
db_print_ifa(struct ifaddr *ifa)
{
	if (ifa == 0)
		return;
	db_printf("  ifa_addr=");
	db_print_sa(ifa->ifa_addr);
	db_printf("  ifa_dsta=");
	db_print_sa(ifa->ifa_dstaddr);
	db_printf("  ifa_mask=");
	db_print_sa(ifa->ifa_netmask);
	db_printf("  flags=0x%x, refcnt=%d, metric=%d\n",
	    ifa->ifa_flags, ifa->ifa_refcnt, ifa->ifa_metric);
}

void
db_print_llinfo(caddr_t li)
{
	struct llinfo_arp *la;

	if (li == 0)
		return;
	la = (struct llinfo_arp *)li;
	db_printf("  la_rt=%p la_hold_head=%p, la_asked=0x%lx\n",
	    la->la_rt, la->la_hold_head, la->la_asked);
}

/*
 * Function to pass to rn_walktree().
 * Return non-zero error to abort walk.
 */
int
db_show_radix_node(struct radix_node *rn, void *w, u_int id)
{
	struct rtentry *rt = (struct rtentry *)rn;

	db_printf("rtentry=%p", rt);

	db_printf(" flags=0x%x refcnt=%d use=%llu expire=%lld rtableid=%u\n",
	    rt->rt_flags, rt->rt_refcnt, rt->rt_use, rt->rt_expire, id);

	db_printf(" key="); db_print_sa(rt_key(rt));
	db_printf(" mask="); db_print_sa(rt_mask(rt));
	db_printf(" gw="); db_print_sa(rt->rt_gateway);

	db_printf(" ifp=%p ", rt->rt_ifp);
	if (rt->rt_ifp)
		db_printf("(%s)", rt->rt_ifp->if_xname);
	else
		db_printf("(NULL)");

	db_printf(" ifa=%p\n", rt->rt_ifa);
	db_print_ifa(rt->rt_ifa);

	db_printf(" gwroute=%p llinfo=%p\n", rt->rt_gwroute, rt->rt_llinfo);
	db_print_llinfo(rt->rt_llinfo);
	return (0);
}

/*
 * Function to print all the route trees.
 * Use this from ddb:  "call db_show_arptab"
 */
int
db_show_arptab(void)
{
	struct radix_node_head *rnh;
	rnh = rtable_get(0, AF_INET);
	db_printf("Route tree for AF_INET\n");
	if (rnh == NULL) {
		db_printf(" (not initialized)\n");
		return (0);
	}
	rn_walktree(rnh, db_show_radix_node, NULL);
	return (0);
}
#endif
#endif /* INET */
