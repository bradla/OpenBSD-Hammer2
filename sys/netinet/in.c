/*	$OpenBSD: in.c,v 1.96 2014/04/25 09:44:38 mpi Exp $	*/
/*	$NetBSD: in.c,v 1.26 1996/02/13 23:41:39 christos Exp $	*/

/*
 * Copyright (C) 2001 WIDE Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)in.c	8.2 (Berkeley) 11/15/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>

#include "carp.h"
#if NCARP > 0
#include <net/if_types.h>
#endif

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/igmp_var.h>

#ifdef MROUTING
#include <netinet/ip_mroute.h>
#endif

#include "ether.h"

#ifdef INET

void in_socktrim(struct sockaddr_in *);
void in_len2mask(struct in_addr *, int);
int in_lifaddr_ioctl(struct socket *, u_long, caddr_t,
	struct ifnet *);

void in_purgeaddr(struct ifaddr *);
int in_addprefix(struct in_ifaddr *);
int in_scrubprefix(struct in_ifaddr *);
int in_addhost(struct in_ifaddr *);
int in_scrubhost(struct in_ifaddr *);

/*
 * Determine whether an IP address is in a reserved set of addresses
 * that may not be forwarded, or whether datagrams to that destination
 * may be forwarded.
 */
int
in_canforward(struct in_addr in)
{
	u_int32_t net;

	if (IN_EXPERIMENTAL(in.s_addr) || IN_MULTICAST(in.s_addr))
		return (0);
	if (IN_CLASSA(in.s_addr)) {
		net = in.s_addr & IN_CLASSA_NET;
		if (net == 0 ||
		    net == htonl(IN_LOOPBACKNET << IN_CLASSA_NSHIFT))
			return (0);
	}
	return (1);
}

/*
 * Trim a mask in a sockaddr
 */
void
in_socktrim(struct sockaddr_in *ap)
{
	char *cplim = (char *) &ap->sin_addr;
	char *cp = (char *) (&ap->sin_addr + 1);

	ap->sin_len = 0;
	while (--cp >= cplim)
		if (*cp) {
			(ap)->sin_len = cp - (char *) (ap) + 1;
			break;
		}
}

int
in_mask2len(struct in_addr *mask)
{
	int x, y;
	u_char *p;

	p = (u_char *)mask;
	for (x = 0; x < sizeof(*mask); x++) {
		if (p[x] != 0xff)
			break;
	}
	y = 0;
	if (x < sizeof(*mask)) {
		for (y = 0; y < 8; y++) {
			if ((p[x] & (0x80 >> y)) == 0)
				break;
		}
	}
	return x * 8 + y;
}

void
in_len2mask(struct in_addr *mask, int len)
{
	int i;
	u_char *p;

	p = (u_char *)mask;
	bzero(mask, sizeof(*mask));
	for (i = 0; i < len / 8; i++)
		p[i] = 0xff;
	if (len % 8)
		p[i] = (0xff00 >> (len % 8)) & 0xff;
}

/*
 * Generic internet control operations (ioctl's).
 * Ifp is 0 if not an interface-specific ioctl.
 */
