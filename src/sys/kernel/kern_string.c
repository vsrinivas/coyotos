/*
 * Copyright (C) 2005, Jonathan S. Shapiro.
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
 * @brief Miscellaneous string-related functions.
 *
 * Unfortunately, we cannot use the libc versions of these functions
 * reliably, because the libc versions are implemented and compiled
 * with different runtime assumptions.
 *
 * Note that the kernel is not internationalized. All characters
 * appearing in strings fall within the ISO-LATIN-1 subset.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <kerninc/ctype.h>
#include <kerninc/string.h>

size_t
strlen(const char *s)
{
  size_t len = 0;
  while(*s++)
    len++;

  return len;
}

char *
strcpy(char *dest, const char *src)
{
  char *pdest = dest;
  while (*src)
    *pdest++ = *src++;

  *pdest = 0;

  return dest;
}

int
strcmp(const char *s1, const char *s2)
{
  for ( ; *s1 && *s2; s1++,s2++) {
    if (*s1 < *s2)
      return -1;
    else if (*s1 > *s2)
      return 1;
  }
  
  /* Either s1 or s2 == 0 */
  if (*s2)
    return -1;
  if (*s1)
    return 1;
  return 0;
}

int
memcmp(const void *s1, const void *s2, size_t n)
{
  uint8_t *b1 = (uint8_t *) s1;
  uint8_t *b2 = (uint8_t *) s2;

  for (size_t i = 0; i < n; i++) {
    if ( b1[i] < b2[i] )
      return -1;
    else if (b1[i] > b2[i])
      return 1;
  }

  return 0;
}

void *
memcpy(void *dest, const void *src, size_t n)
{
  unsigned char *cdest = dest;
  const unsigned char *csrc = src;

  while (n--)
    *cdest++ = *csrc++;
  return dest;
}

void *
memset(void *s, int c, size_t n)
{
  char *cs = (char *)s;
  for (size_t i = 0; i < n; i++)
    cs[i] = c;

  return s;
}

int
ffs(int val)
{
  for (size_t i = 0; i < (sizeof(val) * 8); i++) {
    if ((1u << i) & (unsigned int) val)
      return i+1;
  }

  return 0;
}

int
ffsl(long val)
{
  for (size_t i = 0; i < (sizeof(val) * 8); i++) {
    if ((1u << i) & (unsigned long) val)
      return i+1;
  }

  return 0;
}

int
ffsll(long long val)
{
  for (size_t i = 0; i < (sizeof(val) * 8); i++) {
    if ((1llu << i) & (unsigned long) val)
      return i+1;
  }

  return 0;
}

unsigned long 
strtoul(const char *src, const char **endptr, int base)
{
  unsigned long ul = 0;
  bool negate = false;

  while (isspace(*src))
    src++;

  if (*src == '-') negate = true;
  if (*src == '-' || *src == '+') src++;

  // Leading 0x is legal if base is 16
  if (src[0] == '0' && src[1] == 'x') {
    if (base == 0) base = 16;
    if (base != 16) goto done;
    src += 2;
  }
  else if (src[0] == '0') {
    if (base == 0) base = 8;
    if (base != 8) goto done;
    // No need to increment. While loop will handle the zero.
  }

  while (*src) {
    // Note 37 is not a legal base!
    int digit = -1;

    if (isdigit(*src))
      digit = *src - '0';
    else if (isalpha(*src))
      digit = toupper(*src) - 'A' + 10;

    if (digit < 0 || digit >= base)
      break;

    ul *= base;
    ul += digit;
    src++;
  }

  done:
  if(endptr)
    *endptr = src;

  if (negate)
    ul = -ul;

  return ul;
}
