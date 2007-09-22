/*
 * Copyright (C) 2005, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief Shellsort implementation
 *
 * Shellsort is like insertion sort, except that we do insertion sort in a
 * number of passes.  At each pass, we k-sort the file, which means that
 * for all N, element N is sorted with respect to elements N + ak, where a is
 * any integer yielding a result in range.  k starts large, and decreases.
 * This means that disordered elements are moved towards their eventual spot
 * early on, so that the later passes have to do less and less work.
 *
 * See Knuth, Vol 3, section 5.2.1, for a discussion of the algorithm.
 *
 * This algorithm is preferable to the standard Quicksort, since it works
 * without recursion.
 */

#include <kerninc/shellsort.h>
#include <kerninc/string.h>

/** @brief sort array @p a, of @p n elements of size @p es, 
 * using the comparison function @p cmp.
 */
void
shellsort(void *a, size_t n, size_t es, int (*cmp)(const void *, const void *))
{
  char buffer[es];	// Temporary storage used for the insertion.

  char *array = a;
  size_t stride;

  // Calculate the initial stride (Knuth's "simple" increment sequence)
  for (stride = 1;
       stride <= n/9;
       stride = 3 * stride + 1)
    ;

  for (; stride > 0; stride /= 3) {
    size_t idx;

    for (idx = stride; idx < n; idx++) {
      char *entry = &array[es * idx];

      /* find the insertion point */
      size_t nidx;
      for (nidx = idx;
	   nidx >= stride;
	   nidx -= stride) {
	char *cur = &array[es * (nidx - stride)];
	if (cmp(entry, cur) >= 0)
	  break;
      }

      /* Do the insertion */
      if (nidx != idx) {
	size_t curidx;
	memcpy(buffer, entry, es);
	for (curidx = idx; curidx > nidx; curidx -= stride)
	  memcpy(&array[es * curidx], &array[es * (curidx - stride)], es);
	memcpy(&array[es * nidx], buffer, es);
      }
    }
  }
}
