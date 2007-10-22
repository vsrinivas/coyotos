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
 *
 * The EROS version of this code was significantly more
 * complicated. In Coyotos, we do not attempt to optimize the
 * interrupt path quite so aggressively, because we are going to be
 * using either SYSCALL/SYSRET or SYSENTER/SYSEXIT for the system call
 * handling path. This means that the only system calls coming through
 * here are going to be from alien (non-Coyotos) processes or from
 * Virtual-8086 processes.
 *
 * At this point, my personal opinion is that Virtual-8086 is doomed,
 * because running a dynamic translator is plenty good enough. In
 * consequence, this path no longer tries to support Virtual-8086
 * mode. Note that a process cannot set the VM bit without kernel
 * consent, because the only ways to do so are by executing a ring-0
 * IRET with the VM bit set in EFLAGS or by performing a task switch
 * to a TSS having the VM bit set in the saved EFLAGS value. So: we no
 * longer provide even vestigial support for v8086 mode.
 */
#include <coyotos/i386/asm.h>
#include <coyotos/i386/pagesize.h>
#include "Selector.h"
#include "asm-offsets.h"
#include <hal/irq.h>
#include <hal/config.h>
#include <kerninc/event.h>
	
#ifdef BRING_UP
LED_STATE_MASK=3
LED_NLED=72
	.data
L_led_pos:
	.asciz "|/-\\"
L_console_leds:
	.space LED_NLED*4,3
#endif
	
	/* Define a trap entry for a trap that does not push
	   an error code. */
#define DEFENTRY(vecno) \
.text; \
1:	pushl	$0; \
	pushl	$vecno; \
	jmp	EXT(intr_common); \
.data; \
	.long 1b
	
	/* Define a trap entry for a trap that DOES push
	   an error code. Some of these end up in strange 
	   places. */
#define DEFENTRY_EC(vecno, label) \
.text; \
1:	pushl	$vecno; \
	jmp	EXT(label); \
.data; \
	.long 1b
	
	.data
ENTRY(irq_stubs)
	/* Set up the hardware trap entry stubs: */
DEFENTRY(0x00)			/* #DE:	Divide error */
DEFENTRY(0x01)			/* #DB:	Debug exception */
DEFENTRY(0x02)			/* NMI */
DEFENTRY(0x03)			/* #BP:	Breakpoint */
DEFENTRY(0x04)			/* #OF:	Overflow (from INTO) */
DEFENTRY(0x05)			/* #BR:	Bounds error (from BOUND) */
DEFENTRY(0x06)			/* #UD:	Invalid Opcode */
DEFENTRY(0x07)			/* #NM:	Device (coprocessor) not available */
DEFENTRY(0x08)			/* #DF:	Double Fault */
DEFENTRY(0x09)			/* Coprocessor segment overrun (not used
				   in recent IA32 processors, reserved)
				   Occurs only if 387 coprocessor is present. */
	/*
	 * if invaltss happens in the kernel return path we'll never see 
	 * it, so don't even bother:
	 */
DEFENTRY_EC(0x0a, intr_common)	/* #TS: Invalid TSS */
DEFENTRY_EC(0x0b, maybe_iret)	/* #NP: Segment Not Present */
DEFENTRY_EC(0x0c, maybe_iret)	/* #SS: Stack Fault */
DEFENTRY_EC(0x0d, maybe_iret)	/* #GP:	General Protection */
DEFENTRY_EC(0x0e, maybe_iret) /* #PF:	Page Fault */
DEFENTRY(0x0f)			/* ??? */
	
DEFENTRY(0x10)			/* #MF:	x87 Floating Point Error */
DEFENTRY_EC(0x11, maybe_iret)	/* #AC:	alignment check */
DEFENTRY(0x12)			/* #MC:	machine check */
DEFENTRY(0x13)			/* #XC:	SIMD Floating Point Error */
	/* 0x13 is the last defined hardware trap. The rest of these entries
	   exist only to ensure that the table contains only valid 
	   entries. Not sure if this is the right strategy or if I
	   should be leaving them marked invalid. */
