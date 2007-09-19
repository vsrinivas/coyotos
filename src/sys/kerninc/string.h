#ifndef __KERNINC_STRING_H__
#define __KERNINC_STRING_H__
/*
 * Copyright (C) 2005, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief Miscellaneous string-related functions.
 *
 * Unfortunately, we cannot use the libc versions of these functions
 * reliably, because the libc versions are implemented and compiled
 * with different runtime assumptions.
 *
 * Note that the kernel is not internationalized. All characters
 * appearing in strings fall within the ISO-LATIN-1 subset.
 */

#include <stddef.h>

size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
char *strcpy(char *dest, const char *src);
unsigned long strtoul(const char *src, const char **endptr, int base);

#define INIT_TO_ZERO(ob) memset(ob, 0, sizeof(__typeof__(*ob)))

int ffs(int) __attribute__ ((__const__));
int ffsl(long l) __attribute__ ((__const__));
int ffsll(long long l) __attribute__ ((__const__));

#endif /* __KERNINC_STRING_H__ */
