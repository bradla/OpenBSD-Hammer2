/*	$OpenBSD: ip_input.c,v 1.231 2014/04/21 12:22:26 henning Exp $	*/
/*	$NetBSD: ip_input.c,v 1.30 1996/03/16 23:53:58 christos Exp $	*/

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
 *	@(#)ip_input.c	8.2 (Berkeley) 1/4/94
 */

#include "pf.h"
#include "carp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/pool.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>

#if NPF > 0
#include <net/pfvar.h>
#endif

#ifdef MROUTING
#include <netinet/ip_mroute.h>
#endif

#ifdef IPSEC
#include <netinet/ip_ipsp.h>
#endif /* IPSEC */

#if NCARP > 0
#include <net/if_types.h>
#include <netinet/ip_carp.h>
#endif

struct ipqhead ipq;

int encdebug = 0;
int ipsec_keep_invalid = IPSEC_DEFAULT_EMBRYONIC_SA_TIMEOUT;
int ipsec_require_pfs = IPSEC_DEFAULT_PFS;
int ipsec_soft_allocations = IPSEC_DEFAULT_SOFT_ALLOCATIONS;
int ipsec_exp_allocations = IPSEC_DEFAULT_EXP_ALLOCATIONS;
int ipsec_soft_bytes = IPSEC_DEFAULT_SOFT_BYTES;
int ipsec_exp_bytes = IPSEC_DEFAULT_EXP_BYTES;
int ipsec_soft_timeout = IPSEC_DEFAULT_SOFT_TIMEOUT;
int ipsec_exp_timeout = IPSEC_DEFAULT_EXP_TIMEOUT;
int ipsec_soft_first_use = IPSEC_DEFAULT_SOFT_FIRST_USE;
int ipsec_exp_first_use = IPSEC_DEFAULT_EXP_FIRST_USE;
int ipsec_expire_acquire = IPSEC_DEFAULT_EXPIRE_ACQUIRE;
char ipsec_def_enc[20];
char ipsec_def_auth[20];
char ipsec_def_comp[20];

/* values controllable via sysctl */
int	ipforwarding = 0;
int	ipmforwarding = 0;
int	ipmultipath = 0;
int	ipsendredirects = 1;
int	ip_dosourceroute = 0;
int	ip_defttl = IPDEFTTL;
int	ip_mtudisc = 1;
u_int	ip_mtudisc_timeout = IPMTUDISCTIMEOUT;
int	ip_directedbcast = 0;

struct rttimer_queue *ip_mtudisc_timeout_q = NULL;

/* Keep track of memory used for reassembly */
int	ip_maxqueue = 300;
int	ip_frags = 0;

int *ipctl_vars[IPCTL_MAXID] = IPCTL_VARS;

struct	in_ifaddrhead in_ifaddr;
struct	ifqueue ipintrq;

struct pool ipqent_pool;
struct pool ipq_pool;

struct ipstat ipstat;

void	ip_ours(struct mbuf *);
int	ip_dooptions(struct mbuf *, struct ifnet *);
int	in_ouraddr(struct mbuf *, struct ifnet *, struct in_addr);
void	ip_forward(struct mbuf *, struct ifnet *, int);

/*
 * Used to save the IP options in case a protocol wants to respond
 * to an incoming packet over the same route if the packet got here
 * using IP source routing.  This allows connection establishment and
 * maintenance when the remote end is on a network that is not known
 * to us.
 */
struct ip_srcrt {
	int		isr_nhops;		   /* number of hops */
	struct in_addr	isr_dst;		   /* final destination */
	char		isr_nop;		   /* one NOP to align */
	char		isr_hdr[IPOPT_OFFSET + 1]; /* OPTVAL, OLEN & OFFSET */
	struct in_addr	isr_routes[MAX_IPOPTLEN/sizeof(struct in_addr)];
};

void save_rte(struct mbuf *, u_char *, struct in_addr);

/*
 * IP initialization: fill in IP protocol switch table.
 * All protocols not implemented in kernel go to raw IP protocol handler.
 */
void
ip_init(void)
{
	struct protosw *pr;
	int i;
	const u_int16_t defbaddynamicports_tcp[] = DEFBADDYNAMICPORTS_TCP;
	const u_int16_t defbaddynamicports_udp[] = DEFBADDYNAMICPORTS_UDP;

	pool_init(&ipqent_pool, sizeof(struct ipqent), 0, 0, 0, "ipqepl",
	    NULL);
	pool_init(&ipq_pool, sizeof(struct ipq), 0, 0, 0, "ipqpl",
	    NULL);

	pr = pffindproto(PF_INET, IPPROTO_RAW, SOCK_RAW);
	if (pr == 0)
		panic("ip_init");
	for (i = 0; i < IPPROTO_MAX; i++)
		ip_protox[i] = pr - inetsw;
	for (pr = inetdomain.dom_protosw;
	    pr < inetdomain.dom_protoswNPROTOSW; pr++)
		if (pr->pr_domain->dom_family == PF_INET &&
		    pr->pr_protocol && pr->pr_protocol != IPPROTO_RAW &&
		    pr->pr_protocol < IPPROTO_MAX)
			ip_protox[pr->pr_protocol] = pr - inetsw;
	LIST_INIT(&ipq);
	IFQ_SET_MAXLEN(&ipintrq, IFQ_MAXLEN);
	TAILQ_INIT(&in_ifaddr);
	if (ip_mtudisc != 0)
		ip_mtudisc_timeout_q =
		    rt_timer_queue_create(ip_mtudisc_timeout);

	/* Fill in list of ports not to allocate dynamically. */
	memset(&baddynamicports, 0, sizeof(baddynamicports));
	for (i = 0; defbaddynamicports_tcp[i] != 0; i++)
		DP_SET(baddynamicports.tcp, defbaddynamicports_tcp[i]);
	for (i = 0; defbaddynamicports_udp[i] != 0; i++)
		DP_SET(baddynamicports.udp, defbaddynamicports_udp[i]);

	strlcpy(ipsec_def_enc, IPSEC_DEFAULT_DEF_ENC, sizeof(ipsec_def_enc));
	strlcpy(ipsec_def_auth, IPSEC_DEFAULT_DEF_AUTH, sizeof(ipsec_def_auth));
	strlcpy(ipsec_def_comp, IPSEC_DEFAULT_DEF_COMP, sizeof(ipsec_def_comp));
}

struct	sockaddr_in ipaddr = { sizeof(ipaddr), AF_INET };
struct	route ipforward_rt;

void
ipintr(void)
{
	struct mbuf *m;
	int s;

	for (;;) {
		/*
		 * Get next datagram off input queue and get IP header
		 * in first mbuf.
		 */
		s = splnet();
		IF_DEQUEUE(&ipintrq, m);
		splx(s);
		if (m == NULL)
			return;
#ifdef	DIAGNOSTIC
		if ((m->m_flags & M_PKTHDR) == 0)
			panic("ipintr no HDR");
#endif
		ipv4_input(m);
	}
}

/*
 * IPv4 input routine.
 *
 * Checksum and byte swap header.  Process options. Forward or deliver.
 */
void
ipv4_input(struct mbuf *m)
{
	struct ifnet *ifp;
	struct ip *ip;
	int hlen, len;
	in_addr_t pfrdr = 0;
#ifdef IPSEC
	int error;
	struct tdb *tdb;
	struct tdb_ident *tdbi;
	struct m_tag *mtag;
#endif /* IPSEC */

	ifp = m->m_pkthdr.rcvif;

	/*
	 * If no IP addresses have been set yet but the interfaces
	 * are receiving, can't do anything with incoming packets yet.
	 */
	if (TAILQ_EMPTY(&in_ifaddr))
		goto bad;
	ipstat.ips_total++;
	if (m->m_len < sizeof (struct ip) &&
	    (m = m_pullup(m, sizeof (struct ip))) == NULL) {
		ipstat.ips_toosmall++;
		return;
	}
	ip = mtod(m, struct ip *);
	if (ip->ip_v != IPVERSION) {
		ipstat.ips_badvers++;
		goto bad;
	}
	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(struct ip)) {	/* minimum header length */
		ipstat.ips_badhlen++;
		goto bad;
	}
	if (hlen > m->m_len) {
		if ((m = m_pullup(m, hlen)) == NULL) {
			ipstat.ips_badhlen++;
			return;
		}
		ip = mtod(m, struct ip *);
	}

	/* 127/8 must not appear on wire - RFC1122 */
	if ((ntohl(ip->ip_dst.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET ||
	    (ntohl(ip->ip_src.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET) {
		if ((ifp->if_flags & IFF_LOOPBACK) == 0) {
			ipstat.ips_badaddr++;
			goto bad;
		}
	}

	if ((m->m_pkthdr.csum_flags & M_IPV4_CSUM_IN_OK) == 0) {
		if (m->m_pkthdr.csum_flags & M_IPV4_CSUM_IN_BAD) {
			ipstat.ips_badsum++;
			goto bad;
		}

		ipstat.ips_inswcsum++;
		if (in_cksum(m, hlen) != 0) {
			ipstat.ips_badsum++;
			goto bad;
		}
	}

	/* Retrieve the packet length. */
	len = ntohs(ip->ip_len);

	/*
	 * Convert fields to host representation.
	 */
	if (len < hlen) {
		ipstat.ips_badlen++;
		goto bad;
	}

	/*
	 * Check that the amount of data in the buffers
	 * is at least as much as the IP header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len < len) {
		ipstat.ips_tooshort++;
		goto bad;
	}
	if (m->m_pkthdr.len > len) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = len;
			m->m_pkthdr.len = len;
		} else
			m_adj(m, len - m->m_pkthdr.len);
	}

#if NCARP > 0
	if (ifp->if_type == IFT_CARP && ip->ip_p != IPPROTO_ICMP &&
	    carp_lsdrop(m, AF_INET, &ip->ip_src.s_addr, &ip->ip_dst.s_addr))
		goto bad;
#endif

#if NPF > 0
	/*
	 * Packet filter
	 */
	pfrdr = ip->ip_dst.s_addr;
	if (pf_test(AF_INET, PF_IN, ifp, &m, NULL) != PF_PASS)
		goto bad;
	if (m == NULL)
		return;

	ip = mtod(m, struct ip *);
	hlen = ip->ip_hl << 2;
	pfrdr = (pfrdr != ip->ip_dst.s_addr);
#endif

	/*
	 * Process options and, if not destined for us,
	 * ship it on.  ip_dooptions returns 1 when an
	 * error was detected (causing an icmp message
	 * to be sent and the original packet to be freed).
	 */
	if (hlen > sizeof (struct ip) && ip_dooptions(m, ifp)) {
	        return;
	}

	if (in_ouraddr(m, ifp, ip->ip_dst)) {
		ip_ours(m);
		return;
	}

	if (IN_MULTICAST(ip->ip_dst.s_addr)) {
		struct in_multi *inm;
#ifdef MROUTING
		if (ipmforwarding && ip_mrouter) {
			if (m->m_flags & M_EXT) {
				if ((m = m_pullup(m, hlen)) == NULL) {
					ipstat.ips_toosmall++;
					return;
				}
				ip = mtod(m, struct ip *);
			}
			/*
			 * If we are acting as a multicast router, all
			 * incoming multicast packets are passed to the
			 * kernel-level multicast forwarding function.
			 * The packet is returned (relatively) intact; if
			 * ip_mforward() returns a non-zero value, the packet
			 * must be discarded, else it may be accepted below.
			 *
			 * (The IP ident field is put in the same byte order
			 * as expected when ip_mforward() is called from
			 * ip_output().)
			 */
			if (ip_mforward(m, ifp) != 0) {
				ipstat.ips_cantforward++;
				goto bad;
			}

			/*
			 * The process-level routing daemon needs to receive
			 * all multicast IGMP packets, whether or not this
			 * host belongs to their destination groups.
			 */
			if (ip->ip_p == IPPROTO_IGMP) {
				ip_ours(m);
				return;
			}
			ipstat.ips_forward++;
		}
#endif
		/*
		 * See if we belong to the destination multicast group on the
		 * arrival interface.
		 */
		IN_LOOKUP_MULTI(ip->ip_dst, ifp, inm);
		if (inm == NULL) {
			ipstat.ips_notmember++;
			if (!IN_LOCAL_GROUP(ip->ip_dst.s_addr))
				ipstat.ips_cantforward++;
			goto bad;
		}
		ip_ours(m);
		return;
	}

	if (ip->ip_dst.s_addr == INADDR_BROADCAST ||
	    ip->ip_dst.s_addr == INADDR_ANY) {
		ip_ours(m);
		return;
	}

#if NCARP > 0
	if (ifp->if_type == IFT_CARP && ip->ip_p == IPPROTO_ICMP &&
	    carp_lsdrop(m, AF_INET, &ip->ip_src.s_addr, &ip->ip_dst.s_addr))
		goto bad;
#endif
	/*
	 * Not for us; forward if possible and desirable.
	 */
	if (ipforwarding == 0) {
		ipstat.ips_cantforward++;
		goto bad;
	}
#ifdef IPSEC
	if (ipsec_in_use) {
	        /*
		 * IPsec policy check for forwarded packets. Look at
		 * inner-most IPsec SA used.
		 */
		mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL);
		if (mtag != NULL) {
			tdbi = (struct tdb_ident *)(mtag + 1);
			tdb = gettdb(tdbi->rdomain, tdbi->spi,
			    &tdbi->dst, tdbi->proto);
		} else
			tdb = NULL;
	        ipsp_spd_lookup(m, AF_INET, hlen, &error,
		    IPSP_DIRECTION_IN, tdb, NULL, 0);

		/* Error or otherwise drop-packet indication */
		if (error) {
			ipstat.ips_cantforward++;
			goto bad;
		}

		/*
		 * Fall through, forward packet. Outbound IPsec policy
		 * checking will occur in ip_output().
		 */
	}
#endif /* IPSEC */

	ip_forward(m, ifp, pfrdr);
	return;
bad:
	m_freem(m);
}

/*
 * IPv4 local-delivery routine.
 *
 * If fragmented try to reassemble.  Pass to next level.
 */
void
ip_ours(struct mbuf *m)
{
	struct ip *ip = mtod(m, struct ip *);
	struct ipq *fp;
	struct ipqent *ipqe;
	int mff, hlen;
#ifdef IPSEC
	int error;
	struct tdb *tdb;
	struct tdb_ident *tdbi;
	struct m_tag *mtag;
#endif /* IPSEC */

	hlen = ip->ip_hl << 2;

	/* pf might have modified stuff, might have to chksum */
	in_proto_cksum_out(m, NULL);

	/*
	 * If offset or IP_MF are set, must reassemble.
	 * Otherwise, nothing need be done.
	 * (We could look in the reassembly queue to see
	 * if the packet was previously fragmented,
	 * but it's not worth the time; just let them time out.)
	 */
	if (ip->ip_off &~ htons(IP_DF | IP_RF)) {
		if (m->m_flags & M_EXT) {		/* XXX */
			if ((m = m_pullup(m, hlen)) == NULL) {
				ipstat.ips_toosmall++;
				return;
			}
			ip = mtod(m, struct ip *);
		}

		/*
		 * Look for queue of fragments
		 * of this datagram.
		 */
		LIST_FOREACH(fp, &ipq, ipq_q)
			if (ip->ip_id == fp->ipq_id &&
			    ip->ip_src.s_addr == fp->ipq_src.s_addr &&
			    ip->ip_dst.s_addr == fp->ipq_dst.s_addr &&
			    ip->ip_p == fp->ipq_p)
				goto found;
		fp = 0;
found:

		/*
		 * Adjust ip_len to not reflect header,
		 * set ipqe_mff if more fragments are expected,
		 * convert offset of this to bytes.
		 */
		ip->ip_len = htons(ntohs(ip->ip_len) - hlen);
		mff = (ip->ip_off & htons(IP_MF)) != 0;
		if (mff) {
			/*
			 * Make sure that fragments have a data length
			 * that's a non-zero multiple of 8 bytes.
			 */
			if (ntohs(ip->ip_len) == 0 ||
			    (ntohs(ip->ip_len) & 0x7) != 0) {
				ipstat.ips_badfrags++;
				goto bad;
			}
		}
		ip->ip_off = htons(ntohs(ip->ip_off) << 3);

		/*
		 * If datagram marked as having more fragments
		 * or if this is not the first fragment,
		 * attempt reassembly; if it succeeds, proceed.
		 */
		if (mff || ip->ip_off) {
			ipstat.ips_fragments++;
			if (ip_frags + 1 > ip_maxqueue) {
				ip_flush();
				ipstat.ips_rcvmemdrop++;
				goto bad;
			}

			ipqe = pool_get(&ipqent_pool, PR_NOWAIT);
			if (ipqe == NULL) {
				ipstat.ips_rcvmemdrop++;
				goto bad;
			}
			ip_frags++;
			ipqe->ipqe_mff = mff;
			ipqe->ipqe_m = m;
			ipqe->ipqe_ip = ip;
			m = ip_reass(ipqe, fp);
			if (m == 0) {
				return;
			}
			ipstat.ips_reassembled++;
			ip = mtod(m, struct ip *);
			hlen = ip->ip_hl << 2;
			ip->ip_len = htons(ntohs(ip->ip_len) + hlen);
		} else
			if (fp)
				ip_freef(fp);
	}

#ifdef IPSEC
	if (!ipsec_in_use)
		goto skipipsec;

        /*
         * If it's a protected packet for us, skip the policy check.
         * That's because we really only care about the properties of
         * the protected packet, and not the intermediate versions.
         * While this is not the most paranoid setting, it allows
         * some flexibility in handling nested tunnels (in setting up
	 * the policies).
         */
        if ((ip->ip_p == IPPROTO_ESP) || (ip->ip_p == IPPROTO_AH) ||
	    (ip->ip_p == IPPROTO_IPCOMP))
          goto skipipsec;

	/*
	 * If the protected packet was tunneled, then we need to
	 * verify the protected packet's information, not the
	 * external headers. Thus, skip the policy lookup for the
	 * external packet, and keep the IPsec information linked on
	 * the packet header (the encapsulation routines know how
	 * to deal with that).
	 */
	if ((ip->ip_p == IPPROTO_IPIP) || (ip->ip_p == IPPROTO_IPV6))
	  goto skipipsec;

	/*
	 * If the protected packet is TCP or UDP, we'll do the
	 * policy check in the respective input routine, so we can
	 * check for bypass sockets.
	 */
	if ((ip->ip_p == IPPROTO_TCP) || (ip->ip_p == IPPROTO_UDP))
	  goto skipipsec;

	/*
	 * IPsec policy check for local-delivery packets. Look at the
	 * inner-most SA that protected the packet. This is in fact
	 * a bit too restrictive (it could end up causing packets to
	 * be dropped that semantically follow the policy, e.g., in
	 * certain SA-bundle configurations); but the alternative is
	 * very complicated (and requires keeping track of what
	 * kinds of tunneling headers have been seen in-between the
	 * IPsec headers), and I don't think we lose much functionality
	 * that's needed in the real world (who uses bundles anyway ?).
	 */
	mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL);
	if (mtag) {
		tdbi = (struct tdb_ident *)(mtag + 1);
	        tdb = gettdb(tdbi->rdomain, tdbi->spi, &tdbi->dst,
		    tdbi->proto);
	} else
		tdb = NULL;
	ipsp_spd_lookup(m, AF_INET, hlen, &error, IPSP_DIRECTION_IN,
	    tdb, NULL, 0);

	/* Error or otherwise drop-packet indication. */
	if (error) {
	        ipstat.ips_cantforward++;
	        goto bad;
	}

 skipipsec:
	/* Otherwise, just fall through and deliver the packet */
#endif /* IPSEC */

	/*
	 * Switch out to protocol's input routine.
	 */
	ipstat.ips_delivered++;
	(*inetsw[ip_protox[ip->ip_p]].pr_input)(m, hlen, NULL, 0);
	return;
bad:
	m_freem(m);
}

int
in_ouraddr(struct mbuf *m, struct ifnet *ifp, struct in_addr ina)
{
	struct in_ifaddr	*ia;
	struct sockaddr_in	 sin;
#if NPF > 0
	struct pf_state_key	*key;

	if (m->m_pkthdr.pf.flags & PF_TAG_DIVERTED)
		return (1);

	key = m->m_pkthdr.pf.statekey;
	if (key != NULL) {
		if (key->inp != NULL)
			return (1);

		/* If we have linked state keys it is certainly forwarded. */
		if (key->reverse != NULL)
			return (0);
	}
#endif

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr = ina;
	ia = ifatoia(ifa_ifwithaddr(sintosa(&sin), m->m_pkthdr.ph_rtableid));

	if (ia == NULL) {
		struct ifaddr *ifa;

		/*
		 * No local address or broadcast address found, so check for
		 * ancient classful broadcast addresses.
		 * It must have been broadcast on the link layer, and for an
		 * address on the interface it was received on.
		 */
		if (!ISSET(m->m_flags, M_BCAST) ||
		    !IN_CLASSFULBROADCAST(ina.s_addr, ina.s_addr))
			return (0);

		if (ifp->if_rdomain != rtable_l2(m->m_pkthdr.ph_rtableid))
			return (0);
		/*
		 * The check in the loop assumes you only rx a packet on an UP
		 * interface, and that M_BCAST will only be set on a BROADCAST
		 * interface.
		 */
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;

			if (IN_CLASSFULBROADCAST(ina.s_addr,
			    ifatoia(ifa)->ia_addr.sin_addr.s_addr))
				return (1);
		}

		return (0);
	}

	if (ina.s_addr != ia->ia_addr.sin_addr.s_addr) {
		/*
		 * This matches a broadcast address on one of our interfaces.
		 * If directedbcast is enabled we only consider it local if it
		 * is received on the interface with that address.
		 */
		if (ip_directedbcast && ia->ia_ifp != ifp)
			return (0);

		/* Make sure M_BCAST is set */
		if (m)
			m->m_flags |= M_BCAST;
	}

	return (ISSET(ia->ia_ifp->if_flags, IFF_UP));
}

struct in_ifaddr *
in_iawithaddr(struct in_addr ina, u_int rtableid)
{
	struct in_ifaddr	*ia;
	struct sockaddr_in	 sin;

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr = ina;
	ia = ifatoia(ifa_ifwithaddr(sintosa(&sin), rtableid));
	if (ia == NULL || ina.s_addr == ia->ia_addr.sin_addr.s_addr)
		return (ia);

	return (NULL);
}

/*
 * Take incoming datagram fragment and try to
 * reassemble it into whole datagram.  If a chain for
 * reassembly of this datagram already exists, then it
 * is given as fp; otherwise have to make a chain.
 */
struct mbuf *
ip_reass(struct ipqent *ipqe, struct ipq *fp)
{
	struct mbuf *m = ipqe->ipqe_m;
	struct ipqent *nq, *p, *q;
	struct ip *ip;
	struct mbuf *t;
	int hlen = ipqe->ipqe_ip->ip_hl << 2;
	int i, next;
	u_int8_t ecn, ecn0;

	/*
	 * Presence of header sizes in mbufs
	 * would confuse code below.
	 */
	m->m_data += hlen;
	m->m_len -= hlen;

	/*
	 * If first fragment to arrive, create a reassembly queue.
	 */
	if (fp == NULL) {
		fp = pool_get(&ipq_pool, PR_NOWAIT);
		if (fp == NULL)
			goto dropfrag;
		LIST_INSERT_HEAD(&ipq, fp, ipq_q);
		fp->ipq_ttl = IPFRAGTTL;
		fp->ipq_p = ipqe->ipqe_ip->ip_p;
		fp->ipq_id = ipqe->ipqe_ip->ip_id;
		LIST_INIT(&fp->ipq_fragq);
		fp->ipq_src = ipqe->ipqe_ip->ip_src;
		fp->ipq_dst = ipqe->ipqe_ip->ip_dst;
		p = NULL;
		goto insert;
	}

	/*
	 * Handle ECN by comparing this segment with the first one;
	 * if CE is set, do not lose CE.
	 * drop if CE and not-ECT are mixed for the same packet.
	 */
	ecn = ipqe->ipqe_ip->ip_tos & IPTOS_ECN_MASK;
	ecn0 = LIST_FIRST(&fp->ipq_fragq)->ipqe_ip->ip_tos & IPTOS_ECN_MASK;
	if (ecn == IPTOS_ECN_CE) {
		if (ecn0 == IPTOS_ECN_NOTECT)
			goto dropfrag;
		if (ecn0 != IPTOS_ECN_CE)
			LIST_FIRST(&fp->ipq_fragq)->ipqe_ip->ip_tos |= IPTOS_ECN_CE;
	}
	if (ecn == IPTOS_ECN_NOTECT && ecn0 != IPTOS_ECN_NOTECT)
		goto dropfrag;

	/*
	 * Find a segment which begins after this one does.
	 */
	for (p = NULL, q = LIST_FIRST(&fp->ipq_fragq); q != NULL;
	    p = q, q = LIST_NEXT(q, ipqe_q))
		if (ntohs(q->ipqe_ip->ip_off) > ntohs(ipqe->ipqe_ip->ip_off))
			break;

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us.
	 */
	if (p != NULL) {
		i = ntohs(p->ipqe_ip->ip_off) + ntohs(p->ipqe_ip->ip_len) -
		    ntohs(ipqe->ipqe_ip->ip_off);
		if (i > 0) {
			if (i >= ntohs(ipqe->ipqe_ip->ip_len))
				goto dropfrag;
			m_adj(ipqe->ipqe_m, i);
			ipqe->ipqe_ip->ip_off =
			    htons(ntohs(ipqe->ipqe_ip->ip_off) + i);
			ipqe->ipqe_ip->ip_len =
			    htons(ntohs(ipqe->ipqe_ip->ip_len) - i);
		}
	}

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	for (; q != NULL &&
	    ntohs(ipqe->ipqe_ip->ip_off) + ntohs(ipqe->ipqe_ip->ip_len) >
	    ntohs(q->ipqe_ip->ip_off); q = nq) {
		i = (ntohs(ipqe->ipqe_ip->ip_off) +
		    ntohs(ipqe->ipqe_ip->ip_len)) - ntohs(q->ipqe_ip->ip_off);
		if (i < ntohs(q->ipqe_ip->ip_len)) {
			q->ipqe_ip->ip_len =
			    htons(ntohs(q->ipqe_ip->ip_len) - i);
			q->ipqe_ip->ip_off =
			    htons(ntohs(q->ipqe_ip->ip_off) + i);
			m_adj(q->ipqe_m, i);
			break;
		}
		nq = LIST_NEXT(q, ipqe_q);
		m_freem(q->ipqe_m);
		LIST_REMOVE(q, ipqe_q);
		pool_put(&ipqent_pool, q);
		ip_frags--;
	}

insert:
	/*
	 * Stick new segment in its place;
	 * check for complete reassembly.
	 */
	if (p == NULL) {
		LIST_INSERT_HEAD(&fp->ipq_fragq, ipqe, ipqe_q);
	} else {
		LIST_INSERT_AFTER(p, ipqe, ipqe_q);
	}
	next = 0;
	for (p = NULL, q = LIST_FIRST(&fp->ipq_fragq); q != NULL;
	    p = q, q = LIST_NEXT(q, ipqe_q)) {
		if (ntohs(q->ipqe_ip->ip_off) != next)
			return (0);
		next += ntohs(q->ipqe_ip->ip_len);
	}
	if (p->ipqe_mff)
		return (0);

	/*
	 * Reassembly is complete.  Check for a bogus message size and
	 * concatenate fragments.
	 */
	q = LIST_FIRST(&fp->ipq_fragq);
	ip = q->ipqe_ip;
	if ((next + (ip->ip_hl << 2)) > IP_MAXPACKET) {
		ipstat.ips_toolong++;
		ip_freef(fp);
		return (0);
	}
	m = q->ipqe_m;
	t = m->m_next;
	m->m_next = 0;
	m_cat(m, t);
	nq = LIST_NEXT(q, ipqe_q);
	pool_put(&ipqent_pool, q);
	ip_frags--;
	for (q = nq; q != NULL; q = nq) {
		t = q->ipqe_m;
		nq = LIST_NEXT(q, ipqe_q);
		pool_put(&ipqent_pool, q);
		ip_frags--;
		m_cat(m, t);
	}

	/*
	 * Create header for new ip packet by
	 * modifying header of first packet;
	 * dequeue and discard fragment reassembly header.
	 * Make header visible.
	 */
	ip->ip_len = htons(next);
	ip->ip_src = fp->ipq_src;
	ip->ip_dst = fp->ipq_dst;
	LIST_REMOVE(fp, ipq_q);
	pool_put(&ipq_pool, fp);
	m->m_len += (ip->ip_hl << 2);
	m->m_data -= (ip->ip_hl << 2);
	/* some debugging cruft by sklower, below, will go away soon */
	if (m->m_flags & M_PKTHDR) { /* XXX this should be done elsewhere */
		int plen = 0;
		for (t = m; t; t = t->m_next)
			plen += t->m_len;
		m->m_pkthdr.len = plen;
	}
	return (m);

dropfrag:
	ipstat.ips_fragdropped++;
	m_freem(m);
	pool_put(&ipqent_pool, ipqe);
	ip_frags--;
	return (0);
}

/*
 * Free a fragment reassembly header and all
 * associated datagrams.
 */
void
ip_freef(struct ipq *fp)
{
	struct ipqent *q, *p;

	for (q = LIST_FIRST(&fp->ipq_fragq); q != NULL; q = p) {
		p = LIST_NEXT(q, ipqe_q);
		m_freem(q->ipqe_m);
		LIST_REMOVE(q, ipqe_q);
		pool_put(&ipqent_pool, q);
		ip_frags--;
	}
	LIST_REMOVE(fp, ipq_q);
	pool_put(&ipq_pool, fp);
}

/*
 * IP timer processing;
 * if a timer expires on a reassembly queue, discard it.
 * clear the forwarding cache, there might be a better route.
 */
void
ip_slowtimo(void)
{
	struct ipq *fp, *nfp;
	int s = splsoftnet();

	for (fp = LIST_FIRST(&ipq); fp != NULL; fp = nfp) {
		nfp = LIST_NEXT(fp, ipq_q);
		if (--fp->ipq_ttl == 0) {
			ipstat.ips_fragtimeout++;
			ip_freef(fp);
		}
	}
	if (ipforward_rt.ro_rt) {
		RTFREE(ipforward_rt.ro_rt);
		ipforward_rt.ro_rt = 0;
	}
	splx(s);
}

/*
 * Drain off all datagram fragments.
 */
void
ip_drain(void)
{
	while (!LIST_EMPTY(&ipq)) {
		ipstat.ips_fragdropped++;
		ip_freef(LIST_FIRST(&ipq));
	}
}

/*
 * Flush a bunch of datagram fragments, till we are down to 75%.
 */
void
ip_flush(void)
{
	int max = 50;

	/* ipq already locked */
	while (!LIST_EMPTY(&ipq) && ip_frags > ip_maxqueue * 3 / 4 && --max) {
		ipstat.ips_fragdropped++;
		ip_freef(LIST_FIRST(&ipq));
	}
}

/*
 * Do option processing on a datagram,
 * possibly discarding it if bad options are encountered,
 * or forwarding it if source-routed.
 * Returns 1 if packet has been forwarded/freed,
 * 0 if the packet should be processed further.
 */
int
ip_dooptions(struct mbuf *m, struct ifnet *ifp)
{
	struct ip *ip = mtod(m, struct ip *);
	u_char *cp;
	struct ip_timestamp ipt;
	struct in_ifaddr *ia;
	int opt, optlen, cnt, off, code, type = ICMP_PARAMPROB, forward = 0;
	struct in_addr sin, dst;
	n_time ntime;

	dst = ip->ip_dst;
	cp = (u_char *)(ip + 1);
	cnt = (ip->ip_hl << 2) - sizeof (struct ip);

	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			if (cnt < IPOPT_OLEN + sizeof(*cp)) {
				code = &cp[IPOPT_OLEN] - (u_char *)ip;
				goto bad;
			}
			optlen = cp[IPOPT_OLEN];
			if (optlen < IPOPT_OLEN + sizeof(*cp) || optlen > cnt) {
				code = &cp[IPOPT_OLEN] - (u_char *)ip;
				goto bad;
			}
		}

		switch (opt) {

		default:
			break;

		/*
		 * Source routing with record.
		 * Find interface with current destination address.
		 * If none on this machine then drop if strictly routed,
		 * or do nothing if loosely routed.
		 * Record interface address and bring up next address
		 * component.  If strictly routed make sure next
		 * address is on directly accessible net.
		 */
		case IPOPT_LSRR:
		case IPOPT_SSRR:
			if (!ip_dosourceroute) {
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_SRCFAIL;
				goto bad;
			}
			if ((off = cp[IPOPT_OFFSET]) < IPOPT_MINOFF) {
				code = &cp[IPOPT_OFFSET] - (u_char *)ip;
				goto bad;
			}
			ipaddr.sin_addr = ip->ip_dst;
			ia = ifatoia(ifa_ifwithaddr(sintosa(&ipaddr),
			    m->m_pkthdr.ph_rtableid));
			if (ia == 0) {
				if (opt == IPOPT_SSRR) {
					type = ICMP_UNREACH;
					code = ICMP_UNREACH_SRCFAIL;
					goto bad;
				}
				/*
				 * Loose routing, and not at next destination
				 * yet; nothing to do except forward.
				 */
				break;
			}
			off--;			/* 0 origin */
			if ((off + sizeof(struct in_addr)) > optlen) {
				/*
				 * End of source route.  Should be for us.
				 */
				save_rte(m, cp, ip->ip_src);
				break;
			}

			/*
			 * locate outgoing interface
			 */
			memcpy(&ipaddr.sin_addr, cp + off,
			    sizeof(ipaddr.sin_addr));
			if (opt == IPOPT_SSRR) {
			    if ((ia = ifatoia(ifa_ifwithdstaddr(sintosa(&ipaddr),
				m->m_pkthdr.ph_rtableid))) == NULL)
				ia = ifatoia(ifa_ifwithnet(sintosa(&ipaddr),
				    m->m_pkthdr.ph_rtableid));
			} else
				/* keep packet in the virtual instance */
				ia = ip_rtaddr(ipaddr.sin_addr,
				    m->m_pkthdr.ph_rtableid);
			if (ia == 0) {
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_SRCFAIL;
				goto bad;
			}
			ip->ip_dst = ipaddr.sin_addr;
			memcpy(cp + off, &ia->ia_addr.sin_addr,
			    sizeof(struct in_addr));
			cp[IPOPT_OFFSET] += sizeof(struct in_addr);
			/*
			 * Let ip_intr's mcast routing check handle mcast pkts
			 */
			forward = !IN_MULTICAST(ip->ip_dst.s_addr);
			break;

		case IPOPT_RR:
			if (optlen < IPOPT_OFFSET + sizeof(*cp)) {
				code = &cp[IPOPT_OLEN] - (u_char *)ip;
				goto bad;
			}
			if ((off = cp[IPOPT_OFFSET]) < IPOPT_MINOFF) {
				code = &cp[IPOPT_OFFSET] - (u_char *)ip;
				goto bad;
			}

			/*
			 * If no space remains, ignore.
			 */
			off--;			/* 0 origin */
			if ((off + sizeof(struct in_addr)) > optlen)
				break;
			memcpy(&ipaddr.sin_addr, &ip->ip_dst,
			    sizeof(ipaddr.sin_addr));
			/*
			 * locate outgoing interface; if we're the destination,
			 * use the incoming interface (should be same).
			 * Again keep the packet inside the virtual instance.
			 */
			if ((ia = ifatoia(ifa_ifwithaddr(sintosa(&ipaddr),
			    m->m_pkthdr.ph_rtableid))) == 0 &&
			    (ia = ip_rtaddr(ipaddr.sin_addr,
			    m->m_pkthdr.ph_rtableid)) == 0) {
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_HOST;
				goto bad;
			}
			memcpy(cp + off, &ia->ia_addr.sin_addr,
			    sizeof(struct in_addr));
			cp[IPOPT_OFFSET] += sizeof(struct in_addr);
			break;

		case IPOPT_TS:
			code = cp - (u_char *)ip;
			if (optlen < sizeof(struct ip_timestamp))
				goto bad;
			memcpy(&ipt, cp, sizeof(struct ip_timestamp));
			if (ipt.ipt_ptr < 5 || ipt.ipt_len < 5)
				goto bad;
			if (ipt.ipt_ptr - 1 + sizeof(n_time) > ipt.ipt_len) {
				if (++ipt.ipt_oflw == 0)
					goto bad;
				break;
			}
			memcpy(&sin, cp + ipt.ipt_ptr - 1, sizeof sin);
			switch (ipt.ipt_flg) {

			case IPOPT_TS_TSONLY:
				break;

			case IPOPT_TS_TSANDADDR:
				if (ipt.ipt_ptr - 1 + sizeof(n_time) +
				    sizeof(struct in_addr) > ipt.ipt_len)
					goto bad;
				ipaddr.sin_addr = dst;
				ia = ifatoia(ifaof_ifpforaddr(sintosa(&ipaddr),
				    ifp));
				if (ia == 0)
					continue;
				memcpy(&sin, &ia->ia_addr.sin_addr,
				    sizeof(struct in_addr));
				ipt.ipt_ptr += sizeof(struct in_addr);
				break;

			case IPOPT_TS_PRESPEC:
				if (ipt.ipt_ptr - 1 + sizeof(n_time) +
				    sizeof(struct in_addr) > ipt.ipt_len)
					goto bad;
				memcpy(&ipaddr.sin_addr, &sin,
				    sizeof(struct in_addr));
				if (ifa_ifwithaddr(sintosa(&ipaddr),
				    m->m_pkthdr.ph_rtableid) == 0)
					continue;
				ipt.ipt_ptr += sizeof(struct in_addr);
				break;

			default:
				/* XXX can't take &ipt->ipt_flg */
				code = (u_char *)&ipt.ipt_ptr -
				    (u_char *)ip + 1;
				goto bad;
			}
			ntime = iptime();
			memcpy(cp + ipt.ipt_ptr - 1, &ntime, sizeof(n_time));
			ipt.ipt_ptr += sizeof(n_time);
		}
	}
	if (forward && ipforwarding) {
		ip_forward(m, ifp, 1);
		return (1);
	}
	return (0);
