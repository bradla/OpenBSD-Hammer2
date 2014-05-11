/*	$OpenBSD: route.h,v 1.92 2014/04/25 10:41:09 mpi Exp $	*/
/*	$NetBSD: route.h,v 1.9 1996/02/13 22:00:49 christos Exp $	*/

/*
 * Copyright (c) 1980, 1986, 1993
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
 *	@(#)route.h	8.3 (Berkeley) 4/19/94
 */

#ifndef _NET_ROUTE_H_
#define _NET_ROUTE_H_

/*
 * Kernel resident routing tables.
 * 
 * The routing tables are initialized when interface addresses
 * are set by making entries for all directly connected interfaces.
 */

/*
 * These numbers are used by reliable protocols for determining
 * retransmission behavior and are included in the routing structure.
 */
struct rt_kmetrics {
	u_int64_t	rmx_pksent;	/* packets sent using this route */
	int64_t		rmx_expire;	/* lifetime for route, e.g. redirect */
	u_int		rmx_locks;	/* Kernel must leave these values */
	u_int		rmx_mtu;	/* MTU for this path */
};

/*
 * Huge version for userland compatibility.
 */
struct rt_metrics {
	u_int64_t	rmx_pksent;	/* packets sent using this route */
	int64_t		rmx_expire;	/* lifetime for route, e.g. redirect */
	u_int		rmx_locks;	/* Kernel must leave these values */
	u_int		rmx_mtu;	/* MTU for this path */
	u_int		rmx_refcnt;	/* # references hold */
	/* some apps may still need these no longer used metrics */
	u_int		rmx_hopcount;	/* max hops expected */
	u_int		rmx_recvpipe;	/* inbound delay-bandwidth product */
	u_int		rmx_sendpipe;	/* outbound delay-bandwidth product */
	u_int		rmx_ssthresh;	/* outbound gateway buffer limit */
	u_int		rmx_rtt;	/* estimated round trip time */
	u_int		rmx_rttvar;	/* estimated rtt variance */
	u_int		rmx_pad;
};

/*
 * rmx_rtt and rmx_rttvar are stored as microseconds;
 * RTTTOPRHZ(rtt) converts to a value suitable for use
 * by a protocol slowtimo counter.
 */
#define	RTM_RTTUNIT	1000000	/* units for rtt, rttvar, as units per sec */
#define	RTTTOPRHZ(r)	((r) / (RTM_RTTUNIT / PR_SLOWHZ))

#ifdef _KERNEL

#include <sys/queue.h>

/*
 * We distinguish between routes to hosts and routes to networks,
 * preferring the former if available.  For each route we infer
 * the interface to use from the gateway address supplied when
 * the route was entered.  Routes that forward packets through
 * gateways are marked so that the output routines know to address the
 * gateway rather than the ultimate destination.
 */
#include <net/radix.h>
#include <net/radix_mpath.h>

struct rtentry {
	struct	radix_node rt_nodes[2];	/* tree glue, and other values */
#define	rt_key(r)	((struct sockaddr *)((r)->rt_nodes->rn_key))
#define	rt_mask(r)	((struct sockaddr *)((r)->rt_nodes->rn_mask))
	struct	sockaddr *rt_gateway;	/* value */
	u_int	rt_flags;		/* up/down?, host/net */
	int	rt_refcnt;		/* # held references */
	struct	ifnet *rt_ifp;		/* the answer: interface to use */
	struct	ifaddr *rt_ifa;		/* the answer: interface addr to use */
	caddr_t	rt_llinfo;		/* pointer to link level info cache or
					   to an MPLS structure */ 
	struct	rt_kmetrics rt_rmx;	/* metrics used by rx'ing protocols */
	struct	rtentry *rt_gwroute;	/* implied entry for gatewayed routes */
	struct	rtentry *rt_parent;	/* If cloned, parent of this route. */
	LIST_HEAD(, rttimer) rt_timer;  /* queue of timeouts for misc funcs */
	u_int16_t rt_labelid;		/* route label ID */
	u_int8_t rt_priority;		/* routing priority to use */
};
#define	rt_use	rt_rmx.rmx_pksent

#endif /* _KERNEL */