/* ARGSUSED */
int
in_control(struct socket *so, u_long cmd, caddr_t data, struct ifnet *ifp)
{
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa;						\
	struct in_ifaddr *ia = NULL;
	struct in_aliasreq *ifra = (struct in_aliasreq *)data;
	struct sockaddr_in oldaddr;
	int error;
	int newifaddr;
	int s;

	switch (cmd) {
	case SIOCALIFADDR:
	case SIOCDLIFADDR:
		if ((so->so_state & SS_PRIV) == 0)
			return (EPERM);
		/* FALLTHROUGH */
	case SIOCGLIFADDR:
		if (!ifp)
			return EINVAL;
		return in_lifaddr_ioctl(so, cmd, data, ifp);
	}

	/*
	 * Find address for this interface, if it exists.
	 */
	if (ifp)
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list)
			if (ifa->ifa_addr->sa_family == AF_INET) {
				ia = ifatoia(ifa);
				break;
			}

	switch (cmd) {

	case SIOCAIFADDR:
	case SIOCDIFADDR:
		if (ifra->ifra_addr.sin_family == AF_INET) {
			for (; ifa != NULL; ifa = TAILQ_NEXT(ifa, ifa_list)) {
				if ((ifa->ifa_addr->sa_family == AF_INET) &&
				    ifatoia(ifa)->ia_addr.sin_addr.s_addr ==
				    ifra->ifra_addr.sin_addr.s_addr)
					break;
			}
			ia = ifatoia(ifa);
		}
		if (cmd == SIOCDIFADDR && ia == NULL)
			return (EADDRNOTAVAIL);
		/* FALLTHROUGH */
	case SIOCSIFADDR:
		if ((so->so_state & SS_PRIV) == 0)
			return (EPERM);

		if (ifp == 0)
			panic("in_control");
		if (ia == NULL) {
			ia = malloc(sizeof *ia, M_IFADDR, M_WAITOK | M_ZERO);
			ia->ia_addr.sin_family = AF_INET;
			ia->ia_addr.sin_len = sizeof(ia->ia_addr);
			ia->ia_ifa.ifa_addr = sintosa(&ia->ia_addr);
			ia->ia_ifa.ifa_dstaddr = sintosa(&ia->ia_dstaddr);
			ia->ia_ifa.ifa_netmask = sintosa(&ia->ia_sockmask);
			ia->ia_sockmask.sin_len = 8;
			if (ifp->if_flags & IFF_BROADCAST) {
				ia->ia_broadaddr.sin_len = sizeof(ia->ia_addr);
				ia->ia_broadaddr.sin_family = AF_INET;
			}
			ia->ia_ifp = ifp;

			newifaddr = 1;
		} else
			newifaddr = 0;
		break;

	case SIOCSIFNETMASK:
	case SIOCSIFDSTADDR:
	case SIOCSIFBRDADDR:
		if ((so->so_state & SS_PRIV) == 0)
			return (EPERM);
		/* FALLTHROUGH */

	case SIOCGIFADDR:
	case SIOCGIFNETMASK:
	case SIOCGIFDSTADDR:
	case SIOCGIFBRDADDR:
		if (ia && satosin(&ifr->ifr_addr)->sin_addr.s_addr) {
			for (; ifa != NULL; ifa = TAILQ_NEXT(ifa, ifa_list)) {
				if ((ifa->ifa_addr->sa_family == AF_INET) &&
				    ifatoia(ifa)->ia_addr.sin_addr.s_addr ==
				    satosin(&ifr->ifr_addr)->sin_addr.s_addr) {
					ia = ifatoia(ifa);
					break;
				}
			}
		}
		if (ia == NULL)
			return (EADDRNOTAVAIL);
		break;
	}
	switch (cmd) {

	case SIOCGIFADDR:
		*satosin(&ifr->ifr_addr) = ia->ia_addr;
		break;

	case SIOCGIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (EINVAL);
		*satosin(&ifr->ifr_dstaddr) = ia->ia_broadaddr;
		break;

	case SIOCGIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		*satosin(&ifr->ifr_dstaddr) = ia->ia_dstaddr;
		break;

	case SIOCGIFNETMASK:
		*satosin(&ifr->ifr_addr) = ia->ia_sockmask;
		break;

	case SIOCSIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		s = splsoftnet();
		oldaddr = ia->ia_dstaddr;
		ia->ia_dstaddr = *satosin(&ifr->ifr_dstaddr);
		if (ifp->if_ioctl && (error = (*ifp->if_ioctl)
					(ifp, SIOCSIFDSTADDR, (caddr_t)ia))) {
			ia->ia_dstaddr = oldaddr;
			splx(s);
			return (error);
		}
		if (ia->ia_flags & IFA_ROUTE) {
			rt_ifa_del(&ia->ia_ifa, RTF_HOST, sintosa(&oldaddr));
			rt_ifa_add(&ia->ia_ifa, RTF_UP | RTF_HOST,
			    ia->ia_ifa.ifa_dstaddr);
		}
		splx(s);
		break;

	case SIOCSIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (EINVAL);
		ifa_update_broadaddr(ifp, &ia->ia_ifa, &ifr->ifr_broadaddr);
		break;

	case SIOCSIFADDR:
		s = splsoftnet();
		in_ifscrub(ifp, ia);
		error = in_ifinit(ifp, ia, satosin(&ifr->ifr_addr), newifaddr);
		if (!error)
			dohooks(ifp->if_addrhooks, 0);
		splx(s);
		return (error);

	case SIOCSIFNETMASK:
		ia->ia_netmask = ia->ia_sockmask.sin_addr.s_addr =
		    ifra->ifra_addr.sin_addr.s_addr;
		break;

	case SIOCAIFADDR: {
		int needinit = 0;

		error = 0;

		s = splsoftnet();
		if (ia->ia_addr.sin_family == AF_INET) {
			if (ifra->ifra_addr.sin_len == 0)
				ifra->ifra_addr = ia->ia_addr;
			else if (ifra->ifra_addr.sin_addr.s_addr !=
			    ia->ia_addr.sin_addr.s_addr || newifaddr)
				needinit = 1;
		}
		if (ifra->ifra_mask.sin_len) {
			in_ifscrub(ifp, ia);
			ia->ia_sockmask = ifra->ifra_mask;
			ia->ia_netmask = ia->ia_sockmask.sin_addr.s_addr;
			needinit = 1;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    (ifra->ifra_dstaddr.sin_family == AF_INET)) {
			in_ifscrub(ifp, ia);
			ia->ia_dstaddr = ifra->ifra_dstaddr;
			needinit  = 1;
		}
		if ((ifp->if_flags & IFF_BROADCAST) &&
		    (ifra->ifra_broadaddr.sin_family == AF_INET)) {
			if (newifaddr)
				ia->ia_broadaddr = ifra->ifra_broadaddr;
			else
				ifa_update_broadaddr(ifp, &ia->ia_ifa,
				    sintosa(&ifra->ifra_broadaddr));
		}
		if (ifra->ifra_addr.sin_family == AF_INET && needinit) {
			error = in_ifinit(ifp, ia, &ifra->ifra_addr, newifaddr);
		}
		if (!error)
			dohooks(ifp->if_addrhooks, 0);
		splx(s);
		return (error);
		}
	case SIOCDIFADDR:
		/*
		 * Even if the individual steps were safe, shouldn't
		 * these kinds of changes happen atomically?  What 
		 * should happen to a packet that was routed after
		 * the scrub but before the other steps? 
		 */
		s = splsoftnet();
		in_purgeaddr(&ia->ia_ifa);
		dohooks(ifp->if_addrhooks, 0);
		splx(s);
		break;

#ifdef MROUTING
	case SIOCGETVIFCNT:
	case SIOCGETSGCNT:
		return (mrt_ioctl(so, cmd, data));
#endif /* MROUTING */

	default:
		if (ifp == 0 || ifp->if_ioctl == 0)
			return (EOPNOTSUPP);
		return ((*ifp->if_ioctl)(ifp, cmd, data));
	}
	return (0);
}

