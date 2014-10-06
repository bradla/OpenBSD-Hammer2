/*	$OpenBSD: sockio.h,v 1.53 2013/10/13 10:10:04 reyk Exp $	*/
/*	$NetBSD: sockio.h,v 1.5 1995/08/23 00:40:47 thorpej Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1990, 1993, 1994
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
 *	@(#)sockio.h	8.1 (Berkeley) 3/28/94
 */

#ifndef	_SYS_SOCKIO_H_
#define	_SYS_SOCKIO_H_

#include <sys/ioccom.h>

/* Socket ioctl's. */
#define	SIOCSHIWAT	 _IOW('s',  0, int)		/* set high watermark */
#define	SIOCGHIWAT	 _IOR('s',  1, int)		/* get high watermark */
#define	SIOCSLOWAT	 _IOW('s',  2, int)		/* set low watermark */
#define	SIOCGLOWAT	 _IOR('s',  3, int)		/* get low watermark */
#define	SIOCATMARK	 _IOR('s',  7, int)		/* at oob mark? */
#define	SIOCSPGRP	 _IOW('s',  8, int)		/* set process group */
#define	SIOCGPGRP	 _IOR('s',  9, int)		/* get process group */

#define	SIOCSIFADDR	 _IOW('i', 12, struct ifreq)	/* set ifnet address */
#define	OSIOCGIFADDR	_IOWR('i', 13, struct ifreq)	/* get ifnet address */
#define	SIOCGIFADDR	_IOWR('i', 33, struct ifreq)	/* get ifnet address */
#define	SIOCSIFDSTADDR	 _IOW('i', 14, struct ifreq)	/* set p-p address */
#define	OSIOCGIFDSTADDR	_IOWR('i', 15, struct ifreq)	/* get p-p address */
#define	SIOCGIFDSTADDR	_IOWR('i', 34, struct ifreq)	/* get p-p address */
#define	SIOCSIFFLAGS	 _IOW('i', 16, struct ifreq)	/* set ifnet flags */
#define	SIOCGIFFLAGS	_IOWR('i', 17, struct ifreq)	/* get ifnet flags */
#define	OSIOCGIFBRDADDR	_IOWR('i', 18, struct ifreq)	/* get broadcast addr */
#define	SIOCGIFBRDADDR	_IOWR('i', 35, struct ifreq)	/* get broadcast addr */
#define	SIOCSIFBRDADDR	 _IOW('i', 19, struct ifreq)	/* set broadcast addr */
#define	OSIOCGIFCONF	_IOWR('i', 20, struct ifconf)	/* get ifnet list */
#define	SIOCGIFCONF	_IOWR('i', 36, struct ifconf)	/* get ifnet list */
#define	OSIOCGIFNETMASK	_IOWR('i', 21, struct ifreq)	/* get net addr mask */
#define	SIOCGIFNETMASK	_IOWR('i', 37, struct ifreq)	/* get net addr mask */
#define	SIOCSIFNETMASK	 _IOW('i', 22, struct ifreq)	/* set net addr mask */
#define	SIOCGIFMETRIC	_IOWR('i', 23, struct ifreq)	/* get IF metric */
#define	SIOCSIFMETRIC	 _IOW('i', 24, struct ifreq)	/* set IF metric */
#define	SIOCDIFADDR	 _IOW('i', 25, struct ifreq)	/* delete IF addr */
#define	SIOCAIFADDR	 _IOW('i', 26, struct ifaliasreq)/* add/chg IF alias */
#define	SIOCGIFDATA	_IOWR('i', 27, struct ifreq)	/* get if_data */
#define	SIOCSIFLLADDR	_IOW('i', 31, struct ifreq)	/* set link level addr */

/* KAME IPv6 */
/* SIOCAIFALIAS? */
#define SIOCALIFADDR	 _IOW('i', 28, struct if_laddrreq) /* add IF addr */
#define SIOCGLIFADDR	_IOWR('i', 29, struct if_laddrreq) /* get IF addr */
#define SIOCDLIFADDR	 _IOW('i', 30, struct if_laddrreq) /* delete IF addr */

#define	SIOCADDMULTI	 _IOW('i', 49, struct ifreq)	/* add m'cast addr */
#define	SIOCDELMULTI	 _IOW('i', 50, struct ifreq)	/* del m'cast addr */
#define	SIOCGETVIFCNT	_IOWR('u', 51, struct sioc_vif_req)/* vif pkt cnt */
#define	SIOCGETSGCNT	_IOWR('u', 52, struct sioc_sg_req) /* sg pkt cnt */

#define	SIOCSIFMEDIA	_IOWR('i', 53, struct ifreq)	/* set net media */
#define	SIOCGIFMEDIA	_IOWR('i', 54, struct ifmediareq) /* get net media */

