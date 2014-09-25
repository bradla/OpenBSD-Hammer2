/*
 * Copyright (c) 2005 Jeffrey M. Hsu.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Jeffrey M. Hsu.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $DragonFly: src/sys/kern/kern_objcache.c,v 1.23 2008/10/26 04:29:19 sephe Exp $
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/globaldata.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/objcache.h>
//#include <sys/spinlock.h>
//#include <sys/thread.h>
//#include <sys/thread2.h>
//#include <sys/spinlock2.h>

char *kstrdup(const char *);

//static MALLOC_DEFINE(M_OBJCACHE, "objcache", "Object Cache");
//static MALLOC_DEFINE(M_OBJMAG, "objcache magazine", "Object Cache Magazine");
#define M_OBJCACHE "objcache"
#define M_OBJMAG "objcache magazine"
#define	M_NULLOK	0x0400	/* ok to return NULL */

#define	INITIAL_MAG_CAPACITY	64

struct magazine {
	int			 rounds;
	int			 capacity;
	SLIST_ENTRY(magazine)	 nextmagazine;
	void			*objects[];
};

SLIST_HEAD(magazinelist, magazine);

#define MAGAZINE_HDRSIZE	__offsetof(struct magazine, objects[0])
#define MAGAZINE_CAPACITY_MAX	128
#define MAGAZINE_CAPACITY_MIN	4

/*
 * per-cluster cache of magazines
 *
 * All fields in this structure are protected by the spinlock.
 */
struct magazinedepot {
	/*
	 * The per-cpu object caches only exchanges completely full or
	 * completely empty magazines with the depot layer, so only have
	 * to cache these two types of magazines.
	 */
	struct magazinelist	fullmagazines;
	struct magazinelist	emptymagazines;
	int			magcapacity;

	/* protect this structure */
	struct lock		spin;

	/* magazines not yet allocated towards limit */
	int			unallocated_objects;

	/* infrequently used fields */
	int			waiting;	/* waiting for another cpu to
						 * return a full magazine to
						 * the depot */
	int			contested;	/* depot contention count */
} __cachealign;

/*
 * per-cpu object cache
 * All fields in this structure are protected by crit_enter().
 */
struct percpu_objcache {
	struct magazine	*loaded_magazine;	/* active magazine */
	struct magazine	*previous_magazine;	/* backup magazine */

	/* statistics */
	int		gets_cumulative;	/* total calls to get */
	int		gets_null;		/* objcache_get returned NULL */
	int		puts_cumulative;	/* total calls to put */
	int		puts_othercluster;	/* returned to other cluster */

	/* infrequently used fields */
	int		waiting;	/* waiting for a thread on this cpu to
					 * return an obj to the per-cpu cache */
}  __cachealign2;

/* only until we have NUMA cluster topology information XXX */
#define MAXCLUSTERS 1
#define myclusterid 0
#define CLUSTER_OF(obj) 0

/*
 * Two-level object cache consisting of NUMA cluster-level depots of
 * fully loaded or completely empty magazines and cpu-level caches of
 * individual objects.
 */
struct objcache {
	char			*name;

	/* object constructor and destructor from blank storage */
	objcache_ctor_fn	*ctor;
	objcache_dtor_fn	*dtor;
	void			*privdata;

	/* interface to underlying allocator */
	objcache_alloc_fn	*alloc;
	objcache_free_fn	*free;
	void			*allocator_args;

	LIST_ENTRY(objcache)	oc_next;
	int			exhausted;	/* oops */

	/* NUMA-cluster level caches */
	struct magazinedepot	depot[MAXCLUSTERS];

	struct percpu_objcache	cache_percpu[];		/* per-cpu caches */
};

//static struct spinlock objcachelist_spin;
static LIST_HEAD(objcachelist, objcache) allobjcaches;
static int magazine_capmin;
static int magazine_capmax;

/*
* Its convenient to put these here rather then create another header file.
*/
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
#define __offsetof(type, field) __builtin_offsetof(type, field)
#else
#ifndef __cplusplus
#define __offsetof(type, field) ((__size_t)(&((type *)0)->field))
#else
#define __offsetof(type, field)					\
	(__offsetof__ (reinterpret_cast <__size_t>		\
		 (&reinterpret_cast <const volatile char &>	\
		  (static_cast<type *> (0)->field))))
