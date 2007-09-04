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

/** @file
 *
 * @brief The small-space runtime hook.
 *
 * The Small Space Runtime Hook's job is to allocate a GPT and stack page,
 * copy the address space into the GPT, install the stack page, and install
 * the new GPT as our address space root.  Once that's done, we set the
 * stack pointer, call a C routine to copy our data pages and set up the
 * BSS.  Once that is done, we reset __rt_runtime_hook to 0, and jump back
 * to _start.
 */

#include <coyotos/syscall.h>
#include <idl/coyotos/GPT.h>
#include <coyotos/machine/pagesize.h>
#include "coyotos.TargetInfo.h"

#define CHECK_FAILURE		\
	test $IPW0_EX, %eax;	\
	jz 1f;		    	\
	hlt;			\
1:

	.text
	.globl __small_runtime_hook
	.type __small_runtime_hook,@function

__small_runtime_hook:
	DO_RO_SYSCALL(__small_runtime_hook_call_1)
	CHECK_FAILURE
	DO_RO_SYSCALL(__small_runtime_hook_call_2)
	CHECK_FAILURE
	DO_RO_SYSCALL(__small_runtime_hook_call_3)
	CHECK_FAILURE
	DO_RO_SYSCALL(__small_runtime_hook_call_4)
	CHECK_FAILURE
	DO_RO_SYSCALL(__small_runtime_hook_call_5)
	CHECK_FAILURE

	/* calls completed:	we should now have a stack. */
	mov $coyotos_TargetInfo_small_stack_pointer, %esp

	/* use the new stack to call a C routine to make data/bss writable */
	pushl _end
	pushl _etext
	call __small_data_init	/* __small_data_init(_etext, _end) */
	addl $8, %esp
	movl $__rt_runtime_hook, %eax
	movl $0, (%eax)
	jmp _start
