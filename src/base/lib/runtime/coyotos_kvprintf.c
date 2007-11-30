/*
 * Copyright (C) 2007, The EROS Group, LLC.
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

#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <coyotos/kprintf.h>
#include <coyotos/ascii.h>
#include <idl/coyotos/KernLog.h>

typedef void (*putc_fn)(char c, void *vp);

/** @brief The actual printf workhorse */
static void 
printf_putc_logbuf(char c, void *vp)
{
  coyotos_KernLog_logString *ls  = vp;
  if (ls->len < ls->max)
    ls->data[ls->len++] = c;
}

/** @brief Internal implementation of printf. */
static void 
printf_guts(putc_fn printf_putc, void * putc_vp,
	    register const char *fmt, va_list ap)
{
#define PUT_DIGIT(d)				\
  do {						\
    *bufp = '0'+(d);				\
    if (d >= 10) *bufp = (cnv + ((d)-10));	\
    bufp++;					\
  } while(false)

  while (*fmt) {
    if (*fmt != '%') {
      if (*fmt == ASCII_LF)
	printf_putc(ASCII_CR, putc_vp);
      printf_putc(*fmt++, putc_vp);
      continue;
    }

    /* *fmt is '%', so we are looking at an argument specification. */
    fmt++;

    {
      char buf[40];		/* long enough for anything */
      char *bufp = buf;
      char *sptr = 0;
      size_t slen = 0;

      bool needSign = false;	/* true if we need space for a sign */
      bool isNegative = false;
      bool ladjust = false;
      char posSign = ' ';
      bool haveWidth = false;
      unsigned width = 0;
      unsigned prec = 0;
      unsigned base = 10;
      char len = ' ';
      char padc = ' ';
      char cnv = ' ';

      /* Parse the flag characters */
      for(;;) {
	switch (*fmt) {
	case ' ': /* Initial sign/space? */
	  {
	    needSign = true;
	    fmt++;
	    continue;
	  }

	case '+': /* Initial +/- mandatory */
	  {
	    needSign = true;
	    posSign = '+';
	    fmt++;
	    continue;
	  }

	case '-': /* Left adjust */
	  {
	    ladjust = true;
	    fmt++;
	    continue;
	  }

	case '0': /* Leading zero? */
	  {
	    padc = '0';
	    fmt++;
	    continue;
	  }

	  /* Following are ignored: */
	case '#': /* Alternate output format */
	case '\'': /* SUSv2 Group by thousands according to locale */
	case 'I': /* GLIBC use locale-specific digits */
	  fmt++;
	  continue;

	default:
	  break;
	}

	break;			/* proceed to field width */
      }

      /* Field width */
      while (isdigit(*fmt)) {
	haveWidth = true;
	width *= 10;
	width += (*fmt - '0');
	fmt++;
      }

      /* Precision */
      if (*fmt == '.') {
	fmt++;
	while (isdigit(*fmt)) {
	  prec *= 10;
	  prec += (*fmt - '0');
	  fmt++;
	}
      }

      /* Length modifier */
      switch (len = *fmt) {
      case 'h':			/* short, char */
	{
	  fmt++;

	  if (*fmt == 'h') {
	    fmt++;
	    len = 'c';
	  }
	  break;
	}
      case 'l':			/* long, long long */
	{
	  fmt++;
	  if (*fmt == 'l') {
	    fmt++;
	    len = 'q';
	  }
	  break;
	}
#if 0
      case 'L':			/* long double */
	{
	  fmt++;
	  len = sizeof(long double);
	  break;
	}
#endif
	// 'q' legacy BSD 4.4 not supported
      case 'j':			/* intmax_t */
	{
	  fmt++;
	  len = sizeof(intmax_t);
	  break;
	}
      case 'z':			/* size_t */
	{
	  fmt++;
	  len = sizeof(size_t);
	  break;
	}
      case 't':			/* ptrdiff_t */
	{
	  fmt++;
	  len = sizeof(ptrdiff_t);
	  break;
	}
      }

      /* Conversion specifier */
      switch (cnv = *fmt++) {
	/* FIRST ALL THE SIGNED INTEGER CASES */
      case 'o':			/* octal */
	{
	  base = 8;
	  /* FALL THROUGH */
	}
      case 'd':			/* signed int */
      case 'i':
	{
	  long long ll;

	  switch (len) {
	  case 'q':
	    ll = va_arg(ap, long long);
	    break;
	  case 'j':
	    ll = va_arg(ap, intmax_t);
	    break;
	  case 'z':
	    ll = va_arg(ap, size_t);
	    break;
	  case 't':
	    ll = va_arg(ap, ptrdiff_t);
	    break;
	  default:
	    ll = va_arg(ap, long); /* handles char, short too */
	    break;
	  }

	  if (ll < 0ll) {
	    isNegative = true;
	    needSign = true;
	    ll = -ll;
	  }

	  if (ll == 0ll)
	    PUT_DIGIT(0);

	  while (ll) {
	    int digit = ll % base;
	    ll = ll / base;
	    PUT_DIGIT(digit);
	  }
	  break;
	}
      case 'p':			/* pointer */
	{
	  /* Hack: absent a user-specified width, print pointers at
	     their natural width for the native word size. This allows
	     us to write 0x%0p and get naturally-sized pointers. */
	  if ((haveWidth == false) && (padc == '0'))
	    width = sizeof(void *) * 2;

	  cnv = 'x';
	  if (len == ' ')
	    len = 'p';

	  /* FALL THROUGH */
	}
      case 'x':			/* unsigned hexadecimal, lowercase */
      case 'X':			/* unsigned hexadecimal, uppercase */
	{
	  base = 16;
	  cnv = cnv - ('x' - 'a');
	  /* FALL THROUGH */
	}
      case 'u':			/* unsigned int */
	{
	  unsigned long long ull;

	  switch (len) {
	  case 'q':
	    ull = va_arg(ap, unsigned long long);
	    break;
	  case 'j':
	    ull = va_arg(ap, intmax_t);
	    break;
	  case 'z':
	    ull = va_arg(ap, size_t);
	    break;
	  case 't':
	    ull = va_arg(ap, ptrdiff_t);
	    break;
	  case 'p':
	    ull = (uintptr_t) va_arg(ap, void *);
	    break;
	  default:
	    ull = va_arg(ap, unsigned long); /* handles char, short too */
	    break;
	  }

	  if (ull == 0llu)
	    PUT_DIGIT(0);

	  while (ull) {
	    unsigned int digit = ull % base;
	    ull = ull / base;
	    PUT_DIGIT(digit);
	  }
	  break;
	}
      case 'c':			/* character, long character */
	// SUSv2 'C' not supported 
	{
	  char c = va_arg(ap, int);
	  *bufp++ = c;
	  break;
	}
      case 's':			/* string, long string */
	// SUSv2 'S' not supported 
	{
	  sptr = va_arg(ap, char *);
	  slen = strlen(sptr);
	  break;
	}
	// 'n' not supported
      case '%':
	{
	  // FIX: What is supposed to be done about field widths?
	  printf_putc('%', putc_vp);
	  continue;
	}
#if 0
	/* Floating point formats not supported */
      case 'e':			/* double */
      case 'E':			/* double */
      case 'f':			/* double */
      case 'F':			/* double */
      case 'g':			/* double */
      case 'G':			/* double */
      case 'a':			/* double */
      case 'A':			/* double */
	{
	  break;
	}
#endif
      }

      if (bufp != buf)
	slen = bufp - buf;

      // FIX: I am not sure if the sign should be considered part of
      // the field for field width purposes. I think that it is, but
      // this complicates the padding algorithm in the right-adjust
      // case:
      //
      //     Sign in field     Sign not in field
      //     xxxxxxxxxx         xxxxxxxxxxxx
      //     -        3        -           3
      //     -000000003        -000000000003
      //
      // My resolution here is to reduce the field width by one if a
      // sign is required, and then treat the sign as appearing
      // outside the field.
      
      if (needSign && width)
	width--;

      if (needSign) {
	printf_putc(isNegative ? '-' : posSign, putc_vp);
      }

      // If we are right adjusting, insert needed pad characters: */
      // FIX: Doesn't zero padding *require* left adjust logic?
      if (ladjust == false) {
	while (slen < width) {
	  printf_putc(padc, putc_vp);
	  slen++;
	}
      }

      if (bufp != buf) {
	do {
	  --bufp;
	  printf_putc(*bufp, putc_vp);
	} while (bufp != buf);
      }
      if (sptr)
	while(*sptr)
	  printf_putc(*sptr++, putc_vp);
      
      /* If we are left adjusting, pad things out to end of field */
      if (ladjust) {
	while (slen < width) {
	  printf_putc(padc, putc_vp);
	  slen++;
	}
      }
    }
  }
}

bool
IDL_ENV_kvprintf(IDL_Environment *_env, caploc_t cap, const char* fmt, 
		 va_list listp)
{
  char buf[128];
  coyotos_KernLog_logString ls  = { 128, 0, buf };

  printf_guts(printf_putc_logbuf, &ls, fmt, listp);

  bool result = IDL_ENV_coyotos_KernLog_log(_env, cap, ls);

  return result;
}