/*
 * SIOC[GAD]LIFADDR.
 *	SIOCGLIFADDR: get first address. (???)
 *	SIOCGLIFADDR with IFLR_PREFIX:
 *		get first address that matches the specified prefix.
 *	SIOCALIFADDR: add the specified address.
 *	SIOCALIFADDR with IFLR_PREFIX:
 *		EINVAL since we can't deduce hostid part of the address.
 *	SIOCDLIFADDR: delete the specified address.
 *	SIOCDLIFADDR with IFLR_PREFIX:
 *		delete the first address that matches the specified prefix.
 * return values:
 *	EINVAL on invalid parameters
 *	EADDRNOTAVAIL on prefix match failed/specified address not found
 *	other values may be returned from in_ioctl()
 */
int
in_lifaddr_ioctl(struct socket *so, u_long cmd, caddr_t data,
    struct ifnet *ifp)
{
	struct if_laddrreq *iflr = (struct if_laddrreq *)data;
	struct ifaddr *ifa;
	struct sockaddr *sa;

	/* sanity checks */
	if (!data || !ifp) {
		panic("invalid argument to in_lifaddr_ioctl");
		/*NOTRECHED*/
	}

	switch (cmd) {
	case SIOCGLIFADDR:
		/* address must be specified on GET with IFLR_PREFIX */
		if ((iflr->flags & IFLR_PREFIX) == 0)
			break;
		/*FALLTHROUGH*/
	case SIOCALIFADDR:
	case SIOCDLIFADDR:
		/* address must be specified on ADD and DELETE */
		sa = (struct sockaddr *)&iflr->addr;
		if (sa->sa_family != AF_INET)
			return EINVAL;
		if (sa->sa_len != sizeof(struct sockaddr_in))
			return EINVAL;
		/* XXX need improvement */
		sa = (struct sockaddr *)&iflr->dstaddr;
		if (sa->sa_family
		 && sa->sa_family != AF_INET)
			return EINVAL;
		if (sa->sa_len && sa->sa_len != sizeof(struct sockaddr_in))
			return EINVAL;
		break;
	default: /*shouldn't happen*/
#if 0
		panic("invalid cmd to in_lifaddr_ioctl");
		/*NOTREACHED*/
#else
		return EOPNOTSUPP;
#endif
	}
	if (sizeof(struct in_addr) * 8 < iflr->prefixlen)
		return EINVAL;

