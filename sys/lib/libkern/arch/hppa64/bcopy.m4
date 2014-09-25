define(_rcsid,``$OpenBSD: bcopy.m4,v 1.12 2013/06/14 12:45:18 kettenis Exp $'')dnl
dnl
dnl
dnl  This is the source file for bcopy.S, spcopy.S
dnl
dnl
define(`versionmacro',substr(_rcsid,1,eval(len(_rcsid)-2)))dnl
dnl
/* This is a generated file. DO NOT EDIT. */
/*
 * Generated from:
 *
 *	versionmacro
 */
/*
 * Copyright (c) 1999,2004 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
dnl
dnl    macro: L(`%arg1',`%arg2')
dnl synopsis: creates an assembly label based on args resulting in $%arg1.%arg2
dnl
define(`L', `$$1.$2')dnl
dnl
dnl
dnl
define(`STWS',`ifelse($5, `u',dnl
`ifelse($1, `22', `shrpw	$4, %r`$1', %sar, %r31
	stby,B,m %r31, F`'4($2, $3)',
`0', `0', `shrpw	%r`'incr($1), %r`$1', %sar, %r31
	stw,M	%r31, F`'4($2, $3)')',dnl
`0', `0',
`ifelse($1, `22',
`stby,B`'ifelse(B, `b', `,m ', `0', `0', `	')`'%r`$1', F`'4($2, $3)',
`0', `0', `stw,M	%r`$1', F`'4($2, $3)')')')dnl
define(`STWSS', `ifelse(`$3', `22', `dnl',
`0', `0', `STWSS($1, $2, eval($3 + 1), $4, $5)')
	STWS($3, $1, $2, $4, $5)dnl
')dnl
define(`LDWSS', `ifelse(`$3', `22', `dnl',
`0', `0', `LDWSS($1, $2, eval($3 + 1))')
	ldw,M	F`'4($1, $2), %r`'$3`'dnl
')dnl
dnl
dnl copy data in 4-words blocks
dnl
define(`hppa_blcopy',`
	addi	-16, $6, $6
L($1, `loop16'`$7')
	ldw	F 32($2, $3), %r0
ifelse(F, `-', `dnl
	addi	F`'4, $5, $5', `0', `0', `dnl')
LDWSS($2, $3, 19)
STWSS($4, $5, 20, `%ret1', $7)
ifelse($7, `u', `dnl
	STWS(19, $4, $5, `%ret1', $7)', $7, `a', `dnl')
	addib,*>= -16, $6, L($1, `loop16'`$7')
ifelse($7, `a', `dnl
	STWS(19, $4, $5, `%ret1', $7)dnl
', $7, `u', `dnl
	copy	%r19, %ret1')')dnl
dnl
dnl copy in words
dnl
define(`STWL', `addib,*<,n 12, $6, L($1, cleanup)
ifelse($7, `u', `	copy	%ret1, %r22', $7, `a', `dnl')
L($1, word)
	ldw,M	F`'4($2, $3), %r22
	addib,*>= -4, $6, L($1, word)
	stw,M	%r22, F`'4($4, $5)

L($1, cleanup)
	addib,*=,n 4, $6, L($1, done)
	ldw	0($2, $3), %r22
	add	$5, $6, $5
	b	L($1, done)
	stby,E	%r22, 0($4, $5)
')
dnl
dnl
dnl parameters:
dnl  $1	name
dnl  $2	source space
dnl  $3	source address
dnl  $4	destination space
dnl  $5	destination address
dnl  $6	length
dnl  $7	direction
dnl
define(hppa_copy,
`dnl
dnl
dnl	if direction is `-' (backwards copy), adjust src, dst
dnl
ifelse($7,`-', `add	$3, $6, $3
	add	$5, $6, $5
define(`F', `-')dnl
define(`R', `')dnl
define(`M', `mb')dnl
define(`B', `e')dnl
define(`E', `b')dnl
',dnl ifelse
`0',`0',
`define(`F', `')dnl
define(`R', `-')dnl
define(`M', `ma')dnl
define(`B', `b')dnl
define(`E', `e')dnl
')dnl ifelse

ifelse($7,`-', `', `0',`0',
`	cmpib,*>=,n 15, $6, L($1, byte)

	extrd,u	$3, 63, 2, %r20
	extrd,u	$5, 63, 2, %r19
	add	$6, %r19, $6
	cmpb,*<> %r20, %r19, L($1, unaligned)
	depd	%r0, 63, 2, $3
	hppa_blcopy($1, $2, $3, $4, $5, $6, `a')

	STWL($1, $2, $3, $4, $5, $6, `a')dnl

L($1, unaligned)
	sub,*>=	%r19, %r20, %r21
	ldw,ma	F`'4($2, $3), %ret1
	depd,z	%r21, 60, 61, %r22
	mtsar	%r22
	hppa_blcopy($1, $2, $3, $4, $5, $6, `u')

dnl	STWL($1, $2, $3, $4, $5, $6, `u')
	addib,*<,n 12, $6, L($1, cleanup_un)
L($1, word_un)
	ldw,M	F`'4($2, $3), %r22
	shrpw	%ret1, %r22, %sar, %r21
	addib,*< -4, $6, L($1, cleanup1_un)
	stw,M	%r21, F`'4($4, $5)
	ldw,M	F`'4($2, $3), %ret1
	shrpw	%r22, %ret1, %sar, %r21
	addib,*>= -4, $6, L($1, word_un)
	stw,M	%r21, F`'4($4, $5)

L($1, cleanup_un)
	addib,*<=,n 4, $6, L($1, done)
	mfctl	%sar, %r19
	add	$5, $6, $5
	extrd,u	%r19, 60, 2, %r19
	sub,*<=	$6, %r19, %r0
	ldw,M	F`'4($2, $3), %r22
	shrpw	%ret1, %r22, %sar, %r21
	b	L($1, done)
	stby,E	%r21, 0($4, $5)

L($1, cleanup1_un)
	b	L($1, cleanup_un)
	copy	%r22, %ret1
')dnl ifelse

L($1, byte)
	cmpb,*>=,n %r0, $6, L($1, done)
L($1, byte_loop)
	ldbs,M	F`'1($2, $3), %r22
	addib,*<> -1, $6, L($1, byte_loop)
	stbs,M	%r22, F`'1($4, $5)
L($1, done)
')dnl
`
#undef _LOCORE
#define _LOCORE
#include <machine/asm.h>
#include <machine/frame.h>
'
ifelse(NAME, `bcopy',
`
LEAF_ENTRY(bcopy)
	copy	%arg0, %ret0
	copy	%arg1, %arg0
	copy	%ret0, %arg1
ALTENTRY(memmove)
	cmpb,*>,n %arg0, %arg1, L(bcopy, reverse)
ALTENTRY(memcpy)
	copy	%arg0, %ret0
	hppa_copy(bcopy_f, %sr0, %arg1, %sr0, %arg0, %arg2, `+')
	bv	%r0(%rp)
	nop
L(bcopy, reverse)
	copy	%arg0, %ret0
	hppa_copy(bcopy_r, %sr0, %arg1, %sr0, %arg0, %arg2, `-')
	bv	%r0(%rp)
	nop
EXIT(bcopy)
')dnl
dnl
ifelse(NAME, `spcopy',
`
#ifdef _KERNEL
#include <assym.h>

/*
 * int spcopy (pa_space_t ssp, const void *src, pa_space_t dsp, void *dst,
 *              size_t size)
 * do a space to space bcopy.
 *
 * assumes that spaces do not clash, otherwise we lose
 */
	.import	copy_on_fault, code
LEAF_ENTRY(spcopy)
	sub,*<>	%r0, arg4, %r0
	bv	%r0(%rp)
	nop
`
	std	%rp, HPPA_FRAME_RP(%sp)
	ldo	HPPA_FRAME_SIZE(%sp), %sp
	/* setup fault handler */
	mfctl	%cr24, %r1
	ldd	CI_CURPROC(%r1), %r1
	ldil	L%copy_on_fault, %r21
	ldd	P_ADDR(%r1), %r2
	ldo	R%copy_on_fault(%r21), %r21
	ldd	PCB_ONFAULT+U_PCB(%r2), %r1
	std	%r21, PCB_ONFAULT+U_PCB(%r2)
'
	mtsp	%arg0, %sr1
	mtsp	%arg2, %sr2

	copy	arg4, %ret0
	hppa_copy(spcopy, %sr1, %arg1, %sr2, %arg3, %ret0, `+')

	mtsp	%r0, %sr1
	mtsp	%r0, %sr2
	/* reset fault handler */
	std	%r1, PCB_ONFAULT+U_PCB(%r2)
	ldo	-HPPA_FRAME_SIZE(%sp), %sp
	ldd	HPPA_FRAME_RP(%sp), %rp
	bv	%r0(%rp)
	copy	%r0, %ret0
EXIT(spcopy)
#endif
')dnl

	.end