#endif
#endif

#define	__VM_CACHELINE_SIZE	64
#define __VM_CACHELINE_MASK	(__VM_CACHELINE_SIZE - 1)
#define	__VM_CACHELINE_ALIGN(n)	\
	(((n) + __VM_CACHELINE_MASK) & ~__VM_CACHELINE_MASK)

static struct magazine *
mag_alloc(int capacity)
{
	struct magazine *mag;
	int size;

	size = __offsetof(struct magazine, objects[capacity]);
	KASSERT(size > 0 && (size & __VM_CACHELINE_MASK) == 0);
	    // ("magazine size is not multiple cache line size"));

	// XXX fix me mag = kmalloc_cachealign(size, M_OBJMAG, M_INTWAIT | M_ZERO);
	mag->capacity = capacity;
	mag->rounds = 0;
	return (mag);
}

static int
mag_capacity_align(int mag_capacity)
{
	int mag_size;

	mag_size = __VM_CACHELINE_ALIGN(
	    __offsetof(struct magazine, objects[mag_capacity]));
	mag_capacity = (mag_size - MAGAZINE_HDRSIZE) / sizeof(void *);

	return mag_capacity;
}

/*
 * Utility routine for objects that don't require any de-construction.
 */

static void
null_dtor(void *obj, void *privdata)
{
	/* do nothing */
}

static int 
null_ctor(void *obj, void *privdata, int ocflags)
{
	return 1;
}


char *
kstrdup(const char *string)
{
        size_t len;
        char *copy;
    
        len = strlen(string) + 1;
	copy = malloc(len, M_TEMP, M_WAITOK);
   	bcopy(string, copy, len);
	return (copy);
}


/*
 * Create an object cache.
 */
struct objcache *
objcache_create(const char *name, int cluster_limit, int nom_cache,
		objcache_ctor_fn *ctor, objcache_dtor_fn *dtor, void *privdata,
		objcache_alloc_fn *alloc, objcache_free_fn *free,
		void *allocator_args)
{
	struct objcache *oc;
	struct magazinedepot *depot;
	int cpuid;
	int nmagdepot;
	int mag_capacity;
	int i;

	/*
	 * Allocate object cache structure
	 */
	/*  XXX oc = kmalloc_cachealign(
	    __offsetof(struct objcache, cache_percpu[ncpus]),
	    M_OBJCACHE, M_WAITOK | M_ZERO);
	*/
	oc->name = kstrdup(name);
	oc->ctor = ctor ? ctor : null_ctor;
	oc->dtor = dtor ? dtor : null_dtor;
	oc->privdata = privdata;
	oc->alloc = alloc;
	oc->free = free;
	oc->allocator_args = allocator_args;

	/*
	 * Initialize depot list(s).
	 */
	depot = &oc->depot[0];

	// XX spin_init(&depot->spin);
	SLIST_INIT(&depot->fullmagazines);
	SLIST_INIT(&depot->emptymagazines);

	/*
	 * Figure out the nominal number of free objects to cache and
	 * the magazine capacity.  By default we want to cache up to
	 * half the cluster_limit.  If there is no cluster_limit then
	 * we want to cache up to 128 objects.
	 */
	if (nom_cache == 0)
		nom_cache = cluster_limit / 2;
	if (cluster_limit && nom_cache > cluster_limit)
		nom_cache = cluster_limit;
	if (nom_cache == 0)
		nom_cache = INITIAL_MAG_CAPACITY * 2;

	/*
	 * Magazine capacity for 2 active magazines per cpu plus 2
	 * magazines in the depot.
	 */
	mag_capacity = mag_capacity_align(nom_cache / (ncpus + 1) / 2 + 1);
	if (mag_capacity > magazine_capmax)
		mag_capacity = magazine_capmax;
	else if (mag_capacity < magazine_capmin)
		mag_capacity = magazine_capmin;
	depot->magcapacity = mag_capacity;

	/*
	 * The cluster_limit must be sufficient to have two magazines per
	 * cpu plus at least two magazines in the depot.  However, because
	 * partial magazines can stay on the cpus what we really need here
	 * is to specify the number of extra magazines we allocate for the
	 * depot.
	 */
	if (cluster_limit == 0) {
		depot->unallocated_objects = -1;
	} else {
		depot->unallocated_objects = ncpus * mag_capacity * 2 +
					     cluster_limit;
	}

	/*
	 * Initialize per-cpu caches
	 */
	for (cpuid = 0; cpuid < ncpus; cpuid++) {
		struct percpu_objcache *cache_percpu = &oc->cache_percpu[cpuid];

		cache_percpu->loaded_magazine = mag_alloc(mag_capacity);
		cache_percpu->previous_magazine = mag_alloc(mag_capacity);
	}

	/*
	 * Compute how many empty magazines to place in the depot.  This
	 * determines the retained cache size and is based on nom_cache.
	 *
	 * The actual cache size is larger because there are two magazines
	 * for each cpu as well but those can be in any fill state so we
	 * just can't count them.
	 *
	 * There is a minimum of two magazines in the depot.
	 */
	nmagdepot = nom_cache / mag_capacity + 1;
	if (nmagdepot < 2)
		nmagdepot = 2;

	/*
	 * Put empty magazines in depot
	 */
	for (i = 0; i < nmagdepot; i++) {
		struct magazine *mag = mag_alloc(mag_capacity);
		SLIST_INSERT_HEAD(&depot->emptymagazines, mag, nextmagazine);
	}

	// XX spin_lock(&objcachelist_spin);
	LIST_INSERT_HEAD(&allobjcaches, oc, oc_next);
	// XX spin_unlock(&objcachelist_spin);

	return (oc);
}

