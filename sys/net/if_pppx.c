/*	$OpenBSD: if_pppx.c,v 1.29 2014/04/08 04:26:53 miod Exp $ */

/*
 * Copyright (c) 2010 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2010 David Gwynne <dlg@openbsd.org>
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

/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/queue.h>
#include <sys/rwlock.h>
#include <sys/pool.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/selinfo.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/netisr.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/if_dl.h>

#ifdef INET
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#endif

#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/nd6.h>
#endif /* INET6 */

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <net/ppp_defs.h>
#include <net/ppp-comp.h>
#include <crypto/arc4.h>

#ifdef PIPEX
#include <net/pipex.h>
#include <net/pipex_local.h>
#else
#error PIPEX option not enabled
#endif

#ifdef PPPX_DEBUG
#define PPPX_D_INIT	(1<<0)

int pppxdebug = 0;

#define DPRINTF(_m, _p...)	do { \
					if (ISSET(pppxdebug, (_m))) \
						printf(_p); \
				} while (0)
#else
#define DPRINTF(_m, _p...)	/* _m, _p */
#endif


struct pppx_if;

struct pppx_dev {
	LIST_ENTRY(pppx_dev)	pxd_entry;
	int			pxd_unit;

	/* kq shizz */
	struct selinfo		pxd_rsel;
	struct mutex		pxd_rsel_mtx;
	struct selinfo		pxd_wsel;
	struct mutex		pxd_wsel_mtx;

	/* queue of packets for userland to service - protected by splnet */
	struct ifqueue		pxd_svcq;
	int			pxd_waiting;
	LIST_HEAD(,pppx_if)	pxd_pxis;
};

struct rwlock			pppx_devs_lk = RWLOCK_INITIALIZER("pppxdevs");
LIST_HEAD(, pppx_dev)		pppx_devs = LIST_HEAD_INITIALIZER(pppx_devs);
struct pool			*pppx_if_pl;

struct pppx_dev			*pppx_dev_lookup(int);
struct pppx_dev			*pppx_dev2pxd(int);

struct pppx_if_key {
	int			pxik_session_id;
	int			pxik_protocol;
};

int				pppx_if_cmp(struct pppx_if *, struct pppx_if *);

struct pppx_if {
	struct pppx_if_key	pxi_key; /* must be first in the struct */

	RB_ENTRY(pppx_if)	pxi_entry;
	LIST_ENTRY(pppx_if)	pxi_list;

	int			pxi_unit;
	struct ifnet		pxi_if;
	struct pipex_session	pxi_session;
	struct pipex_iface_context	pxi_ifcontext;
};

struct rwlock			pppx_ifs_lk = RWLOCK_INITIALIZER("pppxifs");
RB_HEAD(pppx_ifs, pppx_if)	pppx_ifs = RB_INITIALIZER(&pppx_ifs);
RB_PROTOTYPE(pppx_ifs, pppx_if, pxi_entry, pppx_if_cmp);

int		pppx_if_next_unit(void);
struct pppx_if *pppx_if_find(struct pppx_dev *, int, int);
int		pppx_add_session(struct pppx_dev *,
		    struct pipex_session_req *);
int		pppx_del_session(struct pppx_dev *,
		    struct pipex_session_close_req *);
int		pppx_set_session_descr(struct pppx_dev *,
		    struct pipex_session_descr_req *);

void		pppx_if_destroy(struct pppx_dev *, struct pppx_if *);
void		pppx_if_start(struct ifnet *);
int		pppx_if_output(struct ifnet *, struct mbuf *,
		    struct sockaddr *, struct rtentry *);
int		pppx_if_ioctl(struct ifnet *, u_long, caddr_t);


void		pppxattach(int);

void		filt_pppx_rdetach(struct knote *);
int		filt_pppx_read(struct knote *, long);

struct filterops pppx_rd_filtops = {
	1,
	NULL,
	filt_pppx_rdetach,
	filt_pppx_read
};

void		filt_pppx_wdetach(struct knote *);
int		filt_pppx_write(struct knote *, long);