DEFENTRY(0x14)
DEFENTRY(0x15)
DEFENTRY(0x16)
DEFENTRY(0x17)
DEFENTRY(0x18)
DEFENTRY(0x19)
DEFENTRY(0x1a)
DEFENTRY(0x1b)
DEFENTRY(0x1c)
DEFENTRY(0x1d)
DEFENTRY(0x1e)
DEFENTRY(0x1f)
	
vecno=NUM_TRAP
	
	/* Set up the interrupt entry stubs. Note that in contrast
	 * to the earlier EROS code we cannot do eager interrupt
	 * line masking here, because this code has to work for both
	 * PIC and APIC based systems, which have different interrupt 
	 * acknowledge protocols.*/
	.text
.rept NUM_IRQ
DEFENTRY(vecno)
vecno=vecno+1
.endr


	/* On entry to intr_common, the stack frame depends on whether
	 * this was an roung 0 or outer-ring trap:
	 *
	 * outer-ring:
	 *   ??  SS
	 *    ESP
	 *   EFLAGS
	 *   ??  CS
	 *    EIP
	 *    CODE    # canonicalized by trap stub
	 *    VECNO   # canonicalized by trap stub
	 *
	 * ring0:
	 *   EFLAGS
	 *   ??  CS
	 *    EIP
	 *    CODE    # canonicalized by trap handler
	 *    VECNO   # canonicalized by trap stub
	 *
	 * CS and SS have been reloaded by
	 * the hardware, but DS has not yet been reloaded. Curiously,
	 * it is more efficient to use segment overrides than it is
	 * to reload %DS early.
	 */
	.text
	.align 16
LEXT(intr_common)
#ifdef BRING_UP
	/* Disgustingly "clever" diagnostic trick:
	 *
	 * Address of last screen row is 0xc00b8000 * (24 *80 * 2),
	 * which is 0xc00b8F00. Assign each vector a position in 
	 * the last line and run a spinner there. Reserve the first 8
	 * locations for progress messages in this path. This is good 
	 * up to 72 vectors.
	 *
	 * We have already saved condition code, so we don't need
	 * to worry about that, but we do need a scratch register.
	 */
	pushl	%eax
	pushl	%ebp

	/* LED value index: */
	movl	8(%esp),%ebp	        /* index */
	cmpl	$LED_NLED,%ebp
	jae	L_no_led
	
	shll	$2,%ebp		        /* convert to word index */
	
#if 1
	/* Pick up LED Value. On SMP this may race, which is okay
	 * for our purposes: */
	movl	L_console_leds(%ebp),%eax
	incl	L_console_leds(%ebp)

	/* after modulo, %eax now holds "our" value */
	andl	$LED_STATE_MASK,%eax
	movl	%eax,%ebp
	
	movb	L_led_pos(%ebp),%al
	
	movl	8(%esp),%ebp	        /* index */
	addl	%ebp,%ebp	        /* convert to uint16 index */
	addl	$16,%ebp		/* Offset by first 8 positions */
	movb	%al,0xc00b8f00(%ebp)
#endif
	
L_no_led:	
	popl	%ebp
	popl	%eax
#endif
	
#ifdef BRING_UP
	ss movl	$0x07520749,0xc00b8F00	 /* "IR" */
	ss movl	$0x07200751,0xc00b8F04	 /* "Q " */
#endif

	pusha

	/* Save the %cr2 value, in case this may have been a page fault.
	 * If it wasn't, storing this is innocuous. */
	movl	%cr2,%eax
	movl	%eax,FIX_OFF_PFADDR(%esp)
	
	testl	$3,FIX_OFF_CS(%esp)
	jz	L_kernel_interrupt

	movl	%esp,%ebp	/* pointer to save area to EBP */
	