	switch (cmd) {
	case SIOCALIFADDR:
	    {
		struct in_aliasreq ifra;

		if (iflr->flags & IFLR_PREFIX)
			return EINVAL;

		/* copy args to in_aliasreq, perform ioctl(SIOCAIFADDR). */
		bzero(&ifra, sizeof(ifra));
		memcpy(ifra.ifra_name, iflr->iflr_name,
		    sizeof(ifra.ifra_name));

		memcpy(&ifra.ifra_addr, &iflr->addr, 
		    ((struct sockaddr *)&iflr->addr)->sa_len);

		if (((struct sockaddr *)&iflr->dstaddr)->sa_family) {	/*XXX*/
			memcpy(&ifra.ifra_dstaddr, &iflr->dstaddr,
			    ((struct sockaddr *)&iflr->dstaddr)->sa_len);
		}

		ifra.ifra_mask.sin_family = AF_INET;
		ifra.ifra_mask.sin_len = sizeof(struct sockaddr_in);
		in_len2mask(&ifra.ifra_mask.sin_addr, iflr->prefixlen);

		return in_control(so, SIOCAIFADDR, (caddr_t)&ifra, ifp);
	    }
	case SIOCGLIFADDR:
	case SIOCDLIFADDR:
	    {
		struct in_ifaddr *ia;
		struct in_addr mask, candidate, match;
		struct sockaddr_in *sin;
		int cmp;

		bzero(&mask, sizeof(mask));
		if (iflr->flags & IFLR_PREFIX) {
			/* lookup a prefix rather than address. */
			in_len2mask(&mask, iflr->prefixlen);

			sin = (struct sockaddr_in *)&iflr->addr;
			match.s_addr = sin->sin_addr.s_addr;
			match.s_addr &= mask.s_addr;

			/* if you set extra bits, that's wrong */
			if (match.s_addr != sin->sin_addr.s_addr)
				return EINVAL;

			cmp = 1;
		} else {
			if (cmd == SIOCGLIFADDR) {
				/* on getting an address, take the 1st match */
				cmp = 0;	/*XXX*/
			} else {
				/* on deleting an address, do exact match */
				in_len2mask(&mask, 32);
				sin = (struct sockaddr_in *)&iflr->addr;
				match.s_addr = sin->sin_addr.s_addr;

				cmp = 1;
			}
		}

		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			if (!cmp)
				break;
			candidate.s_addr = ((struct sockaddr_in *)&ifa->ifa_addr)->sin_addr.s_addr;
			candidate.s_addr &= mask.s_addr;
			if (candidate.s_addr == match.s_addr)
				break;
		}
		if (!ifa)
			return EADDRNOTAVAIL;
		ia = ifatoia(ifa);

		if (cmd == SIOCGLIFADDR) {
			/* fill in the if_laddrreq structure */
			memcpy(&iflr->addr, &ia->ia_addr, ia->ia_addr.sin_len);

			if ((ifp->if_flags & IFF_POINTOPOINT) != 0) {
				memcpy(&iflr->dstaddr, &ia->ia_dstaddr,
				    ia->ia_dstaddr.sin_len);
			} else
				bzero(&iflr->dstaddr, sizeof(iflr->dstaddr));

			iflr->prefixlen =
				in_mask2len(&ia->ia_sockmask.sin_addr);

			iflr->flags = 0;	/*XXX*/

			return 0;
		} else {
			struct in_aliasreq ifra;

			/* fill in_aliasreq and do ioctl(SIOCDIFADDR) */
			bzero(&ifra, sizeof(ifra));
			memcpy(ifra.ifra_name, iflr->iflr_name,
			    sizeof(ifra.ifra_name));

			memcpy(&ifra.ifra_addr, &ia->ia_addr,
			    ia->ia_addr.sin_len);
			if ((ifp->if_flags & IFF_POINTOPOINT) != 0) {
				memcpy(&ifra.ifra_dstaddr, &ia->ia_dstaddr,
				    ia->ia_dstaddr.sin_len);
			}
			memcpy(&ifra.ifra_dstaddr, &ia->ia_sockmask,
			    ia->ia_sockmask.sin_len);

			return in_control(so, SIOCDIFADDR, (caddr_t)&ifra, ifp);
		}
	    }
	}

	return EOPNOTSUPP;	/*just for safety*/
}

