#include <coyotos/i386/asm.h>
#include <coyotos/syscall.h>

	.global ipc_parameter_block /* defined in a *-data.c file */
	
	.text
GEXT(_start)
	DO_RO_SYSCALL(ipc_parameter_block)
	hlt