bad:
	icmp_error(m, type, code, 0, 0);
	ipstat.ips_badoptions++;
	return (1);
}

/*
 * Given address of next destination (final or next hop),
 * return internet address info of interface to be used to get there.
 */
struct in_ifaddr *
ip_rtaddr(struct in_addr dst, u_int rtableid)
{
	struct sockaddr_in *sin;

	sin = satosin(&ipforward_rt.ro_dst);

	if (ipforward_rt.ro_rt == 0 || dst.s_addr != sin->sin_addr.s_addr) {
		if (ipforward_rt.ro_rt) {
			RTFREE(ipforward_rt.ro_rt);
			ipforward_rt.ro_rt = 0;
		}
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = dst;

		ipforward_rt.ro_rt = rtalloc1(&ipforward_rt.ro_dst, RT_REPORT,
		    rtableid);
	}
	if (ipforward_rt.ro_rt == 0)
		return (NULL);
	return (ifatoia(ipforward_rt.ro_rt->rt_ifa));
}

/*
 * Save incoming source route for use in replies,
 * to be picked up later by ip_srcroute if the receiver is interested.
 */
void
save_rte(struct mbuf *m, u_char *option, struct in_addr dst)
{
	struct ip_srcrt *isr;
	struct m_tag *mtag;
	unsigned olen;

	olen = option[IPOPT_OLEN];
	if (olen > sizeof(isr->isr_hdr) + sizeof(isr->isr_routes))
		return;

	mtag = m_tag_get(PACKET_TAG_SRCROUTE, sizeof(*isr), M_NOWAIT);
	if (mtag == NULL)
		return;
	isr = (struct ip_srcrt *)(mtag + 1);

	memcpy(isr->isr_hdr, option, olen);
	isr->isr_nhops = (olen - IPOPT_OFFSET - 1) / sizeof(struct in_addr);
	isr->isr_dst = dst;
	m_tag_prepend(m, mtag);
}

