#ifndef __KERNINC_EVENT_H__
#define __KERNINC_EVENT_H__
/*
 * Copyright (C) 2007, The EROS Group, LLC.
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

#define ety_None         0
#define ety_Halt         1
#define ety_Trap         2
#define ety_TrapRet      3
#define ety_RunProc      4
#define ety_PageFault    5
#define ety_MapSwitch    6
#define ety_RestartTX    7
#define ety_AbandonTX    8
#define ety_UserPreempt  9
#define ety_KernPreempt  10

#ifndef __ASSEMBLER__

#define LOG_EVENT(ety, v0, v1, v2) \
  event(ety, (uintptr_t) v0, (uintptr_t) v1, (uintptr_t) v2)

#ifdef EVT_TRACE

extern void
event(uint8_t evtType, uintptr_t v0, uintptr_t v1, uintptr_t v2);
extern void event_log_dump();

#else /* EVT_TRACE */

static inline void
event(uint8_t evtType, uintptr_t v0, uintptr_t v1, uintptr_t v2)
{
}
static inline void event_log_dump()
{
}

#endif /* EVT_TRACE */

#endif /* __ASSEMBLER__ */

#endif /* __KERNINC_EVENT_H__ */