#define SIOCSIFPHYADDR   _IOW('i', 70, struct ifaliasreq) /* set gif address */
#define	SIOCGIFPSRCADDR	_IOWR('i', 71, struct ifreq)	/* get gif psrc addr */
#define	SIOCGIFPDSTADDR	_IOWR('i', 72, struct ifreq)	/* get gif pdst addr */
#define	SIOCDIFPHYADDR	 _IOW('i', 73, struct ifreq)	/* delete gif addrs */
#define	SIOCSLIFPHYADDR	 _IOW('i', 74, struct if_laddrreq) /* set gif addrs */
#define	SIOCGLIFPHYADDR	_IOWR('i', 75, struct if_laddrreq) /* get gif addrs */

#define	SIOCBRDGADD	 _IOW('i', 60, struct ifbreq)	/* add bridge ifs */
#define	SIOCBRDGGSIFS	_IOWR('i', 60, struct ifbreq)	/* get span ifs */
#define	SIOCBRDGDEL	 _IOW('i', 61, struct ifbreq)	/* del bridge ifs */
#define	SIOCBRDGGIFFLGS	_IOWR('i', 62, struct ifbreq)	/* get brdg if flags */
#define	SIOCBRDGSIFFLGS	 _IOW('i', 63, struct ifbreq)	/* set brdg if flags */
#define	SIOCBRDGSCACHE	 _IOW('i', 64, struct ifbrparam)/* set cache size */
#define	SIOCBRDGGCACHE	_IOWR('i', 65, struct ifbrparam)/* get cache size */
#define	SIOCBRDGADDS	 _IOW('i', 65, struct ifbreq)	/* add span port */
#define	SIOCBRDGIFS	_IOWR('i', 66, struct ifbreq)	/* get member ifs */
#define	SIOCBRDGDELS	 _IOW('i', 66, struct ifbreq)	/* del span port */
#define	SIOCBRDGRTS	_IOWR('i', 67, struct ifbaconf)	/* get addresses */
#define	SIOCBRDGSADDR	_IOWR('i', 68, struct ifbareq)	/* set addr flags */
#define	SIOCBRDGSTO	 _IOW('i', 69, struct ifbrparam)/* cache timeout */
#define	SIOCBRDGGTO	_IOWR('i', 70, struct ifbrparam)/* cache timeout */
#define	SIOCBRDGDADDR	 _IOW('i', 71, struct ifbareq)	/* delete addr */
#define	SIOCBRDGFLUSH	 _IOW('i', 72, struct ifbreq)	/* flush addr cache */

#define SIOCBRDGARL	 _IOW('i', 77, struct ifbrlreq)	/* add bridge rule */
#define SIOCBRDGFRL	 _IOW('i', 78, struct ifbrlreq)	/* flush brdg rules */
#define SIOCBRDGGRL	_IOWR('i', 79, struct ifbrlconf)/* get bridge rules */
#define	SIOCBRDGGPRI	_IOWR('i', 80, struct ifbrparam)/* get priority */
#define	SIOCBRDGSPRI	 _IOW('i', 80, struct ifbrparam)/* set priority */
#define	SIOCBRDGGHT	_IOWR('i', 81, struct ifbrparam)/* get hello time */
#define	SIOCBRDGSHT	 _IOW('i', 81, struct ifbrparam)/* set hello time */
#define	SIOCBRDGGFD	_IOWR('i', 82, struct ifbrparam)/* get forward delay */
#define	SIOCBRDGSFD	 _IOW('i', 82, struct ifbrparam)/* set forward delay */
#define	SIOCBRDGGMA	_IOWR('i', 83, struct ifbrparam)/* get max age */
#define	SIOCBRDGSMA	 _IOW('i', 83, struct ifbrparam)/* set max age */
#define	SIOCBRDGSIFPRIO	 _IOW('i', 84, struct ifbreq)	/* set if priority */
#define	SIOCBRDGSIFCOST  _IOW('i', 85, struct ifbreq)	/* set if cost */

#define SIOCBRDGGPARAM  _IOWR('i', 88, struct ifbropreq)/* get brdg STP parms */
#define SIOCBRDGSTXHC    _IOW('i', 89, struct ifbrparam)/* set tx hold count */
#define SIOCBRDGSPROTO	 _IOW('i', 90, struct ifbrparam)/* set protocol */
#define SIOCBRDGS

#define GRESADDRS        _IOW('i', 101, struct ifreq)
#define GRESADDRD        _IOW('i', 102, struct ifreq)   
#define GREGADDRS       _IOWR('i', 103, struct ifreq)
#define GREGADDRD       _IOWR('i', 104, struct ifreq)
#define GRESPROTO        _IOW('i', 105, struct ifreq)
#define GREGPROTO       _IOWR('i', 106, struct ifreq)

#define	SIOCSIFMTU	 _IOW('i', 127, struct ifreq)	/* set ifnet mtu */
#define	SIOCGIFMTU	_IOWR('i', 126, struct ifreq)	/* get ifnet mtu */
#define	SIOCSIFASYNCMAP  _IOW('i', 125, struct ifreq)	/* set ppp asyncmap */
#define	SIOCGIFASYNCMAP _IOWR('i', 124, struct ifreq)	/* get ppp asyncmap */