/*
 * Retrieve incoming source route for use in replies,
 * in the same form used by setsockopt.
 * The first hop is placed before the options, will be removed later.
 */
struct mbuf *
ip_srcroute(struct mbuf *m0)
{
	struct in_addr *p, *q;
	struct mbuf *m;
	struct ip_srcrt *isr;
	struct m_tag *mtag;

	if (!ip_dosourceroute)
		return (NULL);

	mtag = m_tag_find(m0, PACKET_TAG_SRCROUTE, NULL);
	if (mtag == NULL)
		return (NULL);
	isr = (struct ip_srcrt *)(mtag + 1);

	if (isr->isr_nhops == 0)
		return (NULL);
	m = m_get(M_DONTWAIT, MT_SOOPTS);
	if (m == NULL)
		return (NULL);

#define OPTSIZ	(sizeof(isr->isr_nop) + sizeof(isr->isr_hdr))

	/* length is (nhops+1)*sizeof(addr) + sizeof(nop + header) */
	m->m_len = (isr->isr_nhops + 1) * sizeof(struct in_addr) + OPTSIZ;

	/*
	 * First save first hop for return route
	 */
	p = &(isr->isr_routes[isr->isr_nhops - 1]);
	*(mtod(m, struct in_addr *)) = *p--;

	/*
	 * Copy option fields and padding (nop) to mbuf.
	 */
	isr->isr_nop = IPOPT_NOP;
	isr->isr_hdr[IPOPT_OFFSET] = IPOPT_MINOFF;
	memcpy(mtod(m, caddr_t) + sizeof(struct in_addr), &isr->isr_nop,
	    OPTSIZ);
	q = (struct in_addr *)(mtod(m, caddr_t) +
	    sizeof(struct in_addr) + OPTSIZ);
#undef OPTSIZ
	/*
	 * Record return path as an IP source route,
	 * reversing the path (pointers are now aligned).
	 */
	while (p >= isr->isr_routes) {
		*q++ = *p--;
	}
	/*
	 * Last hop goes to final destination.
	 */
	*q = isr->isr_dst;
	m_tag_delete(m0, (struct m_tag *)isr);
	return (m);
}

