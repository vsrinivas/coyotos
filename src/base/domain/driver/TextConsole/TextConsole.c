/*
 * Copyright (C) 2007, The EROS Group, LLC
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
 * @brief Basic text console implementation.
 *
 * This is a very basic text display driver implementing a
 * text-oriented display providing minimal color
 * capabilities. Conceptually, it implements the display component of
 * an ASCII terminal, including the display-oriented ANSI terminal
 * escape codes. It does @em not implement the ANSI identify or
 * writeback sequences, mainly because this is an output-only
 * component.
 *
 * This code was thrown together to exercise a PC display running in
 * MDA mode (video mode 25). The main PC dependencies are assumptions
 * about display size, display memory base address and data format,
 * and cursor update handling.  The interface between this and the
 * ANSI terminal support logic needs to be cleaned up so that the ANSI
 * support can be broken out separately.
 */

#include <coyotos/capidl.h>
#include <coyotos/syscall.h>
#include <coyotos/runtime.h>
#include <coyotos/reply_Constructor.h>
#include <coyotos/i386/io.h>

#include <idl/coyotos/SpaceBank.h>
#include <idl/coyotos/GPT.h>
#include <idl/coyotos/Process.h>

/* all of our handler procedures are static */
#define IDL_SERVER_HANDLER_PREDECL static

#include <coyotos.driver.TextConsole.h>
#include <idl/coyotos/driver/TextConsole.server.h>

extern void kprintf(caploc_t cap, const char* fmt, ...);

#define CR_MYSPACE    		coyotos_driver_TextConsole_APP_MYSPACE
#define CR_PHYSRANGE   		coyotos_driver_TextConsole_APP_PHYSRANGE
#define CR_LOG   		coyotos_driver_TextConsole_APP_LOG
#define CR_TMP1   		coyotos_driver_TextConsole_APP_TMP1

typedef union {
  _IDL_IFUNION_coyotos_driver_TextConsole
      coyotos_driver_TextConsole;
  InvParameterBlock_t pb;
  InvExceptionParameterBlock_t except;
  uintptr_t icw;
} _IDL_GRAND_SERVER_UNION;

/* You should supply a function that selects an interface
 * type based on the incoming endpoint ID and protected
 * payload */
static inline uint64_t
choose_if(uint64_t epID, uint32_t pp)
{
  return IKT_coyotos_driver_TextConsole;
}

typedef struct IDL_SERVER_Environment {
  uint32_t pp;
  uint64_t epID;
} ISE;


enum ConsState {
  WaitChar,
  GotEsc,
  WaitParam,
};

/* A few colors commonly used */
#define WHITE 0x700u
#define GREEN 0x200u
#define RED   0x400u

#define CODE_BS  0x08
#define CODE_TAB '\t'
#define CODE_CR  '\r'
#define CODE_LF  '\n'
#define CODE_ESC 0x1B

#define ROWS 20
#define COLS 80 

#define SCREEN_SIZE (ROWS * COLS)

const char *message =
  "\n"
  "\n"
  "  ACHTUNG!\n"
  "\n"
  "  Das machine is nicht fur der fingerpoken und mittengrabben.\n"
  "  Is easy schnappen der springenwerk, blowenfusen und popencorken\n"
  "  mit spitzen sparken. Das machine is diggen by experten only.\n"
  "  Is nicht fur gerwerken by das dummkopfen. Das rubbernecken\n"
  "  sightseeren keepen das cottenpiken hands in das pockets.\n"
  "\n"
  "  Relaxen und watchen das blinkenlights.\n"
  "\n";


#if 0
static int putCharAtPos(unsigned int pos, unsigned char ch);
static void processChar(uint8_t ch);
static void processEsc(uint8_t ch);
static void processInput(uint8_t ch);
static void putCursAt(int psn);
static void scroll(uint32_t spos, uint32_t epos, int amt);
static int putColCharAtPos(unsigned int pos, uint8_t ch, uint8_t color);
#endif

static volatile uint16_t *screen = (uint16_t *) 0xb8000;
static uint32_t startAddrReg = 0;
static uint8_t state = WaitChar;
static uint8_t param[10];
static unsigned int npar = 0;
static int pos = 0;

