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
	 * @brief Bootstrap and startup code for APs
	 */
#include <coyotos/i386/asm.h>
#include <coyotos/i386/pagesize.h>
#include <target-hal/config.h>
#include "asm-offsets.h"
#include "IA32/CR.h"
#include "IA32/PTE.h"
#include "IA32/EFLAGS.h"
#include "IA32/CPUID.h"


	/* 16/32-bit AP bootstrap code.
	 *
	 * Implementation provided by Jeroen Visser, with our thanks.
	 *
	 * This code is used to IPL the bootstrap processors. It is started
	 * in 16-bit mode, and must therefore reside below 1M. When AP's
	 * are present, init.c reserves physical memory page 1 for this purpose.
	 * The code below must be position independent, because it is copied
	 * to page 1 during AP bringup.
	 *
	 * Note that having this code in low memory is pretty undesirable,
	 * because it induces a requirement to have a live low-memory mapping
	 * when an AP is being spun up. At the moment we enable that mapping
	 * during AP bring-up and we do not support re-running an IPL on an AP
	 * later. In order to change that we would probably need to add an
	 * additional mapping structure exclusively for that purpose.
	 *
	 * This code must start at a 16 byte boundary, because the IPL mechanism
	 * branches to offset zero within a 16-bit segment. Since we will copy
	 * the code, no alignment restriction is really required for the code,
	 * but providing a starting alignment ensures that various following
	 * data structures that *do* have alignment constraints are properly aligned
	 * in relative terms.
	 */
	
	
	.align 16
	.text
GEXT(ap_boot)
	.code16

	cli
	xorl	%esp,%esp

	mov	%cs, %ax	/* Recover boot segment base */
	mov	%ax, %ds	/* Load initially sane segments */
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss

	/* Set AP boot stack at top of page */
	movw	$COYOTOS_PAGE_SIZE,%sp
	
	shll	$4,%eax		# segment base to phys addr
	movl	%ebx,%eax
	
	/* This code runs relocated but identity mapped. Relocate base of
	 * temporary GDT using quasi PC-relative addressing:
	 */
	addl	%eax,ap_boot_gdt_base - ap_boot		/* seg relative address */
	lidt	ap_boot_idt_descriptor - ap_boot	/* seg relative address */
	lgdt	ap_boot_gdt_descriptor - ap_boot	/* seg relative address */

	/* Enable protected mode execution. */
	movw	$CR0_PE, %cx
	lmsw	%cx

	leal	ap_boot32-ap_boot(%eax),%ebx
	pushl	$0x08
	pushl	%ebx

	lretl
	
ap_boot_data:
	.align 16
ap_boot_gdt_start:
	/* Entry 0 - Null Segment */
	.word	0x0
	.word	0x0
	.byte	0x0
	.byte	0x0
	.byte	0x0
	.byte	0x0
	
	/* Entry 1 - 32-bit AP boot code. 4G, base at 0, DPL0.  */
	.word	0xffff
	.word	0x0000
	.byte	0x0
	.byte	0x9b
	.byte	0xcf
	
	/* Entry 2 - 32-bit AP boot data. 4G, base at 0, DPL0.  */
	.word	0xffff
	.word	0x0000
	.byte	0x0
	.byte	0x93
	.byte	0xcf
ap_boot_gdt_end:

	/* If 16-bit code can load a GDTR value that exceeds the 16 bit limit,
	 * we should simply load the PA of gdtTable[0] here and drop the
	 * temporary GDT. */
ap_boot_gdt_descriptor:
	.word	ap_boot_gdt_end - ap_boot_gdt_start
ap_boot_gdt_base:
	.long	ap_boot_gdt_start - ap_boot

ap_boot_idt_descriptor:
	.word	0x0
	.long	0x0

ap_boot32:
	.code32
	/* We are now running protected 32-bit mode, but not yet mapped. */
	
	/* On entry, %eax holds PA=VA of copied ap_boot symbol. */
	movw	$0x10, %bx
	mov	%bx, %ds
	mov	%bx, %es
	mov	%bx, %fs
	mov	%bx, %gs
	mov	%bx, %ss
	
	/* Temporary 32-bit stack */
	movl	%eax,%esp
	addl	$COYOTOS_PAGE_SIZE,%esp

	/* Enable some useful features that exist on all SMP-capable processors. */
	movl	%cr4,%ebx
	orl	$CR4_PGE|CR4_PSE|CR4_DE,%ebx
	movl	%ebx,%cr4
	
	/* We are now in a flat-mapped 4G space. Fetch the proper mapping mode to
	 * use and load the master kernel mapping table so that we can enable paging.
	 *
	 * Note that the rest of this code is position independent, so we no longer
	 * need to retain our relocated base address in %eax.
	 */
	movl	$EXT(UsingPAE)-KVA, %eax
	cmpl	$0,%eax
	je	ap_use_legacy_map

	/* Load PAE-mode mapping table, enable PAE-based mapping. */
	movl	$EXT(KernPDPT)-KVA,%edx
	mov	%edx,%cr3
	
	movl	%cr4,%ebx
	orl	$CR4_PAE,%ebx
	movl	%ebx,%cr4
	
	jmp	ap_enable_paging
	
ap_use_legacy_map:
	/* Load PAE-mode mapping table, enable PAE-based mapping. */
	movl	$EXT(KernPageDir)-KVA,%edx
	mov	%edx,%cr3

ap_enable_paging:
	/* Set up the desired configuration in CR0.
	 *
	 * While we are here, set up alignment checking, kernel write
	 * protect, and the numerics unit.
	 *
	 * FIX:	Might want to delay setup of the numerics unit for 
	 * embedded processors. Anything else here that we should delay?
	 */
	mov	%cr0,%edx
	orl	$(CR0_PG|CR0_AM|CR0_WP|CR0_ET|CR0_TS|CR0_MP|CR0_PE),%edx
	mov	%edx,%cr0
	
	/* Flush the I-pipeline */
	movl	$ap_boot_flush, %edx
	jmp	*%edx
	
ap_boot_flush:
	pushl	$0
	popf
	
ap_boot_setup_stack:
	/* Determine pointer to my CPU structure. */
	call	EXT(cpu_getMyCPU)

	/* Load my top of stack pointer */
	movl	CPU_OFF_STACKTOP(%esp),%esp

	/* Current CPU pointer is already sitting at bottom
	 * of my proper stack. */

	call	EXT(cpu_finish_ap_setup)
GEXT(ap_boot_end)