/*
 * Strip out IP options, at higher level protocol in the kernel.
 */
void
ip_stripoptions(struct mbuf *m)
{
	int i;
	struct ip *ip = mtod(m, struct ip *);
	caddr_t opts;
	int olen;

	olen = (ip->ip_hl<<2) - sizeof (struct ip);
	opts = (caddr_t)(ip + 1);
	i = m->m_len - (sizeof (struct ip) + olen);
	memmove(opts, opts  + olen, i);
	m->m_len -= olen;
	if (m->m_flags & M_PKTHDR)
		m->m_pkthdr.len -= olen;
	ip->ip_hl = sizeof(struct ip) >> 2;
	ip->ip_len = htons(ntohs(ip->ip_len) - olen);
}

int inetctlerrmap[PRC_NCMDS] = {
	0,		0,		0,		0,
	0,		EMSGSIZE,	EHOSTDOWN,	EHOSTUNREACH,
	EHOSTUNREACH,	EHOSTUNREACH,	ECONNREFUSED,	ECONNREFUSED,
	EMSGSIZE,	EHOSTUNREACH,	0,		0,
	0,		0,		0,		0,
	ENOPROTOOPT
};

/*
 * Forward a packet.  If some error occurs return the sender
 * an icmp packet.  Note we can't always generate a meaningful
 * icmp message because icmp doesn't have a large enough repertoire
 * of codes and types.
 *
 * If not forwarding, just drop the packet.  This could be confusing
 * if ipforwarding was zero but some routing protocol was advancing
 * us as a gateway to somewhere.  However, we must let the routing
 * protocol deal with that.
 *
 * The srcrt parameter indicates whether the packet is being forwarded
 * via a source route.
 */
