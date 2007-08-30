/*
 * Copyright (C) 2007, The EROS Group, LLC
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

/** @file
 * @brief Event Trace Buffer.
 *
 * This implements a mechanism for logging kernel "events of
 * interest". The idea is to have something we can dump when a kernel
 * bug occurs so that we can see what the heck is going on.
 *
 * This subsystem carries a significant performance cost. It is
 * compiled into the kernel only when -DEVT_DEBUG is enabled.
 */

#ifdef EVT_TRACE
#include <inttypes.h>
#include <kerninc/printf.h>
#include <kerninc/event.h>


#define MAX_EVENTS 256
#define NDUMP 16			/* should be even */

struct TraceEvent {
  uint8_t    evtType;
  uintptr_t  v0;
  uintptr_t  v1;
  uintptr_t  v2;
} events[MAX_EVENTS];

uint8_t nextEvent = 0;

void
event(uint8_t evtType, uintptr_t v0, uintptr_t v1, uintptr_t v2)
{
  events[nextEvent].evtType = evtType;
  events[nextEvent].v0 = v0;
  events[nextEvent].v1 = v1;
  events[nextEvent].v2 = v2;
  nextEvent++;			/* May roll over. That is okay. */
}

void
event_log_dump()
{
  if (events[nextEvent-1].evtType == ety_None)
    return;

  printf("KERNEL EVENT TRACE DUMP:\n");
  /* This loop relies on rollover in the 'ndx' position, which is why
   * the termination condition checks equality rather than magnitude.
   */
  for (uint8_t ndx = nextEvent - NDUMP; ndx != nextEvent; ndx++) {
    if (events[ndx].evtType == ety_None)
      continue;

    printf("Evt 0x%02x 0x%0p 0x%0p 0x%0p\n", 
	   events[ndx].evtType,
	   events[ndx].v0,
	   events[ndx].v1,
	   events[ndx].v2);
  }
  printf("KERNEL EVENT TRACE ENDS:\n");
}


#endif
