/*	$OpenBSD: tcp_subr.c,v 1.129 2014/04/21 12:22:26 henning Exp $	*/
/*	$NetBSD: tcp_subr.c,v 1.22 1996/02/13 23:44:00 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *	@(#)COPYRIGHT	1.1 (NRL) 17 January 1995
 *
 * NRL grants permission for redistribution and use in source and binary
 * forms, with or without modification, of the software and documentation
 * created at NRL provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 	This product includes software developed at the Information
 * 	Technology Division, US Naval Research Laboratory.
 * 4. Neither the name of the NRL nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the US Naval
 * Research Laboratory (NRL).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/timeout.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/pool.h>

#include <net/route.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <dev/rndvar.h>

#ifdef INET6
#include <netinet6/ip6protosw.h>
#endif /* INET6 */

#include <crypto/md5.h>

/* patchable/settable parameters for tcp */
int	tcp_mssdflt = TCP_MSS;
int	tcp_rttdflt = TCPTV_SRTTDFLT / PR_SLOWHZ;

/* values controllable via sysctl */
int	tcp_do_rfc1323 = 1;
#ifdef TCP_SACK
int	tcp_do_sack = 1;	/* RFC 2018 selective ACKs */
#endif
int	tcp_ack_on_push = 0;	/* set to enable immediate ACK-on-PUSH */
#ifdef TCP_ECN
int	tcp_do_ecn = 0;		/* RFC3168 ECN enabled/disabled? */
#endif
int	tcp_do_rfc3390 = 2;	/* Increase TCP's Initial Window to 10*mss */

u_int32_t	tcp_now = 1;

#ifndef TCBHASHSIZE
#define	TCBHASHSIZE	128
#endif
int	tcbhashsize = TCBHASHSIZE;

/* syn hash parameters */
#define	TCP_SYN_HASH_SIZE	293
#define	TCP_SYN_BUCKET_SIZE	35
int	tcp_syn_cache_size = TCP_SYN_HASH_SIZE;
int	tcp_syn_cache_limit = TCP_SYN_HASH_SIZE*TCP_SYN_BUCKET_SIZE;
int	tcp_syn_bucket_limit = 3*TCP_SYN_BUCKET_SIZE;
struct	syn_cache_head tcp_syn_cache[TCP_SYN_HASH_SIZE];

int tcp_reass_limit = NMBCLUSTERS / 2; /* hardlimit for tcpqe_pool */
#ifdef TCP_SACK
int tcp_sackhole_limit = 32*1024; /* hardlimit for sackhl_pool */
#endif

struct pool tcpcb_pool;
struct pool tcpqe_pool;
#ifdef TCP_SACK
struct pool sackhl_pool;
#endif

struct tcpstat tcpstat;		/* tcp statistics */
tcp_seq  tcp_iss;

/*
 * Tcp initialization
 */
void
tcp_init()
{
	tcp_iss = 1;		/* wrong */
	pool_init(&tcpcb_pool, sizeof(struct tcpcb), 0, 0, 0, "tcpcbpl",
	    NULL);
	pool_init(&tcpqe_pool, sizeof(struct tcpqent), 0, 0, 0, "tcpqepl",
	    NULL);
	pool_sethardlimit(&tcpqe_pool, tcp_reass_limit, NULL, 0);
#ifdef TCP_SACK
	pool_init(&sackhl_pool, sizeof(struct sackhole), 0, 0, 0, "sackhlpl",
	    NULL);
	pool_sethardlimit(&sackhl_pool, tcp_sackhole_limit, NULL, 0);
#endif /* TCP_SACK */
	in_pcbinit(&tcbtable, tcbhashsize);

#ifdef INET6
	/*
	 * Since sizeof(struct ip6_hdr) > sizeof(struct ip), we
	 * do max length checks/computations only on the former.
	 */
	if (max_protohdr < (sizeof(struct ip6_hdr) + sizeof(struct tcphdr)))
		max_protohdr = (sizeof(struct ip6_hdr) + sizeof(struct tcphdr));
	if ((max_linkhdr + sizeof(struct ip6_hdr) + sizeof(struct tcphdr)) >
	    MHLEN)
		panic("tcp_init");

	icmp6_mtudisc_callback_register(tcp6_mtudisc_callback);
#endif /* INET6 */

	/* Initialize the compressed state engine. */
	syn_cache_init();

	/* Initialize timer state. */
	tcp_timer_init();
}