static int
IsPrint(uint8_t c)
{
  const uint8_t isPrint[16] = {	/* really a bitmask! */
    0x00,			/* BEL ACK ENQ EOT ETX STX SOH NUL */
    0x00,			/* SI  SO  CR  FF  VT  LF  TAB BS */
    0x00,			/* ETB SYN NAK DC4 DC3 DC2 DC1 DLE */
    0x00,			/* US  RS  GS  FS  ESC SUB EM  CAN */
    0xff,			/* '   &   %   $   #   "   !   SPC */
    0xff,			/* /   .   -   ,   +   *   )   ( */
    0xff,			/* 7   6   5   4   3   2   1   0 */
    0xff,			/* ?   >   =   <   ;   :   9   8 */
    0xff,			/* G   F   E   D   C   B   A   @ */
    0xff,			/* O   N   M   L   K   J   I   H */
    0xff,			/* W   V   U   T   S   R   Q   P */
    0xff,			/* _   ^   ]   \   [   Z   Y   X */
    0xff,			/* g   f   e   d   c   b   a   ` */
    0xff,			/* o   n   m   l   k   j   i   h */
    0xff,			/* w   v   u   t   s   r   q   p */
    0x7f,			/* DEL ~   }   |   {   z   y   x */
  };

  uint8_t mask;

  if (c > 127)
    return 0;
 
  mask = isPrint[(c >> 3)];
  c &= 0x7u;
  if (c)
    mask >>= c;
  if (mask & 1)
    return 1;
  return 0;
}

static void
putCursAt(int psn)
{

  uint32_t cursAddr = (uint32_t) psn;

  cursAddr += startAddrReg;

  outb(0xE, 0x3D4);
  outb((cursAddr >> 8) & 0xFFu, 0x3D5);
  outb(0xF, 0x3D4);
  outb((cursAddr & 0xFFu), 0x3D5);
  return;
}

static void
clearScreen(void)
{
  int i;
  for (i = 0; i < SCREEN_SIZE; i++)
    screen[i] = 0x700u;		/* must reset mode attributes! */

  pos = 0;
  putCursAt(pos);
  return;
}

static int
putCharAtPos(unsigned int pos, uint8_t ch) 
{
  if ((pos < 0) || (pos >= SCREEN_SIZE)) {
    /* Invalid range specified.  Return 1 */
    return 1;
  }

  screen[pos] = ((uint16_t) ch) | WHITE;

  return 0;
}

void
scroll(uint32_t spos, uint32_t epos, int amt)
{

#if 0
  uint16_t oldscreen[SCREEN_SIZE];
  uint32_t gap, p;

  for (p = 0; p < SCREEN_SIZE; p++) {
    oldscreen[p] = screen[p];
  }
  
  if (amt > 0) {
    gap = amt;
    for (p = spos + gap; p < epos; p++) {
      screen[p] = oldscreen[p - gap];
    }

    for (p = spos; p < spos + gap; p++) {
      screen[p] = (0x7 << 8);
    }

  }
  else {
    gap = -amt;
    for (p = spos; p < epos - (gap - 1); p++) {
      screen[p] = oldscreen[p + gap];
    }

    for (p = epos - gap; p < epos; p++) {
      screen[p] = (0x7 << 8);
    }
  }
#endif
  
  return;
}

