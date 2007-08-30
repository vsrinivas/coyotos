#ifndef __KERNINC_GPT_H__
#define __KERNINC_GPT_H__
/*
 * Copyright (C) 2006, Jonathan S. Shapiro.
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
 * @brief Definition of GPT structure. */

#include <kerninc/ObjectHeader.h>
#include <kerninc/util.h>
#include <obstore/GPT.h>


/** @brief The GPT structure.
 */

struct GPT {
  MemHeader         mhdr;

  ExGPT             state;
};
typedef struct GPT GPT;

static inline size_t
gpt_addressable_slots(GPT *gpt) 
{
  return (NUM_GPT_SLOTS - (gpt->state.bg * (NUM_GPT_SLOTS/2)));
}

static inline size_t
gpt_effective_l2g(GPT *gpt) 
{
  return min(gpt->state.l2v + GPT_SLOT_INDEX_BITS - (gpt->state.bg ? 1 : 0),
	     COYOTOS_SOFTADDR_BITS);
}

extern void gpt_gc(GPT *);

#endif /* __KERNINC_GPT_H__ */
