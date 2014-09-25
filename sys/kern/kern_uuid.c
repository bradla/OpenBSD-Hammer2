/*	$NetBSD: kern_uuid.c,v 1.18 2011/11/19 22:51:25 tls Exp $	*/

/*
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
 * $FreeBSD: /repoman/r/ncvs/src/sys/kern/kern_uuid.c,v 1.7 2004/01/12 13:34:11 rse Exp $
 */

//#include <sys/systm.h>
//#include <sys/malloc.h>
//#include <sys/pool.h>
//#include <sys/queue.h>
//#include <sys/kthread.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/uuid.h>

/* NetBSD */
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/uio.h>
#include <lib/libkern/libkern.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <sys/kernel.h>
#include <sys/sched.h>

#include <dev/rndvar.h>

typedef enum kmutex_type_t {
	MUTEX_SPIN = 0,		/* To get a spin mutex at IPL_NONE */
	MUTEX_ADAPTIVE = 1,	/* For porting code written for Solaris */
	MUTEX_DEFAULT = 2,	/* The only native, endorsed type */
	MUTEX_DRIVER = 3,	/* For porting code written for Solaris */
	MUTEX_NODEBUG = 4	/* Disables LOCKDEBUG; use with care */
} kmutex_type_t;

typedef struct kmutex kmutex_t;

#define	KERNEL_LOCKED_P()		_kernel_locked_p()

#define	bswap16	swap16
#define	bswap32	swap32

/*
 * General byte order swapping functions.
 */
//#define	bswap16(x)	__bswap16(x)
//#define	bswap32(x)	__bswap32(x)
#define	bswap64(x)	__bswap64(x)

/*
 * Host to big endian, host to little endian, big endian to host, and little
 * endian to host byte order functions as detailed in byteorder(9).
 */
#if _BYTE_ORDER == _LITTLE_ENDIAN
//#define	htobe16(x)	bswap16((x))
//#define	htobe32(x)	bswap32((x))
//#define	htobe64(x)	bswap64((x))
//#define	htole16(x)	((uint16_t)(x))
//#define	htole32(x)	((uint32_t)(x))
//#define	htole64(x)	((uint64_t)(x))

#define	be16toh(x)	bswap16((x))
#define	be32toh(x)	bswap32((x))
#define	be64toh(x)	bswap64((x))
#define	le16toh(x)	((uint16_t)(x))
#define	le32toh(x)	((uint32_t)(x))
#define	le64toh(x)	((uint64_t)(x))
#else /* _BYTE_ORDER != _LITTLE_ENDIAN */
#define	htobe16(x)	((uint16_t)(x))
#define	htobe32(x)	((uint32_t)(x))
#define	htobe64(x)	((uint64_t)(x))
#define	htole16(x)	bswap16((x))
#define	htole32(x)	bswap32((x))
#define	htole64(x)	bswap64((x))

#define	be16toh(x)	((uint16_t)(x))
#define	be32toh(x)	((uint32_t)(x))
#define	be64toh(x)	((uint64_t)(x))
#define	le16toh(x)	bswap16((x))
#define	le32toh(x)	bswap32((x))
#define	le64toh(x)	bswap64((x))
#endif /* _BYTE_ORDER == _LITTLE_ENDIAN */

/*
 * See also:
 *	http://www.opengroup.org/dce/info/draft-leach-uuids-guids-01.txt
 *	http://www.opengroup.org/onlinepubs/009629399/apdxa.htm
 *
 * Note that the generator state is itself an UUID, but the time and clock
 * sequence fields are written in the native byte order.
 */

//CTASSERT(sizeof(struct uuid) == 16);

/* We use an alternative, more convenient representation in the generator. */
struct uuid_private {
	union {
		uint64_t	ll;		/* internal. */
		struct {
			uint32_t	low;
			uint16_t	mid;
			uint16_t	hi;
		} x;
	} time;
	uint16_t	seq;			/* Big-endian. */
	uint16_t	node[UUID_NODE_LEN>>1];
};