struct objcache *
objcache_create_simple(malloc_type_t mtype, size_t objsize)
{
	struct objcache_malloc_args *margs;
	struct objcache *oc;

	margs = malloc(sizeof(*margs), M_TEMP, M_WAITOK|M_ZERO);
	margs->objsize = objsize;
	margs->mtype = mtype;
	/* XX fix it ptr deref  oc = objcache_create(mtype->ks_shortdesc, 0, 0,
			     NULL, NULL, NULL,
			     objcache_malloc_alloc, objcache_malloc_free,
			     margs);
	*/
	return (oc);
}

struct objcache *
objcache_create_mbacked(malloc_type_t mtype, size_t objsize,
			int cluster_limit, int nom_cache,
			objcache_ctor_fn *ctor, objcache_dtor_fn *dtor,
			void *privdata)
{
	struct objcache_malloc_args *margs;
	struct objcache *oc;

	margs = malloc(sizeof(*margs), M_TEMP, M_WAITOK|M_ZERO);
	margs->objsize = objsize;
	margs->mtype = mtype;
	/* XX ptr deref oc = objcache_create(mtype->ks_shortdesc,
			     cluster_limit, nom_cache,
			     ctor, dtor, privdata,
			     objcache_malloc_alloc, objcache_malloc_free,
			     margs);
	*/
	return(oc);
}


#define MAGAZINE_EMPTY(mag)	(mag->rounds == 0)
#define MAGAZINE_NOTEMPTY(mag)	(mag->rounds != 0)
#define MAGAZINE_FULL(mag)	(mag->rounds == mag->capacity)

#define	swap(x, y)	({ struct magazine *t = x; x = y; y = t; })

/*
 * Get an object from the object cache.
 *
 * WARNING!  ocflags are only used when we have to go to the underlying
 * allocator, so we cannot depend on flags such as M_ZERO.
 */
