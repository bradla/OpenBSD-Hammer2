/*	$OpenBSD: uipc_mbuf.c,v 1.194 2014/09/14 14:17:26 jsg Exp $	*/
/*	$NetBSD: uipc_mbuf.c,v 1.15.4.1 1996/06/13 17:11:44 cgd Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1991, 1993
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
 *	@(#)uipc_mbuf.c	8.2 (Berkeley) 1/4/94
 */

/*
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
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/pool.h>

#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>


#include <uvm/uvm_extern.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#endif

struct	mbstat mbstat;		/* mbuf stats */
struct	pool mbpool;		/* mbuf pool */
struct	pool mtagpool;

/* mbuf cluster pools */
u_int	mclsizes[] = {
	MCLBYTES,	/* must be at slot 0 */
	4 * 1024,
	8 * 1024,
	9 * 1024,
	12 * 1024,
	16 * 1024,
	64 * 1024
};
static	char mclnames[MCLPOOLS][8];
struct	pool mclpools[MCLPOOLS];

struct pool *m_clpool(u_int);

int max_linkhdr;		/* largest link-level header */
int max_protohdr;		/* largest protocol header */
int max_hdr;			/* largest link+protocol header */

void	m_extfree(struct mbuf *);
struct mbuf *m_copym0(struct mbuf *, int, int, int, int);
void	nmbclust_update(void);
void	m_zero(struct mbuf *);


const char *mclpool_warnmsg =
    "WARNING: mclpools limit reached; increase kern.maxclusters";

/*
 * Initialize the mbuf allocator.
 */
void
mbinit(void)
{
	int i;

#if DIAGNOSTIC
	if (mclsizes[nitems(mclsizes) - 1] != MAXMCLBYTES)
		panic("mbinit: the largest cluster size != MAXMCLBYTES");
#endif

	pool_init(&mbpool, MSIZE, 0, 0, 0, "mbpl", NULL);
	pool_set_constraints(&mbpool, &kp_dma_contig);
	pool_setlowat(&mbpool, mblowat);

	pool_init(&mtagpool, PACKET_TAG_MAXSIZE + sizeof(struct m_tag),
	    0, 0, 0, "mtagpl", NULL);
	pool_setipl(&mtagpool, IPL_NET);

	for (i = 0; i < nitems(mclsizes); i++) {
		snprintf(mclnames[i], sizeof(mclnames[0]), "mcl%dk",
		    mclsizes[i] >> 10);
		pool_init(&mclpools[i], mclsizes[i], 0, 0, 0,
		    mclnames[i], NULL);
		pool_set_constraints(&mclpools[i], &kp_dma_contig);
		pool_setlowat(&mclpools[i], mcllowat);
	}

	nmbclust_update();
}

void
nmbclust_update(void)
{
	int i;
	/*
	 * Set the hard limit on the mclpools to the number of
	 * mbuf clusters the kernel is to support.  Log the limit
	 * reached message max once a minute.
	 */
	for (i = 0; i < nitems(mclsizes); i++) {
		(void)pool_sethardlimit(&mclpools[i], nmbclust,
		    mclpool_warnmsg, 60);
		/*
		 * XXX this needs to be reconsidered.
		 * Setting the high water mark to nmbclust is too high
		 * but we need to have enough spare buffers around so that
		 * allocations in interrupt context don't fail or mclgeti()
		 * drivers may end up with empty rings.
		 */
		pool_sethiwat(&mclpools[i], nmbclust);
	}
	pool_sethiwat(&mbpool, nmbclust);
}

void
m_reclaim(void *arg, int flags)
{
	struct domain *dp;
	struct protosw *pr;
	int s = splnet();

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_drain)
				(*pr->pr_drain)();
	mbstat.m_drain++;
	splx(s);
}

/*
 * Space allocation routines.
 */
struct mbuf *
m_get(int nowait, int type)
{
	struct mbuf *m;
	int s;

	s = splnet();
	m = pool_get(&mbpool, nowait == M_WAIT ? PR_WAITOK : PR_NOWAIT);
	if (m)
		mbstat.m_mtypes[type]++;
	splx(s);
	if (m) {
		m->m_type = type;
		m->m_next = NULL;
		m->m_nextpkt = NULL;
		m->m_data = m->m_dat;
		m->m_flags = 0;
	}
	return (m);
}

/*
 * ATTN: When changing anything here check m_inithdr() and m_defrag() those
 * may need to change as well.
 */
