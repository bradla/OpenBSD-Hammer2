/*	$OpenBSD: radix.c,v 1.39 2014/01/22 10:17:59 claudio Exp $	*/
/*	$NetBSD: radix.c,v 1.20 2003/08/07 16:32:56 agc Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1993
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
 *	@(#)radix.c	8.6 (Berkeley) 10/17/95
 */

/*
 * Routines to build and maintain radix trees for routing lookups.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/domain.h>
#include <sys/syslog.h>
#include <sys/pool.h>
#include <net/radix.h>

#ifndef SMALL_KERNEL
#include <sys/socket.h>
#include <net/route.h>
#include <net/radix_mpath.h>
#endif

int	max_keylen;
struct radix_node_head *mask_rnhead;
static char *addmask_key;
static char normal_chars[] = {0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, -1};
static char *rn_zeros, *rn_ones;

struct pool rtmask_pool;	/* pool for radix_mask structures */

#define rn_masktop (mask_rnhead->rnh_treetop)

static inline int rn_satisfies_leaf(char *, struct radix_node *, int);
static inline int rn_lexobetter(void *, void *);
static inline struct radix_mask *rn_new_radix_mask(struct radix_node *,
    struct radix_mask *);

struct radix_node *rn_insert(void *, struct radix_node_head *, int *,
    struct radix_node [2]);
struct radix_node *rn_newpair(void *, int, struct radix_node[2]);

static inline struct radix_node *rn_search(void *, struct radix_node *);
struct radix_node *rn_search_m(void *, struct radix_node *, void *);

/*
 * The data structure for the keys is a radix tree with one way
 * branching removed.  The index rn_b at an internal node n represents a bit
 * position to be tested.  The tree is arranged so that all descendants
 * of a node n have keys whose bits all agree up to position rn_b - 1.
 * (We say the index of n is rn_b.)
 *
 * There is at least one descendant which has a one bit at position rn_b,
 * and at least one with a zero there.
 *
 * A route is determined by a pair of key and mask.  We require that the
 * bit-wise logical and of the key and mask to be the key.
 * We define the index of a route to associated with the mask to be
 * the first bit number in the mask where 0 occurs (with bit number 0
 * representing the highest order bit).
 * 
 * We say a mask is normal if every bit is 0, past the index of the mask.
 * If a node n has a descendant (k, m) with index(m) == index(n) == rn_b,
 * and m is a normal mask, then the route applies to every descendant of n.
 * If the index(m) < rn_b, this implies the trailing last few bits of k
 * before bit b are all 0, (and hence consequently true of every descendant
 * of n), so the route applies to all descendants of the node as well.
 * 
 * Similar logic shows that a non-normal mask m such that
 * index(m) <= index(n) could potentially apply to many children of n.
 * Thus, for each non-host route, we attach its mask to a list at an internal
 * node as high in the tree as we can go. 
 *
 * The present version of the code makes use of normal routes in short-
 * circuiting an explicit mask and compare operation when testing whether
 * a key satisfies a normal route, and also in remembering the unique leaf
 * that governs a subtree.
 */

static inline struct radix_node *
rn_search(void *v_arg, struct radix_node *head)
{
	struct radix_node *x = head;
	caddr_t v = v_arg;

	while (x->rn_b >= 0) {
		if (x->rn_bmask & v[x->rn_off])
			x = x->rn_r;
		else
			x = x->rn_l;
	}
	return (x);
}

struct radix_node *
rn_search_m(void *v_arg, struct radix_node *head, void *m_arg)
{
	struct radix_node *x = head;
	caddr_t v = v_arg;
	caddr_t m = m_arg;

	while (x->rn_b >= 0) {
		if ((x->rn_bmask & m[x->rn_off]) &&
		    (x->rn_bmask & v[x->rn_off]))
			x = x->rn_r;
		else
			x = x->rn_l;
	}
	return x;
}

