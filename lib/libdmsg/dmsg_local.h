/*
 * Copyright (c) 2011-2012 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/endian.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/dmsg.h>
#include <sys/poll.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <assert.h>
#include <pthread.h>
//#include <libutil.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <uuid.h>

#include <openssl/rsa.h>	/* public/private key functions */
#include <openssl/pem.h>	/* public/private key file load */
#include <openssl/err.h>
#include <openssl/evp.h>	/* aes_256_cbc functions */

#include <machine/atomic.h>
#include <dev/pci/drm/drm_atomic.h>
#include <sys/atomic.h>

#include "dmsg.h"


#define __byte_swap_long_var(x) \
__extension__ ({ register __uint64_t __X = (x); \
	__asm ("bswap %0" : "+r" (__X)); \
__X; })

#ifdef __i386__

#define __byte_swap_long(x)     __byte_swap_long_const(x)

#else
 
  #ifdef __OPTIMIZE__
    #define __byte_swap_long(x)     (__builtin_constant_p(x) ? \
	 __byte_swap_long_const(x) : __byte_swap_long_var(x))
  #else   /* __OPTIMIZE__ */
	#define __byte_swap_long(x)     __byte_swap_long_var(x)
  #endif  /* __OPTIMIZE__ */

#endif  /* __i386__ */

static __inline __uint64_t
__bswap64(__uint64_t _x)
{
	return (__byte_swap_long(_x));
}

#define bswap16 swap16
#define bswap32 swap32

/*
 * General byte order swapping functions.
 */
//#define       bswap16(x)      __bswap16(x)
//#define       bswap32(x)      __bswap32(x)
#define bswap64(x)      __bswap64(x)


