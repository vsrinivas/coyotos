#include <coyotos/i386/asm.h>
	
	.text
GEXT(_start)
	mov	tmp, %eax
	inc	%eax
	mov	%eax, tmp
	jmp	_start
	
	.data
tmp:	.long 0