/*
 * Create template to be used to send tcp packets on a connection.
 * Call after host entry created, allocates an mbuf and fills
 * in a skeletal tcp/ip header, minimizing the amount of work
 * necessary when the connection is used.
 *
 * To support IPv6 in addition to IPv4 and considering that the sizes of
 * the IPv4 and IPv6 headers are not the same, we now use a separate pointer
 * for the TCP header.  Also, we made the former tcpiphdr header pointer
 * into just an IP overlay pointer, with casting as appropriate for v6. rja
 */
struct mbuf *
tcp_template(tp)
	struct tcpcb *tp;
{
	struct inpcb *inp = tp->t_inpcb;
	struct mbuf *m;
	struct tcphdr *th;

	if ((m = tp->t_template) == 0) {
		m = m_get(M_DONTWAIT, MT_HEADER);
		if (m == NULL)
			return (0);

		switch (tp->pf) {
		case 0:	/*default to PF_INET*/
#ifdef INET
		case AF_INET:
			m->m_len = sizeof(struct ip);
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			m->m_len = sizeof(struct ip6_hdr);
			break;
#endif /* INET6 */
		}
		m->m_len += sizeof (struct tcphdr);

		/*
		 * The link header, network header, TCP header, and TCP options
		 * all must fit in this mbuf. For now, assume the worst case of
		 * TCP options size. Eventually, compute this from tp flags.
		 */
		if (m->m_len + MAX_TCPOPTLEN + max_linkhdr >= MHLEN) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_free(m);
				return (0);
			}
		}
	}

	switch(tp->pf) {
#ifdef INET
	case AF_INET:
		{
			struct ipovly *ipovly;

			ipovly = mtod(m, struct ipovly *);

			bzero(ipovly->ih_x1, sizeof ipovly->ih_x1);
			ipovly->ih_pr = IPPROTO_TCP;
			ipovly->ih_len = htons(sizeof (struct tcphdr));
			ipovly->ih_src = inp->inp_laddr;
			ipovly->ih_dst = inp->inp_faddr;

			th = (struct tcphdr *)(mtod(m, caddr_t) +
				sizeof(struct ip));
		}
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		{
			struct ip6_hdr *ip6;

			ip6 = mtod(m, struct ip6_hdr *);

			ip6->ip6_src = inp->inp_laddr6;
			ip6->ip6_dst = inp->inp_faddr6;
			ip6->ip6_flow = htonl(0x60000000) |
			    (inp->inp_flowinfo & IPV6_FLOWLABEL_MASK);

			ip6->ip6_nxt = IPPROTO_TCP;
			ip6->ip6_plen = htons(sizeof(struct tcphdr)); /*XXX*/
			ip6->ip6_hlim = in6_selecthlim(inp, NULL);	/*XXX*/

			th = (struct tcphdr *)(mtod(m, caddr_t) +
				sizeof(struct ip6_hdr));
		}
		break;
#endif /* INET6 */
	}

	th->th_sport = inp->inp_lport;
	th->th_dport = inp->inp_fport;
	th->th_seq = 0;
	th->th_ack = 0;
	th->th_x2  = 0;
	th->th_off = 5;
	th->th_flags = 0;
	th->th_win = 0;
	th->th_urp = 0;
	th->th_sum = 0;
	return (m);
}

/*
 * Send a single message to the TCP at address specified by
 * the given TCP/IP header.  If m == 0, then we make a copy
 * of the tcpiphdr at ti and send directly to the addressed host.
 * This is used to force keep alive messages out using the TCP
 * template for a connection tp->t_template.  If flags are given
 * then we send a message back to the TCP which originated the
 * segment ti, and discard the mbuf containing it and any other
 * attached mbufs.
 *
 * In any case the ack and sequence number of the transmitted
 * segment are as specified by the parameters.
 */
