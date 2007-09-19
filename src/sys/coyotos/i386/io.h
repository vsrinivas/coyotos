#ifndef __I686_IO_H__
#define __I686_IO_H__
/*
 * Copyright (C) 2005, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Port 0x80 is an unused port.  We read/write it to slow down the
 * I/O.  Without the slow down, the NVRAM chip get's into horrible
 * states.
 */

static inline void
outb(uint8_t value, uint16_t port)
{
  __asm__ __volatile__ ("outb %b0,%w1"
	      : /* no outputs */
	      :"a" (value),"d" (port));
  __asm__ __volatile__ ("outb %al,$0x80");	 /* delay */
#ifdef __SLOW_IO
  __asm__ __volatile__ ("outb %al,$0x80");	 /* delay */
#endif
}

static inline uint8_t
inb(uint16_t port)
{
  unsigned int _v;
  __asm__ __volatile__ ("inb %w1,%b0"
	      :"=a" (_v):"d" (port),"0" (0));
  __asm__ __volatile__ ("outb %al,$0x80");	 /* delay */
#ifdef __SLOW_IO
  __asm__ __volatile__ ("outb %al,$0x80");	 /* delay */
#endif
  return _v;
}

static inline void
outw(uint16_t value, uint16_t port)
{
  __asm__ __volatile__ ("outw %w0,%w1"
			: /* no outputs */
			:"a" (value),"d" (port));
  __asm__ __volatile__ ("outb %al,$0x80");	 /* delay */
#ifdef __SLOW_IO
  __asm__ __volatile__ ("outb %al,$0x80");	 /* delay */
#endif
}

static inline uint16_t
inw(uint16_t port)
{
  unsigned int _v;

  __asm__ __volatile__ ("inw %w1,%w0"
			:"=a" (_v):"d" (port),"0" (0));
  __asm__ __volatile__ ("outb %al,$0x80");	 /* delay */
#ifdef __SLOW_IO
  __asm__ __volatile__ ("outb %al,$0x80");	 /* delay */
#endif
  return _v;
}

static inline void
outl(uint32_t value, uint16_t port)
{
  __asm__ __volatile__ ("outl %0,%w1"
			: /* no outputs */
			:"a" (value),"d" (port));
  __asm__ __volatile__ ("outb %al,$0x80");	 /* delay */
#ifdef __SLOW_IO
  __asm__ __volatile__ ("outb %al,$0x80");	 /* delay */
#endif
}

static inline uint32_t
inl(uint16_t port)
{
  unsigned int _v;
  __asm__ __volatile__ ("inl %w1,%0"
			:"=a" (_v): "Nd" (port),"0" (0));
  __asm__ __volatile__ ("outb %al,$0x80");	 /* delay */
#ifdef __SLOW_IO
  __asm__ __volatile__ ("outb %al,$0x80");	 /* delay */
#endif
  return _v;
}

static inline void
insb(uint16_t port, void *addr, uint32_t count)
{
  __asm__ __volatile__ ("cld; rep; insb"
			: "=D" (addr), "=c" (count)
			: "d" (port), "0" (addr), "1" (count));
}

static inline void
insw(uint16_t port, void *addr, uint32_t count)
{
  __asm__ __volatile__ ("cld; rep; insw"
			: "=D" (addr), "=c" (count)
			: "d" (port), "0" (addr), "1" (count));
}

static inline void
insl(uint16_t port, void *addr, uint32_t count)
{
  __asm__ __volatile__ ("cld; rep; insl"
			: "=D" (addr), "=c" (count)
			: "d" (port), "0" (addr), "1" (count));
}

static inline void
outsb(uint16_t port, void *addr, uint32_t count)
{
  __asm__ __volatile__ ("cld; rep; outsb"
			: "=S" (addr), "=c" (count)
			: "d" (port), "0" (addr), "1" (count));
}

static inline void
outsw(uint16_t port, void *addr, uint32_t count)
{
  __asm__ __volatile__ ("cld; rep; outsw"
			: "=S" (addr), "=c" (count)
			: "d" (port), "0" (addr), "1" (count));
}

static inline void
outsl(uint16_t port, void *addr, uint32_t count)
{
  __asm__ __volatile__ ("cld; rep; outsl"
			: "=S" (addr), "=c" (count)
			: "d" (port), "0" (addr), "1" (count));
}

#endif  /* __I686_IO_H__ */
