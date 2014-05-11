/*	$OpenBSD: armv7_machdep.h,v 1.1 2013/10/30 20:20:23 syl Exp $	*/
/*
 * Copyright (c) 2013 Sylvestre Gallon <ccna.syl@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __PLATFORMVAR_H__
#define __PLATFORMVAR_H__

void platform_powerdown(void);
void platform_watchdog_reset(void);
void platform_init_cons(void);
void platform_print_board_type(void);
void platform_bootconfig_dram(BootConfig *, psize_t *, psize_t *);
void platform_disable_l2_if_needed(void);
extern const char *platform_boot_name;

#endif /* __PLATFORMVAR_H__ */