#ifdef INET6
/* This function looks hairy, because it was so IPv4-dependent. */
#endif /* INET6 */
void
tcp_respond(struct tcpcb *tp, caddr_t template, struct tcphdr *th0,
    tcp_seq ack, tcp_seq seq, int flags, u_int rtableid)
{
	int tlen;
	int win = 0;
	struct mbuf *m = 0;
	struct route *ro = 0;
	struct tcphdr *th;
	struct ip *ip;
#ifdef INET6
	struct ip6_hdr *ip6;
#endif
	int af;		/* af on wire */

	if (tp) {
		win = sbspace(&tp->t_inpcb->inp_socket->so_rcv);
		/*
		 * If this is called with an unconnected
		 * socket/tp/pcb (tp->pf is 0), we lose.
		 */
		af = tp->pf;

		/*
		 * The route/route6 distinction is meaningless
		 * unless you're allocating space or passing parameters.
		 */
		ro = &tp->t_inpcb->inp_route;
	} else
		af = (((struct ip *)template)->ip_v == 6) ? AF_INET6 : AF_INET;

	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		return;
	m->m_data += max_linkhdr;
	tlen = 0;

#define xchg(a,b,type) do { type t; t=a; a=b; b=t; } while (0)
	switch (af) {
#ifdef INET6
	case AF_INET6:
		ip6 = mtod(m, struct ip6_hdr *);
		th = (struct tcphdr *)(ip6 + 1);
		tlen = sizeof(*ip6) + sizeof(*th);
		if (th0) {
			bcopy(template, ip6, sizeof(*ip6));
			bcopy(th0, th, sizeof(*th));
			xchg(ip6->ip6_dst, ip6->ip6_src, struct in6_addr);
		} else {
			bcopy(template, ip6, tlen);
		}
		break;
#endif /* INET6 */
	case AF_INET:
		ip = mtod(m, struct ip *);
		th = (struct tcphdr *)(ip + 1);
		tlen = sizeof(*ip) + sizeof(*th);
		if (th0) {
			bcopy(template, ip, sizeof(*ip));
			bcopy(th0, th, sizeof(*th));
			xchg(ip->ip_dst.s_addr, ip->ip_src.s_addr, u_int32_t);
		} else {
			bcopy(template, ip, tlen);
		}
		break;
	}
	if (th0)
		xchg(th->th_dport, th->th_sport, u_int16_t);
	else
		flags = TH_ACK;
#undef xchg

	m->m_len = tlen;
	m->m_pkthdr.len = tlen;
	m->m_pkthdr.rcvif = (struct ifnet *) 0;
	m->m_pkthdr.csum_flags |= M_TCP_CSUM_OUT;
	th->th_seq = htonl(seq);
	th->th_ack = htonl(ack);
	th->th_x2 = 0;
	th->th_off = sizeof (struct tcphdr) >> 2;
	th->th_flags = flags;
	if (tp)
		win >>= tp->rcv_scale;
	if (win > TCP_MAXWIN)
		win = TCP_MAXWIN;
	th->th_win = htons((u_int16_t)win);
	th->th_urp = 0;

	/* force routing table */
	if (tp)
		m->m_pkthdr.ph_rtableid = tp->t_inpcb->inp_rtableid;
	else
		m->m_pkthdr.ph_rtableid = rtableid;

	switch (af) {
#ifdef INET6
	case AF_INET6:
		ip6->ip6_flow = htonl(0x60000000);
		ip6->ip6_nxt  = IPPROTO_TCP;
		ip6->ip6_hlim = in6_selecthlim(tp ? tp->t_inpcb : NULL, NULL);	/*XXX*/
		ip6->ip6_plen = tlen - sizeof(struct ip6_hdr);
		HTONS(ip6->ip6_plen);
		ip6_output(m, tp ? tp->t_inpcb->inp_outputopts6 : NULL,
		    (struct route_in6 *)ro, 0, NULL, NULL,
		    tp ? tp->t_inpcb : NULL);
		break;
#endif /* INET6 */
	case AF_INET:
		ip->ip_len = htons(tlen);
		ip->ip_ttl = ip_defttl;
		ip->ip_tos = 0;
		ip_output(m, NULL, ro, ip_mtudisc ? IP_MTUDISC : 0,
		    NULL, tp ? tp->t_inpcb : NULL, 0);
	}
}

/*
 * Create a new TCP control block, making an
 * empty reassembly queue and hooking it to the argument
 * protocol control block.
 */