struct filterops pppx_wr_filtops = {
	1,
	NULL,
	filt_pppx_wdetach,
	filt_pppx_write
};

struct pppx_dev *
pppx_dev_lookup(dev_t dev)
{
	struct pppx_dev *pxd;
	int unit = minor(dev);

	/* must hold pppx_devs_lk */

	LIST_FOREACH(pxd, &pppx_devs, pxd_entry) {
		if (pxd->pxd_unit == unit)
			return (pxd);
	}

	return (NULL);
}

struct pppx_dev *
pppx_dev2pxd(dev_t dev)
{
	struct pppx_dev *pxd;

	rw_enter_read(&pppx_devs_lk);
	pxd = pppx_dev_lookup(dev);
	rw_exit_read(&pppx_devs_lk);

	return (pxd);
}

void
pppxattach(int n)
{
	pipex_init();
}

int
pppxopen(dev_t dev, int flags, int mode, struct proc *p)
{
	struct pppx_dev *pxd;
	int rv = 0;

	rv = rw_enter(&pppx_devs_lk, RW_WRITE | RW_INTR);
	if (rv != 0)
		return (rv);

	pxd = pppx_dev_lookup(dev);
	if (pxd != NULL) {
		rv = EBUSY;
		goto out;
	}

	if (LIST_EMPTY(&pppx_devs) && pppx_if_pl == NULL) {
		pppx_if_pl = malloc(sizeof(*pppx_if_pl), M_DEVBUF, M_WAITOK);
		pool_init(pppx_if_pl, sizeof(struct pppx_if), 0, 0, 0,
		    "pppxif", &pool_allocator_nointr);
	}

	pxd = malloc(sizeof(*pxd), M_DEVBUF, M_WAITOK | M_ZERO);

	pxd->pxd_unit = minor(dev);
	mtx_init(&pxd->pxd_rsel_mtx, IPL_NET);
	mtx_init(&pxd->pxd_wsel_mtx, IPL_NET);
	LIST_INIT(&pxd->pxd_pxis);

	IFQ_SET_MAXLEN(&pxd->pxd_svcq, 128);
	LIST_INSERT_HEAD(&pppx_devs, pxd, pxd_entry);

out:
	rw_exit(&pppx_devs_lk);
	return (rv);
}

int
pppxread(dev_t dev, struct uio *uio, int ioflag)
{
	struct pppx_dev *pxd = pppx_dev2pxd(dev);
	struct mbuf *m, *m0;
	int error = 0;
	int len, s;

	if (!pxd)
		return (ENXIO);

	s = splnet();
	for (;;) {
		IF_DEQUEUE(&pxd->pxd_svcq, m0);
		if (m0 != NULL)
			break;

		if (ISSET(ioflag, IO_NDELAY)) {
			splx(s);
			return (EWOULDBLOCK);
		}

		pxd->pxd_waiting = 1;
		error = tsleep(pxd, (PZERO + 1)|PCATCH, "pppxread", 0);
		if (error != 0) {
			splx(s);
			return (error);
		}
	}
	splx(s);

	while (m0 != NULL && uio->uio_resid > 0 && error == 0) {
		len = min(uio->uio_resid, m0->m_len);
		if (len != 0)
			error = uiomove(mtod(m0, caddr_t), len, uio);
		MFREE(m0, m);
		m0 = m;
	}

	if (m0 != NULL)
		m_freem(m0);

	return (error);
}

