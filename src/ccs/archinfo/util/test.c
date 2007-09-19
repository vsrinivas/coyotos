/*
 * Copyright (C) 2007, The EROS Group, LLC.
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
 *
 * @brief Generator for alignment and offset information for a new
 * target architecture.
 *
 * Procedure: compile this source file into a .S file using the cross
 * compiler. Turn on the optimizer so that the endian assessment stuff
 * will be constant-folded. Examine the .S file to determine the
 * required alignment information.
 *
 * This test file relies on GCC extensions for inline assembler. If
 * your compiler uses a different mechanism for such things, you @em
 * might try to update the various ASM macros for your compiler. In
 * the ASM macros below, the "%0" indicates the point of argument
 * insertion into the asm statement, and the "i" indicates that the
 * correponding argument must be an immediate constant.
 *
 * It is not expected that this will emit well-formed assembly code.
 *
 * On some compilers the output #define's will contain something like:
 *
 *   #define FLOAT_ALIGN $4
 *
 * where the '$' is assembler syntax for a constant integer. The '$'
 * must be stripped to get legal output. Some platforms use other
 * syntax for this, to how to fix up the constant is target dependent.
 *
 * On some targets, notably ARM, the trick used below to determine
 * endian properties will not be something that the compiler's
 * constant folding logic can optimize away (though since this is a
 * middle pass optimization, it is entirely beyond me why this should
 * be so -- the GCC ARM target seems badly borked). In that case the
 * way to find out is to compile emptymain.c into a .o, and then
 * examine byte
 */

#include <stddef.h>

#if defined(__GNUC__)

#define INLINE_ASM(...) __asm__ __volatile__(__VA_ARGS__)

#define ASMDEF(sym) \
  INLINE_ASM("\n#define " #sym)

#define ASM_DEFCONSTANT(sym, c) \
  INLINE_ASM("\n#define " #sym " %0" :: "i" (c))

#define ASM_DEFOFFSET(sym, ty, fld) \
  ASM_DEFCONSTANT(sym, (offsetof(ty, fld) * 8))

#define ASM_DEFSIZEOF(sym, ty) \
  ASM_DEFCONSTANT(sym, (sizeof(ty) * 8))

#else /* __GNUC__ */

#error "Need to define INLINE_ASM macros for your compiler."

#endif /* __GNUC__ */


typedef struct ptr_align {
  char c;
  void *ptr;
} ptr_align;

typedef struct short_align {
  char c;
  short s;
} short_align;

typedef struct int_align {
  char c;
  int i;
} int_align;

typedef struct long_align {
  char c;
  long l;
} long_align;

typedef struct float_align {
  char c;
  float f;
} float_align;

typedef struct double_align {
  char c;
  double d;
} double_align;


typedef struct longlong_align {
  char c;
  long long ll;
} longlong_align;


int main(void)
{
  ASM_DEFSIZEOF(WORD_SIZE, void *);

  if (offsetof(float_align, f) == offsetof(double_align, d))
    ASM_DEFOFFSET(WORST_FLOAT_ALIGN, float_align, f);
  else
    ASM_DEFOFFSET(WORST_FLOAT_ALIGN, double_align, d);

  /* Determine worst integer alignment, excluding long long */

  if (offsetof(short_align, s) == 1) {
    ASM_DEFOFFSET(WORST_INT_ALIGN, short_align, s);
  }
  else if (offsetof(int_align, i) == offsetof(short_align, s)) {
    ASM_DEFOFFSET(WORST_INT_ALIGN, short_align, s);
  }
  else if (offsetof(long_align, l) == offsetof(int_align, i)) {
    ASM_DEFOFFSET(WORST_INT_ALIGN, int_align, i);
  }
  else {
    ASM_DEFOFFSET(WORST_INT_ALIGN, long_align, l);
  }

  ASM_DEFOFFSET(PTR_ALIGN, ptr_align, ptr);

  ASM_DEFOFFSET(LONGLONG_ALIGN, longlong_align, ll);

  /* Following is nice when it works, but compiler cannot always
     constant fold this, so you will sometimes get both outputs. In
     that case, examine the ELF header of the object file emptymain.o
     by hand.

     To find the ELF architecture (which we need):

       od -d2 -j 18 -N 2 emptymain.o

     and match the result against the EM_xxx macros in
     /usr/include/elf.h.

     To find the endian information:

       od -d1 -j 5 -N 1 emptymain.o

     The value "1" means little endian, "2" means big endian.
  */

  const unsigned long ul = 0x04030201llu;

  if ( *((char *) &ul) == 0x1 )
    ASMDEF(LITTLE_ENDIAN);
  else
    ASMDEF(BIG_ENDIAN);
}
