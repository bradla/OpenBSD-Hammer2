/*	$OpenBSD: lock.h,v 1.7 2014/03/29 18:09:29 guenther Exp $	*/

/* public domain */

#ifndef	_MACHINE_LOCK_H_
#define	_MACHINE_LOCK_H_

#include <machine/atomic.h>

typedef volatile u_int __cpu_simple_lock_t __attribute__((__aligned__(16)));

#define	__SIMPLELOCK_LOCKED	0
#define	__SIMPLELOCK_UNLOCKED	1

static __inline__ void
__cpu_simple_lock_init(__cpu_simple_lock_t *l)
{
	*l = __SIMPLELOCK_UNLOCKED;
}

static __inline__ void
__cpu_simple_lock(__cpu_simple_lock_t *l)
{
	volatile u_int old;

	do {
		__asm__ volatile
		    ("ldcws 0(%2), %0" : "=&r" (old), "+m" (l) : "r" (l));
	} while (old != __SIMPLELOCK_UNLOCKED);
}

static __inline__ int
__cpu_simple_lock_try(__cpu_simple_lock_t *l)
{
	volatile u_int old;

	__asm__ volatile
	    ("ldcws 0(%2), %0" : "=&r" (old), "+m" (l) : "r" (l));

	return (old == __SIMPLELOCK_UNLOCKED);
}

static __inline__ void
__cpu_simple_unlock(__cpu_simple_lock_t *l)
{
	*l = __SIMPLELOCK_UNLOCKED;
}

#if defined(_KERNEL) && defined(MULTIPROCESSOR)
int	rw_cas_hppa(volatile unsigned long *, unsigned long, unsigned long);
#define	rw_cas rw_cas_hppa
#endif

#endif	/* _MACHINE_LOCK_H_ */
