/*	$OpenBSD: conf.h,v 1.1.1.1 2006/04/27 15:47:48 miod Exp $	*/
/*
 * Copyright (c) 2002, Miodrag Vallat.
 * All rights reserved.
 *
 * Permission to redistribute, use, copy, and modify this software
 * is hereby granted without fee, provided that the following
 * conditions are met:
 *
 * 1. This entire notice is included in all source code copies of any
 *    software which is or includes a copy or modification of this
 *    software.
 * 2. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/conf.h>

#define	mmread	mmrw
#define	mmwrite	mmrw
cdev_decl(mm);

cdev_decl(nvram);

cdev_decl(vme);

cdev_decl(dart);