//CTASSERT(sizeof(struct uuid_private) == 16);

static struct uuid_private uuid_last;

/* "UUID generator mutex lock" */
//static kmutex_t uuid_mutex;
struct mutex uuid_mutex = MUTEX_INITIALIZER(IPL_NONE);


void
uuid_init(void)
{

	mtx_init(&uuid_mutex, MUTEX_DEFAULT); // IPL_NONE);
}

void _arc4randbytes(void *, size_t);
//uint32_t _arc4random(void);

static inline size_t
cprng_fast(void *p, size_t len)
{
	_arc4randbytes(p, len);
	return len;
}

static inline uint32_t
cprng_fast32(void)
{
	return arc4random();
}


/*
 * Return the first MAC address we encounter or, if none was found,
 * construct a sufficiently random multicast address. We don't try
 * to return the same MAC address as previously returned. We always
 * generate a new multicast address if no MAC address exists in the
 * system.
 * It would be nice to know if 'ifnet' or any of its sub-structures
 * has been changed in any way. If not, we could simply skip the
 * scan and safely return the MAC address we returned before.
 */
static void
uuid_node(uint16_t *node)
{
	//struct ifnet *ifp;
	//struct ifaddr *ifa;
	//struct sockaddr_dl *sdl;
	int i, s;

	s = splnet();
	// XXX KERNEL_LOCK(1, NULL);
	//IFNET_FOREACH(ifp) {
		/* Walk the address list */
	//	IFADDR_FOREACH(ifa, ifp) {
	//		sdl = (struct sockaddr_dl*)ifa->ifa_addr;
	//		if (sdl != NULL && sdl->sdl_family == AF_LINK &&
	//		    sdl->sdl_type == IFT_ETHER) {
	//			/* Got a MAC address. */
	//			memcpy(node, CLLADDR(sdl), UUID_NODE_LEN);
	//			KERNEL_UNLOCK_ONE(NULL);
	//			splx(s);
	//			return;
	//		}
	//	}
	//}
	//KERNEL_UNLOCK_ONE(NULL);
	splx(s);

	for (i = 0; i < (UUID_NODE_LEN>>1); i++)
		node[i] = (uint16_t)cprng_fast32();
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
	struct timespec tsp;
	uint64_t xtime = 0x01B21DD213814000LL;

	nanotime(&tsp);
	xtime += (uint64_t)tsp.tv_sec * 10000000LL;
	xtime += (uint64_t)(tsp.tv_nsec / 100);
	return (xtime & ((1LL << 60) - 1LL));
}

/*
 * Internal routine to actually generate the UUID.
 */
static void
uuid_generate(struct uuid_private *uuid, uint64_t *timep, int count)
{
	uint64_t xtime;

	mtx_enter(&uuid_mutex);

	uuid_node(uuid->node);
	xtime = uuid_time();
	*timep = xtime;

	if (uuid_last.time.ll == 0LL || uuid_last.node[0] != uuid->node[0] ||
	    uuid_last.node[1] != uuid->node[1] ||
	    uuid_last.node[2] != uuid->node[2])
		uuid->seq = (uint16_t)cprng_fast32() & 0x3fff;
	else if (uuid_last.time.ll >= xtime)
		uuid->seq = (uuid_last.seq + 1) & 0x3fff;
	else
		uuid->seq = uuid_last.seq;

	uuid_last = *uuid;
	uuid_last.time.ll = (xtime + count - 1) & ((1LL << 60) - 1LL);

	mtx_leave(&uuid_mutex);
}

