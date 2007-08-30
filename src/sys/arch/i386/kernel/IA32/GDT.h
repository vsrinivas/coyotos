#ifndef __IA32_GDT_H__
#define __IA32_GDT_H__
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
 *
 * @brief Structure definitions for hardware GDT.
 */

/** @brief Normal segment descriptor. */
struct ia32_SegDescriptor {
  uint32_t loLimit     : 16;
  uint32_t loBase      : 16;
  uint32_t midBase     : 8;
  uint32_t type        : 4;
  uint32_t system      : 1;
  uint32_t dpl         : 2;
  uint32_t present     : 1;
  uint32_t hiLimit     : 4;
  uint32_t avl         : 1;
  uint32_t zero        : 1;
  uint32_t sz          : 1;
  uint32_t granularity : 1;
  uint32_t hiBase      : 8;
};
typedef struct ia32_SegDescriptor ia32_SegDescriptor;

/** @brief Gate descriptor.
 */
struct ia32_GateDescriptor {
  uint32_t loOffset : 16;
  uint32_t selector : 16;
  uint32_t zero : 8;
  uint32_t type : 4;
  uint32_t system : 1;
  uint32_t dpl : 2;
  uint32_t present : 1;
  uint32_t hiOffset : 16;
};
typedef struct ia32_GateDescriptor ia32_GateDescriptor;

#endif /* __IA32_GDT_H__ */
