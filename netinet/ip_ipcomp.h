/* $OpenBSD: ip_ipcomp.h,v 1.7 2007/12/14 18:33:41 deraadt Exp $ */

/*
 * Copyright (c) 2001 Jean-Jacques Bernard-Gundol (jj@wabbitt.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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
 */

/* IP payload compression protocol (IPComp), see RFC 2393 */

#ifndef _NETINET_IP_IPCOMP_H_
#define _NETINET_IP_IPCOMP_H_

struct ipcompstat {
	u_int32_t	ipcomps_hdrops;	/* Packet shorter than header shows */
	u_int32_t	ipcomps_nopf;	/* Protocol family not supported */
	u_int32_t	ipcomps_notdb;
	u_int32_t	ipcomps_badkcr;
	u_int32_t	ipcomps_qfull;
	u_int32_t	ipcomps_noxform;
	u_int32_t	ipcomps_wrap;
	u_int32_t	ipcomps_input;	/* Input IPcomp packets */
	u_int32_t	ipcomps_output;	/* Output IPcomp packets */
	u_int32_t	ipcomps_invalid;	/* Trying to use an invalid
						 * TDB */
	u_int64_t	ipcomps_ibytes;	/* Input bytes */
	u_int64_t	ipcomps_obytes;	/* Output bytes */
	u_int32_t	ipcomps_toobig;	/* Packet got larger than
					 * IP_MAXPACKET */
	u_int32_t	ipcomps_pdrops;	/* Packet blocked due to policy */
	u_int32_t	ipcomps_crypto;	/* "Crypto" processing failure */
	u_int32_t	ipcomps_minlen;	/* packets too short for compress */
};

/* IPCOMP header */
struct ipcomp {
	u_int8_t	ipcomp_nh;	/* Next header */
	u_int8_t	ipcomp_flags;	/* Flags: reserved field: 0 */
	u_int16_t	ipcomp_cpi;	/* Compression Parameter Index,
					 * Network order */
};

/* Length of IPCOMP header */
#define IPCOMP_HLENGTH		4

/*
 * Names for IPCOMP sysctl objects
 */
#define IPCOMPCTL_ENABLE	1	/* Enable COMP processing */
#define IPCOMPCTL_STATS		2	/* COMP stats */
#define IPCOMPCTL_MAXID		3

#define IPCOMPCTL_NAMES { \
	{ 0, 0 }, \
	{ "enable", CTLTYPE_INT }, \
	{ "stats", CTLTYPE_STRUCT }, \
}

#define IPCOMPCTL_VARS { \
	NULL, \
	&ipcomp_enable, \
	NULL \
}

#ifdef _KERNEL
extern int ipcomp_enable;
extern struct ipcompstat ipcompstat;
#endif				/* _KERNEL */
#endif	/* _NETINET_IP_IPCOMP_H_ */