static int
kern_uuidgen(struct uuid *store, int count, bool to_user)
{
	struct uuid_private uuid;
	uint64_t xtime;
	int error = 0, i;

	KASSERT(count >= 1);

	/* Generate the base UUID. */
	uuid_generate(&uuid, &xtime, count);

	/* Set sequence and variant and deal with byte order. */
	uuid.seq = htobe16(uuid.seq | 0x8000);

	for (i = 0; i < count; xtime++, i++) {
		/* Set time and version (=1) and deal with byte order. */
		uuid.time.x.low = (uint32_t)xtime;
		uuid.time.x.mid = (uint16_t)(xtime >> 32);
		uuid.time.x.hi = ((uint16_t)(xtime >> 48) & 0xfff) | (1 << 12);
		if (to_user) {
			error = copyout(&uuid, store + i, sizeof(uuid));
			if (error != 0)
				break;
		} else {
			memcpy(store + i, &uuid, sizeof(uuid));
		}
	}

	return error;
}

//int
//sys_uuidgen(struct lwp *l, const struct sys_uuidgen_args *uap, register_t *retval)
//{
	/*
	 * Limit the number of UUIDs that can be created at the same time
	 * to some arbitrary number. This isn't really necessary, but I
	 * like to have some sort of upper-bound that's less than 2G :-)
	 * XXX needs to be tunable.
	 */
	/* XXX FIX ME if (SCARG(uap,count) < 1 || SCARG(uap,count) > 2048)
		return (EINVAL);
	*/

	//return kern_uuidgen(SCARG(uap, store), SCARG(uap,count), true);
//}

int
uuidgen(struct uuid *store, int count)
{
	return kern_uuidgen(store,count, false);
}

int
uuid_snprintf(char *buf, size_t sz, const struct uuid *uuid)
{
	const struct uuid_private *id;
	int cnt;

	id = (const struct uuid_private *)uuid;
	cnt = snprintf(buf, sz, "%08x-%04x-%04x-%04x-%04x%04x%04x",
	    id->time.x.low, id->time.x.mid, id->time.x.hi, be16toh(id->seq),
	    be16toh(id->node[0]), be16toh(id->node[1]), be16toh(id->node[2]));
	return (cnt);
}

int
uuid_printf(const struct uuid *uuid)
{
	char buf[UUID_STR_LEN];

	(void) uuid_snprintf(buf, sizeof(buf), uuid);
	// Fix me XXX printf("%s", buf);
	return (0);
}

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


static __inline void __unused
be16enc(void *buf, uint16_t u)
{
	uint8_t *p = (uint8_t *)buf;

	p[0] = ((unsigned)u >> 8) & 0xff;
	p[1] = u & 0xff;
}

static __inline void __unused
le16enc(void *buf, uint16_t u)
{
	uint8_t *p = (uint8_t *)buf;

	p[0] = u & 0xff;
	p[1] = ((unsigned)u >> 8) & 0xff;
}

static __inline uint16_t __unused
be16dec(const void *buf)
{
	const uint8_t *p = (const uint8_t *)buf;

	return ((p[0] << 8) | p[1]);
}

static __inline uint16_t __unused
le16dec(const void *buf)
{
	const uint8_t *p = (const uint8_t *)buf;

	return ((p[1] << 8) | p[0]);
}

static __inline void __unused
be32enc(void *buf, uint32_t u)
{
	uint8_t *p = (uint8_t *)buf;

	p[0] = (u >> 24) & 0xff;
	p[1] = (u >> 16) & 0xff;
	p[2] = (u >> 8) & 0xff;
	p[3] = u & 0xff;
}

static __inline void __unused
le32enc(void *buf, uint32_t u)
{
	uint8_t *p = (uint8_t *)buf;

	p[0] = u & 0xff;
	p[1] = (u >> 8) & 0xff;
	p[2] = (u >> 16) & 0xff;
	p[3] = (u >> 24) & 0xff;
}

static __inline uint32_t __unused
be32dec(const void *buf)
{
	const uint8_t *p = (const uint8_t *)buf;

	return ((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

static __inline uint32_t __unused
le32dec(const void *buf)
{
	const uint8_t *p = (const uint8_t *)buf;

	return ((p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0]);
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
uuid_dec_le(void const *buf, struct uuid *uuid)
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
uuid_dec_be(void const *buf, struct uuid *uuid)
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