void *
objcache_get(struct objcache *oc, int ocflags)
{
	struct percpu_objcache *cpucache; // = &oc->cache_percpu[mycpuid]; // XX find cache per cpu
	struct magazine *loadedmag;
	struct magazine *emptymag;
	void *obj;
	struct magazinedepot *depot;

	// XX KKASSERT((ocflags & M_ZERO) == 0);
	//XX fix huh crit_enter();
	++cpucache->gets_cumulative;

retry:
	/*
	 * Loaded magazine has an object.  This is the hot path.
	 * It is lock-free and uses a critical section to block
	 * out interrupt handlers on the same processor.
	 */
	loadedmag = cpucache->loaded_magazine;
	if (MAGAZINE_NOTEMPTY(loadedmag)) {
		obj = loadedmag->objects[--loadedmag->rounds];
		// XX crit_exit();
		return (obj);
	}

	/* Previous magazine has an object. */
	if (MAGAZINE_NOTEMPTY(cpucache->previous_magazine)) {
		swap(cpucache->loaded_magazine, cpucache->previous_magazine);
		loadedmag = cpucache->loaded_magazine;
		obj = loadedmag->objects[--loadedmag->rounds];
		// XX fix me if needed crit_exit();
		return (obj);
	}

	/*
	 * Both magazines empty.  Get a full magazine from the depot and
	 * move one of the empty ones to the depot.
	 *
	 * Obtain the depot spinlock.
	 *
	 * NOTE: Beyond this point, M_* flags are handled via oc->alloc()
	 */
	depot = &oc->depot[myclusterid];
	// XX spin_lock(&depot->spin);

	/*
	 * Recheck the cpucache after obtaining the depot spinlock.  This
	 * shouldn't be necessary now but don't take any chances.
	 */
	if (MAGAZINE_NOTEMPTY(cpucache->loaded_magazine) ||
	    MAGAZINE_NOTEMPTY(cpucache->previous_magazine)
	) {
		// XX spin_unlock(&depot->spin);
		goto retry;
	}

	/* Check if depot has a full magazine. */
	if (!SLIST_EMPTY(&depot->fullmagazines)) {
		emptymag = cpucache->previous_magazine;
		cpucache->previous_magazine = cpucache->loaded_magazine;
		cpucache->loaded_magazine = SLIST_FIRST(&depot->fullmagazines);
		SLIST_REMOVE_HEAD(&depot->fullmagazines, nextmagazine);

		/*
		 * Return emptymag to the depot.
		 */
		// XX KKASSERT(MAGAZINE_EMPTY(emptymag));
		SLIST_INSERT_HEAD(&depot->emptymagazines,
				  emptymag, nextmagazine);
		// XX spin_unlock(&depot->spin);
		goto retry;
	}

	/*
	 * The depot does not have any non-empty magazines.  If we have
	 * not hit our object limit we can allocate a new object using
	 * the back-end allocator.
	 *
	 * note: unallocated_objects can be initialized to -1, which has
	 * the effect of removing any allocation limits.
	 */
	if (depot->unallocated_objects) {
		--depot->unallocated_objects;
		// XX spin_unlock(&depot->spin);
		// XX crit_exit();

		obj = oc->alloc(oc->allocator_args, ocflags);
		if (obj) {
			if (oc->ctor(obj, oc->privdata, ocflags))
				return (obj);
			oc->free(obj, oc->allocator_args);
			obj = NULL;
		}
		if (obj == NULL) {
			// XX spin_lock(&depot->spin);
			++depot->unallocated_objects;
			// XX spin_unlock(&depot->spin);
			if (depot->waiting)
				wakeup(depot);

			// XX crit_enter();
			/*
			 * makes debugging easier when gets_cumulative does
			 * not include gets_null.
			 */
			++cpucache->gets_null;
			--cpucache->gets_cumulative;
			// XX crit_exit();
		}
		return(obj);
	}
	if (oc->exhausted == 0) {
		printf("Warning, objcache(%s): Exhausted!\n", oc->name);
		oc->exhausted = 1;
	}

	/*
	 * Otherwise block if allowed to.
	 */
	if ((ocflags & (M_WAITOK|M_NULLOK)) == M_WAITOK) {
		++cpucache->waiting;
		++depot->waiting;
		// XX ssleep(depot, &depot->spin, 0, "objcache_get", 0);
		--cpucache->waiting;
		--depot->waiting;
		// XX spin_unlock(&depot->spin);
		goto retry;
	}

	/*
	 * Otherwise fail
	 */
	++cpucache->gets_null;
	--cpucache->gets_cumulative;
	//XX crit_exit();
	//XX spin_unlock(&depot->spin);
	return (NULL);
}