static void
processEsc(uint8_t ch)
{
  uint32_t p;
  uint32_t posRow = pos / COLS;
  uint32_t posCol = pos % COLS;

  if (ch != 'J' && ch != 'K') {
    for (p = 0; p < 10; p++) {
      if (param[p] == 0) {
	param[p] = 1;
      }
    }
  }

  switch (ch) {
  case '@':
    {
      int distance;
      if (param[0] < (COLS - posCol))
	distance = param[0];
      else
	distance = COLS - posCol;

      scroll (pos, pos + (COLS - posCol), distance);
      break;
    }
  case 'A':
    {
      uint32_t count;
      if (param[0] < posRow)
	count = param[0];
      else
	count = posRow;

      pos -= (COLS * count);
      putCursAt(pos);
      break;
    }
  case 'B':
    {
      uint32_t count;
      if (param[0] < ROWS - posRow - 1)
	count = param[0];
      else
	count = ROWS - posRow - 1;

      pos += (COLS * count);
      putCursAt(pos);
      break;
    }
  case 'C':
    {
      uint32_t count;
      if (param[0] < COLS - posCol - 1)
	count = param[0];
      else
	count = COLS - posCol - 1;

      pos += count;
      putCursAt(pos);
      break;
    }
  case 'D':
    {
      uint32_t count;
      if (param[0] < posCol)
	count = param[0];
      else
	count = posCol;

      pos -= count;
      putCursAt(pos);
      break;
    }
  case 'f':
  case 'H':
    {
      uint32_t therow, thecol;

      if (param[0] < ROWS)
	therow = param[0];
      else
	therow = ROWS;
      if (param[1] < COLS)
	thecol = param[1];
      else
	thecol = COLS;

      therow--;
      thecol--;

      pos = (therow * COLS) + thecol;
      putCursAt(pos);
      break;
    }
  case 'I':
    {
      uint32_t dist;
      if ((param[0] * 8) < (ROWS*COLS) - pos)
	dist = param[0] * 8;
      else
	dist = (ROWS*COLS) - pos;

      pos += dist - (pos % 8);
      putCursAt(pos);
      break;
    }
  case 'J':
    {
      int newpos, oldpos;
      oldpos = pos;


      if (param[0] == 0) {
	newpos = ROWS * COLS;
	while (pos < newpos) {
	  putCharAtPos(pos, ' ');
	  pos++;
	}
	pos = oldpos;
	putCursAt(pos);
      }
      else if (param[0] == 1) {
	newpos = 0;
	while (pos > (newpos - 1)) {
	  putCharAtPos(pos, ' ');
	  pos--;
	}
	pos = oldpos;
	putCursAt(pos);
      }
      else if (param[0] == 2)
	clearScreen();

      break;
    }
  case 'K':
    {
      int start, end, oldpos;

      start = 0;

      oldpos = pos;

      if (param[0] == 0) {
	end = (pos / 80) + 80;
	while (pos < end) {
	  putCharAtPos(pos, ' ');
	  pos++;
	}
	pos = oldpos;
	putCursAt(pos);
      }
      else if (param[0] == 1) {
	end = (pos / 80);
	while (pos > end - 1) {
	  putCharAtPos(pos, ' ');
	  pos--;
	}
	pos = oldpos;
	putCursAt(pos);
      }
      else if (param[0] == 2) {
	start = pos / 80;
	end = (pos / 80) + 80;
	pos = start;
	while (pos < end) {
	  putCharAtPos(pos, ' ');
	  pos++;
	}
	pos = start;
	putCursAt(pos);
      }
      break;
    }
  case 'L':
    {
      uint32_t nrows;
      if (param[0] < ROWS - posRow)
	nrows = param[0];
      else
	nrows = ROWS - posRow;

      scroll(posRow * COLS, ROWS * COLS, nrows * COLS);
#if 0 /* Do we want the cursor to move with the line? */
      pos = pos + (nrows * COLS);
      putCursAt(pos);
#endif
      break;
    }
  case 'M':
    {
      uint32_t nrows;
      int endpos, oldpos;
      oldpos = pos;
      if (param[0] < ROWS - posRow)
	nrows = param[0];
      else
	nrows = ROWS - posRow;

      if (pos % COLS == 0) 
	endpos = pos + (nrows * COLS);
      else
	endpos = ((pos / COLS) * COLS) +  COLS + (nrows * COLS);
      while (pos < endpos) {
	putCharAtPos(pos, ' ');
	pos++;
      }
      pos = oldpos;
      putCursAt(pos);
      break;
    }
  case 'X':
  case 'P':
    {
      int dist, oldpos, endpos;
      oldpos = pos;
      
      if (param[0] < COLS - posCol)
	dist = param[0];
      else
	dist = COLS - posCol;
      endpos = pos + dist;
      while (pos < endpos) {
	putCharAtPos(pos, ' ');
	pos++;
      }
      pos = oldpos;
      putCursAt(pos);
      
      break;
    }
  case 'S':
    {
      /* This will be cooler once we have a scrollback buffer */
      uint32_t lines;
      lines = param[0];

      scroll(0, ROWS * COLS, (int) lines * COLS);
      break;
    }
  case 'T':
    {
      /* This will be more exciting once we have a scrollback buffer */
      uint32_t lines;
      lines = param[0];

      scroll(0, ROWS * COLS, - (int) lines * COLS);
      break;
    }
  case 'Z':
    {
      uint32_t dist;
      if (pos - (param[0] * 8) > 0)
	dist = param[0] * 8;
      else {
	pos = 0;
	putCursAt(0);
	break;
      }
      
      pos -= dist - (pos % 8);
      putCursAt(pos);
      break;
    }
  }

  state = WaitChar;
  return;
}

