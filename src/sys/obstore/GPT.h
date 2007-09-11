#ifndef __OBSTORE_GPT_H__
#define __OBSTORE_GPT_H__
/*
 * Copyright (C) 2006, The EROS Group, LLC.
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
 * @brief Externalization of GPT state. */

#include <obstore/capability.h>

/** @brief Externalized GPT state.
 */

enum { GPT_SLOT_INDEX_BITS = 4,
       NUM_GPT_SLOTS = (1u << GPT_SLOT_INDEX_BITS),

       GPT_HANDLER_SLOT = NUM_GPT_SLOTS - 1,
       GPT_BACKGROUND_SLOT = NUM_GPT_SLOTS - 2,
};

struct ExGPT {
  uint32_t       l2v : 6;
  uint32_t       ha  : 1;
  uint32_t       bg  : 1;
  uint32_t           : 24;	/* reserved */
  uint32_t           : 32;      /* PAD */
  capability     cap[NUM_GPT_SLOTS];
};
typedef struct ExGPT ExGPT;

#endif /* __OBSTORE_GPT_H__ */