int
pppxwrite(dev_t dev, struct uio *uio, int ioflag)
{
/*	struct pppx_dev *pxd = pppx_dev2pxd(dev);	*/
	struct pppx_hdr *th;
	struct mbuf *top, **mp, *m;
	struct ifqueue *ifq;
	int tlen, mlen;
	int isr, s, error = 0;

	if (uio->uio_resid < sizeof(*th) || uio->uio_resid > MCLBYTES)
		return (EMSGSIZE);

	tlen = uio->uio_resid;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);
	mlen = MHLEN;
	if (uio->uio_resid >= MINCLSIZE) {
		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			m_free(m);
			return (ENOBUFS);
		}
		mlen = MCLBYTES;
	}

	top = NULL;
	mp = &top;

	while (error == 0 && uio->uio_resid > 0) {
		m->m_len = min(mlen, uio->uio_resid);
		error = uiomove(mtod (m, caddr_t), m->m_len, uio);
		*mp = m;
		mp = &m->m_next;
		if (error == 0 && uio->uio_resid > 0) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				error = ENOBUFS;
				break;
			}
			mlen = MLEN;
			if (uio->uio_resid >= MINCLSIZE) {
				MCLGET(m, M_DONTWAIT);
				if (!(m->m_flags & M_EXT)) {
					error = ENOBUFS;
					m_free(m);
					break;
				}
				mlen = MCLBYTES;
			}
		}
	}

	if (error) {
		if (top != NULL)
			m_freem(top);
		return (error);
	}

	top->m_pkthdr.len = tlen;

	/* strip the tunnel header */
	th = mtod(top, struct pppx_hdr *);
	m_adj(top, sizeof(struct pppx_hdr));

	switch (ntohl(th->pppx_proto)) {
#ifdef INET
	case AF_INET:
		ifq = &ipintrq;
		isr = NETISR_IP;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		ifq = &ip6intrq;
		isr = NETISR_IPV6;
		break;
#endif
	default:
		m_freem(top);
		return (EAFNOSUPPORT);
	}

	s = splnet();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		splx(s);
		m_freem(top);
		return (ENOBUFS);
	}
	IF_ENQUEUE(ifq, top);
	schednetisr(isr);
	splx(s);

	return (error);
}

