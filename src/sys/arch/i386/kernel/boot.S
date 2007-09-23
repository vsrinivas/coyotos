/*
 * Copyright (C) 2005, The EROS Group, LLC.
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
	 * @brief Bootstrap and startup code.
	 */
#include <coyotos/i386/asm.h>
#include <coyotos/i386/pagesize.h>
#include <target-hal/config.h>
#include "IA32/CR.h"
#include "IA32/PTE.h"
#include "IA32/EFLAGS.h"
#include "IA32/CPUID.h"

#include "MultiBoot.h"

	/* This is a multiboot OS. We currently make no provision
	 * for other forms of native boot. If there comes to be a
	 * need for that, we should probably implement it using a
	 * trampoline bootstrap loader.
	 *
	 * On entry to the kernel, multiboot standard provides the
	 * following environment:
	 *
	 *  EAX:    magic value 0x2BADB002
	 *
	 *  EBX:    32-bit physical address of multiboot info
	 *          structure
	 *
	 *  CS:     32-bit RX flat model (0-4G) segment, selector 
	 *          unspecified
	 *
	 *  DS..SS: 32-bit RW flat model (0-4G) segment, selector 
	 *
	 *  A20 Gate: enabled
	 *
	 *  CR0:    PG cleared, PE set, others undefined
	 *  EFLAGS: VM cleared, IF cleared, others undefined
	 *
	 * Other registers undefined, specifically including ESP,
	 * GDTR, IDTR, and so forth. OS must establish a stack.
	 *
	 * Note that since IF is clear, interrupts are disabled 
	 * on entry.
	 *
	 * The multiboot specification can be found online at
	 *
	 * http://www.gnu.org/software/grub/manual/multiboot/
	 */

	.text
GEXT(start)
GEXT(_start)
	jmp general_entry
	/*
	 * The multiboot header is required to appear within the
	 * first 8192 bytes of the OS image (*including* file header),
	 * which means that we want it to appear very early. In some
	 * sense this is regrettable, because it would be very pleasant
	 * to stick the syscall trampoline at 0xC1000000, which would
	 * make a convenient well-known address.
	 */
	.align 32		/* 32 bit boundary */
multiboot_header:
	.long  MULTIBOOT_HEADER_MAGIC
	.long  MULTIBOOT_HEADER_FLAGS
	.long  -(MULTIBOOT_HEADER_MAGIC+MULTIBOOT_HEADER_FLAGS)
#ifndef __ELF__
	.long  multiboot_header
	.long  _start
	.long  _edata
	.long  _end
	.long  $(general_entry-KVA)
#endif /* !__ELF__ */

general_entry:
#if HAVE_CONSOLE
	movl	$0x076f0743,0x000b8000	 /* "Co" */