#ifdef BRING_UP
	ss movl	$0x07490755,0xc00b8F00	 /* "UI" */
	ss movl	$0x07510752,0xc00b8F04	 /* "RQ" */
#endif
	/* This is a process interrupt. Need to save to process structure
	 * and then switch to kernel stack. */
	mov	%gs,FIX_OFF_GS(%ebp)
	mov	%fs,FIX_OFF_FS(%ebp)
	mov	%ds,FIX_OFF_DS(%ebp)
	mov	%es,FIX_OFF_ES(%ebp)

	/* Load up the kernel segment regster values */
	mov	$sel_KernelData,%ax
	mov	%ax,%ds
	mov	%ax,%es
	
	subl	$PR_OFF_FIXREGS,%ebp

	/* Re-load per-CPU kernel stack */
	movl	PR_OFF_ONCPU(%ebp),%esp
	movl	CPU_OFF_STACKTOP(%esp),%esp
	
	pushl	%ebp	/* pointer to save area */
	addl	$PR_OFF_FIXREGS,(%esp)
	
	pushl	%ebp	/* pointer to process structure */
	
	/* Call generic user mode interrupt/trap handler: */
	call	EXT(irq_OnTrapOrInterrupt)
	
	/* The call above should not ever return, but just in case... */
#ifdef BRING_UP
	movl	$0x07520755,0xc00b8F00	 /* "UR" */
	movl	$0x07540745,0xc00b8F04	 /* "ET" */
1:	hlt
	jmp 1b
#endif
	
L_kernel_interrupt:
#ifdef BRING_UP
	movl	$0x0749074b,0xc00b8F00	 /* "KI" */
	movl	$0x07510752,0xc00b8F04	 /* "RQ" */
#endif
		
	/* We have interrupted the kernel (or possibly taken a trap).
	   This will be a fast-path trap. it will run disabled. */
	pushl	%esp	/* pointer to save area */
	pushl	$0	/* pointer to process (none */
	
#ifdef BRING_UP
	movl	$0x0753074b,0xc00b8F00	 /* "KS" */
	movl	$0x07560741,0xc00b8F04	 /* "AV" */
#endif
	
	/* Call generic kernel mode interrupt/trap handler: */
	call	EXT(irq_OnTrapOrInterrupt)
	
	addl	$8,%esp	/* unwind arguments */
#ifdef EVT_TRACE
	movl	%esp,%eax
	
	pushl	$0
	pushl	$0
	pushl	%eax
	pushl	$ety_TrapRet
	call	EXT(event)
	addl	$16,%esp
#endif
	
	/* Return immediately to interrupted context. */
	
	popa			/* restore registers */
	addl $8,%esp		/* skip error, vecno */
L_kernel_iret:	
	iret			/* return to kernel */
	
	.text
ENTRY(sched_low_level_yield)
	andl	$KSTACK_MASK,%esp
	addl	$KSTACK_SIZE,%esp
	call	EXT(sched_dispatch_something)
	
	/* Take the processor into its idle mode setting until the
	 * next interrupt appears. Note that there is an intentional 
	 * race here, because we might take an interrupt before we 
	 * actually hit the HLT instruction. The thing that wakes us 
	 * up from HLT is an incoming interrupt, and the interrupt handler 
	 * path is going to call sched_resched, with the consequence that 
	 * a non-local control transfer occurs. */ 
	.text
ENTRY(IdleThisProcessor)
1:	hlt
	jmp	1b
	
	/* This is called with interrupts already disabled. */
ENTRY(asm_proc_resume)
#ifdef EVT_TRACE
	pushl	$0
	pushl	$0
	pushl	12(%esp)	/* Current process pointer */
	pushl	$ety_RunProc
	call	EXT(event)
	addl	$16,%esp
