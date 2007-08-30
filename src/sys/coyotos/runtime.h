#ifndef __COYOTOS_RUNTIME_H__
#define __COYOTOS_RUNTIME_H__
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
 * @brief Constants for various runtime components. */

#define CAPREG_NULL	 0 /**< @brief hardwired-to-NULL (kernel-enforced) */
#define CAPREG_REPLYEPT	 1 /**< @brief Endpoint cap to reply endpoint */
#define CAPREG_SELF	 2 /**< @brief Process cap to self */
#define CAPREG_SPACEBANK 3 /**< @brief SpaceBank for process */
#define CAPREG_TOOLS	 4 /**< @brief Tools node */
#define CAPREG_INITEPT	 5 /**< @brief Initial Invocation Endpoint (epID 1) */
#define CAPREG_RUNTIME   6 /**< @brief Runtime environment */
#define CAPREG_APP	 7 /**< @brief first capability available to app */

#define CAPREG_REPLYBASE 24 /**< @brief first of 4 reply capabilities. */
#define CAPREG_REPLYCAP(n) (CAPREG_REPLYBASE + (n)) /* 0 <= n <= 3 */

#define CAPREG_RETURN	28 /**< @brief Entry cap to return to */
#define CAPREG_ARGCAP(n) (CAPREG_RETURN + (n))  /* 1 <= n <= 3 */

#define TOOL_PROTOSPACE		0
#define TOOL_SPACEBANK_VERIFY	16
#define TOOL_CONSTRUCTOR_VERIFY	32
#define TOOL_AUDIT_LOG		48

/* XXX still need C runtime stuff */

#endif /* __COYOTOS_SYSCALL_H__ */