struct mbuf *
m_gethdr(int nowait, int type)
{
	struct mbuf *m;
	int s;

	s = splnet();
	m = pool_get(&mbpool, nowait == M_WAIT ? PR_WAITOK : PR_NOWAIT);
	if (m)
		mbstat.m_mtypes[type]++;
	splx(s);
	if (m) {
		m->m_type = type;

		/* keep in sync with m_inithdr */
		m->m_next = NULL;
		m->m_nextpkt = NULL;
		m->m_data = m->m_pktdat;
		m->m_flags = M_PKTHDR;
		memset(&m->m_pkthdr, 0, sizeof(m->m_pkthdr));
		m->m_pkthdr.pf.prio = IFQ_DEFPRIO;
	}
	return (m);
}

struct mbuf *
m_inithdr(struct mbuf *m)
{
	/* keep in sync with m_gethdr */
	m->m_next = NULL;
	m->m_nextpkt = NULL;
	m->m_data = m->m_pktdat;
	m->m_flags = M_PKTHDR;
	memset(&m->m_pkthdr, 0, sizeof(m->m_pkthdr));
	m->m_pkthdr.pf.prio = IFQ_DEFPRIO;

	return (m);
}

struct mbuf *
m_getclr(int nowait, int type)
{
	struct mbuf *m;

	MGET(m, nowait, type);
	if (m == NULL)
		return (NULL);
	memset(mtod(m, caddr_t), 0, MLEN);
	return (m);
}

struct pool *
m_clpool(u_int pktlen)
{
	struct pool *pp;
	int pi;

	for (pi = 0; pi < nitems(mclpools); pi++) {
		pp = &mclpools[pi];
		if (pktlen <= pp->pr_size)
			return (pp);
	}

	return (NULL);
}

struct mbuf *
m_clget(struct mbuf *m, int how, struct ifnet *ifp, u_int pktlen)
{
	struct mbuf *m0 = NULL;
	struct pool *pp;
	caddr_t buf;
	int s;

	pp = m_clpool(pktlen);
#ifdef DIAGNOSTIC
	if (pp == NULL)
		panic("m_clget: request for %u byte cluster", pktlen);
#endif

	s = splnet();
	if (m == NULL) {
		MGETHDR(m0, M_DONTWAIT, MT_DATA);
		if (m0 == NULL) {
			splx(s);
			return (NULL);
		}
		m = m0;
	}
	buf = pool_get(pp, how == M_WAIT ? PR_WAITOK : PR_NOWAIT);
	if (buf == NULL) {
		if (m0)
			m_freem(m0);
		splx(s);
		return (NULL);
	}
	splx(s);

	MEXTADD(m, buf, pp->pr_size, M_EXTWR, m_extfree_pool, pp);
	return (m);
}

void
m_extfree_pool(caddr_t buf, u_int size, void *pp)
{
	splassert(IPL_NET);
	pool_put(pp, buf);
}

struct mbuf *
m_free_unlocked(struct mbuf *m)
{
	struct mbuf *n;

	mbstat.m_mtypes[m->m_type]--;
	n = m->m_next;
	if (m->m_flags & M_ZEROIZE) {
		m_zero(m);
		/* propagate M_ZEROIZE to the next mbuf in the chain */
		if (n)
			n->m_flags |= M_ZEROIZE;
	}
	if (m->m_flags & M_PKTHDR)
		m_tag_delete_chain(m);
	if (m->m_flags & M_EXT)
		m_extfree(m);
	pool_put(&mbpool, m);

	return (n);
}

struct mbuf *
m_free(struct mbuf *m)
{
	struct mbuf *n;
	int s;

	s = splnet();
	n = m_free_unlocked(m);
	splx(s);

	return (n);
}

void
m_extfree(struct mbuf *m)
{
	if (MCLISREFERENCED(m)) {
		m->m_ext.ext_nextref->m_ext.ext_prevref =
		    m->m_ext.ext_prevref;
		m->m_ext.ext_prevref->m_ext.ext_nextref =
		    m->m_ext.ext_nextref;
	} else if (m->m_ext.ext_free)
		(*(m->m_ext.ext_free))(m->m_ext.ext_buf,
		    m->m_ext.ext_size, m->m_ext.ext_arg);
	else
		panic("unknown type of extension buffer");
	m->m_ext.ext_size = 0;
	m->m_flags &= ~(M_EXT|M_EXTWR);
}

void
m_freem(struct mbuf *m)
{
	struct mbuf *n;
	int s;

	if (m == NULL)
		return;
	s = splnet();
	do {
		n = m_free_unlocked(m);
	} while ((m = n) != NULL);
	splx(s);
}

/*
 * mbuf chain defragmenter. This function uses some evil tricks to defragment
 * an mbuf chain into a single buffer without changing the mbuf pointer.
 * This needs to know a lot of the mbuf internals to make this work.
 */
