/*	$OpenBSD: unixdev.h,v 1.7 2011/03/13 00:13:53 deraadt Exp $	*/

/*
 * Copyright (c) 1996 Michael Shalayeff
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


/* unixdev.c */
int unixstrategy(void *, int, daddr32_t, size_t, void *, size_t *);
int unixopen(struct open_file *, ...);
int unixclose(struct open_file *);
int unixioctl(struct open_file *, u_long, void *);

void unix_probe(struct consdev *);
void unix_init(struct consdev *);
int unix_getc(dev_t);
void unix_putc(dev_t, int);
int unix_ischar(dev_t);

/* unixsys.S */
int uopen(const char *, int, ...);
int uread(int, void *, size_t);
int uwrite(int, void *, size_t);
int uioctl(int, u_long, char *);
int uclose(int);
off_t ulseek(int, off_t, int);
void uexit(int) __attribute__((noreturn));
int syscall(int, ...);
int __syscall(quad_t, ...);