#endif
	
	movl	4(%esp),%esp	/* Current process pointer */
	
	/* For now, simply turn off pi_SysCallDone. Later we will
	 * use it to determine if we can do a SYSEXIT safely. */
	movl	$0, PR_OFF_ISSUES(%esp)
	
	leal	PR_OFF_FIXREGS(%esp),%esp

	/* Pop saved registers */
	popa

	/* Skip over the saved error number and exception number */
	addl	$8,%esp

        /* IMPORTANT: Each of the following loads can fail if the user
	 * has established an invalidor malformed segment selector
	 * value. Regrettably, "invalid" includes a Null selector for
	 * SS during IRET. There are two possible policies:
	 *
	 *  1. Forcibly set the segment registers to known
	 *     values. This is simple, but it will preclude later use
	 *     of the LDT, which we may want someday for WINE.
	 *
	 *  2. Recover. This is the solution we use here. Basically,
	 *     if these mis-load we will end up with a GP fault, and
	 *     we can check in the GP fault path to see if the fault
	 *     address was one of these instructions. In that case
	 *     we can fix up the save area and attribute the fault
	 *     to the user-level instruction that we would have
	 *     returned to if successful.
	 *
	 * We have chosen to recover. See block comment below at
	 * L_iret_recover.
	 */
	/* Reload the segment registers. */
L_reload_es:
	mov	FIX_OFF_ES-FIX_OFF_EIP(%esp),%es
L_reload_ds:
	mov	FIX_OFF_DS-FIX_OFF_EIP(%esp),%ds
L_reload_fs:
	mov	FIX_OFF_FS-FIX_OFF_EIP(%esp),%fs
L_reload_gs:
	mov	FIX_OFF_FS-FIX_OFF_EIP(%esp),%gs

	/* BELOW THIS POINT, ALL MEMORY REFERENCES MUST BE TO CODE
	 * OR STACK SEGMENTS, BECAUSE ds NOW HOLDS THE USER DATA
	 * SEGMENT, AND WE DO NOT KNOW THAT THIS SEGMENT IS VALID! */

L_iret:
	iret

	/* This path checks whether the fault occurred on the IRET path.
	 * If not, it proceeds back to the normal path
	 */
maybe_iret:
	/* Faults on the IRET path always happen on the kernel-mode
	 * selector. If not, this wasn't an IRET fault. */
	testl	$3,TRAP_OFF_CS(%esp)
	jnz	intr_common

maybe_kernel_iret:
	/* Run a seive to see if this happened at one of the instructions
	 * that require recovery.
	 */
	cmpl	$L_reload_ds,TRAP_OFF_EIP(%esp)
	je	iret_recover
	cmpl	$L_reload_es,TRAP_OFF_EIP(%esp)
	je	iret_recover
	cmpl	$L_reload_fs,TRAP_OFF_EIP(%esp)
	je	iret_recover
	cmpl	$L_reload_gs,TRAP_OFF_EIP(%esp)
	je	iret_recover
	cmpl	$L_iret,TRAP_OFF_EIP(%esp)
	je	iret_recover

	jmp	intr_common
	