int
m_defrag(struct mbuf *m, int how)
{
	struct mbuf *m0;

	if (m->m_next == NULL)
		return (0);

#ifdef DIAGNOSTIC
	if (!(m->m_flags & M_PKTHDR))
		panic("m_defrag: no packet hdr or not a chain");
#endif

	if ((m0 = m_gethdr(how, m->m_type)) == NULL)
		return (ENOBUFS);
	if (m->m_pkthdr.len > MHLEN) {
		MCLGETI(m0, how, NULL, m->m_pkthdr.len);
		if (!(m0->m_flags & M_EXT)) {
			m_free(m0);
			return (ENOBUFS);
		}
	}
	m_copydata(m, 0, m->m_pkthdr.len, mtod(m0, caddr_t));
	m0->m_pkthdr.len = m0->m_len = m->m_pkthdr.len;

	/* free chain behind and possible ext buf on the first mbuf */
	m_freem(m->m_next);
	m->m_next = NULL;

	if (m->m_flags & M_EXT) {
		int s = splnet();
		m_extfree(m);
		splx(s);
	}

	/*
	 * Bounce copy mbuf over to the original mbuf and set everything up.
	 * This needs to reset or clear all pointers that may go into the
	 * original mbuf chain.
	 */
	if (m0->m_flags & M_EXT) {
		memcpy(&m->m_ext, &m0->m_ext, sizeof(struct mbuf_ext));
		MCLINITREFERENCE(m);
		m->m_flags |= m0->m_flags & (M_EXT|M_EXTWR);
		m->m_data = m->m_ext.ext_buf;
	} else {
		m->m_data = m->m_pktdat;
		memcpy(m->m_data, m0->m_data, m0->m_len);
	}
	m->m_pkthdr.len = m->m_len = m0->m_len;

	m0->m_flags &= ~(M_EXT|M_EXTWR);	/* cluster is gone */
	m_free(m0);

	return (0);
}

/*
 * Mbuffer utility routines.
 */

/*
 * Ensure len bytes of contiguous space at the beginning of the mbuf chain
 */
struct mbuf *
m_prepend(struct mbuf *m, int len, int how)
{
	struct mbuf *mn;

	if (len > MHLEN)
		panic("mbuf prepend length too big");

	if (M_LEADINGSPACE(m) >= len) {
		m->m_data -= len;
		m->m_len += len;
	} else {
		MGET(mn, how, m->m_type);
		if (mn == NULL) {
			m_freem(m);
			return (NULL);
		}
		if (m->m_flags & M_PKTHDR)
			M_MOVE_PKTHDR(mn, m);
		mn->m_next = m;
		m = mn;
		MH_ALIGN(m, len);
		m->m_len = len;
	}
	if (m->m_flags & M_PKTHDR)
		m->m_pkthdr.len += len;
	return (m);
}

/*
 * Make a copy of an mbuf chain starting "off" bytes from the beginning,
 * continuing for "len" bytes.  If len is M_COPYALL, copy to end of mbuf.
 * The wait parameter is a choice of M_WAIT/M_DONTWAIT from caller.
 */
struct mbuf *
m_copym(struct mbuf *m, int off, int len, int wait)
{
	return m_copym0(m, off, len, wait, 0);	/* shallow copy on M_EXT */
}

/*
 * m_copym2() is like m_copym(), except it COPIES cluster mbufs, instead
 * of merely bumping the reference count.
 */
struct mbuf *
m_copym2(struct mbuf *m, int off, int len, int wait)
{
	return m_copym0(m, off, len, wait, 1);	/* deep copy */
}

struct mbuf *
m_copym0(struct mbuf *m0, int off, int len, int wait, int deep)
{
	struct mbuf *m, *n, **np;
	struct mbuf *top;
	int copyhdr = 0;

	if (off < 0 || len < 0)
		panic("m_copym0: off %d, len %d", off, len);
	if (off == 0 && m0->m_flags & M_PKTHDR)
		copyhdr = 1;
	if ((m = m_getptr(m0, off, &off)) == NULL)
		panic("m_copym0: short mbuf chain");
	np = &top;
	top = NULL;
	while (len > 0) {
		if (m == NULL) {
			if (len != M_COPYALL)
				panic("m_copym0: m == NULL and not COPYALL");
			break;
		}
		MGET(n, wait, m->m_type);
		*np = n;
		if (n == NULL)
			goto nospace;
		if (copyhdr) {
			if (m_dup_pkthdr(n, m0, wait))
				goto nospace;
			if (len != M_COPYALL)
				n->m_pkthdr.len = len;
			copyhdr = 0;
		}
		n->m_len = min(len, m->m_len - off);
		if (m->m_flags & M_EXT) {
			if (!deep) {
				n->m_data = m->m_data + off;
				n->m_ext = m->m_ext;
				MCLADDREFERENCE(m, n);
			} else {
				/*
				 * we are unsure about the way m was allocated.
				 * copy into multiple MCLBYTES cluster mbufs.
				 */
				MCLGET(n, wait);
				n->m_len = 0;
				n->m_len = M_TRAILINGSPACE(n);
				n->m_len = min(n->m_len, len);
				n->m_len = min(n->m_len, m->m_len - off);
				memcpy(mtod(n, caddr_t), mtod(m, caddr_t) + off,
				    n->m_len);
			}
		} else
			memcpy(mtod(n, caddr_t), mtod(m, caddr_t) + off,
			    n->m_len);
		if (len != M_COPYALL)
			len -= n->m_len;
		off += n->m_len;
#ifdef DIAGNOSTIC
		if (off > m->m_len)
			panic("m_copym0 overrun");
#endif
		if (off == m->m_len) {
			m = m->m_next;
			off = 0;
		}
		np = &n->m_next;
	}
	return (top);
nospace:
	m_freem(top);
	return (NULL);
}