struct tcpcb *
tcp_newtcpcb(struct inpcb *inp)
{
	struct tcpcb *tp;
	int i;

	tp = pool_get(&tcpcb_pool, PR_NOWAIT|PR_ZERO);
	if (tp == NULL)
		return (NULL);
	TAILQ_INIT(&tp->t_segq);
	tp->t_maxseg = tcp_mssdflt;
	tp->t_maxopd = 0;

	TCP_INIT_DELACK(tp);
	for (i = 0; i < TCPT_NTIMERS; i++)
		TCP_TIMER_INIT(tp, i);
	timeout_set(&tp->t_reap_to, tcp_reaper, tp);

#ifdef TCP_SACK
	tp->sack_enable = tcp_do_sack;
#endif
	tp->t_flags = tcp_do_rfc1323 ? (TF_REQ_SCALE|TF_REQ_TSTMP) : 0;
	tp->t_inpcb = inp;
	/*
	 * Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
	 * rtt estimate.  Set rttvar so that srtt + 2 * rttvar gives
	 * reasonable initial retransmit time.
	 */
	tp->t_srtt = TCPTV_SRTTBASE;
	tp->t_rttvar = tcp_rttdflt * PR_SLOWHZ <<
	    (TCP_RTTVAR_SHIFT + TCP_RTT_BASE_SHIFT - 1);
	tp->t_rttmin = TCPTV_MIN;
	TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp),
	    TCPTV_MIN, TCPTV_REXMTMAX);
	tp->snd_cwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->snd_ssthresh = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	
	tp->t_pmtud_mtu_sent = 0;
	tp->t_pmtud_mss_acked = 0;
	
#ifdef INET6
	/* we disallow IPv4 mapped address completely. */
	if ((inp->inp_flags & INP_IPV6) == 0)
		tp->pf = PF_INET;
	else
		tp->pf = PF_INET6;
#else
	tp->pf = PF_INET;
#endif

#ifdef INET6
	if (inp->inp_flags & INP_IPV6)
		inp->inp_ipv6.ip6_hlim = ip6_defhlim;
	else
#endif /* INET6 */
		inp->inp_ip.ip_ttl = ip_defttl;

	inp->inp_ppcb = (caddr_t)tp;
	return (tp);
}

/*
 * Drop a TCP connection, reporting
 * the specified error.  If connection is synchronized,
 * then send a RST to peer.
 */
struct tcpcb *
tcp_drop(tp, errno)
	struct tcpcb *tp;
	int errno;
{
	struct socket *so = tp->t_inpcb->inp_socket;

	if (TCPS_HAVERCVDSYN(tp->t_state)) {
		tp->t_state = TCPS_CLOSED;
		(void) tcp_output(tp);
		tcpstat.tcps_drops++;
	} else
		tcpstat.tcps_conndrops++;
	if (errno == ETIMEDOUT && tp->t_softerror)
		errno = tp->t_softerror;
	so->so_error = errno;
	return (tcp_close(tp));
}

/*
 * Close a TCP control block:
 *	discard all space held by the tcp
 *	discard internet protocol block
 *	wake up any sleepers
 */
struct tcpcb *
tcp_close(struct tcpcb *tp)
{
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp->inp_socket;
#ifdef TCP_SACK
	struct sackhole *p, *q;
#endif

	/* free the reassembly queue, if any */
	tcp_freeq(tp);

	tcp_canceltimers(tp);
	TCP_CLEAR_DELACK(tp);
	syn_cache_cleanup(tp);

#ifdef TCP_SACK
	/* Free SACK holes. */
	q = p = tp->snd_holes;
	while (p != 0) {
		q = p->next;
		pool_put(&sackhl_pool, p);
		p = q;
	}
#endif
	if (tp->t_template)
		(void) m_free(tp->t_template);

	tp->t_flags |= TF_DEAD;
	timeout_add(&tp->t_reap_to, 0);

	inp->inp_ppcb = 0;
	soisdisconnected(so);
	in_pcbdetach(inp);
	return (NULL);
}

void
tcp_reaper(void *arg)
{
	struct tcpcb *tp = arg;
	int s;

	s = splsoftnet();
	pool_put(&tcpcb_pool, tp);
	splx(s);
	tcpstat.tcps_closed++;
}

int
tcp_freeq(struct tcpcb *tp)
{
	struct tcpqent *qe;
	int rv = 0;

	while ((qe = TAILQ_FIRST(&tp->t_segq)) != NULL) {
		TAILQ_REMOVE(&tp->t_segq, qe, tcpqe_q);
		m_freem(qe->tcpqe_m);
		pool_put(&tcpqe_pool, qe);
		rv = 1;
	}
	return (rv);
}

/*
 * Compute proper scaling value for receiver window from buffer space
 */

void
tcp_rscale(struct tcpcb *tp, u_long hiwat)
{
	tp->request_r_scale = 0;
	while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
	       TCP_MAXWIN << tp->request_r_scale < hiwat)
		tp->request_r_scale++;
}

/*
 * Notify a tcp user of an asynchronous error;
 * store error as soft error, but wake up user
 * (for now, won't do anything until can select for soft error).
 */