/* ARCHITECTURAL BRAIN DEATH ALERT
 *
 * One of the more special "features" of the x86 is that it can take
 * exceptions on the IRET instruction.  This shouldn't happen when
 * returning to a kernel-mode thread, where we fully control what goes
 * on to the stack, but there really isn't much we can do to stop user
 * code from, say, attempting to load invalid segment register values.
 *
 * This isn't all that big a problem, given that we can arrange things
 * so as to recover properly, but one needs to be aware of it in order to
 * understand how the hell reload works.
 *
 * There are 5 instructions in the user process reload sequence that can 
 * cause a cascaded exception:
 *
 *	popl    %es
 *	popl    %ds
 *	popl    %fs
 *	popl    %gs
 * and
 *      iret
 *
 * The cascaded exception happens if any of the segment selectors are
 * inappropriate, or if fetching the instruction at CS:EIP causes a
 * page fault.  In that event, we will end up taking an exception back
 * onto the user save area before the old exception has been
 * completely dealt with. The exceptions that might be taken in such a
 * case are:
 *
 *       #GP    -- if code seg was bogus
 *       #SS    -- if stack seg was bogus
 *       #NP    -- if stack segment was not present
 *       #TS    -- if returning to invalid task segment
 *       #AC    -- if alignment checking enabled
 *       #PF    -- if instruction page not present
 *
 * If one of these occurs, it will push a minimum of 5 words before we
 * get a chance to set things right:
 *
 *      exception number
 *      error code
 *      eip
 *      cs
 *      eflags
 *
 * We rule out the TS case by design.
 *
 * The trick in such a case is to patch up the stack so that it looks
 * like this exception was generated by the user instruction we were
 * returning to rather than by the return path.  In the iret case, the
 * 5 words that get clobbered can be reconstructed from the state on
 * the processor.  Unfortunately, the same is NOT true when a fault
 * occurs during one of the segment reloads.
 *
 * If a cascaded interrupt is taken, we examine the return PC to see
 * if it was the PC of the IRET instruction.  If so, the portion of
 * the save area that was smashed is:
 *
 *      SMASHED                           WITH
 *      error code (zero if none)         eflags
 *      trap number/interrupt number      kern code cs
 *      eax   \                           eip  of IRET instr
 *      ecx   Part of PUSHA frame         err code
 * sp-> edx   /                           vector no
 *
 * Note that we do not *need* the old error code or trap number -- we
 * are going to overwrite them in any case. While we need the register
 * set, they are presently sitting in the hardware registers because
 * we have just executed a PUSHA. Note further that the registers
 * OTHER THAN eax, ecx, edx are still in their desired save area
 * slots, so we can use those as scratch registers.
 *
 * What we do in this case is move the new error code and trap number
 * up 3 words (i.e. copy them into their proper positions), rewrite
 * the %eax, %ecx, and %edx values from the processor registers,
 * adjust the stack pointer to point to the bottom of the save area,
 * and dispatch back into OnTrapOrInterrupt
 *
 * When this is done we have reconstructed the desired trap frame,
 * and we simply re-enter the kernel as if from the normal interrupt
 * path.
 */

iret_recover:
	/* Recover the proper kernel segment register values */
	mov	$sel_KernelData,%ax
	mov	%ax,%ds
	mov	%ax,%es
	
	movl	%esp,%ebp
	subl	$FIX_OFF_EDX,%ebp

#ifdef BRING_UP
	movl	$0x07520749,0xc00b8F00	 /* "IR" */
	movl	$0x07540745,0xc00b8F04	 /* "ET" */
#endif
	/* %EBP now points to save area base.
	 * %ESP points to the dumped frame.
	 *
	 * Move the vector number and error code to their expected
	 * location.
	 */
	movl	(%esp),%ebx	/* pooped vector number */
	movl	%ebx,FIX_OFF_VECNO(%ebp)
	movl	4(%esp),%ebx	/* pooped error code */
	movl	%ebx,FIX_OFF_ERRCODE(%ebp)
	
	/* Save %ecx,%edx,%eax back to the save area. */
	movl	%ecx,FIX_OFF_ECX(%ebp)
	movl	%edx,FIX_OFF_EDX(%ebp)
	movl	%eax,FIX_OFF_EAX(%ebp)
	
	subl	$PR_OFF_FIXREGS,%ebp
	
	/* Re-load kernel stack */
	movl	PR_OFF_ONCPU(%ebp),%esp
	movl	CPU_OFF_STACKTOP(%esp),%esp
	addl	$KSTACK_SIZE, %esp
	
	pushl	%ebp	/* pointer to save area */
	addl	$PR_OFF_FIXREGS,(%esp)
	
	pushl	%ebp	/* pointer to process structure */
	
	/* Call generic kernel mode interrupt/trap handler: */
	call	EXT(irq_OnTrapOrInterrupt)