/*
 * Copy data from an mbuf chain starting "off" bytes from the beginning,
 * continuing for "len" bytes, into the indicated buffer.
 */
void
m_copydata(struct mbuf *m, int off, int len, caddr_t cp)
{
	unsigned count;

	if (off < 0)
		panic("m_copydata: off %d < 0", off);
	if (len < 0)
		panic("m_copydata: len %d < 0", len);
	if ((m = m_getptr(m, off, &off)) == NULL)
		panic("m_copydata: short mbuf chain");
	while (len > 0) {
		if (m == NULL)
			panic("m_copydata: null mbuf");
		count = min(m->m_len - off, len);
		bcopy(mtod(m, caddr_t) + off, cp, count);
		len -= count;
		cp += count;
		off = 0;
		m = m->m_next;
	}
}

/*
 * Copy data from a buffer back into the indicated mbuf chain,
 * starting "off" bytes from the beginning, extending the mbuf
 * chain if necessary. The mbuf needs to be properly initialized
 * including the setting of m_len.
 */
int
m_copyback(struct mbuf *m0, int off, int len, const void *_cp, int wait)
{
	int mlen, totlen = 0;
	struct mbuf *m = m0, *n;
	caddr_t cp = (caddr_t)_cp;
	int error = 0;

	if (m0 == NULL)
		return (0);
	while (off > (mlen = m->m_len)) {
		off -= mlen;
		totlen += mlen;
		if (m->m_next == NULL) {
			if ((n = m_get(wait, m->m_type)) == NULL) {
				error = ENOBUFS;
				goto out;
			}

			if (off + len > MLEN) {
				MCLGETI(n, wait, NULL, off + len);
				if (!(n->m_flags & M_EXT)) {
					m_free(n);
					error = ENOBUFS;
					goto out;
				}
			}
			memset(mtod(n, caddr_t), 0, off);
			n->m_len = len + off;
			m->m_next = n;
		}
		m = m->m_next;
	}
	while (len > 0) {
		/* extend last packet to be filled fully */
		if (m->m_next == NULL && (len > m->m_len - off))
			m->m_len += min(len - (m->m_len - off),
			    M_TRAILINGSPACE(m));
		mlen = min(m->m_len - off, len);
		bcopy(cp, mtod(m, caddr_t) + off, (size_t)mlen);
		cp += mlen;
		len -= mlen;
		totlen += mlen + off;
		if (len == 0)
			break;
		off = 0;

		if (m->m_next == NULL) {
			if ((n = m_get(wait, m->m_type)) == NULL) {
				error = ENOBUFS;
				goto out;
			}

			if (len > MLEN) {
				MCLGETI(n, wait, NULL, len);
				if (!(n->m_flags & M_EXT)) {
					m_free(n);
					error = ENOBUFS;
					goto out;
				}
			}
			n->m_len = len;
			m->m_next = n;
		}
		m = m->m_next;
	}
out:
	if (((m = m0)->m_flags & M_PKTHDR) && (m->m_pkthdr.len < totlen))
		m->m_pkthdr.len = totlen;

	return (error);
}

/*
 * Concatenate mbuf chain n to m.
 * n might be copied into m (when n->m_len is small), therefore data portion of
 * n could be copied into an mbuf of different mbuf type.
 * Therefore both chains should be of the same type (e.g. MT_DATA).
 * Any m_pkthdr is not updated.
 */
void
m_cat(struct mbuf *m, struct mbuf *n)
{
	while (m->m_next)
		m = m->m_next;
	while (n) {
		if (M_READONLY(m) || n->m_len > M_TRAILINGSPACE(m)) {
			/* just join the two chains */
			m->m_next = n;
			return;
		}
		/* splat the data from one into the other */
		bcopy(mtod(n, caddr_t), mtod(m, caddr_t) + m->m_len,
		    (u_int)n->m_len);
		m->m_len += n->m_len;
		n = m_free(n);
	}
}