void
tcp_notify(inp, error)
	struct inpcb *inp;
	int error;
{
	struct tcpcb *tp = intotcpcb(inp);
	struct socket *so = inp->inp_socket;

	/*
	 * Ignore some errors if we are hooked up.
	 * If connection hasn't completed, has retransmitted several times,
	 * and receives a second error, give up now.  This is better
	 * than waiting a long time to establish a connection that
	 * can never complete.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	     (error == EHOSTUNREACH || error == ENETUNREACH ||
	      error == EHOSTDOWN)) {
		return;
	} else if (TCPS_HAVEESTABLISHED(tp->t_state) == 0 &&
	    tp->t_rxtshift > 3 && tp->t_softerror)
		so->so_error = error;
	else
		tp->t_softerror = error;
	wakeup((caddr_t) &so->so_timeo);
	sorwakeup(so);
	sowwakeup(so);
}

#ifdef INET6
void
tcp6_ctlinput(int cmd, struct sockaddr *sa, u_int rdomain, void *d)
{
	struct tcphdr th;
	struct tcpcb *tp;
	void (*notify)(struct inpcb *, int) = tcp_notify;
	struct ip6_hdr *ip6;
	const struct sockaddr_in6 *sa6_src = NULL;
	struct sockaddr_in6 *sa6 = satosin6(sa);
	struct inpcb *inp;
	struct mbuf *m;
	tcp_seq seq;
	int off;
	struct {
		u_int16_t th_sport;
		u_int16_t th_dport;
		u_int32_t th_seq;
	} *thp;

	if (sa->sa_family != AF_INET6 ||
	    sa->sa_len != sizeof(struct sockaddr_in6) ||
	    IN6_IS_ADDR_UNSPECIFIED(&sa6->sin6_addr) ||
	    IN6_IS_ADDR_V4MAPPED(&sa6->sin6_addr))
		return;
	if ((unsigned)cmd >= PRC_NCMDS)
		return;
	else if (cmd == PRC_QUENCH) {
		/* 
		 * Don't honor ICMP Source Quench messages meant for
		 * TCP connections.
		 */
		/* XXX there's no PRC_QUENCH in IPv6 */
		return;
	} else if (PRC_IS_REDIRECT(cmd))
		notify = in_rtchange, d = NULL;
	else if (cmd == PRC_MSGSIZE)
		; /* special code is present, see below */
	else if (cmd == PRC_HOSTDEAD)
		d = NULL;
	else if (inet6ctlerrmap[cmd] == 0)
		return;

	/* if the parameter is from icmp6, decode it. */
	if (d != NULL) {
		struct ip6ctlparam *ip6cp = (struct ip6ctlparam *)d;
		m = ip6cp->ip6c_m;
		ip6 = ip6cp->ip6c_ip6;
		off = ip6cp->ip6c_off;
		sa6_src = ip6cp->ip6c_src;
	} else {
		m = NULL;
		ip6 = NULL;
		sa6_src = &sa6_any;
	}

	if (ip6) {
		/*
		 * XXX: We assume that when ip6 is non NULL,
		 * M and OFF are valid.
		 */

		/* check if we can safely examine src and dst ports */
		if (m->m_pkthdr.len < off + sizeof(*thp))
			return;

		bzero(&th, sizeof(th));
#ifdef DIAGNOSTIC
		if (sizeof(*thp) > sizeof(th))
			panic("assumption failed in tcp6_ctlinput");
#endif
		m_copydata(m, off, sizeof(*thp), (caddr_t)&th);

		/*
		 * Check to see if we have a valid TCP connection
		 * corresponding to the address in the ICMPv6 message
		 * payload.
		 */
		inp = in6_pcbhashlookup(&tcbtable, &sa6->sin6_addr,
		    th.th_dport, (struct in6_addr *)&sa6_src->sin6_addr,
		    th.th_sport, rdomain);
		if (cmd == PRC_MSGSIZE) {
			/*
			 * Depending on the value of "valid" and routing table
			 * size (mtudisc_{hi,lo}wat), we will:
			 * - recalcurate the new MTU and create the
			 *   corresponding routing entry, or
			 * - ignore the MTU change notification.
			 */
			icmp6_mtudisc_update((struct ip6ctlparam *)d, inp != NULL);
			return;
		}
		if (inp) {
			seq = ntohl(th.th_seq);
			if (inp->inp_socket &&
			    (tp = intotcpcb(inp)) &&
			    SEQ_GEQ(seq, tp->snd_una) &&
			    SEQ_LT(seq, tp->snd_max))
				notify(inp, inet6ctlerrmap[cmd]);
		} else if (syn_cache_count &&
		    (inet6ctlerrmap[cmd] == EHOSTUNREACH ||
		     inet6ctlerrmap[cmd] == ENETUNREACH ||
		     inet6ctlerrmap[cmd] == EHOSTDOWN))
			syn_cache_unreach((struct sockaddr *)sa6_src,
			    sa, &th, rdomain);
	} else {
		(void) in6_pcbnotify(&tcbtable, sa6, 0,
		    sa6_src, 0, rdomain, cmd, NULL, notify);
	}
}
#endif

