/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2003-2017 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
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
 *	from: @(#)vm_page.h	8.2 (Berkeley) 12/13/93
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Resident memory system definitions.
 */

#ifndef	_VM_VM_PAGE_H_
#define	_VM_VM_PAGE_H_

#ifndef _SYS_TYPES_H_
#include <sys/types.h>
#endif
#ifndef _SYS_TREE_H_
#include <sys/tree.h>
#endif
#ifndef _MACHINE_PMAP_H_
#include <machine/pmap.h>
#endif
#ifndef _VM_PMAP_H_
#include <vm/pmap.h>
#endif
#include <machine/atomic.h>

#ifdef _KERNEL

#ifndef _SYS_SYSTM_H_
#include <sys/systm.h>
#endif
#ifndef _SYS_SPINLOCK_H_
#include <sys/spinlock.h>
#endif
#ifndef _SYS_THREAD2_H_
#include <sys/thread2.h>
#endif

#ifdef __x86_64__
#include <machine/vmparam.h>
#endif

#endif

/*
 * vm_page structure
 */
TAILQ_HEAD(pglist, vm_page);

struct vm_object;

int rb_vm_page_compare(struct vm_page *, struct vm_page *);

struct vm_page_rb_tree;
RB_PROTOTYPE2(vm_page_rb_tree, vm_page, rb_entry,
	      rb_vm_page_compare, vm_pindex_t);

struct vm_page {
	TAILQ_ENTRY(vm_page) pageq;	/* vm_page_queues[] list (P)	*/
	RB_ENTRY(vm_page) rb_entry;	/* Red-Black tree based at object */
	struct spinlock	spin;
	struct vm_object *object;	/* which object am I in (O,P)*/
	vm_pindex_t pindex;		/* offset into object (O,P) */
	vm_paddr_t phys_addr;		/* physical address of page */
	struct md_page md;		/* machine dependant stuff */
	uint16_t queue;			/* page queue index */
	uint16_t pc;			/* page color */
	uint8_t	act_count;		/* page usage count */
	uint8_t	pat_mode;		/* hardware page attribute */
	uint8_t	valid;			/* map of valid DEV_BSIZE chunks */
	uint8_t	dirty;			/* map of dirty DEV_BSIZE chunks */
	uint32_t flags;			/* see below */
	uint32_t wire_count;		/* wired down maps refs (P) */
	uint32_t busy_count;		/* soft-busy and hard-busy */
	int 	hold_count;		/* page hold count */
	int	ku_pagecnt;		/* kmalloc helper */
#ifdef VM_PAGE_DEBUG
	const char *busy_func;
	int	busy_line;
#endif
};

#define PBUSY_LOCKED		0x80000000U
#define PBUSY_WANTED		0x40000000U
#define PBUSY_SWAPINPROG	0x20000000U
#define PBUSY_MASK		0x1FFFFFFFU

#ifndef __VM_PAGE_T_DEFINED__
#define __VM_PAGE_T_DEFINED__
typedef struct vm_page *vm_page_t;
#endif

/*
 * Page coloring parameters.  We use generous parameters designed to
 * statistically spread pages over available cpu cache space.  This has
 * become less important over time as cache associativity is higher
 * in modern times but we still use the core algorithm to help reduce
 * lock contention between cpus.
 *
 * Page coloring cannot be disabled.
 */
#define PQ_PRIME1 31	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME2 23	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_L2_SIZE 512	/* A number of colors opt for 1M cache */
#define PQ_L2_MASK	(PQ_L2_SIZE - 1)

#define PQ_NONE		0
#define PQ_FREE		(1 + 0*PQ_L2_SIZE)
#define PQ_INACTIVE	(1 + 1*PQ_L2_SIZE)
#define PQ_ACTIVE	(1 + 2*PQ_L2_SIZE)
#define PQ_CACHE	(1 + 3*PQ_L2_SIZE)
#define PQ_HOLD		(1 + 4*PQ_L2_SIZE)
#define PQ_COUNT	(1 + 5*PQ_L2_SIZE)

/*
 * Scan support
 */
struct vm_map;

struct rb_vm_page_scan_info {
	vm_pindex_t	start_pindex;
	vm_pindex_t	end_pindex;
	int		limit;
	int		desired;
	int		error;
	int		pagerflags;
	int		count;
	int		unused01;
	vm_offset_t	addr;
	vm_pindex_t	backing_offset_index;
	struct vm_object *object;
	struct vm_object *backing_object;
	struct vm_page	*mpte;
	struct pmap	*pmap;
	struct vm_map	*map;
};

int rb_vm_page_scancmp(struct vm_page *, void *);

struct vpgqueues {
	struct spinlock spin;
	struct pglist pl;
	int	cnt_offset;	/* offset into vmstats structure (int) */
	int	lcnt;
	int	flipflop;	/* probably not the best place */
	int	unused00;
	int	unused01;
	char	unused[64 - sizeof(struct pglist) -
			sizeof(int *) - sizeof(int) * 4];
};

extern struct vpgqueues vm_page_queues[PQ_COUNT];

/*
 * These are the flags defined for vm_page.
 *
 *  PG_UNMANAGED (used by OBJT_PHYS) indicates that the page is
 *  not under PV management but otherwise should be treated as a
 *  normal page.  Pages not under PV management cannot be paged out
 *  via the object/vm_page_t because there is no knowledge of their
 *  pte mappings, nor can they be removed from their objects via 
 *  the object, and such pages are also not on any PQ queue.  The
 *  PG_MAPPED and PG_WRITEABLE flags are not applicable.
 *
 *  PG_MAPPED only applies to managed pages, indicating whether the page
 *  is mapped onto one or more pmaps.  A page might still be mapped to
 *  special pmaps in an unmanaged fashion, for example when mapped into a
 *  buffer cache buffer, without setting PG_MAPPED.
 *
 *  PG_WRITEABLE indicates that there may be a writeable managed pmap entry
 *  somewhere, and that the page can be dirtied by hardware at any time
 *  and may have to be tested for that.  The modified bit in unmanaged
 *  mappings or in the special clean map is not tested.
 *
 *  PG_SWAPPED indicates that the page is backed by a swap block.  Any
 *  VM object type other than OBJT_DEFAULT can have swap-backed pages now.
 */
#define	PG_UNUSED0001	0x00000001
#define	PG_UNUSED0002	0x00000002
#define PG_WINATCFLS	0x00000004	/* flush dirty page on inactive q */
#define	PG_FICTITIOUS	0x00000008	/* physical page doesn't exist (O) */
#define	PG_WRITEABLE	0x00000010	/* page is writeable */
#define PG_MAPPED	0x00000020	/* page is mapped (managed) */
#define	PG_UNUSED0040	0x00000040
#define PG_REFERENCED	0x00000080	/* page has been referenced */
#define PG_CLEANCHK	0x00000100	/* page will be checked for cleaning */
#define PG_UNUSED0200	0x00000200
#define PG_NOSYNC	0x00000400	/* do not collect for syncer */
#define PG_UNMANAGED	0x00000800	/* No PV management for page */
#define PG_MARKER	0x00001000	/* special queue marker page */
#define PG_RAM		0x00002000	/* read ahead mark */
#define PG_SWAPPED	0x00004000	/* backed by swap */
#define PG_NOTMETA	0x00008000	/* do not back with swap */
#define PG_UNUSED10000	0x00010000
#define PG_UNUSED20000	0x00020000
#define PG_NEED_COMMIT	0x00040000	/* clean page requires commit */

#define PG_KEEP_NEWPAGE_MASK	(0)

/*
 * Misc constants.
 */

#define ACT_DECLINE		1
#define ACT_ADVANCE		3
#define ACT_INIT		5
#define ACT_MAX			64

#ifdef VM_PAGE_DEBUG
#define VM_PAGE_DEBUG_EXT(name)	name ## _debug
#define VM_PAGE_DEBUG_ARGS	, const char *func, int lineno
#else
#define VM_PAGE_DEBUG_EXT(name)	name
#define VM_PAGE_DEBUG_ARGS
#endif

#ifdef _KERNEL
/*
 * Each pageable resident page falls into one of four lists:
 *
 *	free
 *		Available for allocation now.
 *
 * The following are all LRU sorted:
 *
 *	cache
 *		Almost available for allocation. Still in an
 *		object, but clean and immediately freeable at
 *		non-interrupt times.
 *
 *	inactive
 *		Low activity, candidates for reclamation.
 *		This is the list of pages that should be
 *		paged out next.
 *
 *	active
 *		Pages that are "active" i.e. they have been
 *		recently referenced.
 *
 *	zero
 *		Pages that are really free and have been pre-zeroed
 *
 */

extern struct vm_page *vm_page_array;	/* First resident page in table */
extern int vm_page_array_size;		/* number of vm_page_t's */
extern long first_page;			/* first physical page number */

#define VM_PAGE_TO_PHYS(entry)	\
		((entry)->phys_addr)

#define PHYS_TO_VM_PAGE(pa)	\
		(&vm_page_array[atop(pa) - first_page])


#if PAGE_SIZE == 4096
#define VM_PAGE_BITS_ALL 0xff
#endif

/*
 * Note: the code will always use nominally free pages from the free list
 * before trying other flag-specified sources. 
 *
 * At least one of VM_ALLOC_NORMAL|VM_ALLOC_SYSTEM|VM_ALLOC_INTERRUPT 
 * must be specified.  VM_ALLOC_RETRY may only be specified if VM_ALLOC_NORMAL
 * is also specified.
 */
#define VM_ALLOC_NORMAL		0x0001	/* ok to use cache pages */
#define VM_ALLOC_SYSTEM		0x0002	/* ok to exhaust most of free list */
#define VM_ALLOC_INTERRUPT	0x0004	/* ok to exhaust entire free list */
#define	VM_ALLOC_ZERO		0x0008	/* req pre-zero'd memory if avail */
#define	VM_ALLOC_QUICK		0x0010	/* like NORMAL but do not use cache */
#define VM_ALLOC_FORCE_ZERO	0x0020	/* zero page even if already valid */
#define VM_ALLOC_NULL_OK	0x0040	/* ok to return NULL on collision */
#define	VM_ALLOC_RETRY		0x0080	/* indefinite block (vm_page_grab()) */
#define VM_ALLOC_USE_GD		0x0100	/* use per-gd cache */
#define VM_ALLOC_CPU_SPEC	0x0200

#define VM_ALLOC_CPU_SHIFT	16
#define VM_ALLOC_CPU(n)		(((n) << VM_ALLOC_CPU_SHIFT) | \
				 VM_ALLOC_CPU_SPEC)