void
m_adj(struct mbuf *mp, int req_len)
{
	int len = req_len;
	struct mbuf *m;
	int count;

	if ((m = mp) == NULL)
		return;
	if (len >= 0) {
		/*
		 * Trim from head.
		 */
		while (m != NULL && len > 0) {
			if (m->m_len <= len) {
				len -= m->m_len;
				m->m_len = 0;
				m = m->m_next;
			} else {
				m->m_len -= len;
				m->m_data += len;
				len = 0;
			}
		}
		if (mp->m_flags & M_PKTHDR)
			mp->m_pkthdr.len -= (req_len - len);
	} else {
		/*
		 * Trim from tail.  Scan the mbuf chain,
		 * calculating its length and finding the last mbuf.
		 * If the adjustment only affects this mbuf, then just
		 * adjust and return.  Otherwise, rescan and truncate
		 * after the remaining size.
		 */
		len = -len;
		count = 0;
		for (;;) {
			count += m->m_len;
			if (m->m_next == NULL)
				break;
			m = m->m_next;
		}
		if (m->m_len >= len) {
			m->m_len -= len;
			if (mp->m_flags & M_PKTHDR)
				mp->m_pkthdr.len -= len;
			return;
		}
		count -= len;
		if (count < 0)
			count = 0;
		/*
		 * Correct length for chain is "count".
		 * Find the mbuf with last data, adjust its length,
		 * and toss data from remaining mbufs on chain.
		 */
		m = mp;
		if (m->m_flags & M_PKTHDR)
			m->m_pkthdr.len = count;
		for (; m; m = m->m_next) {
			if (m->m_len >= count) {
				m->m_len = count;
				break;
			}
			count -= m->m_len;
		}
		while ((m = m->m_next) != NULL)
			m->m_len = 0;
	}
}

/*
 * Rearrange an mbuf chain so that len bytes are contiguous
 * and in the data area of an mbuf (so that mtod will work
 * for a structure of size len).  Returns the resulting
 * mbuf chain on success, frees it and returns null on failure.
 */
struct mbuf *
m_pullup(struct mbuf *n, int len)
{
	struct mbuf *m;
	int count;

	/*
	 * If first mbuf has no cluster, and has room for len bytes
	 * without shifting current data, pullup into it,
	 * otherwise allocate a new mbuf to prepend to the chain.
	 */
	if ((n->m_flags & M_EXT) == 0 && n->m_next &&
	    n->m_data + len < &n->m_dat[MLEN]) {
		if (n->m_len >= len)
			return (n);
		m = n;
		n = n->m_next;
		len -= m->m_len;
	} else if ((n->m_flags & M_EXT) != 0 && len > MHLEN && n->m_next &&
	    n->m_data + len < &n->m_ext.ext_buf[n->m_ext.ext_size]) {
		if (n->m_len >= len)
			return (n);
		m = n;
		n = n->m_next;
		len -= m->m_len;
	} else {
		if (len > MAXMCLBYTES)
			goto bad;
		MGET(m, M_DONTWAIT, n->m_type);
		if (m == NULL)
			goto bad;
		if (len > MHLEN) {
			MCLGETI(m, M_DONTWAIT, NULL, len);
			if ((m->m_flags & M_EXT) == 0) {
				m_free(m);
				goto bad;
			}
		}
		m->m_len = 0;
		if (n->m_flags & M_PKTHDR)
			M_MOVE_PKTHDR(m, n);
	}

	do {
		count = min(len, n->m_len);
		bcopy(mtod(n, caddr_t), mtod(m, caddr_t) + m->m_len,
		    (unsigned)count);
		len -= count;
		m->m_len += count;
		n->m_len -= count;
		if (n->m_len)
			n->m_data += count;
		else
			n = m_free(n);
	} while (len > 0 && n);
	if (len > 0) {
		(void)m_free(m);
		goto bad;
	}
	m->m_next = n;

	return (m);
bad:
	m_freem(n);
	return (NULL);
}

/*
 * Return a pointer to mbuf/offset of location in mbuf chain.
 */
struct mbuf *
m_getptr(struct mbuf *m, int loc, int *off)
{
	while (loc >= 0) {
		/* Normal end of search */
		if (m->m_len > loc) {
	    		*off = loc;
	    		return (m);
		} else {
	    		loc -= m->m_len;

	    		if (m->m_next == NULL) {
				if (loc == 0) {
 					/* Point at the end of valid data */
		    			*off = m->m_len;
		    			return (m);
				} else {
		  			return (NULL);
				}
	    		} else {
				m = m->m_next;
			}
		}
    	}

	return (NULL);
}