void *
tcp_ctlinput(int cmd, struct sockaddr *sa, u_int rdomain, void *v)
{
	struct ip *ip = v;
	struct tcphdr *th;
	struct tcpcb *tp;
	struct inpcb *inp;
	struct in_addr faddr;
	tcp_seq seq;
	u_int mtu;
	void (*notify)(struct inpcb *, int) = tcp_notify;
	int errno;

	if (sa->sa_family != AF_INET)
		return NULL;
	faddr = satosin(sa)->sin_addr;
	if (faddr.s_addr == INADDR_ANY)
		return NULL;

	if ((unsigned)cmd >= PRC_NCMDS)
		return NULL;
	errno = inetctlerrmap[cmd];
	if (cmd == PRC_QUENCH)
		/* 
		 * Don't honor ICMP Source Quench messages meant for
		 * TCP connections.
		 */
		return NULL;
	else if (PRC_IS_REDIRECT(cmd))
		notify = in_rtchange, ip = 0;
	else if (cmd == PRC_MSGSIZE && ip_mtudisc && ip) {
		/*
		 * Verify that the packet in the icmp payload refers
		 * to an existing TCP connection.
		 */
		th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		seq = ntohl(th->th_seq);
		inp = in_pcbhashlookup(&tcbtable,
		    ip->ip_dst, th->th_dport, ip->ip_src, th->th_sport,
		    rdomain);
		if (inp && (tp = intotcpcb(inp)) &&
		    SEQ_GEQ(seq, tp->snd_una) &&
		    SEQ_LT(seq, tp->snd_max)) {
			struct icmp *icp;
			icp = (struct icmp *)((caddr_t)ip -
					      offsetof(struct icmp, icmp_ip));

			/* 
			 * If the ICMP message advertises a Next-Hop MTU
			 * equal or larger than the maximum packet size we have
			 * ever sent, drop the message.
			 */
			mtu = (u_int)ntohs(icp->icmp_nextmtu);
			if (mtu >= tp->t_pmtud_mtu_sent)
				return NULL;
			if (mtu >= tcp_hdrsz(tp) + tp->t_pmtud_mss_acked) {
				/* 
				 * Calculate new MTU, and create corresponding
				 * route (traditional PMTUD).
				 */
				tp->t_flags &= ~TF_PMTUD_PEND;
				icmp_mtudisc(icp, inp->inp_rtableid);
			} else {
				/*
				 * Record the information got in the ICMP
				 * message; act on it later.
				 * If we had already recorded an ICMP message,
				 * replace the old one only if the new message
				 * refers to an older TCP segment
				 */
				if (tp->t_flags & TF_PMTUD_PEND) {
					if (SEQ_LT(tp->t_pmtud_th_seq, seq))
						return NULL;
				} else
					tp->t_flags |= TF_PMTUD_PEND;
				tp->t_pmtud_th_seq = seq;
				tp->t_pmtud_nextmtu = icp->icmp_nextmtu;
				tp->t_pmtud_ip_len = icp->icmp_ip.ip_len;
				tp->t_pmtud_ip_hl = icp->icmp_ip.ip_hl;
				return NULL;
			}
		} else {
			/* ignore if we don't have a matching connection */
			return NULL;
		}
		notify = tcp_mtudisc, ip = 0;
	} else if (cmd == PRC_MTUINC)
		notify = tcp_mtudisc_increase, ip = 0;
	else if (cmd == PRC_HOSTDEAD)
		ip = 0;
	else if (errno == 0)
		return NULL;

	if (ip) {
		th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		inp = in_pcbhashlookup(&tcbtable,
		    ip->ip_dst, th->th_dport, ip->ip_src, th->th_sport,
		    rdomain);
		if (inp) {
			seq = ntohl(th->th_seq);
			if (inp->inp_socket &&
			    (tp = intotcpcb(inp)) &&
			    SEQ_GEQ(seq, tp->snd_una) &&
			    SEQ_LT(seq, tp->snd_max))
				notify(inp, errno);
		} else if (syn_cache_count &&
		    (inetctlerrmap[cmd] == EHOSTUNREACH ||
		     inetctlerrmap[cmd] == ENETUNREACH ||
		     inetctlerrmap[cmd] == EHOSTDOWN)) {
			struct sockaddr_in sin;

			bzero(&sin, sizeof(sin));
			sin.sin_len = sizeof(sin);
			sin.sin_family = AF_INET;
			sin.sin_port = th->th_sport;
			sin.sin_addr = ip->ip_src;
			syn_cache_unreach((struct sockaddr *)&sin,
			    sa, th, rdomain);
		}
	} else
		in_pcbnotifyall(&tcbtable, sa, rdomain, errno, notify);

	return NULL;
}


