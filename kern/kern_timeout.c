/*	$OpenBSD: kern_timeout.c,v 1.41 2013/11/27 04:28:32 dlg Exp $	*/
/*
 * Copyright (c) 2001 Thomas Nordin <nordin@openbsd.org>
 * Copyright (c) 2000-2001 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/timeout.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/queue.h>			/* _Q_INVALIDATE */

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#endif

/*
 * Timeouts are kept in a hierarchical timing wheel. The to_time is the value
 * of the global variable "ticks" when the timeout should be called. There are
 * four levels with 256 buckets each. See 'Scheme 7' in
 * "Hashed and Hierarchical Timing Wheels: Efficient Data Structures for
 * Implementing a Timer Facility" by George Varghese and Tony Lauck.
 */
#define BUCKETS 1024
#define WHEELSIZE 256
#define WHEELMASK 255
#define WHEELBITS 8

struct circq timeout_wheel[BUCKETS];	/* Queues of timeouts */
struct circq timeout_todo;		/* Worklist */

#define MASKWHEEL(wheel, time) (((time) >> ((wheel)*WHEELBITS)) & WHEELMASK)

#define BUCKET(rel, abs)						\
    (timeout_wheel[							\
	((rel) <= (1 << (2*WHEELBITS)))					\
	    ? ((rel) <= (1 << WHEELBITS))				\
		? MASKWHEEL(0, (abs))					\
		: MASKWHEEL(1, (abs)) + WHEELSIZE			\
	    : ((rel) <= (1 << (3*WHEELBITS)))				\
		? MASKWHEEL(2, (abs)) + 2*WHEELSIZE			\
		: MASKWHEEL(3, (abs)) + 3*WHEELSIZE])

#define MOVEBUCKET(wheel, time)						\
    CIRCQ_APPEND(&timeout_todo,						\
        &timeout_wheel[MASKWHEEL((wheel), (time)) + (wheel)*WHEELSIZE])

/*
 * The first thing in a struct timeout is its struct circq, so we
 * can get back from a pointer to the latter to a pointer to the
 * whole timeout with just a cast.
 */
static __inline struct timeout *
timeout_from_circq(struct circq *p)
{
	return ((struct timeout *)(p));
}

/*
 * All wheels are locked with the same mutex.
 *
 * We need locking since the timeouts are manipulated from hardclock that's
 * not behind the big lock.
 */
struct mutex timeout_mutex = MUTEX_INITIALIZER(IPL_HIGH);

/*
 * Circular queue definitions.
 */

#define CIRCQ_INIT(elem) do {                   \
        (elem)->next = (elem);                  \
        (elem)->prev = (elem);                  \
} while (0)

#define CIRCQ_INSERT(elem, list) do {           \
        (elem)->prev = (list)->prev;            \
        (elem)->next = (list);                  \
        (list)->prev->next = (elem);            \
        (list)->prev = (elem);                  \
} while (0)

#define CIRCQ_APPEND(fst, snd) do {             \
        if (!CIRCQ_EMPTY(snd)) {                \
                (fst)->prev->next = (snd)->next;\
                (snd)->next->prev = (fst)->prev;\
                (snd)->prev->next = (fst);      \
                (fst)->prev = (snd)->prev;      \
                CIRCQ_INIT(snd);                \
        }                                       \
} while (0)

#define CIRCQ_REMOVE(elem) do {                 \
        (elem)->next->prev = (elem)->prev;      \
        (elem)->prev->next = (elem)->next;      \
	_Q_INVALIDATE((elem)->prev);		\
	_Q_INVALIDATE((elem)->next);		\
} while (0)

#define CIRCQ_FIRST(elem) ((elem)->next)

#define CIRCQ_EMPTY(elem) (CIRCQ_FIRST(elem) == (elem))

/*
 * Some of the "math" in here is a bit tricky.
 *
 * We have to beware of wrapping ints.
 * We use the fact that any element added to the queue must be added with a
 * positive time. That means that any element `to' on the queue cannot be
 * scheduled to timeout further in time than INT_MAX, but to->to_time can
 * be positive or negative so comparing it with anything is dangerous.
 * The only way we can use the to->to_time value in any predictable way
 * is when we calculate how far in the future `to' will timeout -
 * "to->to_time - ticks". The result will always be positive for future
 * timeouts and 0 or negative for due timeouts.
 */