/*
 * Wrapper for malloc allocation routines.
 */
void *
objcache_malloc_alloc(void *allocator_args, int ocflags)
{
	struct objcache_malloc_args *alloc_args = allocator_args;

	return (malloc(alloc_args->objsize, M_TEMP /* alloc_args->mtype*/,
		       ocflags & OC_MFLAGS));
}

void
objcache_malloc_free(void *obj, void *allocator_args)
{
	//struct objcache_malloc_args *alloc_args = allocator_args;

	free(obj, M_TEMP /* alloc_args->mtype */);
}

/*
 * Wrapper for allocation policies that pre-allocate at initialization time
 * and don't do run-time allocation.
 */
void *
objcache_nop_alloc(void *allocator_args, int ocflags)
{
	return (NULL);
}

void
objcache_nop_free(void *obj, void *allocator_args)
{
}

/*
 * Return an object to the object cache.
 */
void
objcache_put(struct objcache *oc, void *obj)
{
	struct percpu_objcache *cpucache; // XX cache per cpu  = &oc->cache_percpu[mycpuid];
	struct magazine *loadedmag;
	struct magazinedepot *depot;

	// XX crit_enter();
	++cpucache->puts_cumulative;

	if (CLUSTER_OF(obj) != myclusterid) {
#ifdef notyet
		/* use lazy IPI to send object to owning cluster XXX todo */
		++cpucache->puts_othercluster;
		// XX crit_exit();
		return;
#endif
	}

retry:
	/*
	 * Free slot available in loaded magazine.  This is the hot path.
	 * It is lock-free and uses a critical section to block out interrupt
	 * handlers on the same processor.
	 */
	loadedmag = cpucache->loaded_magazine;
	if (!MAGAZINE_FULL(loadedmag)) {
		loadedmag->objects[loadedmag->rounds++] = obj;
		/* XX if (cpucache->waiting)
			wakeup_mycpu(&oc->depot[myclusterid]);
		crit_exit();
		*/
		return;
	}

	/*
	 * Current magazine full, but previous magazine has room.  XXX
	 */
	if (!MAGAZINE_FULL(cpucache->previous_magazine)) {
		swap(cpucache->loaded_magazine, cpucache->previous_magazine);
		loadedmag = cpucache->loaded_magazine;
		loadedmag->objects[loadedmag->rounds++] = obj;
		/* XX if (cpucache->waiting)
			wakeup_mycpu(&oc->depot[myclusterid]);
		crit_exit();
		*/
		return;
	}

	/*
	 * Both magazines full.  Get an empty magazine from the depot and
	 * move a full loaded magazine to the depot.  Even though the
	 * magazine may wind up with space available after we block on
	 * the spinlock, we still cycle it through to avoid the non-optimal
	 * corner-case.
	 *
	 * Obtain the depot spinlock.
	 */
	depot = &oc->depot[myclusterid];
	// XX spin_lock(&depot->spin);

	/*
	 * If an empty magazine is available in the depot, cycle it
	 * through and retry.
	 */
	if (!SLIST_EMPTY(&depot->emptymagazines)) {
		loadedmag = cpucache->previous_magazine;
		cpucache->previous_magazine = cpucache->loaded_magazine;
		cpucache->loaded_magazine = SLIST_FIRST(&depot->emptymagazines);
		SLIST_REMOVE_HEAD(&depot->emptymagazines, nextmagazine);

		/*
		 * Return loadedmag to the depot.  Due to blocking it may
		 * not be entirely full and could even be empty.
		 */
		if (MAGAZINE_EMPTY(loadedmag)) {
			SLIST_INSERT_HEAD(&depot->emptymagazines,
					  loadedmag, nextmagazine);
			// XX spin_unlock(&depot->spin);
		} else {
			SLIST_INSERT_HEAD(&depot->fullmagazines,
					  loadedmag, nextmagazine);
			// XX spin_unlock(&depot->spin);
			if (depot->waiting)
				wakeup(depot);
		}
		goto retry;
	}

	/*
	 * An empty mag is not available.  This is a corner case which can
	 * occur due to cpus holding partially full magazines.  Do not try
	 * to allocate a mag, just free the object.
	 */
	++depot->unallocated_objects;
	// XX spin_unlock(&depot->spin);
	if (depot->waiting)
		wakeup(depot);
	// XX crit_exit();
	oc->dtor(obj, oc->privdata);
	oc->free(obj, oc->allocator_args);
}