#ifdef INET6
/*
 * Path MTU Discovery handlers.
 */
void
tcp6_mtudisc_callback(sin6, rdomain)
	struct sockaddr_in6 *sin6;
	u_int rdomain;
{
	(void) in6_pcbnotify(&tcbtable, sin6, 0,
	    &sa6_any, 0, rdomain, PRC_MSGSIZE, NULL, tcp_mtudisc);
}
#endif /* INET6 */

/*
 * On receipt of path MTU corrections, flush old route and replace it
 * with the new one.  Retransmit all unacknowledged packets, to ensure
 * that all packets will be received.
 */
void
tcp_mtudisc(inp, errno)
	struct inpcb *inp;
	int errno;
{
	struct tcpcb *tp = intotcpcb(inp);
	struct rtentry *rt = in_pcbrtentry(inp);
	int change = 0;

	if (tp != 0) {
		int orig_maxseg = tp->t_maxseg;
		if (rt != 0) {
			/*
			 * If this was not a host route, remove and realloc.
			 */
			if ((rt->rt_flags & RTF_HOST) == 0) {
				in_rtchange(inp, errno);
				if ((rt = in_pcbrtentry(inp)) == 0)
					return;
			}
			if (orig_maxseg != tp->t_maxseg ||
			    (rt->rt_rmx.rmx_locks & RTV_MTU))
				change = 1;
		}
		tcp_mss(tp, -1);

		/*
		 * Resend unacknowledged packets
		 */
		tp->snd_nxt = tp->snd_una;
		if (change || errno > 0)
			tcp_output(tp);
	}
}

void
tcp_mtudisc_increase(inp, errno)
	struct inpcb *inp;
	int errno;
{
	struct tcpcb *tp = intotcpcb(inp);
	struct rtentry *rt = in_pcbrtentry(inp);

	if (tp != 0 && rt != 0) {
		/*
		 * If this was a host route, remove and realloc.
		 */
		if (rt->rt_flags & RTF_HOST)
			in_rtchange(inp, errno);

		/* also takes care of congestion window */
		tcp_mss(tp, -1);
	}
}

#define TCP_ISS_CONN_INC 4096
int tcp_secret_init;
u_char tcp_secret[16];
MD5_CTX tcp_secret_ctx;

void
tcp_set_iss_tsm(struct tcpcb *tp)
{
	MD5_CTX ctx;
	u_int32_t digest[4];

	if (tcp_secret_init == 0) {
		arc4random_buf(tcp_secret, sizeof(tcp_secret));
		MD5Init(&tcp_secret_ctx);
		MD5Update(&tcp_secret_ctx, tcp_secret, sizeof(tcp_secret));
		tcp_secret_init = 1;
	}
	ctx = tcp_secret_ctx;
	MD5Update(&ctx, (char *)&tp->t_inpcb->inp_lport, sizeof(u_short));
	MD5Update(&ctx, (char *)&tp->t_inpcb->inp_fport, sizeof(u_short));
	if (tp->pf == AF_INET6) {
		MD5Update(&ctx, (char *)&tp->t_inpcb->inp_laddr6,
		    sizeof(struct in6_addr));
		MD5Update(&ctx, (char *)&tp->t_inpcb->inp_faddr6,
		    sizeof(struct in6_addr));
	} else {
		MD5Update(&ctx, (char *)&tp->t_inpcb->inp_laddr,
		    sizeof(struct in_addr));
		MD5Update(&ctx, (char *)&tp->t_inpcb->inp_faddr,
		    sizeof(struct in_addr));
	}
	MD5Final((u_char *)digest, &ctx);
	tcp_iss += TCP_ISS_CONN_INC;
	tp->iss = digest[0] + tcp_iss;
	tp->ts_modulate = digest[1];
}