#define	RTF_UP		0x1		/* route usable */
#define	RTF_GATEWAY	0x2		/* destination is a gateway */
#define	RTF_HOST	0x4		/* host entry (net otherwise) */
#define	RTF_REJECT	0x8		/* host or net unreachable */
#define	RTF_DYNAMIC	0x10		/* created dynamically (by redirect) */
#define	RTF_MODIFIED	0x20		/* modified dynamically (by redirect) */
#define RTF_DONE	0x40		/* message confirmed */
#define RTF_MASK	0x80		/* subnet mask present */
#define RTF_CLONING	0x100		/* generate new routes on use */
#define RTF_XRESOLVE	0x200		/* external daemon resolves name */
#define RTF_LLINFO	0x400		/* generated by ARP or ESIS */
#define RTF_STATIC	0x800		/* manually added */
#define RTF_BLACKHOLE	0x1000		/* just discard pkts (during updates) */
#define RTF_PROTO3	0x2000		/* protocol specific routing flag */
#define RTF_PROTO2	0x4000		/* protocol specific routing flag */
#define RTF_PROTO1	0x8000		/* protocol specific routing flag */
#define RTF_CLONED	0x10000		/* this is a cloned route */
#define RTF_MPATH	0x40000		/* multipath route or operation */
#define RTF_MPLS	0x100000	/* MPLS additional infos */

/* mask of RTF flags that are allowed to be modified by RTM_CHANGE */
#define RTF_FMASK	\
    (RTF_PROTO1 | RTF_PROTO2 | RTF_PROTO3 | RTF_BLACKHOLE | \
     RTF_REJECT | RTF_STATIC | RTF_MPLS)

#ifndef _KERNEL
/* obsoleted */
#define RTF_SOURCE	0x20000		/* this route has a source selector */
#define RTF_TUNNEL	0x100000	/* Tunnelling bit. */
#endif

/* Routing priorities used by the different routing protocols */
#define RTP_NONE	0	/* unset priority use sane default */
#define RTP_CONNECTED	4	/* directly connected routes */
#define RTP_STATIC	8	/* static routes base priority */
#define RTP_OSPF	32	/* OSPF routes */
#define RTP_ISIS	36	/* IS-IS routes */
#define RTP_RIP		40	/* RIP routes */
#define RTP_BGP		48	/* BGP routes */
#define RTP_DEFAULT	56	/* routes that have nothing set */
#define RTP_MAX		63	/* maximum priority */
#define RTP_ANY		64	/* any of the above */
#define RTP_MASK	0x7f
#define RTP_DOWN	0x80	/* route/link is down */

/*
 * Routing statistics.
 */
struct	rtstat {
	u_int32_t rts_badredirect;	/* bogus redirect calls */
	u_int32_t rts_dynamic;		/* routes created by redirects */
	u_int32_t rts_newgateway;	/* routes modified by redirects */
	u_int32_t rts_unreach;		/* lookups which failed */
	u_int32_t rts_wildcard;		/* lookups satisfied by a wildcard */
};

/*
 * Routing Table Info.
 */
struct rt_tableinfo {
	u_short rti_tableid;	/* routing table id */
	u_short rti_domainid;	/* routing domain id */
};

/*
 * Structures for routing messages.
 */
struct rt_msghdr {
	u_short	rtm_msglen;	/* to skip over non-understood messages */
	u_char	rtm_version;	/* future binary compatibility */
	u_char	rtm_type;	/* message type */
	u_short	rtm_hdrlen;	/* sizeof(rt_msghdr) to skip over the header */
	u_short	rtm_index;	/* index for associated ifp */
	u_short rtm_tableid;	/* routing table id */
	u_char	rtm_priority;	/* routing priority */
	u_char	rtm_mpls;	/* MPLS additional infos */
	int	rtm_addrs;	/* bitmask identifying sockaddrs in msg */
	int	rtm_flags;	/* flags, incl. kern & message, e.g. DONE */
	int	rtm_fmask;	/* bitmask used in RTM_CHANGE message */
	pid_t	rtm_pid;	/* identify sender */
	int	rtm_seq;	/* for sender to identify action */
	int	rtm_errno;	/* why failed */
	u_int	rtm_inits;	/* which metrics we are initializing */
	struct	rt_metrics rtm_rmx; /* metrics themselves */
};
/* overload no longer used field */
#define rtm_use	rtm_rmx.rmx_pksent

#define RTM_VERSION	5	/* Up the ante and ignore older versions */

#define RTM_MAXSIZE	2048	/* Maximum size of an accepted route msg */

#define RTM_ADD		0x1	/* Add Route */
#define RTM_DELETE	0x2	/* Delete Route */
#define RTM_CHANGE	0x3	/* Change Metrics or flags */
#define RTM_GET		0x4	/* Report Metrics */
#define RTM_LOSING	0x5	/* Kernel Suspects Partitioning */
#define RTM_REDIRECT	0x6	/* Told to use different route */
#define RTM_MISS	0x7	/* Lookup failed on this address */
#define RTM_LOCK	0x8	/* fix specified metrics */
#define RTM_RESOLVE	0xb	/* req to resolve dst to LL addr */
#define RTM_NEWADDR	0xc	/* address being added to iface */
#define RTM_DELADDR	0xd	/* address being removed from iface */
#define RTM_IFINFO	0xe	/* iface going up/down etc. */
#define RTM_IFANNOUNCE	0xf	/* iface arrival/departure */
#define RTM_DESYNC	0x10	/* route socket buffer overflow */

