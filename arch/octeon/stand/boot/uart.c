/*	$OpenBSD: uart.c,v 1.4 2013/06/13 20:01:01 jasper Exp $	*/

/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/tty.h>

#include <machine/bus.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/cons.h>

#include "libsa.h"

#define  OCTEON_MIO_UART0               0x8001180000000800ull
#define  OCTEON_MIO_UART0_LSR           0x8001180000000828ull
#define  OCTEON_MIO_UART0_RBR           0x8001180000000800ull
#define  OCTEON_MIO_UART0_USR           0x8001180000000938ull
#define  OCTEON_MIO_UART0_LCR           0x8001180000000818ull
#define  OCTEON_MIO_UART0_DLL           0x8001180000000880ull
#define  OCTEON_MIO_UART0_DLH           0x8001180000000888ull
#define  USR_TXFIFO_NOTFULL		2

int cn30xxuart_delay(void);
void cn30xxuart_wait_txhr_empty(int);

/*
 * Early console routines.
 */
int
cn30xxuart_delay(void)
{
	int divisor;
	u_char lcr;

	lcr = (u_char)*(uint64_t*)OCTEON_MIO_UART0_LCR;
	*(uint64_t*)OCTEON_MIO_UART0_LCR = lcr | LCR_DLAB;
	divisor = (int)(*(uint64_t*)OCTEON_MIO_UART0_DLL |
		*(uint64_t*)OCTEON_MIO_UART0_DLH << 8);
	*(uint64_t*)OCTEON_MIO_UART0_LCR = lcr;

	return 10; /* return an approx delay value */
}

void
cn30xxuart_wait_txhr_empty(int d)
{
	while (((*(uint64_t*)OCTEON_MIO_UART0_LSR & LSR_TXRDY) == 0) &&
               ((*(uint64_t*)OCTEON_MIO_UART0_USR & USR_TXFIFO_NOTFULL) == 0))
		delay(d);
}

void
cn30xxuartcninit(struct consdev *consdev)
{
}

void
cn30xxuartcnprobe(struct consdev *cn)
{
	cn->cn_pri = CN_HIGHPRI;
	cn->cn_dev = makedev(CONSMAJOR, 0);
}

void
cn30xxuartcnputc (dev_t dev, int c)
{
	int d;

	/* 1/10th the time to transmit 1 character (estimate). */
	d = cn30xxuart_delay();
        cn30xxuart_wait_txhr_empty(d);
	*(uint64_t*)OCTEON_MIO_UART0_RBR = (uint8_t)c;
        cn30xxuart_wait_txhr_empty(d);
}

int
cn30xxuartcngetc (dev_t dev)
{
	int c, d;

	/* 1/10th the time to transmit 1 character (estimate). */
	d = cn30xxuart_delay();

	while ((*(uint64_t*)OCTEON_MIO_UART0_LSR & LSR_RXRDY) == 0)
		delay(d);

	c = (uint8_t)*(uint64_t*)OCTEON_MIO_UART0_RBR;

	return (c);
}
