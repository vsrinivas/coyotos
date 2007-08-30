#include <coyotos/i386/asm.h>
	
	.text
GEXT(_start)
	inc	%eax
	jmp	_start