#define VM_ALLOC_GETCPU(flags)	((flags) >> VM_ALLOC_CPU_SHIFT)

void vm_page_queue_spin_lock(vm_page_t);
void vm_page_queues_spin_lock(u_short);
void vm_page_and_queue_spin_lock(vm_page_t);

void vm_page_queue_spin_unlock(vm_page_t);
void vm_page_queues_spin_unlock(u_short);
void vm_page_and_queue_spin_unlock(vm_page_t m);

void vm_page_init(vm_page_t m);
void vm_page_io_finish(vm_page_t m);
void vm_page_io_start(vm_page_t m);
void vm_page_need_commit(vm_page_t m);
void vm_page_clear_commit(vm_page_t m);
void vm_page_wakeup(vm_page_t m);
void vm_page_hold(vm_page_t);
void vm_page_unhold(vm_page_t);
void vm_page_activate (vm_page_t);

vm_size_t vm_contig_avail_pages(void);
vm_page_t vm_page_alloc (struct vm_object *, vm_pindex_t, int);
vm_page_t vm_page_alloc_contig(vm_paddr_t low, vm_paddr_t high,
                     unsigned long alignment, unsigned long boundary,
		     unsigned long size, vm_memattr_t memattr);

vm_page_t vm_page_grab (struct vm_object *, vm_pindex_t, int);
void vm_page_cache (vm_page_t);
int vm_page_try_to_cache (vm_page_t);
int vm_page_try_to_free (vm_page_t);
void vm_page_dontneed (vm_page_t);
void vm_page_deactivate (vm_page_t);
void vm_page_deactivate_locked (vm_page_t);
void vm_page_initfake(vm_page_t m, vm_paddr_t paddr, vm_memattr_t memattr);
int vm_page_insert (vm_page_t, struct vm_object *, vm_pindex_t);
vm_page_t vm_page_lookup (struct vm_object *, vm_pindex_t);
vm_page_t vm_page_lookup_sbusy_try(struct vm_object *object,
		vm_pindex_t pindex);
