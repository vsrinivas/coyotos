#include <coyotos/i386/asm.h>
	
	.text
GEXT(_start)
	nop
	movl	$end_stack,%esp
	pushf
	orl	$0x800,(%esp)	/* set OF bit */
	popf
	into

	.data
stack:	
	.rept	16
	.long	0
	.endr
end_stack:	