void
ip_forward(struct mbuf *m, struct ifnet *ifp, int srcrt)
{
	struct mbuf mfake, *mcopy = NULL;
	struct ip *ip = mtod(m, struct ip *);
	struct sockaddr_in *sin;
	struct rtentry *rt;
	int error, type = 0, code = 0, destmtu = 0, fake = 0, len;
	u_int rtableid = 0;
	n_long dest;

	dest = 0;
	if (m->m_flags & (M_BCAST|M_MCAST) || in_canforward(ip->ip_dst) == 0) {
		ipstat.ips_cantforward++;
		m_freem(m);
		return;
	}
	if (ip->ip_ttl <= IPTTLDEC) {
		icmp_error(m, ICMP_TIMXCEED, ICMP_TIMXCEED_INTRANS, dest, 0);
		return;
	}

	rtableid = m->m_pkthdr.ph_rtableid;

	sin = satosin(&ipforward_rt.ro_dst);
	if ((rt = ipforward_rt.ro_rt) == 0 ||
	    ip->ip_dst.s_addr != sin->sin_addr.s_addr ||
	    rtableid != ipforward_rt.ro_tableid) {
		if (ipforward_rt.ro_rt) {
			RTFREE(ipforward_rt.ro_rt);
			ipforward_rt.ro_rt = 0;
		}
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = ip->ip_dst;
		ipforward_rt.ro_tableid = rtableid;

		rtalloc_mpath(&ipforward_rt, &ip->ip_src.s_addr);
		if (ipforward_rt.ro_rt == 0) {
			icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_HOST, dest, 0);
			return;
		}
		rt = ipforward_rt.ro_rt;
	}

	/*
	 * Save at most 68 bytes of the packet in case
	 * we need to generate an ICMP message to the src.
	 * The data is saved in the mbuf on the stack that
	 * acts as a temporary storage not intended to be
	 * passed down the IP stack or to the mfree.
	 */
	memset(&mfake.m_hdr, 0, sizeof(mfake.m_hdr));
	mfake.m_type = m->m_type;
	if (m_dup_pkthdr(&mfake, m, M_DONTWAIT) == 0) {
		mfake.m_data = mfake.m_pktdat;
		len = min(ntohs(ip->ip_len), 68);
		m_copydata(m, 0, len, mfake.m_pktdat);
		mfake.m_pkthdr.len = mfake.m_len = len;
		fake = 1;
	}

	ip->ip_ttl -= IPTTLDEC;

	/*
	 * If forwarding packet using same interface that it came in on,
	 * perhaps should send a redirect to sender to shortcut a hop.
	 * Only send redirect if source is sending directly to us,
	 * and if packet was not source routed (or has any options).
	 * Also, don't send redirect if forwarding using a default route
	 * or a route modified by a redirect.
	 * Don't send redirect if we advertise destination's arp address
	 * as ours (proxy arp).
	 */
	if (rt->rt_ifp == ifp &&
	    (rt->rt_flags & (RTF_DYNAMIC|RTF_MODIFIED)) == 0 &&
	    satosin(rt_key(rt))->sin_addr.s_addr != 0 &&
	    ipsendredirects && !srcrt &&
	    !arpproxy(satosin(rt_key(rt))->sin_addr, m->m_pkthdr.ph_rtableid)) {
		if (rt->rt_ifa &&
		    (ip->ip_src.s_addr & ifatoia(rt->rt_ifa)->ia_netmask) ==
		    ifatoia(rt->rt_ifa)->ia_net) {
		    if (rt->rt_flags & RTF_GATEWAY)
			dest = satosin(rt->rt_gateway)->sin_addr.s_addr;
		    else
			dest = ip->ip_dst.s_addr;
		    /* Router requirements says to only send host redirects */
		    type = ICMP_REDIRECT;
		    code = ICMP_REDIRECT_HOST;
		}
	}

	error = ip_output(m, NULL, &ipforward_rt,
	    (IP_FORWARDING | (ip_directedbcast ? IP_ALLOWBROADCAST : 0)),
	    NULL, NULL, 0);
	if (error)
		ipstat.ips_cantforward++;
	else {
		ipstat.ips_forward++;
		if (type)
			ipstat.ips_redirectsent++;
		else
			goto freecopy;
	}
	if (!fake)
		goto freert;

	switch (error) {

	case 0:				/* forwarded, but need redirect */
		/* type, code set above */
		break;

	case ENETUNREACH:		/* shouldn't happen, checked above */
	case EHOSTUNREACH:
	case ENETDOWN:
	case EHOSTDOWN:
	default:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_HOST;
		break;

	case EMSGSIZE:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_NEEDFRAG;

#ifdef IPSEC
		if (ipforward_rt.ro_rt) {
			struct rtentry *rt = ipforward_rt.ro_rt;

			if (rt->rt_rmx.rmx_mtu)
				destmtu = rt->rt_rmx.rmx_mtu;
			else
				destmtu = ipforward_rt.ro_rt->rt_ifp->if_mtu;
		}
#endif /*IPSEC*/
		ipstat.ips_cantfrag++;
		break;

	case EACCES:
		/*
		 * pf(4) blocked the packet. There is no need to send an ICMP
		 * packet back since pf(4) takes care of it.
		 */
		goto freecopy;
	case ENOBUFS:
		/*
		 * a router should not generate ICMP_SOURCEQUENCH as
		 * required in RFC1812 Requirements for IP Version 4 Routers.
		 * source quench could be a big problem under DoS attacks,
		 * or the underlying interface is rate-limited.
		 */
		goto freecopy;
	}

	mcopy = m_copym(&mfake, 0, len, M_DONTWAIT);
	if (mcopy)
		icmp_error(mcopy, type, code, dest, destmtu);

 freecopy:
	if (fake)
		m_tag_delete_chain(&mfake);
 freert:
