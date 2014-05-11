/*	$OpenBSD: if_le_vmereg.h,v 1.1.1.1 2006/05/09 18:25:00 miod Exp $ */

/*-
 * Copyright (c) 1982, 1992, 1993
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
 * @(#)if_lereg.h	8.2 (Berkeley) 10/30/93
 */

#define	VLEMEMSIZE	0x00040000	/* 256 KB */

/*
 * LANCE registers for Interphase 3207 Hawk
 */

#define	LEREG_CSR	0x00
#define	LEREG_VEC	0x02
#define	LEREG_RDP	0x04
#define	LEREG_RAP	0x06
#define	LEREG_EAR	0x08

/* CSR bits */
#define	NVRAM_EN	0x0008	/* NVRAM enable bit (active low) */
#define	INTR_EN		0x0010	/* interrupt enable bit (active low) */
#define	PARITYB		0x0020	/* parity error clear bit */
#define	HW_RS		0x0040	/* hardware reset bit (active low) */
#define	SYSFAILB	0x0080	/* SYSFAIL bit */

#define	NVRAM_RWEL	0xe0	/* Reset write enable latch      */
#define	NVRAM_STO	0x60	/* Store ram to eeprom           */
#define	NVRAM_SLP	0xa0	/* Novram into low power mode    */
#define	NVRAM_WRITE	0x20	/* Writes word from location x   */
#define	NVRAM_SWEL	0xc0	/* Set write enable latch        */
#define	NVRAM_RCL	0x40	/* Recall eeprom data into ram   */
#define	NVRAM_READ	0x00	/* Reads word from location x    */

#define	CDELAY		delay(10000)
#define	WRITE_CSR_OR(x) \
	do { \
		lesc->sc_csr |= (x); \
		bus_space_write_2(lesc->sc_iot, lesc->sc_ioh, \
		    LEREG_CSR, lesc->sc_csr); \
	} while (0)
#define	WRITE_CSR_AND(x) \
	do { \
		lesc->sc_csr &= ~(x); \
		bus_space_write_2(lesc->sc_iot, lesc->sc_ioh, \
		    LEREG_CSR, lesc->sc_csr); \
	} while (0)
#define	ENABLE_NVRAM	WRITE_CSR_AND(NVRAM_EN)
#define	DISABLE_NVRAM	WRITE_CSR_OR(NVRAM_EN)
#define	ENABLE_INTR	WRITE_CSR_AND(INTR_EN)
#define	DISABLE_INTR	WRITE_CSR_OR(INTR_EN)
#define	RESET_HW \
	do { \
		WRITE_CSR_AND(HW_RS); \
		CDELAY; \
	} while (0)
#define	SET_VEC(x) \
	bus_space_write_2(lesc->sc_iot, lesc->sc_ioh, LEREG_VEC, (x))
#define	SYSFAIL_CL	WRITE_CSR_AND(SYSFAILB)