int
pppxioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
{
	struct pppx_dev *pxd = pppx_dev2pxd(dev);
	int error = 0;

	switch (cmd) {
	case PIPEXSMODE:
		/*
		 * npppd always enables on open, and only disables before
		 * closing. we cheat and let open and close do that, so lie
		 * to npppd.
		 */
		break;
	case PIPEXGMODE:
		*(int *)addr = 1;
		break;

	case PIPEXASESSION:
		error = pppx_add_session(pxd,
		    (struct pipex_session_req *)addr);
		break;

	case PIPEXDSESSION:
		error = pppx_del_session(pxd,
		    (struct pipex_session_close_req *)addr);
		break;

	case PIPEXCSESSION:
		error = pipex_config_session(
		    (struct pipex_session_config_req *)addr);
		break;

	case PIPEXGSTAT:
		error = pipex_get_stat((struct pipex_session_stat_req *)addr);
		break;

	case PIPEXGCLOSED:
		error = pipex_get_closed((struct pipex_session_list_req *)addr);
		break;

	case PIPEXSIFDESCR:
		error = pppx_set_session_descr(pxd,
		    (struct pipex_session_descr_req *)addr);
		break;

	case FIONBIO:
	case FIOASYNC:
	case FIONREAD:
		return (0);

	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

int
pppxpoll(dev_t dev, int events, struct proc *p)
{
	struct pppx_dev *pxd = pppx_dev2pxd(dev);
	int s, revents = 0;

	if (events & (POLLIN | POLLRDNORM)) {
		s = splnet();
		if (!IF_IS_EMPTY(&pxd->pxd_svcq))
			revents |= events & (POLLIN | POLLRDNORM);
		splx(s);
	}
	if (events & (POLLOUT | POLLWRNORM))
		revents |= events & (POLLOUT | POLLWRNORM);

	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(p, &pxd->pxd_rsel);
	}

	return (revents);
}

int
pppxkqfilter(dev_t dev, struct knote *kn)
{
	struct pppx_dev *pxd = pppx_dev2pxd(dev);
	struct mutex *mtx;
	struct klist *klist;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		mtx = &pxd->pxd_rsel_mtx;
		klist = &pxd->pxd_rsel.si_note;
		kn->kn_fop = &pppx_rd_filtops;
		break;
	case EVFILT_WRITE:
		mtx = &pxd->pxd_wsel_mtx;
		klist = &pxd->pxd_wsel.si_note;
		kn->kn_fop = &pppx_wr_filtops;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = (caddr_t)pxd;

	mtx_enter(mtx);
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	mtx_leave(mtx);

	return (0);
}

void
filt_pppx_rdetach(struct knote *kn)
{
	struct pppx_dev *pxd = (struct pppx_dev *)kn->kn_hook;
	struct klist *klist = &pxd->pxd_rsel.si_note;

	if (ISSET(kn->kn_status, KN_DETACHED))
		return;

	mtx_enter(&pxd->pxd_rsel_mtx);
	SLIST_REMOVE(klist, kn, knote, kn_selnext);
	mtx_leave(&pxd->pxd_rsel_mtx);
}

int
filt_pppx_read(struct knote *kn, long hint)
{
	struct pppx_dev *pxd = (struct pppx_dev *)kn->kn_hook;
	int s, event = 0;

	if (ISSET(kn->kn_status, KN_DETACHED)) {
		kn->kn_data = 0;
		return (1);
	}

	s = splnet();
	if (!IF_IS_EMPTY(&pxd->pxd_svcq)) {
		event = 1;
		kn->kn_data = IF_LEN(&pxd->pxd_svcq);
	}
	splx(s);

	return (event);
}

void
filt_pppx_wdetach(struct knote *kn)
{
	struct pppx_dev *pxd = (struct pppx_dev *)kn->kn_hook;
	struct klist *klist = &pxd->pxd_wsel.si_note;

	if (ISSET(kn->kn_status, KN_DETACHED))
		return;

	mtx_enter(&pxd->pxd_wsel_mtx);
	SLIST_REMOVE(klist, kn, knote, kn_selnext);
	mtx_leave(&pxd->pxd_wsel_mtx);
}

int
filt_pppx_write(struct knote *kn, long hint)
{
	/* We're always ready to accept a write. */
	return (1);
}

int
pppxclose(dev_t dev, int flags, int mode, struct proc *p)
{
	struct pppx_dev *pxd;
	struct pppx_if	*pxi;
	int s;

	rw_enter_write(&pppx_devs_lk);

	pxd = pppx_dev_lookup(dev);

	/* XXX */
	while ((pxi = LIST_FIRST(&pxd->pxd_pxis)))
		pppx_if_destroy(pxd, pxi);

	LIST_REMOVE(pxd, pxd_entry);

	s = splnet();
	IF_PURGE(&pxd->pxd_svcq);
	splx(s);

	free(pxd, M_DEVBUF);

	if (LIST_EMPTY(&pppx_devs)) {
		pool_destroy(pppx_if_pl);
		free(pppx_if_pl, M_DEVBUF);
		pppx_if_pl = NULL;
	}

	rw_exit_write(&pppx_devs_lk);
	return (0);
}

int
pppx_if_cmp(struct pppx_if *a, struct pppx_if *b)
{
	return memcmp(&a->pxi_key, &b->pxi_key, sizeof(a->pxi_key));
}

int
pppx_if_next_unit(void)
{
	struct pppx_if *pxi;
	int unit = 0;

	rw_assert_wrlock(&pppx_ifs_lk);

	/* this is safe without splnet since we're not modifying it */
	do {
		int found = 0;
		RB_FOREACH(pxi, pppx_ifs, &pppx_ifs) {
			if (pxi->pxi_unit == unit) {
				found = 1;
				break;
			}
		}

		if (found == 0)
			break;
		unit++;
	} while (unit > 0);

	return (unit);
}

struct pppx_if *
pppx_if_find(struct pppx_dev *pxd, int session_id, int protocol)
{
	struct pppx_if *s, *p;
	s = malloc(sizeof(*s), M_DEVBUF, M_WAITOK | M_ZERO);

	s->pxi_key.pxik_session_id = session_id;
	s->pxi_key.pxik_protocol = protocol;

	rw_enter_read(&pppx_ifs_lk);
	p = RB_FIND(pppx_ifs, &pppx_ifs, s);
	rw_exit_read(&pppx_ifs_lk);

	free(s, M_DEVBUF);
	return (p);
}

int
pppx_add_session(struct pppx_dev *pxd, struct pipex_session_req *req)
{
	struct pppx_if *pxi;
	struct pipex_session *session;
	struct pipex_hash_head *chain;
	struct ifnet *ifp;
	int unit, s, error = 0;
	struct in_ifaddr *ia;
	struct sockaddr_in ifaddr;
#ifdef PIPEX_PPPOE
	struct ifnet *over_ifp = NULL;
#endif

	switch (req->pr_protocol) {
#ifdef PIPEX_PPPOE
	case PIPEX_PROTO_PPPOE:
		over_ifp = ifunit(req->pr_proto.pppoe.over_ifname);
		if (over_ifp == NULL)
			return (EINVAL);
		if (req->pr_peer_address.ss_family != AF_UNSPEC)
			return (EINVAL);
		break;
#endif
#ifdef PIPEX_PPTP
	case PIPEX_PROTO_PPTP:
#endif
#ifdef PIPEX_L2TP
	case PIPEX_PROTO_L2TP:
#endif
		switch (req->pr_peer_address.ss_family) {
		case AF_INET:
			if (req->pr_peer_address.ss_len != sizeof(struct sockaddr_in))
				return (EINVAL);
			break;
#ifdef INET6
		case AF_INET6:
			if (req->pr_peer_address.ss_len != sizeof(struct sockaddr_in6))
				return (EINVAL);
			break;
#endif
		default:
			return (EPROTONOSUPPORT);
		}
		if (req->pr_peer_address.ss_family !=
		    req->pr_local_address.ss_family ||
		    req->pr_peer_address.ss_len !=
		    req->pr_local_address.ss_len)
			return (EINVAL);
		break;
	default:
		return (EPROTONOSUPPORT);
	}

	pxi = pool_get(pppx_if_pl, PR_WAITOK | PR_ZERO);
	if (pxi == NULL)
		return (ENOMEM);

	session = &pxi->pxi_session;
	ifp = &pxi->pxi_if;

	/* fake a pipex interface context */
	session->pipex_iface = &pxi->pxi_ifcontext;
	session->pipex_iface->ifnet_this = ifp;
	session->pipex_iface->pipexmode = PIPEX_ENABLED;

	/* setup session */
	session->state = PIPEX_STATE_OPENED;
	session->protocol = req->pr_protocol;
	session->session_id = req->pr_session_id;
	session->peer_session_id = req->pr_peer_session_id;
	session->peer_mru = req->pr_peer_mru;
	session->timeout_sec = req->pr_timeout_sec;
	session->ppp_flags = req->pr_ppp_flags;
	session->ppp_id = req->pr_ppp_id;

	session->ip_forward = 1;

	session->ip_address.sin_family = AF_INET;
	session->ip_address.sin_len = sizeof(struct sockaddr_in);
	session->ip_address.sin_addr = req->pr_ip_address;

	session->ip_netmask.sin_family = AF_INET;
	session->ip_netmask.sin_len = sizeof(struct sockaddr_in);
	session->ip_netmask.sin_addr = req->pr_ip_netmask;

	if (session->ip_netmask.sin_addr.s_addr == 0L)
		session->ip_netmask.sin_addr.s_addr = 0xffffffffL;
	session->ip_address.sin_addr.s_addr &=
	    session->ip_netmask.sin_addr.s_addr;

	if (req->pr_peer_address.ss_len > 0)
		memcpy(&session->peer, &req->pr_peer_address,
		    MIN(req->pr_peer_address.ss_len, sizeof(session->peer)));
	if (req->pr_local_address.ss_len > 0)
		memcpy(&session->local, &req->pr_local_address,
		    MIN(req->pr_local_address.ss_len, sizeof(session->local)));
#ifdef PIPEX_PPPOE
	if (req->pr_protocol == PIPEX_PROTO_PPPOE)
		session->proto.pppoe.over_ifp = over_ifp;
#endif
#ifdef PIPEX_PPTP
	if (req->pr_protocol == PIPEX_PROTO_PPTP) {
		struct pipex_pptp_session *sess_pptp = &session->proto.pptp;

		sess_pptp->snd_gap = 0;
		sess_pptp->rcv_gap = 0;
		sess_pptp->snd_una = req->pr_proto.pptp.snd_una;
		sess_pptp->snd_nxt = req->pr_proto.pptp.snd_nxt;
		sess_pptp->rcv_nxt = req->pr_proto.pptp.rcv_nxt;
		sess_pptp->rcv_acked = req->pr_proto.pptp.rcv_acked;

		sess_pptp->winsz = req->pr_proto.pptp.winsz;
		sess_pptp->maxwinsz = req->pr_proto.pptp.maxwinsz;
		sess_pptp->peer_maxwinsz = req->pr_proto.pptp.peer_maxwinsz;
		/* last ack number */
		sess_pptp->ul_snd_una = sess_pptp->snd_una - 1;
	}
#endif
#ifdef PIPEX_L2TP
	if (req->pr_protocol == PIPEX_PROTO_L2TP) {
		struct pipex_l2tp_session *sess_l2tp = &session->proto.l2tp;

		/* session keys */
		sess_l2tp->tunnel_id = req->pr_proto.l2tp.tunnel_id;
		sess_l2tp->peer_tunnel_id = req->pr_proto.l2tp.peer_tunnel_id;

		/* protocol options */
		sess_l2tp->option_flags = req->pr_proto.l2tp.option_flags;

		/* initial state of dynamic context */
		sess_l2tp->ns_gap = sess_l2tp->nr_gap = 0;
		sess_l2tp->ns_nxt = req->pr_proto.l2tp.ns_nxt;
		sess_l2tp->nr_nxt = req->pr_proto.l2tp.nr_nxt;
		sess_l2tp->ns_una = req->pr_proto.l2tp.ns_una;
		sess_l2tp->nr_acked = req->pr_proto.l2tp.nr_acked;
		/* last ack number */
		sess_l2tp->ul_ns_una = sess_l2tp->ns_una - 1;
	}
#endif
#ifdef PIPEX_MPPE
	if ((req->pr_ppp_flags & PIPEX_PPP_MPPE_ACCEPTED) != 0)
		pipex_session_init_mppe_recv(session,
		    req->pr_mppe_recv.stateless, req->pr_mppe_recv.keylenbits,
		    req->pr_mppe_recv.master_key);
	if ((req->pr_ppp_flags & PIPEX_PPP_MPPE_ENABLED) != 0)
		pipex_session_init_mppe_send(session,
		    req->pr_mppe_send.stateless, req->pr_mppe_send.keylenbits,
		    req->pr_mppe_send.master_key);

	if (pipex_session_is_mppe_required(session)) {
		if (!pipex_session_is_mppe_enabled(session) ||
		    !pipex_session_is_mppe_accepted(session)) {
			pool_put(pppx_if_pl, pxi);
			return (EINVAL);
		}
	}
#endif

	/* try to set the interface up */
	rw_enter_write(&pppx_ifs_lk);
	unit = pppx_if_next_unit();
	if (unit < 0) {
		pool_put(pppx_if_pl, pxi);
		error = ENOMEM;
		goto out;
	}

	pxi->pxi_unit = unit;
	pxi->pxi_key.pxik_session_id = req->pr_session_id;
	pxi->pxi_key.pxik_protocol = req->pr_protocol;

	/* this is safe without splnet since we're not modifying it */
	if (RB_FIND(pppx_ifs, &pppx_ifs, pxi) != NULL) {
		pool_put(pppx_if_pl, pxi);
		error = EADDRINUSE;
		goto out;
	}

	snprintf(ifp->if_xname, sizeof(ifp->if_xname), "%s%d", "pppx", unit);
	ifp->if_mtu = req->pr_peer_mru;	/* XXX */
	ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST | IFF_UP;
	ifp->if_start = pppx_if_start;
	ifp->if_output = pppx_if_output;
	ifp->if_ioctl = pppx_if_ioctl;
	ifp->if_type = IFT_PPP;
	IFQ_SET_MAXLEN(&ifp->if_snd, 1);
	IFQ_SET_READY(&ifp->if_snd);
	ifp->if_softc = pxi;
	/* ifp->if_rdomain = req->pr_rdomain; */

	s = splnet();

	/* hook up pipex context */
	chain = PIPEX_ID_HASHTABLE(session->session_id);
	LIST_INSERT_HEAD(chain, session, id_chain);
	LIST_INSERT_HEAD(&pipex_session_list, session, session_list);
	switch (req->pr_protocol) {
	case PIPEX_PROTO_PPTP:
	case PIPEX_PROTO_L2TP:
		chain = PIPEX_PEER_ADDR_HASHTABLE(
		    pipex_sockaddr_hash_key((struct sockaddr *)&session->peer));
		LIST_INSERT_HEAD(chain, session, peer_addr_chain);
		break;
	}

	/* if first session is added, start timer */
	if (LIST_NEXT(session, session_list) == NULL)
		pipex_timer_start();

	if_attach(ifp);
	if_addgroup(ifp, "pppx");
	if_alloc_sadl(ifp);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_LOOP, sizeof(u_int32_t));
#endif
	SET(ifp->if_flags, IFF_RUNNING);

	if (RB_INSERT(pppx_ifs, &pppx_ifs, pxi) != NULL)
		panic("pppx_ifs modified while lock was held");
	LIST_INSERT_HEAD(&pxd->pxd_pxis, pxi, pxi_list);

	/* XXX ipv6 support?  how does the caller indicate it wants ipv6
	 * instead of ipv4?
	 */
	memset(&ifaddr, 0, sizeof(ifaddr));
	ifaddr.sin_family = AF_INET;
	ifaddr.sin_len = sizeof(ifaddr);
	ifaddr.sin_addr = req->pr_ip_srcaddr;

	ia = malloc(sizeof (*ia), M_IFADDR, M_WAITOK | M_ZERO);

	ia->ia_addr.sin_family = AF_INET;
	ia->ia_addr.sin_len = sizeof(struct sockaddr_in);
	ia->ia_addr.sin_addr = req->pr_ip_srcaddr;

	ia->ia_dstaddr.sin_family = AF_INET;
	ia->ia_dstaddr.sin_len = sizeof(struct sockaddr_in);
	ia->ia_dstaddr.sin_addr = req->pr_ip_address;

	ia->ia_sockmask.sin_family = AF_INET;
	ia->ia_sockmask.sin_len = sizeof(struct sockaddr_in);
	ia->ia_sockmask.sin_addr = req->pr_ip_netmask;

	ia->ia_ifa.ifa_addr = sintosa(&ia->ia_addr);
	ia->ia_ifa.ifa_dstaddr = sintosa(&ia->ia_dstaddr);
	ia->ia_ifa.ifa_netmask = sintosa(&ia->ia_sockmask);
	ia->ia_ifa.ifa_ifp = ifp;
	
	ia->ia_netmask = ia->ia_sockmask.sin_addr.s_addr;

	error = in_ifinit(ifp, ia, &ifaddr, 1);
	if (error) {
		printf("pppx: unable to set addresses for %s, error=%d\n",
		    ifp->if_xname, error);
	} else {
		dohooks(ifp->if_addrhooks, 0);
	}
	splx(s);

out:
	rw_exit_write(&pppx_ifs_lk);

	return (error);
}

