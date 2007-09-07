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
 * @brief Protospace
 *
 * Protospace's job is to optionally construct the AddrSpace and Handler for
 * this yield, install them in the process, and jump to the start address.
 * If anything fails, we will destroy the bank and return LimitReached.
 */

#include <coyotos/syscall.h>
#include <idl/coyotos/Constructor.h>
#include <coyotos/machine/pagesize.h>
#include "coyotos.Constructor.h"

#define CHECK_FAILURE		\
	test $IPW0_EX, %eax;	\
	jz 1f;		    	\
	jmp fail;		\
1:

	/*
	 * The assembler doesn't give us a good way to compare against
	 * a passed-in 64-bit constant.  Instead, we store the IKT
	 * constant in rodata, and compare directly against the stored
	 * value. 
	 */
#define TEST_KEYTYPE_CONSTRUCTOR(label_failed) \
	test (my_IKT_constructor), %ebx		; \
	jnz label_failed			; \
	test (my_IKT_constructor + 4), %esi	; \
	jnz label_failed

	.sect .rodata
	.align 8
	.type my_IKT_constructor,@object
my_IKT_constructor:	
	.quad IKT_coyotos_Constructor


	.text
	.align 8
	.globl startPC
	.type startPC,@object
	/*
	 * This value is in the text segment because it must be the first
	 * 64-bits of the ProtoSpace page.  This value is overwritten
	 * by users of ProtoSpace with the actual initial PC of the
	 * program.
	 */
startPC:
	.quad 0		/* 64-bit integer with a value of 0 */

	.globl _start
	.type _start,@function

	/* The protospace entrypoint.  Always at 0x8 */
_start:
	/* construct the handler, if needed */
	DO_RO_SYSCALL(handler_call_getType)
	CHECK_FAILURE
	TEST_KEYTYPE_CONSTRUCTOR(handler_not_cons)
	DO_RO_SYSCALL(handler_call_create)
	CHECK_FAILURE
handler_not_cons:
	/* install the handler */	
	DO_RO_SYSCALL(self_call_setSlot_handler)
	CHECK_FAILURE

	/* construct the addrSpace, if needed */
	DO_RO_SYSCALL(addrSpace_call_getType)
	CHECK_FAILURE
	TEST_KEYTYPE_CONSTRUCTOR(addrSpace_not_cons)
	DO_RO_SYSCALL(addrSpace_call_create)
	CHECK_FAILURE
addrSpace_not_cons:
	DO_RO_SYSCALL_LD64(self_call_setSpaceAndPC, (startPC), (startPC + 4))
	CHECK_FAILURE
	
	/* if we *didn't* fail, but *did* succeed, something very wrong has
	 * occurred.
	 */
1:	hlt
	jmp 1b
	
	/* if we continue after the setSpaceAndPC call, we've failed. */
fail:
	DO_RO_SYSCALL_LD64(spaceBank_call_destroy, %ebx, %esi)

halt:	hlt			; /* above call should not have returned. */
	jmp halt