#ifdef TCP_SIGNATURE
int
tcp_signature_tdb_attach()
{
	return (0);
}

int
tcp_signature_tdb_init(tdbp, xsp, ii)
	struct tdb *tdbp;
	struct xformsw *xsp;
	struct ipsecinit *ii;
{
	if ((ii->ii_authkeylen < 1) || (ii->ii_authkeylen > 80))
		return (EINVAL);

	tdbp->tdb_amxkey = malloc(ii->ii_authkeylen, M_XDATA, M_NOWAIT);
	if (tdbp->tdb_amxkey == NULL)
		return (ENOMEM);
	bcopy(ii->ii_authkey, tdbp->tdb_amxkey, ii->ii_authkeylen);
	tdbp->tdb_amxkeylen = ii->ii_authkeylen;

	return (0);
}

int
tcp_signature_tdb_zeroize(tdbp)
	struct tdb *tdbp;
{
	if (tdbp->tdb_amxkey) {
		explicit_bzero(tdbp->tdb_amxkey, tdbp->tdb_amxkeylen);
		free(tdbp->tdb_amxkey, M_XDATA);
		tdbp->tdb_amxkey = NULL;
	}

	return (0);
}

int
tcp_signature_tdb_input(m, tdbp, skip, protoff)
	struct mbuf *m;
	struct tdb *tdbp;
	int skip, protoff;
{
	return (0);
}

int
tcp_signature_tdb_output(m, tdbp, mp, skip, protoff)
	struct mbuf *m;
	struct tdb *tdbp;
	struct mbuf **mp;
	int skip, protoff;
{
	return (EINVAL);
}

int
tcp_signature_apply(fstate, data, len)
	caddr_t fstate;
	caddr_t data;
	unsigned int len;
{
	MD5Update((MD5_CTX *)fstate, (char *)data, len);
	return 0;
}

int
tcp_signature(struct tdb *tdb, int af, struct mbuf *m, struct tcphdr *th,
    int iphlen, int doswap, char *sig)
{
	MD5_CTX ctx;
	int len;
	struct tcphdr th0;

	MD5Init(&ctx);

	switch(af) {
	case 0:
#ifdef INET
	case AF_INET: {
		struct ippseudo ippseudo;
		struct ip *ip;

		ip = mtod(m, struct ip *);

		ippseudo.ippseudo_src = ip->ip_src;
		ippseudo.ippseudo_dst = ip->ip_dst;
		ippseudo.ippseudo_pad = 0;
		ippseudo.ippseudo_p = IPPROTO_TCP;
		ippseudo.ippseudo_len = htons(m->m_pkthdr.len - iphlen);

		MD5Update(&ctx, (char *)&ippseudo,
		    sizeof(struct ippseudo));
		break;
		}
#endif
#ifdef INET6
	case AF_INET6: {
		struct ip6_hdr_pseudo ip6pseudo;
		struct ip6_hdr *ip6;

		ip6 = mtod(m, struct ip6_hdr *);
		bzero(&ip6pseudo, sizeof(ip6pseudo));
		ip6pseudo.ip6ph_src = ip6->ip6_src;
		ip6pseudo.ip6ph_dst = ip6->ip6_dst;
		in6_clearscope(&ip6pseudo.ip6ph_src);
		in6_clearscope(&ip6pseudo.ip6ph_dst);
		ip6pseudo.ip6ph_nxt = IPPROTO_TCP;
		ip6pseudo.ip6ph_len = htonl(m->m_pkthdr.len - iphlen);

		MD5Update(&ctx, (char *)&ip6pseudo,
		    sizeof(ip6pseudo));
		break;
		}
#endif
	}

	th0 = *th;
	th0.th_sum = 0;

	if (doswap) {
		HTONL(th0.th_seq);
		HTONL(th0.th_ack);
		HTONS(th0.th_win);
		HTONS(th0.th_urp);
	}
	MD5Update(&ctx, (char *)&th0, sizeof(th0));

	len = m->m_pkthdr.len - iphlen - th->th_off * sizeof(uint32_t);

	if (len > 0 &&
	    m_apply(m, iphlen + th->th_off * sizeof(uint32_t), len,
	    tcp_signature_apply, (caddr_t)&ctx))
		return (-1); 

	MD5Update(&ctx, tdb->tdb_amxkey, tdb->tdb_amxkeylen);
	MD5Final(sig, &ctx);

	return (0);
}
#endif /* TCP_SIGNATURE */