#endif
	/* *****************************************************
	 *
	 * RUNTIME CONDITIONS (each will be resolved in turn):
	 *
	 * BSS is not zeroed.
	 *
	 * %EAX AND %EBX may hold multiboot info, therefore are
	 * precious!
	 *
	 * No valid stack
	 *
	 * Interrupts are disabled.
	 *
	 * We are relocated at base=KVA, but running at base=0.
	 *
	 *******************************************************/
	
	/* Editorial note:	
	 *
	 * %eax was about the worst choice possible, since we really
	 * would like to use that for zeroing BSS. Pfui!
	 */
	
	/* Zero the BSS region without clobbering %EAX or %EBX. Do this
	 * before setting up a virtual map so that we can save the
	 * multiboot info. Need to move %eax out of the way so that
	 * we can use STOSB.
	 */

	movl	%eax, %edx	/* Save the multiboot signature. */
	
	cld

	movl	$ EXT(_bss_start), %edi	/* start */
	movl	$ EXT(_end), %ecx	/* end */
	subl	%edi, %ecx		/* count */ 
	subl	$KVA, %edi		/* bias the address */
	mov	$0x0, %eax		/* value */

	rep
	stosb
	
	/* The pre-arranged mapping tables from ldscript.S do not
	 * fall within either BSS or any initialized region, and
	 * we need them in a known state. Zero those as well.
	 */
	movl	$EXT(__begin_maps), %edi	/* start */
	movl	$EXT(__end_maps), %ecx	/* end */
	subl	%edi, %ecx		/* count */
	subl	$KVA, %edi		/* bias the address */
	mov	$0x0, %eax		/* value */

	rep
	stosb
	
	
	/* RUNTIME: BSS is now zeroed. */
	
	/* Save the multiboot info, if present. */
	movl	$EXT(multibootSignature)-KVA,%esi
	movl	%edx,(%esi)		/* multiboot signature */
	movl	$EXT(multibootInfo)-KVA,%esi
	movl	%ebx,(%esi)		/* multiboot info pointer */
	
	/* RUNTIME: Multiboot info is now saved. */
	
	/* *****************************************************
	 *
	 * CHECK FOR CPUID SUPPORT, THEN FOR PAE
	 *
	 *******************************************************/
	
	/* To check for CPUID support, we need a temporary stack.
	 * Set up %esp to temporarily point to where the stack *will* 
	 * be after relocation -- all state going on the stack here is 
	 * garbage, and can be thrown away after the CPUID process is 
	 * done.
	 *
	 * This is not intended to be a complete CPUID feature check.
	 * We will do that later. We are only trying to determine here
	 * whether the processor supports PAE.
	 */
	
	/* If AC bit cannot be modified, this is 486 or earlier, and *
	 * CPUID is not available. To check that, we need a stack so that
	 * we can use PUSHF and POPF. We will later overwrite %esp with
	 * a more appropriate value.
	 */
	movl $kstack_hi-KVA, %esp
	
	pushf				/* push flags onto stack */
	xorl	$EFLAGS_AC,(%esp)
	movl	(%esp),%eax
	popf				/* pop and re-push flags to */
	pushf				/* check for AC bit change */
	cmpl	(%esp),%eax
	je	1f
	
	pushl	$EXT(message_prehistoric)-KVA
	call	fail_message
	
1:	/* Restore original flags */
	pushl	%eax
	popf
	
	/* CPU *might* support CPUID. See if CPUID bit is modifiable. */
	pushf				/* push flags onto stack */
	xorl	$EFLAGS_ID,(%esp)
	movl	(%esp),%eax
	popf				/* pop and re-push flags to */
	pushf				/* check for ID bit change */
	cmpl	(%esp),%eax
	je	1f
	
	pushl	$EXT(message_no_cpuid)-KVA
	call	fail_message
	
1:	/* Restore original flags */
	pushl	%eax
	popf

	/* We have CPUID insruction. Check that operation 1 (feature info)
	   is present. */
	xorl	%eax,%eax
	cpuid
	cmp	$0,%eax			/* Check if supports feature info */
	jg	1f
	pushl	$EXT(message_no_cpuid_op1)-KVA
	call	fail_message
		
1:	/* Determine what features are present. */
	movl	$1,%eax
	cpuid

	/* Selectively enable some features in CR4: */
	movl	%cr4,%ebx
	
	/* Enable page global extensions if present */
	movl	%edx,%eax
	andl	$CPUID_EDX_PGE,%eax
	jz	1f
	orl	$CR4_PGE,%ebx

1:	/* Enable large pages if present */
	movl	%edx,%eax
	andl	$CPUID_EDX_PSE,%eax
	jz	1f
	orl	$CR4_PSE,%ebx

1:	/* Enable debugging extensions if present */
	movl	%edx,%eax
	andl	$CPUID_EDX_DE,%eax
	jz	1f
	orl	$CR4_DE,%ebx

1:	movl	%ebx,%cr4

	/* Check for PAE mode and decide which way to build the map */
	movl	%edx,%eax
	andl	$CPUID_EDX_PAE,%eax
#if 1
	jnz	setup_pae_map