#define RTV_MTU		0x1	/* init or lock _mtu */
#define RTV_HOPCOUNT	0x2	/* init or lock _hopcount */
#define RTV_EXPIRE	0x4	/* init or lock _hopcount */
#define RTV_RPIPE	0x8	/* init or lock _recvpipe */
#define RTV_SPIPE	0x10	/* init or lock _sendpipe */
#define RTV_SSTHRESH	0x20	/* init or lock _ssthresh */
#define RTV_RTT		0x40	/* init or lock _rtt */
#define RTV_RTTVAR	0x80	/* init or lock _rttvar */

/*
 * Bitmask values for rtm_addr.
 */
#define RTA_DST		0x1	/* destination sockaddr present */
#define RTA_GATEWAY	0x2	/* gateway sockaddr present */
#define RTA_NETMASK	0x4	/* netmask sockaddr present */
#define RTA_GENMASK	0x8	/* cloning mask sockaddr present */
#define RTA_IFP		0x10	/* interface name sockaddr present */
#define RTA_IFA		0x20	/* interface addr sockaddr present */
#define RTA_AUTHOR	0x40	/* sockaddr for author of redirect */
#define RTA_BRD		0x80	/* for NEWADDR, broadcast or p-p dest addr */
#define RTA_SRC		0x100	/* source sockaddr present */
#define RTA_SRCMASK	0x200	/* source netmask present */
#define	RTA_LABEL	0x400	/* route label present */

/*
 * Index offsets for sockaddr array for alternate internal encoding.
 */
#define RTAX_DST	0	/* destination sockaddr present */
#define RTAX_GATEWAY	1	/* gateway sockaddr present */
#define RTAX_NETMASK	2	/* netmask sockaddr present */
#define RTAX_GENMASK	3	/* cloning mask sockaddr present */
#define RTAX_IFP	4	/* interface name sockaddr present */
#define RTAX_IFA	5	/* interface addr sockaddr present */
#define RTAX_AUTHOR	6	/* sockaddr for author of redirect */
#define RTAX_BRD	7	/* for NEWADDR, broadcast or p-p dest addr */
#define RTAX_SRC	8	/* source sockaddr present */
#define RTAX_SRCMASK	9	/* source netmask present */
#define RTAX_LABEL	10	/* route label present */
#define RTAX_MAX	11	/* size of array to allocate */

/*
 * setsockopt defines used for the filtering.
 */
#define ROUTE_MSGFILTER	1	/* bitmask to specifiy which types should be
				   sent to the client. */
#define ROUTE_TABLEFILTER 2	/* change routing table the socket is listening
				   on, RTABLE_ANY listens on all tables. */

#define ROUTE_FILTER(m)	(1 << (m))
#define RTABLE_ANY	0xffffffff

#define	RTLABEL_LEN	32

struct sockaddr_rtlabel {
	u_int8_t	sr_len;			/* total length */
	sa_family_t	sr_family;		/* address family */
	char		sr_label[RTLABEL_LEN];
};

/*
 * A route consists of a destination address and a reference
 * to a routing entry.  These are often held by protocols
 * in their control blocks, e.g. inpcb.
 */
struct route {
	struct	rtentry *ro_rt;
	u_long		 ro_tableid;	/* u_long because of alignment */
	struct	sockaddr ro_dst;
};

struct rt_addrinfo {
	int	rti_addrs;
	struct	sockaddr *rti_info[RTAX_MAX];
	int	rti_flags;
	struct	ifaddr *rti_ifa;
	struct	rt_msghdr *rti_rtm;
	u_char	rti_mpls;
};

#ifdef _KERNEL

/* 
 * This structure, and the prototypes for the rt_timer_{init,remove_all,
 * add,timer} functions all used with the kind permission of BSDI.
 * These allow functions to be called for routes at specific times.
 */
struct rttimer {
	TAILQ_ENTRY(rttimer)	rtt_next;  /* entry on timer queue */
	LIST_ENTRY(rttimer) 	rtt_link;  /* multiple timers per rtentry */
	struct rttimer_queue	*rtt_queue;/* back pointer to queue */
	struct rtentry  	*rtt_rt;   /* Back pointer to the route */
	void            	(*rtt_func)(struct rtentry *, 
						 struct rttimer *);
	time_t          	rtt_time; /* When this timer was registered */
	u_int			rtt_tableid;	/* routing table id of rtt_rt */
};

