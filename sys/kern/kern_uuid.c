/*	$OpenBSD: kern_uuid.c,v 1.2 2014/08/31 20:15:54 miod Exp $	*/
/*	$NetBSD: kern_uuid.c,v 1.18 2011/11/19 22:51:25 tls Exp $	*/

/*-
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: /repoman/r/ncvs/src/sys/kern/kern_uuid.c,v 1.7 2004/01/12 
13:34:11 rse Exp $
 */
/*-
 * Copyright (c) 2002 Thomas Moestl <tmm@FreeBSD.org>
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
 *
 * $FreeBSD: src/sys/sys/endian.h,v 1.7 2010/05/20 06:16:13 phk Exp $
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/uuid.h>
#include <sys/time.h>
#include <sys/queue.h>

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/disk.h>
#include <sys/reboot.h>
#include <sys/dkio.h>
#include <sys/vnode.h>
#include <sys/workq.h>

#include <sys/socket.h>
#include <sys/socketvar.h>
#include <machine/mplock.h>

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <net/if.h>
#include <net/if_types.h>

#include <dev/rndvar.h>
#include <dev/cons.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/if_ether.h>

//static u_char IBAA_Byte(void);
//u_int read_random(void *buf, u_int nbytes);

int if_getanyethermac(uint16_t *, int);


#define IF_LLSOCKADDR(ifp) \
((struct sockaddr_dl *)(ifp)->if_lladdr->ifa_addr)
#define IF_LLADDR(ifp) LLADDR(IF_LLSOCKADDR(ifp))

/*
#define PAD_(t) (sizeof(register_t) <= sizeof(t) ? \
	0 : sizeof(register_t) - sizeof(t))


struct	uuidgen_args {
#ifdef _KERNEL
	//struct sysmsg sysmsg;
#endif
	struct uuid *store;	
	char store_[PAD_(struct uuid *)];
	int	count;	
	char count_[PAD_(int)];
};
*/

struct uuid *kern_uuidgen(struct uuid *store, size_t count);
int sys_uuidgen(struct proc *, void *, register_t *);


/*
 * For a description of UUIDs see RFC 4122.
 */

struct uuid_private {
	union {
		uint64_t	ll;	/* internal. */
		struct {
			uint32_t low;
			uint16_t mid;
			uint16_t hi;
		} x;
	} time;
	uint16_t seq;
	uint16_t node[_UUID_NODE_LEN>>1];
};

struct uuid_private uuid_last;

/*
 * Extract a byte from IBAAs 256 32-bit u4 results array. 
 *
 * NOTE: This code is designed to prevent MP races from taking
 * IBAA_byte_index out of bounds.
 */
/*
static u_char
IBAA_Byte(void)
{
	u_char result;
	int index;

	index = IBAA_byte_index;
	if (index == sizeof(IBAA_results)) {
		IBAA_Call();
		index = 0;
	}
	result = ((u_char *)IBAA_results)[index];
	IBAA_byte_index = index + 1;
	return result;
}
*/

//#ifdef DEBUG
int
uuid_snprintf(char *buf, size_t sz, const struct uuid *uuid)
{
	const struct uuid_private *id;
	int cnt;

	id = (const struct uuid_private *)uuid;
	cnt = snprintf(buf, sz, "%08x-%04x-%04x-%04x-%04x%04x%04x",
	    id->time.x.low, id->time.x.mid, id->time.x.hi, betoh16(id->seq),
	    betoh16(id->node[0]), betoh16(id->node[1]), betoh16(id->node[2]));
	return (cnt);
}

int
uuid_printf(const struct uuid *uuid)
{
	char buf[_UUID_BUF_LEN];

	(void) uuid_snprintf(buf, sizeof(buf), uuid);
	printf("%s", buf);
	return (0);
}
//#endif

/*
 * Encode/Decode UUID into octet-stream.
 *   http://www.opengroup.org/dce/info/draft-leach-uuids-guids-01.txt
 *
 * 0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                          time_low                             |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |       time_mid                |         time_hi_and_version   |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |clk_seq_hi_res |  clk_seq_low  |         node (0-1)            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                         node (2-5)                            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

/* Alignment-agnostic encode/decode bytestream to/from little/big endian. */