/*
 * Inject a new mbuf chain of length siz in mbuf chain m0 at
 * position len0. Returns a pointer to the first injected mbuf, or
 * NULL on failure (m0 is left undisturbed). Note that if there is
 * enough space for an object of size siz in the appropriate position,
 * no memory will be allocated. Also, there will be no data movement in
 * the first len0 bytes (pointers to that will remain valid).
 *
 * XXX It is assumed that siz is less than the size of an mbuf at the moment.
 */
struct mbuf *
m_inject(struct mbuf *m0, int len0, int siz, int wait)
{
	struct mbuf *m, *n, *n2 = NULL, *n3;
	unsigned len = len0, remain;

	if ((siz >= MHLEN) || (len0 <= 0))
		return (NULL);
	for (m = m0; m && len > m->m_len; m = m->m_next)
		len -= m->m_len;
	if (m == NULL)
		return (NULL);
	remain = m->m_len - len;
	if (remain == 0) {
		if ((m->m_next) && (M_LEADINGSPACE(m->m_next) >= siz)) {
			m->m_next->m_len += siz;
			if (m0->m_flags & M_PKTHDR)
				m0->m_pkthdr.len += siz;
			m->m_next->m_data -= siz;
			return m->m_next;
		}
	} else {
		n2 = m_copym2(m, len, remain, wait);
		if (n2 == NULL)
			return (NULL);
	}

	MGET(n, wait, MT_DATA);
	if (n == NULL) {
		if (n2)
			m_freem(n2);
		return (NULL);
	}

	n->m_len = siz;
	if (m0->m_flags & M_PKTHDR)
		m0->m_pkthdr.len += siz;
	m->m_len -= remain; /* Trim */
	if (n2)	{
		for (n3 = n; n3->m_next != NULL; n3 = n3->m_next)
			;
		n3->m_next = n2;
	} else
		n3 = n;
	for (; n3->m_next != NULL; n3 = n3->m_next)
		;
	n3->m_next = m->m_next;
	m->m_next = n;
	return n;
}

/*
 * Partition an mbuf chain in two pieces, returning the tail --
 * all but the first len0 bytes.  In case of failure, it returns NULL and
 * attempts to restore the chain to its original state.
 */
struct mbuf *
m_split(struct mbuf *m0, int len0, int wait)
{
	struct mbuf *m, *n;
	unsigned len = len0, remain, olen;

	for (m = m0; m && len > m->m_len; m = m->m_next)
		len -= m->m_len;
	if (m == NULL)
		return (NULL);
	remain = m->m_len - len;
	if (m0->m_flags & M_PKTHDR) {
		MGETHDR(n, wait, m0->m_type);
		if (n == NULL)
			return (NULL);
		if (m_dup_pkthdr(n, m0, wait)) {
			m_freem(n);
			return (NULL);
		}
		n->m_pkthdr.len -= len0;
		olen = m0->m_pkthdr.len;
		m0->m_pkthdr.len = len0;
		if (m->m_flags & M_EXT)
			goto extpacket;
		if (remain > MHLEN) {
			/* m can't be the lead packet */
			MH_ALIGN(n, 0);
			n->m_next = m_split(m, len, wait);
			if (n->m_next == NULL) {
				(void) m_free(n);
				m0->m_pkthdr.len = olen;
				return (NULL);
			} else
				return (n);
		} else
			MH_ALIGN(n, remain);
	} else if (remain == 0) {
		n = m->m_next;
		m->m_next = NULL;
		return (n);
	} else {
		MGET(n, wait, m->m_type);
		if (n == NULL)
			return (NULL);
		M_ALIGN(n, remain);
	}
extpacket:
	if (m->m_flags & M_EXT) {
		n->m_ext = m->m_ext;
		MCLADDREFERENCE(m, n);
		n->m_data = m->m_data + len;
	} else {
		bcopy(mtod(m, caddr_t) + len, mtod(n, caddr_t), remain);
	}
	n->m_len = remain;
	m->m_len = len;
	n->m_next = m->m_next;
	m->m_next = NULL;
	return (n);
}

/*
 * Routine to copy from device local memory into mbufs.
 */
struct mbuf *
m_devget(char *buf, int totlen, int off, struct ifnet *ifp)
{
	struct mbuf	*m;
	struct mbuf	*top, **mp;
	int		 len;

	top = NULL;
	mp = &top;

	if (off < 0 || off > MHLEN)
		return (NULL);

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);

	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;

	len = MHLEN;

	while (totlen > 0) {
		if (top != NULL) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				/*
				 * As we might get called by pfkey, make sure
				 * we do not leak sensitive data.
				 */
				top->m_flags |= M_ZEROIZE;
				m_freem(top);
				return (NULL);
			}
			len = MLEN;
		}

		if (totlen + off >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
		} else {
			/* Place initial small packet/header at end of mbuf. */
			if (top == NULL && totlen + off + max_linkhdr <= len) {
				m->m_data += max_linkhdr;
				len -= max_linkhdr;
			}
		}

		if (off) {
			m->m_data += off;
			len -= off;
			off = 0;
		}

		m->m_len = len = min(totlen, len);
		memcpy(mtod(m, void *), buf, (size_t)len);

		buf += len;
		*mp = m;
		mp = &m->m_next;
		totlen -= len;
	}
	return (top);
}