/*
 * Delete any existing route for an interface.
 */
void
in_ifscrub(struct ifnet *ifp, struct in_ifaddr *ia)
{
	if ((ifp->if_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) == 0)
		in_scrubprefix(ia);
	else
		in_scrubhost(ia);
}

/*
 * Initialize an interface's internet address
 * and routing table entry.
 */
int
in_ifinit(struct ifnet *ifp, struct in_ifaddr *ia, struct sockaddr_in *sin,
    int newaddr)
{
	u_int32_t i = sin->sin_addr.s_addr;
	struct sockaddr_in oldaddr;
	int s = splnet(), error = 0;

	if (newaddr)
		TAILQ_INSERT_TAIL(&in_ifaddr, ia, ia_list);

	/*
	 * Always remove the address from the tree to make sure its
	 * position gets updated in case the key changes.
	 */
	if (!newaddr)
		ifa_del(ifp, &ia->ia_ifa);
	oldaddr = ia->ia_addr;
	ia->ia_addr = *sin;

	/*
	 * Give the interface a chance to initialize
	 * if this is its first address,
	 * and to validate the address if necessary.
	 */
	if (ifp->if_ioctl &&
	    (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (caddr_t)ia))) {
		ia->ia_addr = oldaddr;
		splx(s);
		goto out;
	}
	splx(s);

	/*
	 * How should a packet be routed during
	 * an address change--and is it safe?
	 * Is the "ifp" even in a consistent state?
	 * Be safe for now.
	 */
	splsoftassert(IPL_SOFTNET);

	if (ia->ia_netmask == 0) {
		if (IN_CLASSA(i))
			ia->ia_netmask = IN_CLASSA_NET;
		else if (IN_CLASSB(i))
			ia->ia_netmask = IN_CLASSB_NET;
		else
			ia->ia_netmask = IN_CLASSC_NET;
		ia->ia_sockmask.sin_addr.s_addr = ia->ia_netmask;
	}

	ia->ia_net = i & ia->ia_netmask;
	in_socktrim(&ia->ia_sockmask);
	/*
	 * Add route for the network.
	 */
	ia->ia_ifa.ifa_metric = ifp->if_metric;
	if (ifp->if_flags & IFF_BROADCAST) {
		if (IN_RFC3021_SUBNET(ia->ia_netmask))
			ia->ia_broadaddr.sin_addr.s_addr = 0;
		else {
			ia->ia_broadaddr.sin_addr.s_addr =
			    ia->ia_net | ~ia->ia_netmask;
		}
	} else if (ifp->if_flags & IFF_LOOPBACK) {
		ia->ia_dstaddr = ia->ia_addr;
	} else if (ifp->if_flags & IFF_POINTOPOINT) {
		if (ia->ia_dstaddr.sin_family != AF_INET)
			goto out;
	}

	if ((ifp->if_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) == 0)
		error = in_addprefix(ia);
	else
		error = in_addhost(ia);

	/*
	 * If the interface supports multicast, join the "all hosts"
	 * multicast group on that interface.
	 */
	if ((ifp->if_flags & IFF_MULTICAST) && ia->ia_allhosts == NULL) {
		struct in_addr addr;

		addr.s_addr = INADDR_ALLHOSTS_GROUP;
		ia->ia_allhosts = in_addmulti(&addr, ifp);
	}