#ifndef SMALL_KERNEL
	if (ipmultipath && ipforward_rt.ro_rt &&
	    (ipforward_rt.ro_rt->rt_flags & RTF_MPATH)) {
		RTFREE(ipforward_rt.ro_rt);
		ipforward_rt.ro_rt = 0;
	}
#endif
	return;
}

int
ip_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen) 
{
	int s, error;
#ifdef MROUTING
	extern int ip_mrtproto;
	extern struct mrtstat mrtstat;
#endif

	/* Almost all sysctl names at this level are terminal. */
	if (namelen != 1 && name[0] != IPCTL_IFQUEUE)
		return (ENOTDIR);

	switch (name[0]) {
#ifdef notyet
	case IPCTL_DEFMTU:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &ip_mtu));
#endif
	case IPCTL_SOURCEROUTE:
		/*
		 * Don't allow this to change in a secure environment.
		 */
		if (newp && securelevel > 0)
			return (EPERM);
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &ip_dosourceroute));
	case IPCTL_MTUDISC:
		error = sysctl_int(oldp, oldlenp, newp, newlen,
		    &ip_mtudisc);
		if (ip_mtudisc != 0 && ip_mtudisc_timeout_q == NULL) {
			ip_mtudisc_timeout_q =
			    rt_timer_queue_create(ip_mtudisc_timeout);
		} else if (ip_mtudisc == 0 && ip_mtudisc_timeout_q != NULL) {
			s = splsoftnet();
			rt_timer_queue_destroy(ip_mtudisc_timeout_q);
			ip_mtudisc_timeout_q = NULL;
			splx(s);
		}
		return error;
	case IPCTL_MTUDISCTIMEOUT:
		error = sysctl_int(oldp, oldlenp, newp, newlen,
		   &ip_mtudisc_timeout);
		if (ip_mtudisc_timeout_q != NULL) {
			s = splsoftnet();
			rt_timer_queue_change(ip_mtudisc_timeout_q,
					      ip_mtudisc_timeout);
			splx(s);
		}
		return (error);
	case IPCTL_IPSEC_ENC_ALGORITHM:
	        return (sysctl_tstring(oldp, oldlenp, newp, newlen,
				       ipsec_def_enc, sizeof(ipsec_def_enc)));
	case IPCTL_IPSEC_AUTH_ALGORITHM:
	        return (sysctl_tstring(oldp, oldlenp, newp, newlen,
				       ipsec_def_auth,
				       sizeof(ipsec_def_auth)));
	case IPCTL_IPSEC_IPCOMP_ALGORITHM:
	        return (sysctl_tstring(oldp, oldlenp, newp, newlen,
				       ipsec_def_comp,
				       sizeof(ipsec_def_comp)));
	case IPCTL_IFQUEUE:
	        return (sysctl_ifq(name + 1, namelen - 1,
		    oldp, oldlenp, newp, newlen, &ipintrq));
	case IPCTL_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &ipstat, sizeof(ipstat)));
	case IPCTL_MRTSTATS:
