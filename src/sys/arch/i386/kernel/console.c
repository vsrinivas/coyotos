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
 * @brief Console output implementation.
 *
 * For interface documentation, see kerninc/console.h.
 *
 * This is a (poor) implementation of a text-mode console for
 * diagnostic output. It is PC motherboard specific, and assumes that
 * the display has been left in VGA mode by the bootstrap logic.
 */

#include <stdbool.h>
#include <hal/kerntypes.h>
#include <kerninc/assert.h>
#include <kerninc/ctype.h>
#include <kerninc/printf.h>
#include <coyotos/ascii.h>
#include <coyotos/i386/io.h>
#include "kva.h"

/* The standard PC console has 25 rows. Reserve the last one for use
   as LEDs so that we can do various sorts of progress tracing. */
#ifdef BRING_UP
#define SCREEN_ROWS 23
#else
#define SCREEN_ROWS 25
#endif
#define REAL_SCREEN_ROWS 25

static uint16_t *const screen = (uint16_t *) PTOKV(0xb8000, uint16_t *);
static const uint32_t screen_rows = SCREEN_ROWS;
static const uint32_t screen_cols = 80;
static bool console_live = true;
static uint32_t StartAddressRegister;
static uint32_t offset;
static const unsigned TABSTOP = 8;

enum VGAColors {
  Black = 0,
  Blue = 1,
  Green = 2,
  Cyan = 3,
  Red = 4,
  Magenta = 5,
  Brown = 6,
  White = 7,
  Gray = 8,
  LightBlue = 9,
  LightGreen = 10,
  LightCyan = 11,
  LightRed = 12,
  LightMagenta = 13,
  LightBrown = 14,	/* yellow */
  BrightWhite = 15,

  /* Combinations used by the console driver: */
  WhiteOnBlack = 0x7,
  blank = 0,
};

inline static void set_position(uint32_t pos, char c)
{
  screen[pos] = ((WhiteOnBlack) << 8) | c;
}

static void clear(uint32_t screen_rows)
{
  for (uint32_t pos = 0; pos < (screen_rows * screen_cols); pos++)
    set_position(pos, ' ');

}

static void
Scroll(uint32_t startPos, uint32_t endPos, int amount)
{
  uint32_t p = 0;

  if (amount > 0) {
    uint32_t gap = amount;
    for (p = startPos + gap; p < endPos; p++)
      screen[p] = screen[p - gap];

    for (p = startPos; p < startPos + gap; p++)
      screen[p] = (WhiteOnBlack << 8);
  }
  else {
    uint32_t gap = -amount;

    for (p = startPos; p < endPos - gap; p++)
      screen[p] = screen[p + gap];

    for (p = endPos - gap; p < endPos; p++)
      screen[p] = (WhiteOnBlack << 8);
  }
}

static void beep() {}

static void
ShowCursorAt(uint32_t pos)
{
  uint32_t cursAddr = (uint32_t) pos;

  cursAddr += StartAddressRegister;

  outb(0xE, 0x3D4);
  outb((cursAddr >> 8) & 0xFFu, 0x3D5);
  outb(0xF, 0x3D4);
  outb((cursAddr & 0xFFu), 0x3D5);
}

extern void console_putc(char c)
{
  if (!console_live)
    return;

  uint32_t posCol = offset % screen_cols;

  if (c == ASCII_CR)
    if (offset % screen_cols)
      Scroll(offset, offset + (screen_cols - posCol), screen_cols - posCol);

  if (isprint(c)) {
    set_position(offset, c);
    offset++;
  }
  else {
    /* Handle the non-printing characters: */
    switch(c) {
    case ASCII_BEL:
      beep();
      break;
    case ASCII_BS:		/* backspace is NONDESTRUCTIVE */
      if ( offset % screen_cols )
	offset--;
      break;
    case ASCII_TAB:		/* NONDESTRUCTIVE until we know how */
      while (offset % TABSTOP) {
	set_position(offset, ' ');
	offset++;
      }
      break;
    case ASCII_LF:
      offset += screen_cols;
      break;
    case ASCII_VT:		/* reverse line feed */
      if (offset > screen_cols)
	offset -= screen_cols;
      break;
#if 0
    case ASCII::FF:		/* reverse line feed */
      offset = 0;
      ClearScreen();
      break;
#endif
    case ASCII_CR:
      offset -= (offset % screen_cols);
      break;
    }
  }
    
  if (offset >= screen_rows * screen_cols) {
    Scroll(0, screen_rows * screen_cols, - (int) screen_cols);
    offset -= screen_cols;
  }

  assert (offset < screen_rows * screen_cols);
  ShowCursorAt(offset);
}

/** @brief Stop using the console subsystem.
 *
 * Advises the console subsystem that we are about to start the
 * first user mode instruction, and that further attempts to put
 * output on the console should be ignored.
 */
extern void console_detach()
{
  console_live = false;
}

void console_init()
{
  uint8_t hi, lo;

  outb(0xc, 0x3d4);		/* get hi start address register */
  hi = inb(0x3d5);

  outb(0xd, 0x3d4);		/* get low start address register */
  lo = inb(0x3d5);

  StartAddressRegister = ((uint32_t) hi) << 8 | lo;
  offset = 0;

  clear(REAL_SCREEN_ROWS);

  printf("Coyotos Rules!\n");
}