int
rn_refines(void *m_arg, void *n_arg)
{
	caddr_t m = m_arg;
	caddr_t n = n_arg;
	caddr_t lim, lim2;
	int longer;
	int masks_are_equal = 1;

	lim2 = lim = n + *(u_char *)n;
	longer = (*(u_char *)n++) - (int)(*(u_char *)m++);
	if (longer > 0)
		lim -= longer;
	while (n < lim) {
		if (*n & ~(*m))
			return 0;
		if (*n++ != *m++)
			masks_are_equal = 0;
	}
	while (n < lim2)
		if (*n++)
			return 0;
	if (masks_are_equal && (longer < 0))
		for (lim2 = m - longer; m < lim2; )
			if (*m++)
				return 1;
	return (!masks_are_equal);
}

struct radix_node *
rn_lookup(void *v_arg, void *m_arg, struct radix_node_head *head)
{
	struct radix_node *x, *tm;
	caddr_t netmask = 0;

	if (m_arg) {
		tm = rn_addmask(m_arg, 1, head->rnh_treetop->rn_off);
		if (tm == NULL)
			return (NULL);
		netmask = tm->rn_key;
	}
	x = rn_match(v_arg, head);
	if (x && netmask) {
		while (x && x->rn_mask != netmask)
			x = x->rn_dupedkey;
	}
	return x;
}

static inline int
rn_satisfies_leaf(char *trial, struct radix_node *leaf, int skip)
{
	char *cp = trial;
	char *cp2 = leaf->rn_key;
	char *cp3 = leaf->rn_mask;
	char *cplim;
	int length;

	length = min(*(u_char *)cp, *(u_char *)cp2);
	if (cp3 == NULL)
		cp3 = rn_ones;
	else
		length = min(length, *(u_char *)cp3);
	cplim = cp + length; cp3 += skip; cp2 += skip;
	for (cp += skip; cp < cplim; cp++, cp2++, cp3++)
		if ((*cp ^ *cp2) & *cp3)
			return 0;
	return 1;
}

struct radix_node *
rn_match(void *v_arg, struct radix_node_head *head)
{
	caddr_t v = v_arg;
	caddr_t cp, cp2, cplim;
	struct radix_node *top = head->rnh_treetop;
	struct radix_node *saved_t, *t;
	int off = top->rn_off;
	int vlen, matched_off;
	int test, b, rn_b;

	t = rn_search(v, top);
	/*
	 * See if we match exactly as a host destination
	 * or at least learn how many bits match, for normal mask finesse.
	 *
	 * It doesn't hurt us to limit how many bytes to check
	 * to the length of the mask, since if it matches we had a genuine
	 * match and the leaf we have is the most specific one anyway;
	 * if it didn't match with a shorter length it would fail
	 * with a long one.  This wins big for class B&C netmasks which
	 * are probably the most common case...
	 */
	if (t->rn_mask)
		vlen = *(u_char *)t->rn_mask;
	else
		vlen = *(u_char *)v;
	cp = v + off;
	cp2 = t->rn_key + off;
	cplim = v + vlen;
	for (; cp < cplim; cp++, cp2++)
		if (*cp != *cp2)
			goto on1;
	/*
	 * This extra grot is in case we are explicitly asked
	 * to look up the default.  Ugh!
	 */
	if ((t->rn_flags & RNF_ROOT) && t->rn_dupedkey)
		t = t->rn_dupedkey;
	return t;
on1:
	test = (*cp ^ *cp2) & 0xff; /* find first bit that differs */
	for (b = 7; (test >>= 1) > 0;)
		b--;
	matched_off = cp - v;
	b += matched_off << 3;
	rn_b = -1 - b;
	/*
	 * If there is a host route in a duped-key chain, it will be first.
	 */
	saved_t = t;
	if (t->rn_mask == NULL)
		t = t->rn_dupedkey;
	for (; t; t = t->rn_dupedkey)
		/*
		 * Even if we don't match exactly as a host,
		 * we may match if the leaf we wound up at is
		 * a route to a net.
		 */
		if (t->rn_flags & RNF_NORMAL) {
			if (rn_b <= t->rn_b)
				return t;
		} else if (rn_satisfies_leaf(v, t, matched_off))
				return t;
	t = saved_t;
	/* start searching up the tree */
	do {
		struct radix_mask *m;
		t = t->rn_p;
		m = t->rn_mklist;
		while (m) {
			/*
			 * If non-contiguous masks ever become important
			 * we can restore the masking and open coding of
			 * the search and satisfaction test and put the
			 * calculation of "off" back before the "do".
			 */
			if (m->rm_flags & RNF_NORMAL) {
				if (rn_b <= m->rm_b)
					return (m->rm_leaf);
			} else {
				struct radix_node *x;
				off = min(t->rn_off, matched_off);
				x = rn_search_m(v, t, m->rm_mask);
				while (x && x->rn_mask != m->rm_mask)
					x = x->rn_dupedkey;
				if (x && rn_satisfies_leaf(v, x, off))
					return x;
			}
			m = m->rm_mklist;
		}
	} while (t != top);
	return NULL;
}