#ifdef MROUTING
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &mrtstat, sizeof(mrtstat)));
#else
		return (EOPNOTSUPP);
#endif
	case IPCTL_MRTPROTO:
#ifdef MROUTING
		return (sysctl_rdint(oldp, oldlenp, newp, ip_mrtproto));
#else
		return (EOPNOTSUPP);
#endif
	default:
		if (name[0] < IPCTL_MAXID)
			return (sysctl_int_arr(ipctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen));
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

void
ip_savecontrol(struct inpcb *inp, struct mbuf **mp, struct ip *ip,
    struct mbuf *m)
{
#ifdef SO_TIMESTAMP
	if (inp->inp_socket->so_options & SO_TIMESTAMP) {
		struct timeval tv;

		microtime(&tv);
		*mp = sbcreatecontrol((caddr_t) &tv, sizeof(tv),
		    SCM_TIMESTAMP, SOL_SOCKET);
		if (*mp)
			mp = &(*mp)->m_next;
	}
#endif
	if (inp->inp_flags & INP_RECVDSTADDR) {
		*mp = sbcreatecontrol((caddr_t) &ip->ip_dst,
		    sizeof(struct in_addr), IP_RECVDSTADDR, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
#ifdef notyet
	/* this code is broken and will probably never be fixed. */
	/* options were tossed already */
	if (inp->inp_flags & INP_RECVOPTS) {
		*mp = sbcreatecontrol((caddr_t) opts_deleted_above,
		    sizeof(struct in_addr), IP_RECVOPTS, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	/* ip_srcroute doesn't do what we want here, need to fix */
	if (inp->inp_flags & INP_RECVRETOPTS) {
		*mp = sbcreatecontrol((caddr_t) ip_srcroute(m),
		    sizeof(struct in_addr), IP_RECVRETOPTS, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
#endif
	if (inp->inp_flags & INP_RECVIF) {
		struct sockaddr_dl sdl;
		struct ifnet *ifp;

		ifp = m->m_pkthdr.rcvif;
		if (ifp == NULL || ifp->if_sadl == NULL) {
			memset(&sdl, 0, sizeof(sdl));
			sdl.sdl_len = offsetof(struct sockaddr_dl, sdl_data[0]);
			sdl.sdl_family = AF_LINK;
			sdl.sdl_index = ifp != NULL ? ifp->if_index : 0;
			sdl.sdl_nlen = sdl.sdl_alen = sdl.sdl_slen = 0;
			*mp = sbcreatecontrol((caddr_t) &sdl, sdl.sdl_len,
			    IP_RECVIF, IPPROTO_IP);
		} else {
			*mp = sbcreatecontrol((caddr_t) ifp->if_sadl,
			    ifp->if_sadl->sdl_len, IP_RECVIF, IPPROTO_IP);
		}
		if (*mp)
			mp = &(*mp)->m_next;
	}
	if (inp->inp_flags & INP_RECVTTL) {
		*mp = sbcreatecontrol((caddr_t) &ip->ip_ttl,
		    sizeof(u_int8_t), IP_RECVTTL, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	if (inp->inp_flags & INP_RECVRTABLE) {
		u_int rtableid = inp->inp_rtableid;
#if NPF > 0
		struct pf_divert *divert;

		if (m && m->m_pkthdr.pf.flags & PF_TAG_DIVERTED &&
		    (divert = pf_find_divert(m)) != NULL)
			rtableid = divert->rdomain;
#endif

		*mp = sbcreatecontrol((caddr_t) &rtableid,
		    sizeof(u_int), IP_RECVRTABLE, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
}