extern int ticks;		/* XXX - move to sys/X.h */

void
timeout_startup(void)
{
	int b;

	CIRCQ_INIT(&timeout_todo);
	for (b = 0; b < nitems(timeout_wheel); b++)
		CIRCQ_INIT(&timeout_wheel[b]);
}

void
timeout_set(struct timeout *new, void (*fn)(void *), void *arg)
{
	new->to_func = fn;
	new->to_arg = arg;
	new->to_flags = TIMEOUT_INITIALIZED;
}


int
timeout_add(struct timeout *new, int to_ticks)
{
	int old_time;
	int ret = 1;

#ifdef DIAGNOSTIC
	if (!(new->to_flags & TIMEOUT_INITIALIZED))
		panic("timeout_add: not initialized");
	if (to_ticks < 0)
		panic("timeout_add: to_ticks (%d) < 0", to_ticks);
#endif

	mtx_enter(&timeout_mutex);
	/* Initialize the time here, it won't change. */
	old_time = new->to_time;
	new->to_time = to_ticks + ticks;
	new->to_flags &= ~TIMEOUT_TRIGGERED;

	/*
	 * If this timeout already is scheduled and now is moved
	 * earlier, reschedule it now. Otherwise leave it in place
	 * and let it be rescheduled later.
	 */
	if (new->to_flags & TIMEOUT_ONQUEUE) {
		if (new->to_time - ticks < old_time - ticks) {
			CIRCQ_REMOVE(&new->to_list);
			CIRCQ_INSERT(&new->to_list, &timeout_todo);
		}
		ret = 0;
	} else {
		new->to_flags |= TIMEOUT_ONQUEUE;
		CIRCQ_INSERT(&new->to_list, &timeout_todo);
	}
	mtx_leave(&timeout_mutex);

	return (ret);
}

int
timeout_add_tv(struct timeout *to, const struct timeval *tv)
{
	long long to_ticks;

	to_ticks = (long long)hz * tv->tv_sec + tv->tv_usec / tick;
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;

	return (timeout_add(to, (int)to_ticks));
}

int
timeout_add_ts(struct timeout *to, const struct timespec *ts)
{
	long long to_ticks;

	to_ticks = (long long)hz * ts->tv_sec + ts->tv_nsec / (tick * 1000);
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;

	return (timeout_add(to, (int)to_ticks));
}

int
timeout_add_bt(struct timeout *to, const struct bintime *bt)
{
	long long to_ticks;

	to_ticks = (long long)hz * bt->sec + (long)(((uint64_t)1000000 *
	    (uint32_t)(bt->frac >> 32)) >> 32) / tick;
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;

	return (timeout_add(to, (int)to_ticks));
}

int
timeout_add_sec(struct timeout *to, int secs)
{
	long long to_ticks;

	to_ticks = (long long)hz * secs;
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;

	return (timeout_add(to, (int)to_ticks));
}

int
timeout_add_msec(struct timeout *to, int msecs)
{
	long long to_ticks;

	to_ticks = (long long)msecs * 1000 / tick;
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;

	return (timeout_add(to, (int)to_ticks));
}

int
timeout_add_usec(struct timeout *to, int usecs)
{
	int to_ticks = usecs / tick;

	return (timeout_add(to, to_ticks));
}

int
timeout_add_nsec(struct timeout *to, int nsecs)
{
	int to_ticks = nsecs / (tick * 1000);

	return (timeout_add(to, to_ticks));
}

int
timeout_del(struct timeout *to)
{
	int ret = 0;

	mtx_enter(&timeout_mutex);
	if (to->to_flags & TIMEOUT_ONQUEUE) {
		CIRCQ_REMOVE(&to->to_list);
		to->to_flags &= ~TIMEOUT_ONQUEUE;
		ret = 1;
	}
	to->to_flags &= ~TIMEOUT_TRIGGERED;
	mtx_leave(&timeout_mutex);

	return (ret);
}