out:
	/*
	 * Add the address to the local list and the global tree
	 * even if an error occured to make sure the various
	 * global structures are consistent.
	 *
	 * XXX This is necessary because we added the address
	 * to the global list in the first place because of
	 * carp(4).
	 */
	ifa_add(ifp, &ia->ia_ifa);

	if (error && newaddr)
		in_purgeaddr(&ia->ia_ifa);

	return (error);
}

void
in_purgeaddr(struct ifaddr *ifa)
{
	struct ifnet *ifp = ifa->ifa_ifp;
	struct in_ifaddr *ia = ifatoia(ifa);

	splsoftassert(IPL_SOFTNET);

	in_ifscrub(ifp, ia);

	ifa_del(ifp, &ia->ia_ifa);
	TAILQ_REMOVE(&in_ifaddr, ia, ia_list);
	if (ia->ia_allhosts != NULL) {
		in_delmulti(ia->ia_allhosts);
		ia->ia_allhosts = NULL;
	}

	ia->ia_ifp = NULL;
	ifafree(&ia->ia_ifa);
}

int
in_addhost(struct in_ifaddr *ia0)
{
	struct in_ifaddr *ia;
	struct in_addr dst;
	int error;

	dst = ia0->ia_dstaddr.sin_addr;

	/*
	 * If an interface already have a route to the same
	 * destination don't do anything.
	 */
	TAILQ_FOREACH(ia, &in_ifaddr, ia_list) {
		if (ia->ia_ifp->if_rdomain != ia0->ia_ifp->if_rdomain)
			continue;

		if (dst.s_addr != ia->ia_dstaddr.sin_addr.s_addr)
			continue;

		if ((ia->ia_flags & IFA_ROUTE) == 0)
			continue;

		return (0);
	}

	error = rt_ifa_add(&ia0->ia_ifa, RTF_UP | RTF_HOST,
	    ia0->ia_ifa.ifa_dstaddr);
	if (!error)
		ia0->ia_flags |= IFA_ROUTE;

	return (error);
}

int
in_scrubhost(struct in_ifaddr *ia0)
{
	struct in_ifaddr *ia;
	struct in_addr dst;
	int error;

	if ((ia0->ia_flags & IFA_ROUTE) == 0)
		return (0);

	dst = ia0->ia_dstaddr.sin_addr;

	/*
	 * Because we only add one route for a given destination at
	 * a time, here we need to do some magic to move this route
	 * to another interface if it has the same destination.
	 */
	TAILQ_FOREACH(ia, &in_ifaddr, ia_list) {
		if (ia->ia_ifp->if_rdomain != ia0->ia_ifp->if_rdomain)
			continue;

		if (dst.s_addr != ia->ia_dstaddr.sin_addr.s_addr)
			continue;

		if ((ia->ia_flags & IFA_ROUTE) != 0)
			continue;

		rt_ifa_del(&ia0->ia_ifa, RTF_HOST, ia0->ia_ifa.ifa_dstaddr);
		ia0->ia_flags &= ~IFA_ROUTE;
		error = rt_ifa_add(&ia->ia_ifa, RTF_UP | RTF_HOST,
		    ia->ia_ifa.ifa_dstaddr);
		if (!error)
			ia->ia_flags |= IFA_ROUTE;

		return (error);
	}

	rt_ifa_del(&ia0->ia_ifa, RTF_HOST, ia0->ia_ifa.ifa_dstaddr);
	ia0->ia_flags &= ~IFA_ROUTE;

	return (0);
}

/*
 * add a route to prefix ("connected route" in cisco terminology).
 * does nothing if there's some interface address with the same prefix already.
 */
