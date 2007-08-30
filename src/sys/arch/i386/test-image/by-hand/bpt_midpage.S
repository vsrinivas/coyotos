#include <coyotos/i386/asm.h>
	
	.text
GEXT(_start)
	.space 16,0x90
	int3
	hlt