int
pppx_del_session(struct pppx_dev *pxd, struct pipex_session_close_req *req)
{
	struct pppx_if *pxi;

	pxi = pppx_if_find(pxd, req->pcr_session_id, req->pcr_protocol);
	if (pxi == NULL)
		return (EINVAL);

	req->pcr_stat = pxi->pxi_session.stat;

	pppx_if_destroy(pxd, pxi);
	return (0);
}

int
pppx_set_session_descr(struct pppx_dev *pxd,
    struct pipex_session_descr_req *req)
{
	struct pppx_if *pxi;

	pxi = pppx_if_find(pxd, req->pdr_session_id, req->pdr_protocol);
	if (pxi == NULL)
		return (EINVAL);

	(void)memset(pxi->pxi_if.if_description, 0, IFDESCRSIZE);
	strlcpy(pxi->pxi_if.if_description, req->pdr_descr, IFDESCRSIZE);

	return (0);
}

void
pppx_if_destroy(struct pppx_dev *pxd, struct pppx_if *pxi)
{
	struct ifnet *ifp;
	struct pipex_session *session;
	int s;

	session = &pxi->pxi_session;
	ifp = &pxi->pxi_if;

	s = splnet();
	LIST_REMOVE(session, id_chain);
	LIST_REMOVE(session, session_list);
	switch (session->protocol) {
	case PIPEX_PROTO_PPTP:
	case PIPEX_PROTO_L2TP:
		LIST_REMOVE((struct pipex_session *)session,
		    peer_addr_chain);
		break;
	}

	/* if final session is destroyed, stop timer */
	if (LIST_EMPTY(&pipex_session_list))
		pipex_timer_stop();
	splx(s);

	if_detach(ifp);

	rw_enter_write(&pppx_ifs_lk);
	if (RB_REMOVE(pppx_ifs, &pppx_ifs, pxi) == NULL)
		panic("pppx_ifs modified while lock was held");
	LIST_REMOVE(pxi, pxi_list);
	rw_exit_write(&pppx_ifs_lk);

	pool_put(pppx_if_pl, pxi);
}