static __inline uint16_t
be16dec(const void *pp)
{
	uint8_t const *p = (uint8_t const *)pp;

	return ((p[0] << 8) | p[1]);
}

static __inline uint32_t
be32dec(const void *pp)
{
	uint8_t const *p = (uint8_t const *)pp;

	return (((unsigned)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

static __inline uint16_t
le16dec(const void *pp)
{
	uint8_t const *p = (uint8_t const *)pp;

	return ((p[1] << 8) | p[0]);
}

static __inline uint32_t
le32dec(const void *pp)
{
	uint8_t const *p = (uint8_t const *)pp;

	return (((unsigned)p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0]);
}

static __inline void
be16enc(void *pp, uint16_t u)
{
	uint8_t *p = (uint8_t *)pp;

	p[0] = (u >> 8) & 0xff;
	p[1] = u & 0xff;
}

static __inline void
be32enc(void *pp, uint32_t u)
{
	uint8_t *p = (uint8_t *)pp;

	p[0] = (u >> 24) & 0xff;
	p[1] = (u >> 16) & 0xff;
	p[2] = (u >> 8) & 0xff;
	p[3] = u & 0xff;
}

static __inline void
le16enc(void *pp, uint16_t u)
{
	uint8_t *p = (uint8_t *)pp;

	p[0] = u & 0xff;
	p[1] = (u >> 8) & 0xff;
}

static __inline void
le32enc(void *pp, uint32_t u)
{
	uint8_t *p = (uint8_t *)pp;

	p[0] = u & 0xff;
	p[1] = (u >> 8) & 0xff;
	p[2] = (u >> 16) & 0xff;
	p[3] = (u >> 24) & 0xff;
}

void
uuid_enc_le(void *buf, const struct uuid *uuid)
{
	uint8_t *p = buf;
	int i;

	le32enc(p, uuid->time_low);
	le16enc(p + 4, uuid->time_mid);
	le16enc(p + 6, uuid->time_hi_and_version);
	p[8] = uuid->clock_seq_hi_and_reserved;
	p[9] = uuid->clock_seq_low;
	for (i = 0; i < _UUID_NODE_LEN; i++)
		p[10 + i] = uuid->node[i];
}

void
uuid_dec_le(const void *buf, struct uuid *uuid)
{
	const uint8_t *p = buf;
	int i;

	uuid->time_low = le32dec(p);
	uuid->time_mid = le16dec(p + 4);
	uuid->time_hi_and_version = le16dec(p + 6);
	uuid->clock_seq_hi_and_reserved = p[8];
	uuid->clock_seq_low = p[9];
	for (i = 0; i < _UUID_NODE_LEN; i++)
		uuid->node[i] = p[10 + i];
}

void
uuid_enc_be(void *buf, const struct uuid *uuid)
{
	uint8_t *p = buf;
	int i;

	be32enc(p, uuid->time_low);
	be16enc(p + 4, uuid->time_mid);
	be16enc(p + 6, uuid->time_hi_and_version);
	p[8] = uuid->clock_seq_hi_and_reserved;
	p[9] = uuid->clock_seq_low;
	for (i = 0; i < _UUID_NODE_LEN; i++)
		p[10 + i] = uuid->node[i];
}

void
uuid_dec_be(const void *buf, struct uuid *uuid)
{
	const uint8_t *p = buf;
	int i;

	uuid->time_low = be32dec(p);
	uuid->time_mid = be16dec(p + 4);
	uuid->time_hi_and_version = be16dec(p + 6);
	uuid->clock_seq_hi_and_reserved = p[8];
	uuid->clock_seq_low = p[9];
	for (i = 0; i < _UUID_NODE_LEN; i++)
		uuid->node[i] = p[10 + i];
}

static struct lock uuid_lock;

/*
 * This function locates the first real ethernet MAC from a network
 * card and loads it into node, returning 0 on success or ENOENT if
 * no suitable interfaces were found.  It is used by the uuid code to
 * generate a unique 6-byte number.
 */

int
if_getanyethermac(uint16_t *node, int minlen)
{
	struct ifnet *ifp;
	struct sockaddr_dl *sdl;

	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_type != IFT_ETHER)
			continue;
		sdl = (struct sockaddr_dl *)IF_LLSOCKADDR(ifp);
		// if (sdl->sdl_alen < minlen)
		// XX if (SCARG(sdl, sdl_alen) < (size_t)minlen)
		//	continue;
	      //bcopy(((struct arpcom *)ifp->if_softc)->ac_enaddr, node,
		bcopy(((struct arpcom *)ifp->if_softc)->ac_enaddr, node,
		      minlen);
		return(0);
	}
	return (ENOENT);
}

/*
 * Ask the network subsystem for a real MAC address from any of the
 * system interfaces.  If we can't find one, generate a random multicast
 * MAC address.
 */
static void
uuid_node(uint16_t *node)
{
	if (if_getanyethermac(node, UUID_NODE_LEN) != 0)
		arc4random_buf(node, UUID_NODE_LEN);
	*((uint8_t*)node) |= 0x01;
}

/*
 * Get the current time as a 60 bit count of 100-nanosecond intervals
 * since 00:00:00.00, October 15,1582. We apply a magic offset to convert
 * the Unix time since 00:00:00.00, January 1, 1970 to the date of the
 * Gregorian reform to the Christian calendar.
 */
static uint64_t
uuid_time(void)
{
	struct timespec ts;
	uint64_t time = 0x01B21DD213814000LL;

	nanotime(&ts);
	time += ts.tv_sec * 10000000LL;		/* 100 ns increments */
	time += ts.tv_nsec / 100;		/* 100 ns increments */
	return (time & ((1LL << 60) - 1LL));	/* limit to 60 bits */
}

struct uuid *
kern_uuidgen(struct uuid *store, size_t count)
{
	struct uuid_private uuid;
	uint64_t time;
	size_t n;

	lockmgr(&uuid_lock, LK_EXCLUSIVE | LK_RETRY, 0);

	uuid_node(uuid.node);
	time = uuid_time();

	if (uuid_last.time.ll == 0LL || uuid_last.node[0] != uuid.node[0] ||
	    uuid_last.node[1] != uuid.node[1] ||
	    uuid_last.node[2] != uuid.node[2]) {
		arc4random_buf(&uuid.seq, sizeof(uuid.seq));
		uuid.seq &= 0x3fff;
	} else if (uuid_last.time.ll >= time) {
		uuid.seq = (uuid_last.seq + 1) & 0x3fff;
	} else {
		uuid.seq = uuid_last.seq;
	}

	uuid_last = uuid;
	uuid_last.time.ll = (time + count - 1) & ((1LL << 60) - 1LL);

	lockmgr(&uuid_lock, LK_RELEASE, 0);

	/* Set sequence and variant and deal with byte order. */
	uuid.seq = htobe16(uuid.seq | 0x8000);

	for (n = 0; n < count; n++) {
		/* Set time and version (=1). */
		uuid.time.x.low = (uint32_t)time;
		uuid.time.x.mid = (uint16_t)(time >> 32);
		uuid.time.x.hi = ((uint16_t)(time >> 48) & 0xfff) | (1 << 12);
		store[n] = *(struct uuid *)&uuid;
		time++;
	}

	return (store);
}

/*
 * uuidgen(struct uuid *store, int count)
 *
 * Generate an array of new UUIDs
 */
int
sys_uuidgen(struct proc *p, void *v, register_t *retval)
{
        struct sys_uuidgen_args  /* {
                syscallarg(struct uuid *) store;
                syscallarg(int) count;
        } */  *uap = v;
	
	struct uuid *store;
	size_t count;
	int error;

	/*
	 * Limit the number of UUIDs that can be created at the same time
	 * to some arbitrary number. This isn't really necessary, but I
	 * like to have some sort of upper-bound that's less than 2G :-)
	 * XXX probably needs to be tunable.
	 */
	//if (uap->count < 1 || uap->count > 2048)
	if (SCARG(uap, count) > (size_t)1024)
		return (EINVAL);

	count = SCARG(uap, count); //uap->count;
	store = malloc(count * sizeof(struct uuid), M_TEMP, M_WAITOK);
	if (store == NULL)
		return (ENOSPC);
	kern_uuidgen(store, count);
	error = copyout(store, SCARG(uap, store)/* uap->store */, count * sizeof(struct uuid));
	free(store, M_TEMP, 0);
	return (error);
}