static void
processChar(uint8_t ch)
{
  const int STDTABSTOP = 8;
  int TABSTOP = STDTABSTOP;

  if (IsPrint(ch)) {
    
    putCharAtPos(pos, ch);    
    pos++;
  }

  else {
    
    switch (ch) {
    case CODE_BS:
      {
	if ((pos - 1) >= 0) {
	  pos--;
	  putCharAtPos(pos, 0);
	}
	break;
      }

    case CODE_TAB:
      {
	/* Deal with case where cursor is sitting on a tab stop: */
	if ((pos % TABSTOP) == 0) {
	  putCharAtPos(pos, ' ');
	  pos++;
	}

	while (pos % TABSTOP) {
	  putCharAtPos(pos, ' ');
	  pos++;
	}
	break;
      }

    case CODE_CR:
    case CODE_LF:
      {
	int newpos;
	newpos = ((pos / COLS) *COLS) + COLS;
	
	while (pos < newpos) {
	  putCharAtPos(pos, 0);
	  pos++;
	}
	break;
      }
    }
  }

  if (pos >= ROWS * COLS) {
    scroll(0, ROWS * COLS, - (int) COLS);
    pos -= COLS;
  }
  putCursAt(pos);

  return;
}

static void
processInput(uint8_t ch)
{
  switch (state) {
  case WaitChar:
    {
      if (ch == CODE_ESC) {
	state = GotEsc;
	return;
      }

      processChar(ch);
      break;
    }

  case GotEsc:
    {
      int i;
      if (ch == '[') {

	state = WaitParam;
	
	npar = 0;
	for (i = 0; i < 10; i++) {
	  param[i] = 0;
	}
	return;
      }
      
      state = WaitChar;
      break;
    }

  case WaitParam:
    {
      if (npar < 2 && ch >= '0' && ch <= '9') {
	param[npar] *= 10;
	param[npar] += (ch - '0');
      }
      else if (npar < 2 && ch == ';') {
	npar++;
      }
      else if (npar == 2) {
	state = WaitChar;
      }
      else {
	processEsc(ch);
      }
    }
    break;
  }
}
      

#if 0
static int
putColCharAtPos(unsigned int pos, uint8_t ch, uint8_t color)
{
  if ((pos < 0) || (pos >= SCREEN_SIZE)) {
    /* Invalid range specified.  Return 1 */
    return 1;
  }
  screen[pos] = ch | color;
  return 0;
}
#endif


IDL_SERVER_HANDLER_PREDECL uint64_t 
HANDLE_coyotos_Cap_destroy(ISE *ise)
{
  coyotos_SpaceBank_destroyBankAndReturn(CR_SPACEBANK,
					 CR_RETURN,
					 RC_coyotos_Cap_OK);
  /* Not Reached */
  return (IDL_exceptCode);
}