struct radix_node *
rn_newpair(void *v, int b, struct radix_node nodes[2])
{
	struct radix_node *tt = nodes, *t = nodes + 1;
	t->rn_b = b;
	t->rn_bmask = 0x80 >> (b & 7);
	t->rn_l = tt;
	t->rn_off = b >> 3;
	tt->rn_b = -1;
	tt->rn_key = v;
	tt->rn_p = t;
	tt->rn_flags = t->rn_flags = RNF_ACTIVE;
	return t;
}

struct radix_node *
rn_insert(void *v_arg, struct radix_node_head *head,
    int *dupentry, struct radix_node nodes[2])
{
	caddr_t v = v_arg;
	struct radix_node *top = head->rnh_treetop;
	struct radix_node *t, *tt;
	int off = top->rn_off;
	int b;

	t = rn_search(v_arg, top);
	/*
	 * Find first bit at which v and t->rn_key differ
	 */
    {
	caddr_t cp, cp2, cplim;
	int vlen, cmp_res;

	vlen =  *(u_char *)v;
	cp = v + off;
	cp2 = t->rn_key + off;
	cplim = v + vlen;

	while (cp < cplim)
		if (*cp2++ != *cp++)
			goto on1;
	*dupentry = 1;
	return t;
on1:
	*dupentry = 0;
	cmp_res = (cp[-1] ^ cp2[-1]) & 0xff;
	for (b = (cp - v) << 3; cmp_res; b--)
		cmp_res >>= 1;
    }
    {
	struct radix_node *p, *x = top;
	caddr_t cp = v;
	do {
		p = x;
		if (cp[x->rn_off] & x->rn_bmask)
			x = x->rn_r;
		else
			x = x->rn_l;
	} while (b > (unsigned int) x->rn_b); /* x->rn_b < b && x->rn_b >= 0 */
	t = rn_newpair(v_arg, b, nodes);
	tt = t->rn_l;
	if ((cp[p->rn_off] & p->rn_bmask) == 0)
		p->rn_l = t;
	else
		p->rn_r = t;
	x->rn_p = t;
	t->rn_p = p; /* frees x, p as temp vars below */
	if ((cp[t->rn_off] & t->rn_bmask) == 0) {
		t->rn_r = x;
	} else {
		t->rn_r = tt;
		t->rn_l = x;
	}
    }
	return (tt);
}