/*
 * The object is being put back into the cache, but the caller has
 * indicated that the object is not in any shape to be reused and should
 * be dtor'd immediately.
 */
void
objcache_dtor(struct objcache *oc, void *obj)
{
	struct magazinedepot *depot;

	depot = &oc->depot[myclusterid];
	// XX spin_lock(&depot->spin);
	++depot->unallocated_objects;
	// XX spin_unlock(&depot->spin);
	if (depot->waiting)
		wakeup(depot);
	oc->dtor(obj, oc->privdata);
	oc->free(obj, oc->allocator_args);
}

/*
 * Deallocate all objects in a magazine and free the magazine if requested.
 * When freeit is TRUE the magazine must already be disassociated from the
 * depot.
 *
 * Must be called with a critical section held when called with a per-cpu
 * magazine.  The magazine may be indirectly modified during the loop.
 *
 * If the magazine moves during a dtor the operation is aborted.  This is
 * only allowed when freeit is FALSE.
 *
 * The number of objects freed is returned.
 */
static int
mag_purge(struct objcache *oc, struct magazine **magp, int freeit)
{
	struct magazine *mag = *magp;
	int count;
	void *obj;

	count = 0;
	while (mag->rounds) {
		obj = mag->objects[--mag->rounds];
		oc->dtor(obj, oc->privdata);		/* MAY BLOCK */
		oc->free(obj, oc->allocator_args);	/* MAY BLOCK */
		++count;

		/*
		 * Cycle for interrupts.
		 */
		if ((count & 15) == 0) {
			// XX crit_exit();
			// XX crit_enter();
		}

		/*
		 * mag may have become invalid either due to dtor/free
		 * blocking or interrupt cycling, do not derefernce it
		 * until we check.
		 */
		if (*magp != mag) {
			printf("mag_purge: mag ripped out\n");
			break;
		}
	}
	if (freeit) {
		// XX KKASSERT(*magp == mag);
		*magp = NULL;
		free(mag, M_TEMP);
	}
	return(count);
}

/*
 * Disassociate zero or more magazines from a magazine list associated with
 * the depot, update the depot, and move the magazines to a temporary
 * list.
 *
 * The caller must check the depot for waiters and wake it up, typically
 * after disposing of the magazines this function loads onto the temporary
 * list.
 */
static void
maglist_disassociate(struct magazinedepot *depot, struct magazinelist *maglist,
		     struct magazinelist *tmplist, int purgeall)
{
	struct magazine *mag;

	while ((mag = SLIST_FIRST(maglist)) != NULL) {
		SLIST_REMOVE_HEAD(maglist, nextmagazine);
		SLIST_INSERT_HEAD(tmplist, mag, nextmagazine);
		depot->unallocated_objects += mag->rounds;
	}
}
			
/*
 * Deallocate all magazines and their contents from the passed temporary
 * list.  The magazines have already been accounted for by their depots.
 *
 * The total number of rounds freed is returned.  This number is typically
 * only used to determine whether a wakeup on the depot is needed or not.
 */
static int
maglist_purge(struct objcache *oc, struct magazinelist *maglist)
{
	struct magazine *mag;
	int count = 0;

	/*
	 * can't use SLIST_FOREACH because blocking releases the depot
	 * spinlock 
	 */
	// XX crit_enter();
	while ((mag = SLIST_FIRST(maglist)) != NULL) {
		SLIST_REMOVE_HEAD(maglist, nextmagazine);
		count += mag_purge(oc, &mag, 1);
	}
	// XX crit_exit();
	return(count);
}