void
m_zero(struct mbuf *m)
{
#ifdef DIAGNOSTIC
	if (M_READONLY(m))
		panic("m_zero: M_READONLY");
#endif /* DIAGNOSTIC */

	if (m->m_flags & M_EXT)
		explicit_bzero(m->m_ext.ext_buf, m->m_ext.ext_size);
	else {
		if (m->m_flags & M_PKTHDR)
			explicit_bzero(m->m_pktdat, MHLEN);
		else
			explicit_bzero(m->m_dat, MLEN);
	}
}

/*
 * Apply function f to the data in an mbuf chain starting "off" bytes from the
 * beginning, continuing for "len" bytes.
 */
int
m_apply(struct mbuf *m, int off, int len,
    int (*f)(caddr_t, caddr_t, unsigned int), caddr_t fstate)
{
	int rval;
	unsigned int count;

	if (len < 0)
		panic("m_apply: len %d < 0", len);
	if (off < 0)
		panic("m_apply: off %d < 0", off);
	while (off > 0) {
		if (m == NULL)
			panic("m_apply: null mbuf in skip");
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	while (len > 0) {
		if (m == NULL)
			panic("m_apply: null mbuf");
		count = min(m->m_len - off, len);

		rval = f(fstate, mtod(m, caddr_t) + off, count);
		if (rval)
			return (rval);

		len -= count;
		off = 0;
		m = m->m_next;
	}

	return (0);
}

int
m_leadingspace(struct mbuf *m)
{
	if (M_READONLY(m))
		return 0;
	return (m->m_flags & M_EXT ? m->m_data - m->m_ext.ext_buf :
	    m->m_flags & M_PKTHDR ? m->m_data - m->m_pktdat :
	    m->m_data - m->m_dat);
}

int
m_trailingspace(struct mbuf *m)
{
	if (M_READONLY(m))
		return 0;
	return (m->m_flags & M_EXT ? m->m_ext.ext_buf +
	    m->m_ext.ext_size - (m->m_data + m->m_len) :
	    &m->m_dat[MLEN] - (m->m_data + m->m_len));
}


/*
 * Duplicate mbuf pkthdr from from to to.
 * from must have M_PKTHDR set, and to must be empty.
 */
int
m_dup_pkthdr(struct mbuf *to, struct mbuf *from, int wait)
{
	int error;

	KASSERT(from->m_flags & M_PKTHDR);

	to->m_flags = (to->m_flags & (M_EXT | M_EXTWR));
	to->m_flags |= (from->m_flags & M_COPYFLAGS);
	to->m_pkthdr = from->m_pkthdr;

	SLIST_INIT(&to->m_pkthdr.tags);

	if ((error = m_tag_copy_chain(to, from, wait)) != 0)
		return (error);

	if ((to->m_flags & M_EXT) == 0)
		to->m_data = to->m_pktdat;

	return (0);
}

#ifdef DDB
void
m_print(void *v,
    int (*pr)(const char *, ...) __attribute__((__format__(__kprintf__,1,2))))
{
	struct mbuf *m = v;

	(*pr)("mbuf %p\n", m);
	(*pr)("m_type: %i\tm_flags: %b\n", m->m_type, m->m_flags, M_BITS);
	(*pr)("m_next: %p\tm_nextpkt: %p\n", m->m_next, m->m_nextpkt);
	(*pr)("m_data: %p\tm_len: %u\n", m->m_data, m->m_len);
	(*pr)("m_dat: %p\tm_pktdat: %p\n", m->m_dat, m->m_pktdat);
	if (m->m_flags & M_PKTHDR) {
		(*pr)("m_ptkhdr.rcvif: %p\tm_pkthdr.len: %i\n",
		    m->m_pkthdr.rcvif, m->m_pkthdr.len);
		(*pr)("m_ptkhdr.tags: %p\tm_pkthdr.tagsset: %b\n",
		    SLIST_FIRST(&m->m_pkthdr.tags),
		    m->m_pkthdr.tagsset, MTAG_BITS);
		(*pr)("m_pkthdr.csum_flags: %b\n",
		    m->m_pkthdr.csum_flags, MCS_BITS);
		(*pr)("m_pkthdr.ether_vtag: %u\tm_ptkhdr.ph_rtableid: %u\n",
		    m->m_pkthdr.ether_vtag, m->m_pkthdr.ph_rtableid);
		(*pr)("m_pkthdr.pf.statekey: %p\tm_pkthdr.pf.inp %p\n",
		    m->m_pkthdr.pf.statekey, m->m_pkthdr.pf.inp);
		(*pr)("m_pkthdr.pf.qid: %u\tm_pkthdr.pf.tag: %u\n",
		    m->m_pkthdr.pf.qid, m->m_pkthdr.pf.tag);
		(*pr)("m_pkthdr.pf.flags: %b\n",
		    m->m_pkthdr.pf.flags, MPF_BITS);
		(*pr)("m_pkthdr.pf.routed: %u\tm_pkthdr.pf.prio: %u\n",
		    m->m_pkthdr.pf.routed, m->m_pkthdr.pf.prio);
	}
	if (m->m_flags & M_EXT) {
		(*pr)("m_ext.ext_buf: %p\tm_ext.ext_size: %u\n",
		    m->m_ext.ext_buf, m->m_ext.ext_size);
		(*pr)("m_ext.ext_free: %p\tm_ext.ext_arg: %p\n",
		    m->m_ext.ext_free, m->m_ext.ext_arg);
		(*pr)("m_ext.ext_nextref: %p\tm_ext.ext_prevref: %p\n",
		    m->m_ext.ext_nextref, m->m_ext.ext_prevref);

	}
}
#endif

/*
 * mbuf lists
 */

void ml_join(struct mbuf_list *, struct mbuf_list *);

void
ml_init(struct mbuf_list *ml)
{
	ml->ml_head = ml->ml_tail = NULL;
	ml->ml_len = 0;
}

void
ml_enqueue(struct mbuf_list *ml, struct mbuf *m)
{
	if (ml->ml_tail == NULL)
		ml->ml_head = ml->ml_tail = m;
	else {
		ml->ml_tail->m_nextpkt = m;
		ml->ml_tail = m;
	}

	m->m_nextpkt = NULL;
	ml->ml_len++;
}

void
ml_join(struct mbuf_list *mla, struct mbuf_list *mlb)
{
	if (mla->ml_tail == NULL)
		*mla = *mlb;
	else if (mlb->ml_tail != NULL) {
		mla->ml_tail->m_nextpkt = mlb->ml_head;
		mla->ml_tail = mlb->ml_tail;
		mla->ml_len += mlb->ml_len;

		ml_init(mlb);
	}
}

struct mbuf *
ml_dequeue(struct mbuf_list *ml)
{
	struct mbuf *m;

	m = ml->ml_head;
	if (m != NULL) {
		ml->ml_head = m->m_nextpkt;
		if (ml->ml_head == NULL)
			ml->ml_tail = NULL;

		m->m_nextpkt = NULL;
		ml->ml_len--;
	}

	return (m);
}

struct mbuf *
ml_dechain(struct mbuf_list *ml)
{
	struct mbuf *m0;

	m0 = ml->ml_head;

	ml_init(ml);

	return (m0);
}

/*
 * mbuf queues
 */

void
mq_init(struct mbuf_queue *mq, u_int maxlen, int ipl)
{
	mtx_init(&mq->mq_mtx, ipl);
	ml_init(&mq->mq_list);
	mq->mq_maxlen = maxlen;
}

int
mq_enqueue(struct mbuf_queue *mq, struct mbuf *m)
{
	int dropped = 0;

	mtx_enter(&mq->mq_mtx);
	if (mq_len(mq) < mq->mq_maxlen)
		ml_enqueue(&mq->mq_list, m);
	else {
		mq->mq_drops++;
		dropped = 1;
	}
	mtx_leave(&mq->mq_mtx);

	if (dropped)
		m_freem(m);

	return (dropped);
}

struct mbuf *
mq_dequeue(struct mbuf_queue *mq)
{
	struct mbuf *m;

	mtx_enter(&mq->mq_mtx);
	m = ml_dequeue(&mq->mq_list);
	mtx_leave(&mq->mq_mtx);

	return (m);
}

int
mq_enlist(struct mbuf_queue *mq, struct mbuf_list *ml)
{
	int full;

	mtx_enter(&mq->mq_mtx);
	ml_join(&mq->mq_list, ml);
	full = mq_len(mq) >= mq->mq_maxlen;
	mtx_leave(&mq->mq_mtx);

	return (full);
}

void
mq_delist(struct mbuf_queue *mq, struct mbuf_list *ml)
{
	mtx_enter(&mq->mq_mtx);
	*ml = mq->mq_list;
	ml_init(&mq->mq_list);
	mtx_leave(&mq->mq_mtx);
}

struct mbuf *
mq_dechain(struct mbuf_queue *mq)
{
	struct mbuf *m0;

	mtx_enter(&mq->mq_mtx);
	m0 = ml_dechain(&mq->mq_list);
	mtx_leave(&mq->mq_mtx);

	return (m0);
}