struct radix_node *
rn_addmask(void *n_arg, int search, int skip)
{
	caddr_t netmask = n_arg;
	struct radix_node *tm, *saved_tm;
	caddr_t cp, cplim;
	int b = 0, mlen, j;
	int maskduplicated, m0, isnormal;
	static int last_zeroed = 0;

	if ((mlen = *(u_char *)netmask) > max_keylen)
		mlen = max_keylen;
	if (skip == 0)
		skip = 1;
	if (mlen <= skip)
		return (mask_rnhead->rnh_nodes);
	if (skip > 1)
		memcpy(addmask_key + 1, rn_ones + 1, skip - 1);
	if ((m0 = mlen) > skip)
		memcpy(addmask_key + skip, netmask + skip, mlen - skip);
	/*
	 * Trim trailing zeroes.
	 */
	for (cp = addmask_key + mlen; (cp > addmask_key) && cp[-1] == 0;)
		cp--;
	mlen = cp - addmask_key;
	if (mlen <= skip) {
		if (m0 >= last_zeroed)
			last_zeroed = mlen;
		return (mask_rnhead->rnh_nodes);
	}
	if (m0 < last_zeroed)
		memset(addmask_key + m0, 0, last_zeroed - m0);
	*addmask_key = last_zeroed = mlen;
	tm = rn_search(addmask_key, rn_masktop);
	if (memcmp(addmask_key, tm->rn_key, mlen) != 0)
		tm = NULL;
	if (tm || search)
		return (tm);
	tm = malloc(max_keylen + 2 * sizeof (*tm), M_RTABLE, M_NOWAIT | M_ZERO);
	if (tm == NULL)
		return (0);
	saved_tm = tm;
	netmask = cp = (caddr_t)(tm + 2);
	memcpy(cp, addmask_key, mlen);
	tm = rn_insert(cp, mask_rnhead, &maskduplicated, tm);
	if (maskduplicated) {
		log(LOG_ERR, "rn_addmask: mask impossibly already in tree\n");
		free(saved_tm, M_RTABLE);
		return (tm);
	}
	/*
	 * Calculate index of mask, and check for normalcy.
	 */
	cplim = netmask + mlen;
	isnormal = 1;
	for (cp = netmask + skip; (cp < cplim) && *(u_char *)cp == 0xff;)
		cp++;
	if (cp != cplim) {
		for (j = 0x80; (j & *cp) != 0; j >>= 1)
			b++;
		if (*cp != normal_chars[b] || cp != (cplim - 1))
			isnormal = 0;
	}
	b += (cp - netmask) << 3;
	tm->rn_b = -1 - b;
	if (isnormal)
		tm->rn_flags |= RNF_NORMAL;
	return (tm);
}

/* rn_lexobetter: return a arbitrary ordering for non-contiguous masks */
static inline int
rn_lexobetter(void *m_arg, void *n_arg)
{
	u_char *mp = m_arg, *np = n_arg;

	/*
	 * Longer masks might not really be lexicographically better,
	 * but longer masks always have precedence since they must be checked
	 * first. The netmasks were normalized before calling this function and
	 * don't have unneeded trailing zeros.
	 */
	if (*mp > *np)
		return 1;
	if (*mp < *np)
		return 0;
	/*
	 * Must return the first difference between the masks
	 * to ensure deterministic sorting.
	 */
	return (memcmp(mp, np, *mp) > 0);
}

static inline struct radix_mask *
rn_new_radix_mask(struct radix_node *tt, struct radix_mask *next)
{
	struct radix_mask *m;

	m = pool_get(&rtmask_pool, PR_NOWAIT | PR_ZERO);
	if (m == NULL) {
		log(LOG_ERR, "Mask for route not entered\n");
		return (0);
	}
	m->rm_b = tt->rn_b;
	m->rm_flags = tt->rn_flags;
	if (tt->rn_flags & RNF_NORMAL)
		m->rm_leaf = tt;
	else
		m->rm_mask = tt->rn_mask;
	m->rm_mklist = next;
	tt->rn_mklist = m;
	return m;
}

struct radix_node *
rn_addroute(void *v_arg, void *n_arg, struct radix_node_head *head,
    struct radix_node treenodes[2], u_int8_t prio)
{
	caddr_t v = v_arg;
	caddr_t netmask = n_arg;
	struct radix_node *top = head->rnh_treetop;
	struct radix_node *t, *tt, *tm = NULL, *x;
	struct radix_node *saved_tt;
	short b = 0, b_leaf = 0;
	int keyduplicated, prioinv = -1;
	caddr_t mmask;
	struct radix_mask *m, **mp;

	/*
	 * In dealing with non-contiguous masks, there may be
	 * many different routes which have the same mask.
	 * We will find it useful to have a unique pointer to
	 * the mask to speed avoiding duplicate references at
	 * nodes and possibly save time in calculating indices.
	 */
	if (netmask)  {
		if ((tm = rn_addmask(netmask, 0, top->rn_off)) == 0)
			return (0);
		b_leaf = tm->rn_b;
		b = -1 - tm->rn_b;
		netmask = tm->rn_key;
	}

