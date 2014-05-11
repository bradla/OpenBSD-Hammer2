/*	$OpenBSD: pxa2x0_gpio.h,v 1.6 2009/08/22 02:54:50 mk Exp $ */
/*	$wasabi$	*/

/*
 * Copyright 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _PXA2X0_GPIO_H
#define _PXA2X0_GPIO_H

/*
 * If you want to fiddle with GPIO registers before the
 * driver has been autoconfigured (e.g. from initarm()),
 * call this function with the virtual address of the
 * GPIO controller's registers
 */
void pxa2x0_gpio_bootstrap(vaddr_t);

/*
 * GPIO pin function query/manipulation functions
 */
u_int pxa2x0_gpio_get_function(u_int);
u_int pxa2x0_gpio_set_function(u_int, u_int);
int pxa2x0_gpio_get_bit(u_int gpio);
void pxa2x0_gpio_set_bit(u_int gpio);
void pxa2x0_gpio_clear_bit(u_int gpio);
void pxa2x0_gpio_set_dir(u_int gpio, int dir);
void pxa2x0_gpio_clear_intr(u_int gpio);

/*
 * Establish/Disestablish interrupt handlers for GPIO pins
 */
void *pxa2x0_gpio_intr_establish(u_int, int, int, int (*)(void *), void *,
    const char *);
void pxa2x0_gpio_intr_disestablish(void *);
const char *pxa2x0_gpio_intr_string(void *);
void pxa2x0_gpio_intr_mask(void *);
void pxa2x0_gpio_intr_unmask(void *);

#endif /* _PXA2X0_GPIO_H */