/*
 * De-allocates all magazines on the full and empty magazine lists.
 *
 * Because this routine is called with a spinlock held, the magazines
 * can only be disassociated and moved to a temporary list, not freed.
 *
 * The caller is responsible for freeing the magazines.
 */
static void
depot_disassociate(struct magazinedepot *depot, struct magazinelist *tmplist)
{
	maglist_disassociate(depot, &depot->fullmagazines, tmplist, 1);
	maglist_disassociate(depot, &depot->emptymagazines, tmplist, 1);
}

#ifdef notneeded
void
objcache_reclaim(struct objcache *oc)
{
	struct percpu_objcache *cache_percpu = &oc->cache_percpu[myclusterid];
	struct magazinedepot *depot = &oc->depot[myclusterid];
	struct magazinelist tmplist;
	int count;

	SLIST_INIT(&tmplist);
	crit_enter();
	count = mag_purge(oc, &cache_percpu->loaded_magazine, FALSE);
	count += mag_purge(oc, &cache_percpu->previous_magazine, FALSE);
	crit_exit();

	spin_lock(&depot->spin);
	depot->unallocated_objects += count;
	depot_disassociate(depot, &tmplist);
	spin_unlock(&depot->spin);
	count += maglist_purge(oc, &tmplist);
	if (count && depot->waiting)
		wakeup(depot);
}
#endif

/*
 * Try to free up some memory.  Return as soon as some free memory is found.
 * For each object cache on the reclaim list, first try the current per-cpu
 * cache, then the full magazine depot.
 */
int
objcache_reclaimlist(struct objcache *oclist[], int nlist, int ocflags)
{
	struct objcache *oc;
	struct percpu_objcache *cpucache;
	struct magazinedepot *depot;
	struct magazinelist tmplist;
	int i, count;

	printf("objcache_reclaimlist\n");

	SLIST_INIT(&tmplist);

	for (i = 0; i < nlist; i++) {
		oc = oclist[i];
		// XX cpu cache cpucache = &oc->cache_percpu[mycpuid];
		depot = &oc->depot[myclusterid];

		// XX crit_enter();
		count = mag_purge(oc, &cpucache->loaded_magazine, 0);
		if (count == 0)
			count += mag_purge(oc, &cpucache->previous_magazine, 0);
		// XX crit_exit();
		if (count > 0) {
			// XX spin_lock(&depot->spin);
			depot->unallocated_objects += count;
			// XX spin_unlock(&depot->spin);
			if (depot->waiting)
				wakeup(depot);
			return (1);
		}
		// XX spin_lock(&depot->spin);
		maglist_disassociate(depot, &depot->fullmagazines,
				     &tmplist, 0);
		// XX spin_unlock(&depot->spin);
		count = maglist_purge(oc, &tmplist);
		if (count > 0) {
			if (depot->waiting)
				wakeup(depot);
			return (1);
		}
	}
	return (0);
}

/*
 * Destroy an object cache.  Must have no existing references.
 */
void
objcache_destroy(struct objcache *oc)
{
	struct percpu_objcache *cache_percpu;
	struct magazinedepot *depot;
	int clusterid, cpuid;
	struct magazinelist tmplist;

	// XX spin_lock(&objcachelist_spin);
	LIST_REMOVE(oc, oc_next);
	// XX spin_unlock(&objcachelist_spin);

	SLIST_INIT(&tmplist);
	for (clusterid = 0; clusterid < MAXCLUSTERS; clusterid++) {
		depot = &oc->depot[clusterid];
		// XX spin_lock(&depot->spin);
		depot_disassociate(depot, &tmplist);
		// XX spin_unlock(&depot->spin);
	}
	maglist_purge(oc, &tmplist);

	for (cpuid = 0; cpuid < ncpus; cpuid++) {
		cache_percpu = &oc->cache_percpu[cpuid];

		// XX crit_enter();
		mag_purge(oc, &cache_percpu->loaded_magazine, 1);
		mag_purge(oc, &cache_percpu->previous_magazine, 1);
		// XX crit_exit();
		cache_percpu->loaded_magazine = NULL;
		cache_percpu->previous_magazine = NULL;
		/* don't bother adjusting depot->unallocated_objects */
	}

	free(oc->name, M_TEMP);
	free(oc, M_TEMP);
}