	saved_tt = tt = rn_insert(v, head, &keyduplicated, treenodes);

	/*
	 * Deal with duplicated keys: attach node to previous instance
	 */
	if (keyduplicated) {
		for (t = tt; tt; t = tt, tt = tt->rn_dupedkey) {
#ifndef SMALL_KERNEL
			/* permit multipath, if enabled for the family */
			if (rn_mpath_capable(head) && netmask == tt->rn_mask) {
				int mid;
				/*
				 * Try to insert the new node in the middle
				 * of the list of any preexisting multipaths,
				 * to reduce the number of path disruptions
				 * that occur as a result of an insertion,
				 * per RFC2992.
				 * Additionally keep the list sorted by route
				 * priority.
				 */
				prioinv = 0;
				tt = rn_mpath_prio(tt, prio);
				if (((struct rtentry *)tt)->rt_priority !=
				    prio) {
					/*
					 * rn_mpath_prio returns the previous
					 * element if no element with the 
					 * requested priority exists. It could
					 * be that the previous element comes
					 * with a bigger priority.
					 */
					if (((struct rtentry *)tt)->
					    rt_priority > prio)
						prioinv = 1;
					t = tt;
					break;
				}

				mid = rn_mpath_active_count(tt) / 2;
				do {
					t = tt;
					tt = rn_mpath_next(tt, 0);
				} while (tt && --mid > 0);
				break;
			}
#endif
			if (tt->rn_mask == netmask)
				return (0);
			if (netmask == 0 ||
			    (tt->rn_mask &&
			     ((b_leaf < tt->rn_b) || /* index(netmask) > node */
			       rn_refines(netmask, tt->rn_mask) ||
			       rn_lexobetter(netmask, tt->rn_mask))))
				break;
		}
		/*
		 * If the mask is not duplicated, we wouldn't
		 * find it among possible duplicate key entries
		 * anyway, so the above test doesn't hurt.
		 *
		 * We sort the masks for a duplicated key the same way as
		 * in a masklist -- most specific to least specific.
		 * This may require the unfortunate nuisance of relocating
		 * the head of the list.
		 *
		 * We also reverse, or doubly link the list through the
		 * parent pointer.
		 */
		if (tt == saved_tt && prioinv) {
			struct	radix_node *xx;
			/* link in at head of list */
			(tt = treenodes)->rn_dupedkey = t;
			tt->rn_flags = t->rn_flags;
			tt->rn_p = xx = t->rn_p;
			t->rn_p = tt;
			if (xx->rn_l == t)
				xx->rn_l = tt;
			else
				xx->rn_r = tt;
			saved_tt = tt;
		} else if (prioinv == 1) {
			(tt = treenodes)->rn_dupedkey = t;
			if (t->rn_p == NULL)
				panic("rn_addroute: t->rn_p is NULL");
			t->rn_p->rn_dupedkey = tt;
			tt->rn_p = t->rn_p;
			t->rn_p = tt;
		} else {
			(tt = treenodes)->rn_dupedkey = t->rn_dupedkey;
			t->rn_dupedkey = tt;
			tt->rn_p = t;
			if (tt->rn_dupedkey)
				tt->rn_dupedkey->rn_p = tt;
		}
		tt->rn_key = (caddr_t) v;
		tt->rn_b = -1;
		tt->rn_flags = RNF_ACTIVE;
	}

