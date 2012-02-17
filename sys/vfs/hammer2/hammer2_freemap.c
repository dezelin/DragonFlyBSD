/*
 * Copyright (c) 2011-2012 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 * by Venkatesh Srinivas <vsrinivas@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
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
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/mountctl.h>

#include "hammer2.h"

int
hammer2_freemap_bytes_to_radix(size_t bytes)
{
	int radix;

	if (bytes < HAMMER2_MIN_ALLOC)
		bytes = HAMMER2_MIN_ALLOC;
	if (bytes == HAMMER2_PBUFSIZE)
		radix = HAMMER2_PBUFRADIX;
	else if (bytes >= 1024)
		radix = 10;
	else
		radix = HAMMER2_MIN_RADIX;

	while (((size_t)1 << radix) < bytes)
		++radix;
	return (radix);
}

/*
 * Allocate media space, returning a combined data offset and radix.
 *
 * XXX when diving a new full block create a clean empty buffer and bqrelse()
 *     it, so small data structures do not have to issue read-IO when they
 *     do the read-modify-write on the backing store.
 */
hammer2_off_t
hammer2_freemap_alloc(hammer2_mount_t *hmp, size_t bytes)
{
	hammer2_off_t data_off;
	hammer2_off_t data_next;
	int radix;

	/*
	 * Figure out the base 2 radix of the allocation (rounded up)
	 */
	radix = hammer2_freemap_bytes_to_radix(bytes);
	bytes = 1 << radix;

	if (radix < HAMMER2_MAX_RADIX && hmp->freecache[radix]) {
		/*
		 * Allocate from our packing cache
		 */
		data_off = hmp->freecache[radix];
		hmp->freecache[radix] += bytes;
		if ((hmp->freecache[radix] & HAMMER2_PBUFMASK) == 0)
			hmp->freecache[radix] = 0;
	} else {
		/*
		 * Allocate from the allocation iterator using a PBUFSIZE
		 * aligned block and reload the packing cache if possible.
		 */
		data_off = hmp->voldata.allocator_beg;
		data_off = (data_off + HAMMER2_PBUFMASK64) &
			   ~HAMMER2_PBUFMASK64;
		data_next = data_off + bytes;

		if ((data_next & HAMMER2_PBUFMASK) == 0) {
			hmp->voldata.allocator_beg = data_next;
		} else {
			KKASSERT(radix < HAMMER2_MAX_RADIX);
			hmp->voldata.allocator_beg =
					(data_next + HAMMER2_PBUFMASK64) &
					~HAMMER2_PBUFMASK64;
			hmp->freecache[radix] = data_next;
		}
	}
	kprintf("hammer2: allocate %016jx: %zd\n", (intmax_t)data_off, bytes);
	return (data_off | radix);
}

#if 0
/*
 * Allocate media space, returning a combined data offset and radix.
 * Also return the related (device) buffer cache buffer.
 */
hammer2_off_t
hammer2_freemap_alloc_bp(hammer2_mount_t *hmp, size_t bytes, struct buf **bpp)
{
}

#endif