int
in_addprefix(struct in_ifaddr *ia0)
{
	struct in_ifaddr *ia;
	struct in_addr prefix, mask, p, m;
	int error;

	prefix = ia0->ia_addr.sin_addr;
	mask = ia0->ia_sockmask.sin_addr;
	prefix.s_addr &= mask.s_addr;

	TAILQ_FOREACH(ia, &in_ifaddr, ia_list) {
		if (ia->ia_ifp->if_rdomain != ia0->ia_ifp->if_rdomain)
			continue;

		if ((ia->ia_ifp->if_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)))
			continue;

		if ((ia->ia_flags & IFA_ROUTE) == 0)
			continue;

		p = ia->ia_addr.sin_addr;
		m = ia->ia_sockmask.sin_addr;
		p.s_addr &= m.s_addr;

		if (prefix.s_addr != p.s_addr || mask.s_addr != m.s_addr)
			continue;

#if NCARP > 0
		/* move to a real interface instead of carp interface */
		if (ia->ia_ifp->if_type == IFT_CARP &&
		    ia0->ia_ifp->if_type != IFT_CARP) {
			rt_ifa_del(&ia->ia_ifa, 0, ia->ia_ifa.ifa_addr);
			ia->ia_flags &= ~IFA_ROUTE;
			break;
		}
#endif
		/*
		 * if we got a matching prefix route inserted by other
		 * interface address, we don't need to bother
		 */
		return 0;
	}

	/*
	 * noone seem to have prefix route.  insert it.
	 */
	error = rt_ifa_add(&ia0->ia_ifa, RTF_UP | RTF_CLONING,
	    ia0->ia_ifa.ifa_addr);
	if (!error)
		ia0->ia_flags |= IFA_ROUTE;
	return error;
}

/*
 * remove a route to prefix ("connected route" in cisco terminology).
 * re-installs the route by using another interface address, if there's one
 * with the same prefix (otherwise we lose the route mistakenly).
 */
int
in_scrubprefix(struct in_ifaddr *ia0)
{
	struct in_ifaddr *ia;
	struct in_addr prefix, mask, p, m;
	int error;

	if ((ia0->ia_flags & IFA_ROUTE) == 0)
		return 0;

	prefix = ia0->ia_addr.sin_addr;
	mask = ia0->ia_sockmask.sin_addr;
	prefix.s_addr &= mask.s_addr;

	TAILQ_FOREACH(ia, &in_ifaddr, ia_list) {
		if (ia->ia_ifp->if_rdomain != ia0->ia_ifp->if_rdomain)
			continue;

		if ((ia->ia_ifp->if_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)))
			continue;

		if ((ia->ia_flags & IFA_ROUTE) != 0)
			continue;

		p = ia->ia_addr.sin_addr;
		m = ia->ia_sockmask.sin_addr;
		p.s_addr &= m.s_addr;

		if (prefix.s_addr != p.s_addr || mask.s_addr != m.s_addr)
			continue;

		/*
		 * if we got a matching prefix route, move IFA_ROUTE to him
		 */
		rt_ifa_del(&ia0->ia_ifa, 0, ia0->ia_ifa.ifa_addr);
		ia0->ia_flags &= ~IFA_ROUTE;
		error = rt_ifa_add(&ia->ia_ifa, RTF_UP | RTF_CLONING,
		    ia->ia_ifa.ifa_addr);
		if (error == 0)
			ia->ia_flags |= IFA_ROUTE;
		return error;
	}

	/*
	 * noone seem to have prefix route.  remove it.
	 */
	rt_ifa_del(&ia0->ia_ifa, 0, ia0->ia_ifa.ifa_addr);
	ia0->ia_flags &= ~IFA_ROUTE;
	return 0;
}

/*
 * Return 1 if the address might be a local broadcast address.
 */