/*
 * This is called from hardclock() once every tick.
 * We return !0 if we need to schedule a softclock.
 */
int
timeout_hardclock_update(void)
{
	int ret;

	mtx_enter(&timeout_mutex);

	ticks++;

	MOVEBUCKET(0, ticks);
	if (MASKWHEEL(0, ticks) == 0) {
		MOVEBUCKET(1, ticks);
		if (MASKWHEEL(1, ticks) == 0) {
			MOVEBUCKET(2, ticks);
			if (MASKWHEEL(2, ticks) == 0)
				MOVEBUCKET(3, ticks);
		}
	}
	ret = !CIRCQ_EMPTY(&timeout_todo);
	mtx_leave(&timeout_mutex);

	return (ret);
}

void
softclock(void *arg)
{
	struct timeout *to;
	void (*fn)(void *);

	mtx_enter(&timeout_mutex);
	while (!CIRCQ_EMPTY(&timeout_todo)) {

		to = timeout_from_circq(CIRCQ_FIRST(&timeout_todo));
		CIRCQ_REMOVE(&to->to_list);

		/* If due run it, otherwise insert it into the right bucket. */
		if (to->to_time - ticks > 0) {
			CIRCQ_INSERT(&to->to_list,
			    &BUCKET((to->to_time - ticks), to->to_time));
		} else {
#ifdef DEBUG
			if (to->to_time - ticks < 0)
				printf("timeout delayed %d\n", to->to_time -
				    ticks);
#endif
			to->to_flags &= ~TIMEOUT_ONQUEUE;
			to->to_flags |= TIMEOUT_TRIGGERED;

			fn = to->to_func;
			arg = to->to_arg;

			mtx_leave(&timeout_mutex);
			fn(arg);
			mtx_enter(&timeout_mutex);
		}
	}
	mtx_leave(&timeout_mutex);
}

#ifndef SMALL_KERNEL
void
timeout_adjust_ticks(int adj)
{
	struct timeout *to;
	struct circq *p;
	int new_ticks, b;

	/* adjusting the monotonic clock backwards would be a Bad Thing */
	if (adj <= 0)
		return;

	mtx_enter(&timeout_mutex);
	new_ticks = ticks + adj;
	for (b = 0; b < nitems(timeout_wheel); b++) {
		p = CIRCQ_FIRST(&timeout_wheel[b]);
		while (p != &timeout_wheel[b]) {
			to = timeout_from_circq(p);
			p = CIRCQ_FIRST(p);

			/* when moving a timeout forward need to reinsert it */
			if (to->to_time - ticks < adj)
				to->to_time = new_ticks;
			CIRCQ_REMOVE(&to->to_list);
			CIRCQ_INSERT(&to->to_list, &timeout_todo);
		}
	}
	ticks = new_ticks;
	mtx_leave(&timeout_mutex);
}
#endif

#ifdef DDB
void db_show_callout_bucket(struct circq *);

void
db_show_callout_bucket(struct circq *bucket)
{
	struct timeout *to;
	struct circq *p;
	db_expr_t offset;
	char *name;

	for (p = CIRCQ_FIRST(bucket); p != bucket; p = CIRCQ_FIRST(p)) {
		to = timeout_from_circq(p);
		db_find_sym_and_offset((db_addr_t)to->to_func, &name, &offset);
		name = name ? name : "?";
		db_printf("%9d %2td/%-4td %p  %s\n", to->to_time - ticks,
		    (bucket - timeout_wheel) / WHEELSIZE,
		    bucket - timeout_wheel, to->to_arg, name);
	}
}

void
db_show_callout(db_expr_t addr, int haddr, db_expr_t count, char *modif)
{
	int b;

	db_printf("ticks now: %d\n", ticks);
	db_printf("    ticks  wheel       arg  func\n");

	db_show_callout_bucket(&timeout_todo);
	for (b = 0; b < nitems(timeout_wheel); b++)
		db_show_callout_bucket(&timeout_wheel[b]);
}
#endif
