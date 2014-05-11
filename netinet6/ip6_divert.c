/*      $OpenBSD: ip6_divert.c,v 1.23 2014/04/28 15:43:04 reyk Exp $ */

/*
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
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
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/pfvar.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_divert.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/icmp6.h>

struct	inpcbtable	divb6table;
struct	div6stat	div6stat;

#ifndef DIVERT_SENDSPACE
#define DIVERT_SENDSPACE	(65536 + 100)
#endif
u_int   divert6_sendspace = DIVERT_SENDSPACE;
#ifndef DIVERT_RECVSPACE
#define DIVERT_RECVSPACE	(65536 + 100)
#endif
u_int   divert6_recvspace = DIVERT_RECVSPACE;

#ifndef DIVERTHASHSIZE
#define DIVERTHASHSIZE	128
#endif

int *divert6ctl_vars[DIVERT6CTL_MAXID] = DIVERT6CTL_VARS;

int divb6hashsize = DIVERTHASHSIZE;

static struct sockaddr_in6 ip6addr = { sizeof(ip6addr), AF_INET6 };

void	divert6_detach(struct inpcb *);
int	divert6_output(struct inpcb *, struct mbuf *, struct mbuf *,
	    struct mbuf *);

void
divert6_init()
{
	in_pcbinit(&divb6table, divb6hashsize);
}

int
divert6_input(struct mbuf **mp, int *offp, int proto)
{
	m_freem(*mp);

	return (0);
}

int
divert6_output(struct inpcb *inp, struct mbuf *m, struct mbuf *nam,
    struct mbuf *control)
{
	struct ifqueue *inq;
	struct sockaddr_in6 *sin6;
	struct socket *so;
	struct ifaddr *ifa;
	int s, error = 0, p_hdrlen = 0, nxt = 0, off;
	struct ip6_hdr *ip6;
	u_int16_t csum = 0;
	size_t p_off = 0;

	m->m_pkthdr.rcvif = NULL;
	m->m_nextpkt = NULL;
	m->m_pkthdr.ph_rtableid = inp->inp_rtableid;

	if (control)
		m_freem(control);

	sin6 = mtod(nam, struct sockaddr_in6 *);
	so = inp->inp_socket;

	/* Do basic sanity checks. */
	if (m->m_pkthdr.len < sizeof(struct ip6_hdr))
		goto fail;
	if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {
		/* m_pullup() has freed the mbuf, so just return. */
		div6stat.divs_errors++;
		return (ENOBUFS);
	}
	ip6 = mtod(m, struct ip6_hdr *);
	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION)
		goto fail;
	if (m->m_pkthdr.len < sizeof(struct ip6_hdr) + ntohs(ip6->ip6_plen))
		goto fail;

	/*
	 * Recalculate the protocol checksum since the userspace application
	 * may have modified the packet prior to reinjection.
	 */
	off = ip6_lasthdr(m, 0, IPPROTO_IPV6, &nxt);
	if (off < sizeof(struct ip6_hdr))
		goto fail;
	switch (nxt) {
	case IPPROTO_TCP:
		p_hdrlen = sizeof(struct tcphdr);
		p_off = offsetof(struct tcphdr, th_sum);
		break;
	case IPPROTO_UDP:
		p_hdrlen = sizeof(struct udphdr);
		p_off = offsetof(struct udphdr, uh_sum);
		break;
	case IPPROTO_ICMPV6:
		p_hdrlen = sizeof(struct icmp6_hdr);
		p_off = offsetof(struct icmp6_hdr, icmp6_cksum);
		break;
	default:
		/* nothing */
		break;
	}
	if (p_hdrlen) {
		if (m->m_pkthdr.len < off + p_hdrlen)
			goto fail;

		if ((error = m_copyback(m, off + p_off, sizeof(csum), &csum, M_NOWAIT)))
			goto fail;
		csum = in6_cksum(m, nxt, off, m->m_pkthdr.len - off);
		if (nxt == IPPROTO_UDP && csum == 0)
			csum = 0xffff;
		if ((error = m_copyback(m, off + p_off, sizeof(csum), &csum, M_NOWAIT)))
			goto fail;
	}

	m->m_pkthdr.pf.flags |= PF_TAG_DIVERTED_PACKET;

	if (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		ip6addr.sin6_addr = sin6->sin6_addr;
		ifa = ifa_ifwithaddr(sin6tosa(&ip6addr),
		    m->m_pkthdr.ph_rtableid);
		if (ifa == NULL) {
			error = EADDRNOTAVAIL;
			goto fail;
		}
		m->m_pkthdr.rcvif = ifa->ifa_ifp;

		inq = &ip6intrq;

		s = splnet();
		IF_INPUT_ENQUEUE(inq, m);
		schednetisr(NETISR_IPV6);
		splx(s);
	} else {
		error = ip6_output(m, NULL, &inp->inp_route6,
		    IP_ALLOWBROADCAST | IP_RAWOUTPUT, NULL, NULL, NULL);
	}

	div6stat.divs_opackets++;
	return (error);