int
in_broadcast(struct in_addr in, struct ifnet *ifp, u_int rtableid)
{
	struct ifnet *ifn, *if_first, *if_target;
	struct ifaddr *ifa;
	u_int rdomain;

	rdomain = rtable_l2(rtableid);

	if (in.s_addr == INADDR_BROADCAST ||
	    in.s_addr == INADDR_ANY)
		return 1;

	if (ifp == NULL) {
	  	if_first = TAILQ_FIRST(&ifnet);
		if_target = 0;
	} else {
		if_first = ifp;
		if_target = TAILQ_NEXT(ifp, if_list);
	}

#define ia (ifatoia(ifa))
	/*
	 * Look through the list of addresses for a match
	 * with a broadcast address.
	 * If ifp is NULL, check against all the interfaces.
	 */
        for (ifn = if_first; ifn != if_target; ifn = TAILQ_NEXT(ifn, if_list)) {
		if (ifn->if_rdomain != rdomain)
			continue;
		if ((ifn->if_flags & IFF_BROADCAST) == 0)
			continue;
		TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list)
			if (ifa->ifa_addr->sa_family == AF_INET &&
			    in.s_addr != ia->ia_addr.sin_addr.s_addr &&
			    in.s_addr == ia->ia_broadaddr.sin_addr.s_addr)
				return 1;
	}
	return (0);
#undef ia
}

/*
 * Add an address to the list of IP multicast addresses for a given interface.
 */
struct in_multi *
in_addmulti(struct in_addr *ap, struct ifnet *ifp)
{
	struct in_multi *inm;
	struct ifreq ifr;
	int s;

	/*
	 * See if address already in list.
	 */
	IN_LOOKUP_MULTI(*ap, ifp, inm);
	if (inm != NULL) {
		/*
		 * Found it; just increment the reference count.
		 */
		++inm->inm_refcnt;
	} else {
		if (ifp->if_ioctl == NULL)
			return (NULL);

		/*
		 * New address; allocate a new multicast record
		 * and link it into the interface's multicast list.
		 */
		inm = malloc(sizeof(*inm), M_IPMADDR, M_NOWAIT);
		if (inm == NULL)
			return (NULL);

		inm->inm_sin.sin_len = sizeof(struct sockaddr_in);
		inm->inm_sin.sin_family = AF_INET;
		inm->inm_sin.sin_addr = *ap;
		inm->inm_refcnt = 1;
		inm->inm_ifidx = ifp->if_index;
		inm->inm_ifma.ifma_addr = sintosa(&inm->inm_sin);

		/*
		 * Ask the network driver to update its multicast reception
		 * filter appropriately for the new address.
		 */
		memcpy(&ifr.ifr_addr, &inm->inm_sin, sizeof(inm->inm_sin));
		if ((*ifp->if_ioctl)(ifp, SIOCADDMULTI,(caddr_t)&ifr) != 0) {
			free(inm, M_IPMADDR);
			return (NULL);
		}

		s = splsoftnet();
		TAILQ_INSERT_HEAD(&ifp->if_maddrlist, &inm->inm_ifma,
		    ifma_list);
		splx(s);

		/*
		 * Let IGMP know that we have joined a new IP multicast group.
		 */
		igmp_joingroup(inm);
	}

	return (inm);
}

/*
 * Delete a multicast address record.
 */
void
in_delmulti(struct in_multi *inm)
{
	struct ifreq ifr;
	struct ifnet *ifp;
	int s;

	if (--inm->inm_refcnt == 0) {
		/*
		 * No remaining claims to this record; let IGMP know that
		 * we are leaving the multicast group.
		 */
		igmp_leavegroup(inm);
		ifp = if_get(inm->inm_ifidx);

		/*
		 * Notify the network driver to update its multicast
		 * reception filter.
		 */
		if (ifp != NULL) {
			satosin(&ifr.ifr_addr)->sin_len =
			    sizeof(struct sockaddr_in);
			satosin(&ifr.ifr_addr)->sin_family = AF_INET;
			satosin(&ifr.ifr_addr)->sin_addr = inm->inm_addr;
			(*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)&ifr);

			s = splsoftnet();
			TAILQ_REMOVE(&ifp->if_maddrlist, &inm->inm_ifma,
			    ifma_list);
			splx(s);
		}

		free(inm, M_IPMADDR);
	}
}

#endif

void
in_ifdetach(struct ifnet *ifp)
{
	struct ifaddr *ifa, *next;

	/* nuke any of IPv4 addresses we have */
	TAILQ_FOREACH_SAFE(ifa, &ifp->if_addrlist, ifa_list, next) {
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;
		in_purgeaddr(ifa);
	}
}
