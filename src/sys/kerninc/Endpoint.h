#ifndef __KERNINC_ENDPOINT_H__
#define __KERNINC_ENDPOINT_H__
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
 * @brief Definition of Wrapper structure. */

#include <hal/kerntypes.h>
#include <kerninc/ObjectHeader.h>
#include <obstore/Endpoint.h>


/** @brief Cached Endpoint structure.
 */
struct Endpoint {
  ObjectHeader      hdr;

  ExEndpoint        state;
};
typedef struct Endpoint Endpoint;

extern void endpoint_gc(Endpoint *);

#endif /* __KERNINC_ENDPOINT_H__ */