#endif
	
	/* Get us a valid mapping without delaying for anything else
	 * first. Note we have been loaded at 0x1000000, but we are
	 * relocated for 0xC1000000, so initializing the page table
	 * requires an adjustment to the KernPageDir directory
	 * address.
	 *
	 * The following arrangement is a temporary expedient, which
	 * is why we do NOT mark any of these entries global.  We will
	 * be re-initializing the kernel master mapping table more
	 * completely during early initialization.
	 *
	 * At this point, we are simply trying to establish a quick
	 * and dirty flat mapping that will make Physical [0g,2M)
	 * appear at virtual [0g,2M) and also at virtual [3g,2M). In
	 * legacy mode we are actually going to get the first 4M this
	 * way.
	 */
setup_legacy_map:
	/* Set up a legacy-style page table for the first 4Mbytes */
	movl	$EXT(KernPageTable)-KVA,%esi
	xorl	%edx,%edx
	orl	$(PTE_V|PTE_W|PTE_ACC|PTE_DRTY),%edx

	movl	$1024,%ecx
1:	movl	%edx,(%esi)
	addl	$0x1000,%edx
	addl	$4,%esi
	loop	1b
	
	/* Adjust the primary video adapter mapping to be write-through. */
	movl	$8,%ecx
	movl	$EXT(KernPageTable)-KVA+(0xB8*4),%esi
1:	orl	$(PTE_PWT|PTE_PCD),(%esi)
	addl	$4,%esi
	loop	1b
	
	movl	$EXT(KernPageDir)-KVA,%esi
	movl	$EXT(KernPageTable)-KVA,%edx
	orl	$(PTE_V|PTE_W|PTE_ACC|PTE_DRTY),%edx
	movl	%edx,(%esi)	/* Map it at VA=0 */
	movl	%edx,3072(%esi)	/* And again at VA=3G */
	
	/* Load KernPageDir into the master mapping register: */
	movl	$ EXT(KernPageDir)-KVA,%edx
	mov	%edx,%cr3
	
	
#if HAVE_CONSOLE
	movl	$0x076f0779,0x000b8004  /* "yo" */
#endif
	
	jmp	enable_paging
	
setup_pae_map:
	movl	$EXT(KernPageTable)-KVA,%esi
	xorl	%edx,%edx
	orl	$(PAE_V|PAE_W|PAE_ACC|PAE_DRTY),%edx
	
	movl	$512,%ecx
1:	movl	%edx,(%esi)
	movl	$0,4(%esi)
	addl	$8,%esi
	addl	$0x1000,%edx
	loop	1b

	/* Adjust the primary video adapter mapping to be write-through. */
	movl	$8,%ecx
	movl	$EXT(KernPageTable)-KVA+(0xB8*8),%esi
1:	orl	$(PAE_PWT|PAE_PCD),(%esi)
	addl	$8,%esi
	loop	1b
	
	movl	$EXT(KernPageDir)-KVA,%esi
	movl	$EXT(KernPageTable)-KVA,%edx
	orl	$(PAE_V|PAE_W|PAE_ACC|PAE_DRTY),%edx
	movl	%edx,(%esi)	/* Map it at VA=0 */
	movl	$0,4(%esi)
	
	/* Now set up the PDBR */
	movl	$EXT(KernPageDir)-KVA,%edx
	orl	$(PAE_V),%edx	/* On legacy PAE, *not* W, USER */
	
	/* Load KernPageDir into the PDBR: */
	movl	$EXT(cpu0_KernPDPT)-KVA,%esi
	movl	%edx,(%esi)	/* lower bits, entry 0 */
	movl	$0,4(%esi)	/* upper bits, entry 0 */
	movl	$0,8(%esi)	/* lower bits, entry 1 */
	movl	$0,12(%esi)	/* upper bits, entry 1 */
	movl	$0,16(%esi)	/* lower bits, entry 2 */
	movl	$0,20(%esi)	/* upper bits, entry 2 */
	movl	%edx,24(%esi)	/* lower bits, entry 3 */
	movl	$0,28(%esi)	/* upper bits, entry 3 */
	
	movl	$EXT(cpu0_KernPDPT)-KVA,%edx
	mov	%edx,%cr3
	