	/*
	 * Put mask in tree.
	 */
	if (netmask) {
		tt->rn_mask = netmask;
		tt->rn_b = tm->rn_b;
		tt->rn_flags |= tm->rn_flags & RNF_NORMAL;
	}
	t = saved_tt->rn_p;
	if (keyduplicated)
		goto on2;
	b_leaf = -1 - t->rn_b;
	if (t->rn_r == saved_tt)
		x = t->rn_l;
	else
		x = t->rn_r;
	/* Promote general routes from below */
	if (x->rn_b < 0) {
	    struct	radix_node *xx = NULL;
	    for (mp = &t->rn_mklist; x; xx = x, x = x->rn_dupedkey) {
		if (xx && xx->rn_mklist && xx->rn_mask == x->rn_mask &&
		    x->rn_mklist == 0) {
			/* multipath route, bump refcount on first mklist */
			x->rn_mklist = xx->rn_mklist;
			x->rn_mklist->rm_refs++;
		}
		if (x->rn_mask && (x->rn_b >= b_leaf) && x->rn_mklist == 0) {
			*mp = m = rn_new_radix_mask(x, 0);
			if (m)
				mp = &m->rm_mklist;
		}
	    }
	} else if (x->rn_mklist) {
		/*
		 * Skip over masks whose index is > that of new node
		 */
		for (mp = &x->rn_mklist; (m = *mp); mp = &m->rm_mklist)
			if (m->rm_b >= b_leaf)
				break;
		t->rn_mklist = m;
		*mp = 0;
	}
on2:
	/* Add new route to highest possible ancestor's list */
	if ((netmask == 0) || (b > t->rn_b ))
		return tt; /* can't lift at all */
	b_leaf = tt->rn_b;
	do {
		x = t;
		t = t->rn_p;
	} while (b <= t->rn_b && x != top);
	/*
	 * Search through routes associated with node to
	 * insert new route according to index.
	 * Need same criteria as when sorting dupedkeys to avoid
	 * double loop on deletion.
	 */
	for (mp = &x->rn_mklist; (m = *mp); mp = &m->rm_mklist) {
		if (m->rm_b < b_leaf)
			continue;
		if (m->rm_b > b_leaf)
			break;
		if (m->rm_flags & RNF_NORMAL) {
			mmask = m->rm_leaf->rn_mask;
			if (keyduplicated) {
				if (m->rm_leaf->rn_p == tt)
					/* new route is better */
					m->rm_leaf = tt;
#ifdef DIAGNOSTIC
				else {
					for (t = m->rm_leaf; t;
						t = t->rn_dupedkey)
						if (t == tt)
							break;
					if (t == NULL) {
						log(LOG_ERR, "Non-unique "
						    "normal route on dupedkey, "
						    "mask not entered\n");
						return tt;
					}
				}
#endif
				m->rm_refs++;
				tt->rn_mklist = m;
				return tt;
			} else if (tt->rn_flags & RNF_NORMAL) {
				log(LOG_ERR, "Non-unique normal route,"
				    " mask not entered\n");
				return tt;
			}
		} else
			mmask = m->rm_mask;
		if (mmask == netmask) {
			m->rm_refs++;
			tt->rn_mklist = m;
			return tt;
		}
		if (rn_refines(netmask, mmask) || rn_lexobetter(netmask, mmask))
			break;
	}
	*mp = rn_new_radix_mask(tt, *mp);
	return tt;
}

struct radix_node *
rn_delete(void *v_arg, void *netmask_arg, struct radix_node_head *head,
    struct radix_node *rn)
{
	caddr_t v = v_arg;
	caddr_t netmask = netmask_arg;
	struct radix_node *top = head->rnh_treetop;
	struct radix_node *t, *p, *tm, *x, *tt;
	struct radix_mask *m, *saved_m, **mp;
	struct radix_node *dupedkey, *saved_tt;
	int off = top->rn_off;
	int b, vlen;