void
pppx_if_start(struct ifnet *ifp)
{
	struct pppx_if *pxi = (struct pppx_if *)ifp->if_softc;
	struct mbuf *m;
	int proto, s;

	if (ISSET(ifp->if_flags, IFF_OACTIVE))
		return;
	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return;

	for (;;) {
		s = splnet();
		IFQ_DEQUEUE(&ifp->if_snd, m);
		splx(s);

		if (m == NULL)
			break;

		proto = *mtod(m, int *);
		m_adj(m, sizeof(proto));

#if NBPFILTER > 0
		if (ifp->if_bpf) {
			switch (proto) {
			case PPP_IP:
				bpf_mtap_af(ifp->if_bpf, AF_INET, m,
					BPF_DIRECTION_OUT);
				break;
			case PPP_IPV6:
				bpf_mtap_af(ifp->if_bpf, AF_INET6, m,
					BPF_DIRECTION_OUT);
				break;
			}
		}
#endif

		ifp->if_obytes += m->m_pkthdr.len;
		ifp->if_opackets++;

		pipex_ppp_output(m, &pxi->pxi_session, proto);
	}
}

int
pppx_if_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	int error = 0;
	int proto, s;

	if (!ISSET(ifp->if_flags, IFF_UP)) {
		m_freem(m);
		error = ENETDOWN;
		goto out;
	}

	switch (dst->sa_family) {
	case AF_INET:
		proto = PPP_IP;
		break;
	default:
		m_freem(m);
		error = EPFNOSUPPORT;
		goto out;
	}

	M_PREPEND(m, sizeof(int), M_DONTWAIT);
	if (m == NULL) {
		error = ENOBUFS;
		goto out;
	}
	*mtod(m, int *) = proto;

	s = splnet();
	IFQ_ENQUEUE(&ifp->if_snd, m, NULL, error);
	if (error) {
		splx(s);
		goto out;
	}
	if_start(ifp);
	splx(s);

out:
	if (error)
		ifp->if_oerrors++;
	return (error);
}

int
pppx_if_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr)
{
	struct pppx_if *pxi = (struct pppx_if *)ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)addr;
	int error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
	case SIOCSIFFLAGS:
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu < 512 ||
		    ifr->ifr_mtu > pxi->pxi_session.peer_mru)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

RB_GENERATE(pppx_ifs, pppx_if, pxi_entry, pppx_if_cmp);
