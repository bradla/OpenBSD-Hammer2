/*	$OpenBSD: qevar.h,v 1.6 2006/06/02 20:00:54 miod Exp $	*/

/*
 * Copyright (c) 1998, 2000 Jason L. Wright.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

struct qesoftc {
	struct	device sc_dev;
	struct	intrhand sc_ih;		/* interrupt vectoring */
	struct	arpcom sc_arpcom;	/* ethernet common */

	struct	qec_softc *sc_qec;	/* QEC parent */
	struct	qecregs *sc_qr;		/* QEC registers */
	struct	qe_mregs *sc_mr;	/* MACE registers */
	struct	qe_cregs *sc_cr;	/* Channel registers */

	int	sc_channel;
	u_int	sc_rev;

	int	sc_burst;

	struct	qe_bufs *sc_bufs, *sc_bufs_dva;
	struct	qe_desc *sc_desc, *sc_desc_dva;

	int	sc_no_td, sc_first_td, sc_last_td;
	int	sc_last_rd;
	struct	ifmedia sc_ifmedia;
};