	vlen =  *(u_char *)v;
	tt = rn_search(v, top);
	saved_tt = tt;
	if (memcmp(v + off, tt->rn_key + off, vlen - off))
		return (0);
	/*
	 * Delete our route from mask lists.
	 */
	if (netmask) {
		if ((tm = rn_addmask(netmask, 1, off)) == NULL)
			return (0);
		netmask = tm->rn_key;
		while (tt->rn_mask != netmask)
			if ((tt = tt->rn_dupedkey) == NULL)
				return (0);
	}
#ifndef SMALL_KERNEL
	if (rn) {
		while (tt != rn)
			if ((tt = tt->rn_dupedkey) == NULL)
				return (0);
	}
#endif
	if (tt->rn_mask == NULL || (saved_m = m = tt->rn_mklist) == NULL)
		goto on1;
	if (tt->rn_flags & RNF_NORMAL) {
		if (m->rm_leaf != tt && m->rm_refs == 0) {
			log(LOG_ERR, "rn_delete: inconsistent normal "
			    "annotation\n");
			return (0);
		}
		if (m->rm_leaf != tt) {
			if (--m->rm_refs >= 0)
				goto on1;
		}
		/* tt is currently the head of the possible multipath chain */
		if (m->rm_refs > 0) {
			if (tt->rn_dupedkey == NULL ||
			    tt->rn_dupedkey->rn_mklist != m) {
				log(LOG_ERR, "rn_delete: inconsistent "
				    "dupedkey list\n");
				return (0);
			}
			m->rm_leaf = tt->rn_dupedkey;
			--m->rm_refs;
			goto on1;
		}
		/* else tt is last and only route */
	} else {
		if (m->rm_mask != tt->rn_mask) {
			log(LOG_ERR, "rn_delete: inconsistent annotation\n");
			goto on1;
		}
		if (--m->rm_refs >= 0)
			goto on1;
	}
	b = -1 - tt->rn_b;
	t = saved_tt->rn_p;
	if (b > t->rn_b)
		goto on1; /* Wasn't lifted at all */
	do {
		x = t;
		t = t->rn_p;
	} while (b <= t->rn_b && x != top);
	for (mp = &x->rn_mklist; (m = *mp); mp = &m->rm_mklist)
		if (m == saved_m) {
			*mp = m->rm_mklist;
			pool_put(&rtmask_pool, m);
			break;
		}
	if (m == NULL) {
		log(LOG_ERR, "rn_delete: couldn't find our annotation\n");
		if (tt->rn_flags & RNF_NORMAL)
			return (0); /* Dangling ref to us */
	}
on1:
	/*
	 * Eliminate us from tree
	 */
	if (tt->rn_flags & RNF_ROOT)
		return (0);
	t = tt->rn_p;
	dupedkey = saved_tt->rn_dupedkey;
	if (dupedkey) {
		/*
		 * Here, tt is the deletion target, and
		 * saved_tt is the head of the dupedkey chain.
		 */
		if (tt == saved_tt) {
			x = dupedkey;
			x->rn_p = t;
			if (t->rn_l == tt)
				t->rn_l = x;
			else
				t->rn_r = x;
		} else {
			x = saved_tt;
			t->rn_dupedkey = tt->rn_dupedkey;
			if (tt->rn_dupedkey)
				tt->rn_dupedkey->rn_p = t;
		}
		t = tt + 1;
		if  (t->rn_flags & RNF_ACTIVE) {
			*++x = *t;
			p = t->rn_p;
			if (p->rn_l == t)
				p->rn_l = x;
			else
				p->rn_r = x;
			x->rn_l->rn_p = x;
			x->rn_r->rn_p = x;
		}
		goto out;
	}
	if (t->rn_l == tt)
		x = t->rn_r;
	else
		x = t->rn_l;
	p = t->rn_p;
	if (p->rn_r == t)
		p->rn_r = x;
	else
		p->rn_l = x;
	x->rn_p = p;
	/*
	 * Demote routes attached to us.
	 */
	if (t->rn_mklist) {
		if (x->rn_b >= 0) {
			for (mp = &x->rn_mklist; (m = *mp);)
				mp = &m->rm_mklist;
			*mp = t->rn_mklist;
		} else {
			/* If there are any key,mask pairs in a sibling
			   duped-key chain, some subset will appear sorted
			   in the same order attached to our mklist */
			for (m = t->rn_mklist; m && x; x = x->rn_dupedkey)
				if (m == x->rn_mklist) {
					struct radix_mask *mm = m->rm_mklist;
					x->rn_mklist = 0;
					if (--(m->rm_refs) < 0)
						pool_put(&rtmask_pool, m);
					else if (m->rm_flags & RNF_NORMAL)
						/*
						 * don't progress because this
						 * a multipath route. Next
						 * route will use the same m.
						 */
						mm = m;
					m = mm;
				}
			if (m)
				log(LOG_ERR, "%s %p at %p\n",
				    "rn_delete: Orphaned Mask", m, x);
		}
	}
	/*
	 * We may be holding an active internal node in the tree.
	 */
	x = tt + 1;
	if (t != x) {
		*t = *x;
		t->rn_l->rn_p = t;
		t->rn_r->rn_p = t;
		p = x->rn_p;
		if (p->rn_l == x)
			p->rn_l = t;
		else
			p->rn_r = t;
	}
out:
	tt->rn_flags &= ~RNF_ACTIVE;
	tt[1].rn_flags &= ~RNF_ACTIVE;
	return (tt);
}