#if 0
/*
 * Populate the per-cluster depot with elements from a linear block
 * of memory.  Must be called for individually for each cluster.
 * Populated depots should not be destroyed.
 */
void
objcache_populate_linear(struct objcache *oc, void *base, int nelts, int size)
{
	char *p = base;
	char *end = (char *)base + (nelts * size);
	struct magazinedepot *depot = &oc->depot[myclusterid];
	struct magazine *emptymag = mag_alloc(depot->magcapcity);

	while (p < end) {
		emptymag->objects[emptymag->rounds++] = p;
		if (MAGAZINE_FULL(emptymag)) {
			spin_lock_wr(&depot->spin);
			SLIST_INSERT_HEAD(&depot->fullmagazines, emptymag,
					  nextmagazine);
			depot->unallocated_objects += emptymag->rounds;
			spin_unlock_wr(&depot->spin);
			if (depot->waiting)
				wakeup(depot);
			emptymag = mag_alloc(depot->magcapacity);
		}
		p += size;
	}
	if (MAGAZINE_EMPTY(emptymag)) {
		crit_enter();
		mag_purge(oc, &emptymag, TRUE);
		crit_exit();
	} else {
		spin_lock_wr(&depot->spin);
		SLIST_INSERT_HEAD(&depot->fullmagazines, emptymag,
				  nextmagazine);
		depot->unallocated_objects += emptymag->rounds;
		spin_unlock_wr(&depot->spin);
		if (depot->waiting)
			wakeup(depot);
		emptymag = mag_alloc(depot->magcapacity);
	}
}
#endif

#if 0
/*
 * Check depot contention once a minute.
 * 2 contested locks per second allowed.
 */
static int objcache_rebalance_period;
static const int objcache_contention_rate = 120;
static struct callout objcache_callout;

#define MAXMAGSIZE 512

/*
 * Check depot contention and increase magazine size if necessary.
 */
static void
objcache_timer(void *dummy)
{
	struct objcache *oc;
	struct magazinedepot *depot;
	struct magazinelist tmplist;

	/* XXX we need to detect when an objcache is destroyed out from under
	    us XXX
	*/

	SLIST_INIT(&tmplist);

	spin_lock_wr(&objcachelist_spin);
	LIST_FOREACH(oc, &allobjcaches, oc_next) {
		depot = &oc->depot[myclusterid];
		if (depot->magcapacity < MAXMAGSIZE) {
			if (depot->contested > objcache_contention_rate) {
				spin_lock_wr(&depot->spin);
				depot_disassociate(depot, &tmplist);
				depot->magcapacity *= 2;
				spin_unlock_wr(&depot->spin);
				kprintf("objcache_timer: increasing cache %s"
				       " magsize to %d, contested %d times\n",
				    oc->name, depot->magcapacity,
				    depot->contested);
			}
			depot->contested = 0;
		}
		spin_unlock_wr(&objcachelist_spin);
		if (maglist_purge(oc, &tmplist) > 0 && depot->waiting)
			wakeup(depot);
		spin_lock_wr(&objcachelist_spin);
	}
	spin_unlock_wr(&objcachelist_spin);

	callout_reset(&objcache_callout, objcache_rebalance_period,
		      objcache_timer, NULL);
}

#endif

/*
static void
objcache_init(void)
{
	spin_init(&objcachelist_spin);

	magazine_capmin = mag_capacity_align(MAGAZINE_CAPACITY_MIN);
	magazine_capmax = mag_capacity_align(MAGAZINE_CAPACITY_MAX);
	if (bootverbose) {
		kprintf("objcache: magazine cap [%d, %d]\n",
		    magazine_capmin, magazine_capmax);
	}

#if 0
	callout_init_mp(&objcache_callout);
	objcache_rebalance_period = 60 * hz;
	callout_reset(&objcache_callout, objcache_rebalance_period,
		      objcache_timer, NULL);
#endif
}
*/
