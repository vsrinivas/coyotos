#include <coyotos/i386/asm.h>
#include <coyotos/syscall.h>
#include "SimpleProc.h"
	
	.text
GEXT(_start)
	mov	$(sc_CopyCap),%eax
	mov	$(1 << 1),%ebx                   /* copy from reg 1 */
	mov	$(SimpleProc_addr_cap + 1),%esi  /* to first cap in capPage */
	mov	$1f, %ecx
1:	int	$0x30
	hlt