#define	SIOCIFCREATE	 _IOW('i', 122, struct ifreq)	/* create clone if */
#define	SIOCIFDESTROY	 _IOW('i', 121, struct ifreq)	/* destroy clone if */
#define	SIOCIFGCLONERS	_IOWR('i', 120, struct if_clonereq) /* get cloners */

#define	SIOCAIFGROUP	_IOW('i', 135, struct ifgroupreq) /* add an ifgroup */
#define	SIOCGIFGROUP   _IOWR('i', 136, struct ifgroupreq) /* get ifgroups */
#define	SIOCDIFGROUP    _IOW('i', 137, struct ifgroupreq) /* delete ifgroup */
#define	SIOCGIFGMEMB   _IOWR('i', 138, struct ifgroupreq) /* get members */
#define	SIOCGIFGATTR   _IOWR('i', 139, struct ifgroupreq) /* get ifgroup attribs */
#define	SIOCSIFGATTR   _IOW('i', 140, struct ifgroupreq) /* set ifgroup attribs */

#define	SIOCSIFDESCR	 _IOW('i', 128, struct ifreq)	/* set ifnet descr */
#define	SIOCGIFDESCR	_IOWR('i', 129, struct ifreq)	/* get ifnet descr */

#define	SIOCSIFRTLABEL	 _IOW('i', 130, struct ifreq)	/* set ifnet rtlabel */
#define	SIOCGIFRTLABEL	_IOWR('i', 131, struct ifreq)	/* set ifnet rtlabel */

#define	SIOCSIFTIMESLOT	 _IOW('i', 133, struct ifreq)	/* set ifnet timeslot */
#define	SIOCGIFTIMESLOT	_IOWR('i', 134, struct ifreq)	/* get ifnet timeslot */

#define	SIOCSETVLAN	 _IOW('i', 143, struct ifreq)	/* set vlan parent if */
#define	SIOCGETVLAN	_IOWR('i', 144, struct ifreq)	/* get vlan parent if */

#define	SIOCSSPPPPARAMS	 _IOW('i', 147, struct ifreq)	/* set pppoe params */
#define	SIOCGSPPPPARAMS	_IOWR('i', 148, struct ifreq)	/* get pppoe params */

#define SIOCSETLABEL	 _IOW('i', 153, struct ifreq)	/* set MPLS label */
#define SIOCGETLABEL	 _IOW('i', 154, struct ifreq)	/* get MPLS label */

#define SIOCSIFPRIORITY	 _IOW('i', 155, struct ifreq)	/* set if priority */
#define SIOCGIFPRIORITY	_IOWR('i', 156, struct ifreq)	/* get if priority */

#define	SIOCSIFXFLAGS	 _IOW('i', 157, struct ifreq)	/* set ifnet xflags */
#define	SIOCGIFXFLAGS	_IOWR('i', 158, struct ifreq)	/* get ifnet xflags */

#define	SIOCSIFRDOMAIN	 _IOW('i', 159, struct ifreq)	/* set ifnet VRF id */
#define	SIOCGIFRDOMAIN	_IOWR('i', 160, struct ifreq)	/* get ifnet VRF id */

#define	SIOCSLIFPHYRTABLE _IOW('i', 161, struct ifreq) /* set tunnel VRF id */
#define	SIOCGLIFPHYRTABLE _IOWR('i', 162, struct ifreq) /* get tunnel VRF id */

#define SIOCSETKALIVE	_IOW('i', 163, struct ifkalivereq)
#define SIOCGETKALIVE	_IOWR('i', 164, struct ifkalivereq)

#define	SIOCGIFHARDMTU	_IOWR('i', 165, struct ifreq)	/* get ifnet hardmtu */

#define SIOCSVNETID	_IOW('i', 166, struct ifreq)	/* set virt net id */
#define SIOCGVNETID	_IOWR('i', 167, struct ifreq)	/* get virt net id */

#define SIOCSLIFPHYTTL	_IOW('i', 168, struct ifreq)	/* set tunnel ttl */
#define SIOCGLIFPHYTTL	_IOWR('i', 169, struct ifreq)	/* get tunnel ttl */

#define	SIOCSVH		_IOWR('i', 245, struct ifreq)	/* set carp param */
#define	SIOCGVH		_IOWR('i', 246, struct ifreq)	/* get carp param */

#define	SIOCSETPFSYNC	_IOW('i', 247, struct ifreq)
#define	SIOCGETPFSYNC	_IOWR('i', 248, struct ifreq)

#define	SIOCSETPFLOW	_IOW('i', 253, struct ifreq)
#define	SIOCGETPFLOW	_IOWR('i', 254, struct ifreq)

#endif /* !_SYS_SOCKIO_H_ */