struct rttimer_queue {
	long				rtq_timeout;
	unsigned long			rtq_count;
	TAILQ_HEAD(, rttimer)		rtq_head;
	LIST_ENTRY(rttimer_queue)	rtq_link;
};

const char	*rtlabel_id2name(u_int16_t);
u_int16_t	 rtlabel_name2id(char *);
struct sockaddr	*rtlabel_id2sa(u_int16_t, struct sockaddr_rtlabel *);
void		 rtlabel_unref(u_int16_t);

#define	RTFREE(rt) do {							\
	if ((rt)->rt_refcnt <= 1)					\
		rtfree(rt);						\
	else								\
		(rt)->rt_refcnt--;					\
} while (/* CONSTCOND */0)

/*
 * Values for additional argument to rtalloc1()
 */
#define	RT_REPORT	0x1
#define	RT_NOCLONING	0x2

struct route_cb {
	int	ip_count;
	int	ip6_count;
	int     mpls_count;
	int	any_count;
};

extern struct route_cb route_cb;
extern struct rtstat rtstat;
extern const struct sockaddr_rtin rt_defmask4;

struct	socket;
void	 route_init(void);
int	 rtable_add(u_int);
struct radix_node_head *rtable_get(u_int, sa_family_t);
u_int	 rtable_l2(u_int);
void	 rtable_l2set(u_int, u_int);
int	 rtable_exists(u_int);

int	 route_output(struct mbuf *, ...);
int	 route_usrreq(struct socket *, int, struct mbuf *,
			   struct mbuf *, struct mbuf *, struct proc *);
void	 rt_ifmsg(struct ifnet *);
void	 rt_ifannouncemsg(struct ifnet *, int);
void	 rt_maskedcopy(struct sockaddr *,
	    struct sockaddr *, struct sockaddr *);
void	 rt_sendmsg(struct rtentry *, int, u_int);
void	 rt_missmsg(int, struct rt_addrinfo *, int, struct ifnet *, int,
	    u_int);
void	 rt_newaddrmsg(int, struct ifaddr *, int, struct rtentry *);
int	 rt_setgate(struct rtentry *, struct sockaddr *,
	    struct sockaddr *, u_int);
void	 rt_setmetrics(u_long, struct rt_metrics *, struct rt_kmetrics *);
void	 rt_getmetrics(struct rt_kmetrics *, struct rt_metrics *);

int			 rt_timer_add(struct rtentry *,
		             void(*)(struct rtentry *, struct rttimer *),
			     struct rttimer_queue *, u_int);
void			 rt_timer_remove_all(struct rtentry *);
struct rttimer_queue	*rt_timer_queue_create(u_int);
void			 rt_timer_queue_change(struct rttimer_queue *, long);
void			 rt_timer_queue_destroy(struct rttimer_queue *);
unsigned long		 rt_timer_queue_count(struct rttimer_queue *);
void			 rt_timer_timer(void *);

void	 rtalloc_noclone(struct route *);
void	 rtalloc(struct route *);
#ifdef SMALL_KERNEL
#define	rtalloc_mpath(r, s)	rtalloc(r)
#endif
struct rtentry *
	 rtalloc1(struct sockaddr *, int, u_int);
void	 rtfree(struct rtentry *);
int	 rt_getifa(struct rt_addrinfo *, u_int);
int	 rt_ifa_add(struct ifaddr *, int, struct sockaddr *);
int	 rt_ifa_del(struct ifaddr *, int, struct sockaddr *);
void	 rt_ifa_addloop(struct ifaddr *);
void	 rt_ifa_delloop(struct ifaddr *);
int	 rtioctl(u_long, caddr_t, struct proc *);
void	 rtredirect(struct sockaddr *, struct sockaddr *,
			 struct sockaddr *, int, struct sockaddr *,
			 struct rtentry **, u_int);
int	 rtrequest1(int, struct rt_addrinfo *, u_int8_t, struct rtentry **,
	     u_int);
void	 rt_if_remove(struct ifnet *);
#ifndef SMALL_KERNEL
void	 rt_if_track(struct ifnet *);
int	 rt_if_linkstate_change(struct radix_node *, void *, u_int);
#endif
int	 rtdeletemsg(struct rtentry *, u_int);

struct rtentry		*rt_lookup(struct sockaddr *, struct sockaddr *, u_int);

struct rtentry *rt_mpath_next(struct rtentry *);
#endif /* _KERNEL */

#endif /* _NET_ROUTE_H_ */
