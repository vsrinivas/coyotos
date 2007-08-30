#ifndef __I686_KVA_H__
#define __I686_KVA_H__
/*
 * Copyright (C) 2005, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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

#define KVTOL(a) (a)
#define PTOKV(a, ty) ((ty) ((kva_t) (((kpa_t)(a))+KVA)))
#define KVTOP(a)     ((kpa_t) (KVTOL((kva_t)(a))-KVA))


#endif /* __I686_KVA_H__ */