fail:
	div6stat.divs_errors++;
	m_freem(m);
	return (error ? error : EINVAL);
}

int
divert6_packet(struct mbuf *m, int dir)
{
	struct inpcb *inp;
	struct socket *sa = NULL;
	struct sockaddr_in6 addr;
	struct pf_divert *divert;

	inp = NULL;
	div6stat.divs_ipackets++;
	
	if (m->m_len < sizeof(struct ip6_hdr) &&
	    (m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {
		div6stat.divs_errors++;
		return (0);
	}

	divert = pf_find_divert(m);
	if (divert == NULL) {
		div6stat.divs_errors++;
		m_freem(m);
		return (0);
	}

	TAILQ_FOREACH(inp, &divb6table.inpt_queue, inp_queue) {
		if (inp->inp_lport != divert->port)
			continue;
		if (inp->inp_divertfl == 0)
			break;
		if (dir == PF_IN && !(inp->inp_divertfl & IPPROTO_DIVERT_RESP))
			return (-1);
		if (dir == PF_OUT && !(inp->inp_divertfl & IPPROTO_DIVERT_INIT))
			return (-1);
		break;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin6_family = AF_INET6;
	addr.sin6_len = sizeof(addr);

	if (dir == PF_IN) {
		struct ifaddr *ifa;
		struct ifnet *ifp;

		ifp = m->m_pkthdr.rcvif;
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			addr.sin6_addr = satosin6(ifa->ifa_addr)->sin6_addr;
			break;
		}
	}
	/* force checksum calculation */
	if (dir == PF_OUT)
		in6_proto_cksum_out(m, NULL);

	if (inp) {
		sa = inp->inp_socket;
		if (sbappendaddr(&sa->so_rcv, sin6tosa(&addr), m, NULL) == 0) {
			div6stat.divs_fullsock++;
			m_freem(m);
			return (0);
		} else
			sorwakeup(inp->inp_socket);
	}

	if (sa == NULL) {
		div6stat.divs_noport++;
		m_freem(m);
	}
	return (0);
}

/*ARGSUSED*/
int
divert6_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *addr,
    struct mbuf *control, struct proc *p)
{
	struct inpcb *inp = sotoinpcb(so);
	int error = 0;
	int s;

	if (req == PRU_CONTROL) {
		return (in6_control(so, (u_long)m, (caddr_t)addr,
		    (struct ifnet *)control));
	}
	if (inp == NULL && req != PRU_ATTACH) {
		error = EINVAL;
		goto release;
	}
	switch (req) {

	case PRU_ATTACH:
		if (inp != NULL) {
			error = EINVAL;
			break;
		}
		if ((so->so_state & SS_PRIV) == 0) {
			error = EACCES;
			break;
		}
		s = splsoftnet();
		error = in_pcballoc(so, &divb6table);
		splx(s);
		if (error)
			break;

		error = soreserve(so, divert6_sendspace, divert6_recvspace);
		if (error)
			break;
		sotoinpcb(so)->inp_flags |= INP_HDRINCL;
		break;

	case PRU_DETACH:
		divert6_detach(inp);
		break;

	case PRU_BIND:
		s = splsoftnet();
		error = in6_pcbbind(inp, addr, p);
		splx(s);
		break;

	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	case PRU_SEND:
		return (divert6_output(inp, m, addr, control));

	case PRU_ABORT:
		soisdisconnected(so);
		divert6_detach(inp);
		break;

	case PRU_SOCKADDR:
		in6_setsockaddr(inp, addr);
		break;

	case PRU_PEERADDR:
		in6_setpeeraddr(inp, addr);
		break;

	case PRU_SENSE:
		return (0);

	case PRU_LISTEN:
	case PRU_CONNECT:
	case PRU_CONNECT2:
	case PRU_ACCEPT:
	case PRU_DISCONNECT:
	case PRU_SENDOOB:
	case PRU_FASTTIMO:
	case PRU_SLOWTIMO:
	case PRU_PROTORCV:
	case PRU_PROTOSEND:
		error =  EOPNOTSUPP;
		break;

	case PRU_RCVD:
	case PRU_RCVOOB:
		return (EOPNOTSUPP);	/* do not free mbuf's */

	default:
		panic("divert6_usrreq");
	}

release:
	if (control) {
		m_freem(control);
	}
	if (m)
		m_freem(m);
	return (error);
}

void
divert6_detach(struct inpcb *inp)
{
	int s = splsoftnet();

	in_pcbdetach(inp);
	splx(s);
}

/*
 * Sysctl for divert variables.
 */
int
divert6_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case DIVERT6CTL_SENDSPACE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &divert6_sendspace));
	case DIVERT6CTL_RECVSPACE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &divert6_recvspace));
	case DIVERT6CTL_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &div6stat, sizeof(div6stat)));
	default:
		if (name[0] < DIVERT6CTL_MAXID)
			return sysctl_int_arr(divert6ctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen);

		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}