int
rn_walktree(struct radix_node_head *h, int (*f)(struct radix_node *, void *,
    u_int), void *w)
{
	int error;
	struct radix_node *base, *next;
	struct radix_node *rn = h->rnh_treetop;
	/*
	 * This gets complicated because we may delete the node
	 * while applying the function f to it, so we need to calculate
	 * the successor node in advance.
	 */
	/* First time through node, go left */
	while (rn->rn_b >= 0)
		rn = rn->rn_l;
	for (;;) {
		base = rn;
		/* If at right child go back up, otherwise, go right */
		while (rn->rn_p->rn_r == rn && (rn->rn_flags & RNF_ROOT) == 0)
			rn = rn->rn_p;
		/* Find the next *leaf* since next node might vanish, too */
		for (rn = rn->rn_p->rn_r; rn->rn_b >= 0;)
			rn = rn->rn_l;
		next = rn;
		/* Process leaves */
		while ((rn = base) != NULL) {
			base = rn->rn_dupedkey;
			if (!(rn->rn_flags & RNF_ROOT) &&
			    (error = (*f)(rn, w, h->rnh_rtableid)))
				return (error);
		}
		rn = next;
		if (rn->rn_flags & RNF_ROOT)
			return (0);
	}
	/* NOTREACHED */
}

int
rn_inithead(void **head, int off)
{
	struct radix_node_head *rnh;

	if (*head)
		return (1);
	rnh = malloc(sizeof(*rnh), M_RTABLE, M_NOWAIT);
	if (rnh == NULL)
		return (0);
	*head = rnh;
	return rn_inithead0(rnh, off);
}

int
rn_inithead0(struct radix_node_head *rnh, int off)
{
	struct radix_node *t, *tt, *ttt;

	memset(rnh, 0, sizeof(*rnh));
	t = rn_newpair(rn_zeros, off, rnh->rnh_nodes);
	ttt = rnh->rnh_nodes + 2;
	t->rn_r = ttt;
	t->rn_p = t;
	tt = t->rn_l;
	tt->rn_flags = t->rn_flags = RNF_ROOT | RNF_ACTIVE;
	tt->rn_b = -1 - off;
	*ttt = *tt;
	ttt->rn_key = rn_ones;
	rnh->rnh_addaddr = rn_addroute;
	rnh->rnh_deladdr = rn_delete;
	rnh->rnh_matchaddr = rn_match;
	rnh->rnh_lookup = rn_lookup;
	rnh->rnh_walktree = rn_walktree;
	rnh->rnh_treetop = t;
	return (1);
}

void
rn_init(void)
{
	char *cp, *cplim;
	struct domain *dom;

	pool_init(&rtmask_pool, sizeof(struct radix_mask), 0, 0, 0, "rtmask",
	    NULL);
	for (dom = domains; dom; dom = dom->dom_next)
		if (dom->dom_maxrtkey > max_keylen)
			max_keylen = dom->dom_maxrtkey;
	if (max_keylen == 0) {
		log(LOG_ERR,
		    "rn_init: radix functions require max_keylen be set\n");
		return;
	}
	rn_zeros = malloc(3 * max_keylen, M_RTABLE, M_NOWAIT | M_ZERO);
	if (rn_zeros == NULL)
		panic("rn_init");
	rn_ones = cp = rn_zeros + max_keylen;
	addmask_key = cplim = rn_ones + max_keylen;
	while (cp < cplim)
		*cp++ = -1;
	if (rn_inithead((void *)&mask_rnhead, 0) == 0)
		panic("rn_init 2");
}