IDL_SERVER_HANDLER_PREDECL uint64_t 
HANDLE_coyotos_Cap_getType(uint64_t *out, ISE *_env)
{
  *out = IKT_coyotos_driver_TextConsole;

  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t 
HANDLE_coyotos_driver_TextConsole_clear(ISE *_env)
{
  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t 
HANDLE_coyotos_driver_TextConsole_putChar(char c,
			   ISE *_env)
{
  processInput(c);

  return RC_coyotos_Cap_OK;
}

IDL_SERVER_HANDLER_PREDECL uint64_t 
HANDLE_coyotos_driver_TextConsole_putCharArray(coyotos_driver_TextConsole_chString s,
					       ISE *_env)
{
  size_t i;
  for (i = 0; i < s.len; i++)
    processInput(s.data[i]);

  return RC_coyotos_Cap_OK;
}

_IDL_GRAND_SERVER_UNION gsu;

void
ProcessRequests(ISE *_env)
{
  gsu.icw = 0;
  gsu.pb.sndPtr = 0;
  gsu.pb.sndLen = 0;
  
  for(;;) {
    gsu.icw &= (IPW0_LDW_MASK|IPW0_LSC_MASK
        |IPW0_SG|IPW0_SP|IPW0_SC|IPW0_EX);
    gsu.icw |= IPW0_MAKE_NR(sc_InvokeCap)|IPW0_RP|IPW0_AC
        |IPW0_MAKE_LRC(3)|IPW0_NB|IPW0_CO;
    
    gsu.pb.u.invCap = CR_RETURN;
    gsu.pb.rcvCap[0] = CR_RETURN;
    gsu.pb.rcvCap[1] = CR_ARG0;
    gsu.pb.rcvCap[2] = CR_ARG1;
    gsu.pb.rcvCap[3] = CR_ARG2;
    gsu.pb.rcvBound = (sizeof(gsu) - sizeof(gsu.pb));
    gsu.pb.rcvPtr = ((char *)(&gsu)) + sizeof(gsu.pb);
    
    invoke_capability(&gsu.pb);
    
    _env->pp = gsu.pb.u.pp;
    _env->epID = gsu.pb.epID;
    
    /* Re-establish defaults. Note we rely on the handler proc
     * to decide how MANY of these caps will be sent by setting ICW.SC
     * and ICW.lsc fields properly.
     */
    gsu.pb.sndCap[0] = CR_REPLY0;
    gsu.pb.sndCap[1] = CR_REPLY1;
    gsu.pb.sndCap[2] = CR_REPLY2;
    gsu.pb.sndCap[3] = CR_REPLY3;
    
    /* We rely on the (de)marshaling procedures to set sndLen to zero
     * if no string is to be sent. We cannot zero it preemptively here
     * because sndLen is an IN parameter telling how many bytes we got.
     * Set sndPtr to zero so that we will fault if this is mishandled.
     */
    gsu.pb.sndPtr = 0;
    
    if ((gsu.icw & IPW0_SC) == 0) {
      /* Protocol violation -- reply slot unpopulated. */
      gsu.icw = 0;
      gsu.pb.sndLen = 0;
      continue;
    }
    
    switch(choose_if(gsu.pb.epID, gsu.pb.u.pp)) {
    case IKT_coyotos_driver_TextConsole:
      _IDL_IFDISPATCH_coyotos_driver_TextConsole(&gsu.coyotos_driver_TextConsole, _env);
      break;
    default:
      {
        gsu.except.icw =
          IPW0_MAKE_LDW((sizeof(gsu.except)/sizeof(uintptr_t))-1)
          |IPW0_EX|IPW0_SP;
        gsu.except.exceptionCode = RC_coyotos_Cap_UnknownRequest;
        gsu.pb.sndLen = 0;
        break;
      }
    }
  }
}

bool 
initialize()
{
  /* The small-space startup has copied our data region, but we need
     to install the larger map as our own. */
  uint8_t hi, lo;

  coyotos_Process_getSlot(CR_SELF, coyotos_Process_cslot_addrSpace, CR_TMP1);
  coyotos_AddressSpace_setSlot(CR_MYSPACE, 0, CR_TMP1);
  coyotos_Process_setSlot(CR_SELF, coyotos_Process_cslot_addrSpace, CR_MYSPACE);

  kprintf(CR_LOG, "Calling outb on 0x3D4\n");
  outb(0xC, 0x3D4);
  hi = inb(0x3D5);
  outb(0xD, 0x3D4);
  lo = inb(0x3D5);
  kprintf(CR_LOG, "Done with cursor init\n");

  startAddrReg = hi << 8 | lo;
  state = WaitChar;
  pos = 0;

  clearScreen();

  putCharAtPos(240, 'A');
  putCharAtPos(241, 'B');
  putCharAtPos(242, 'C');
  putCharAtPos(243, 'D');

  while (*message)
    processInput(*message++);

  return true;
}

int
main(int argc, char *argv[])
{
  struct IDL_SERVER_Environment ise;

  if (!initialize())
    return (0);

  ProcessRequests(&ise);

  return 0;
}