#if HAVE_CONSOLE
	movl	$0x076f0779,0x000b8004  /* "yo" */
#endif
	
	/* Set up the desired configuration in CR4. While we are here,
	 * initialize the debugging extensions and enable the TLB
	 * Global bit. Note that CR4_PSE is ignored in PAE mode, but 
	 * is safe to set.
	 *
	 * FIX:	we should also set MCE, OSFXSR, OSXMMEXCPT here when
	 * we have implemented support for those.
	 */
	mov	%cr4,%edx
	orl	$CR4_PAE,%edx
	mov	%edx,%cr4
	
enable_paging:	
#if HAVE_CONSOLE
	movl	$0x076f0774,0x000b8008 /* "to" */
	movl	$0x07200773,0x000b800c /* "s " */
#endif

	/* Set up the desired configuration in CR0. Note that
	 * we continue to rely on the GRUB-provided segment register
	 * values, because we haven't loaded GDTR yet.
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
	
#if HAVE_CONSOLE
	movl	$0x07750752,0xc00b8010 /* "Ru" */
#endif

	/*
	 * WE ARE NOW RUNNING MAPPED, however, EIP is still a low-memory
	 * address. Fix that by doing a jump register to the next useful 
	 * address.
	 */
	movl	$drop_low_map,%edx
	jmp	*%edx
	
drop_low_map:	
	/* Drop the mapping alias at VA=0x0. How to do this depends on 
	 * whether we are running in PAE mode. 
	 */
	mov	%cr4,%edx
	andl	$CR4_PAE,%edx
	jz	drop_low_legacy_map
	
	/* Record the fact that we are using PAE for later use:	 */
	movb	$1,EXT(UsingPAE)
	
	/* Whack the low-memory mapping: */
	movl	$EXT(cpu0_KernPDPT),%esi
	movl	$0,(%esi)	/* Kill mapping at VA=0 */
	movl	$0,4(%esi)
	jmp	reload_map

drop_low_legacy_map:
	movl	$EXT(KernPageDir),%esi
	movl	$0,(%esi)	/* Kill mapping at VA=0 */


reload_map:
	/* Reload the master mapping table pointer to flush the TLB
	 * of any dangling low mappings: 
	 */
	mov	%cr3,%edx
	mov	%edx,%cr3
	
setup_kstack:
	/* Now set up the stack for the first CPU (CPU0). Stacks for other 
	 * CPUs will be set up by the C code.
	 */
	movl	$EXT(kstack_hi), %esp
	
#if HAVE_CONSOLE
	movl	$0x0765076c,0xc00b8014 /* "le" */
#endif

	/* Clear flags register */
	pushl	$0
	popf

#if HAVE_CONSOLE
	movl	$0x07210773,0xc00b8018 /* "s!" */
#endif
	
start_kernel:
	call	EXT(kernel_main)
	
	/* This CPU is not supported. Say so "by hand":	 */
fail_message:
#if HAVE_CONSOLE
	popl	%edx
	/* Clear the screen: */
	movb	$0x07,%ah	/* White on black */
	movb	$0x20,%al	/* ASCII space */
	movl	$0x000b8000,%edi
	movl	$(25*80),%ecx
	rep
	stosw
	
	movl	$0x000b8000,%edi
	movl	%edx,%esi
	
1:	movb	(%esi),%al
	cmpb	$0,%al
	je	halt
	movw	%ax,(%edi)
	inc	%esi
	addl	$2,%edi
	jmp	1b
	
#endif
	
ENTRY(halt)
ENTRY(sysctl_halt)
1:	cli
	hlt
	jmp 1b

#if HAVE_CONSOLE
	.data
message_no_pae:
	.asciz "Coyotos does not support this CPU (need Pentium-II or later)"
message_prehistoric:
	.asciz "Coyotos does not support prehistoric processors"
message_no_cpuid:
	.asciz "Coyotos does not support this CPU (need CPUID)"
message_no_cpuid_op1:
	.asciz "Coyotos does not support this CPU (need CPUID op 0x1)"
#endif	