vm_page_t VM_PAGE_DEBUG_EXT(vm_page_lookup_busy_wait)(
		struct vm_object *, vm_pindex_t, int, const char *
		VM_PAGE_DEBUG_ARGS);
vm_page_t VM_PAGE_DEBUG_EXT(vm_page_lookup_busy_try)(
		struct vm_object *, vm_pindex_t, int, int *
		VM_PAGE_DEBUG_ARGS);
void vm_page_remove (vm_page_t);
void vm_page_rename (vm_page_t, struct vm_object *, vm_pindex_t);
void vm_page_startup (void);
void vm_numa_organize(vm_paddr_t ran_beg, vm_paddr_t bytes, int physid);
void vm_page_unmanage (vm_page_t);
void vm_page_unwire (vm_page_t, int);
void vm_page_wire (vm_page_t);
void vm_page_unqueue (vm_page_t);
void vm_page_unqueue_nowakeup (vm_page_t);
vm_page_t vm_page_next (vm_page_t);
void vm_page_set_validclean (vm_page_t, int, int);
void vm_page_set_validdirty (vm_page_t, int, int);
void vm_page_set_valid (vm_page_t, int, int);
void vm_page_set_dirty (vm_page_t, int, int);
void vm_page_clear_dirty (vm_page_t, int, int);
void vm_page_set_invalid (vm_page_t, int, int);
int vm_page_is_valid (vm_page_t, int, int);
void vm_page_test_dirty (vm_page_t);
int vm_page_bits (int, int);
vm_page_t vm_page_list_find(int basequeue, int index);
void vm_page_zero_invalid(vm_page_t m, boolean_t setvalid);
void vm_page_free_toq(vm_page_t m);
void vm_page_free_contig(vm_page_t m, unsigned long size);
vm_page_t vm_page_free_fromq_fast(void);
void vm_page_dirty(vm_page_t m);
void vm_page_sleep_busy(vm_page_t m, int also_m_busy, const char *msg);
int vm_page_sbusy_try(vm_page_t m);
void VM_PAGE_DEBUG_EXT(vm_page_busy_wait)(vm_page_t m,
			int also_m_busy, const char *wmsg VM_PAGE_DEBUG_ARGS);
int VM_PAGE_DEBUG_EXT(vm_page_busy_try)(vm_page_t m,
			int also_m_busy VM_PAGE_DEBUG_ARGS);
u_short vm_get_pg_color(int cpuid, vm_object_t object, vm_pindex_t pindex);

#ifdef VM_PAGE_DEBUG

#define vm_page_lookup_busy_wait(object, pindex, alsob, msg)		\
	vm_page_lookup_busy_wait_debug(object, pindex, alsob, msg,	\
					__func__, __LINE__)

#define vm_page_lookup_busy_try(object, pindex, alsob, errorp)		\
	vm_page_lookup_busy_try_debug(object, pindex, alsob, errorp,	\
					__func__, __LINE__)

#define vm_page_busy_wait(m, alsob, msg)				\
	vm_page_busy_wait_debug(m, alsob, msg, __func__, __LINE__)

#define vm_page_busy_try(m, alsob)					\
	vm_page_busy_try_debug(m, alsob, __func__, __LINE__)

#endif

#endif				/* _KERNEL */
#endif				/* !_VM_VM_PAGE_H_ */
