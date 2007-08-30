#include <coyotos/i386/asm.h>
#include <coyotos/syscall.h>
#include "SimpleProc.h"
	
	.text
GEXT(_start)
	mov	$(IPW0_MAKE_CAPREG(1)+sc_LoadCap),%eax
	mov	$(SimpleProc_addr_cap + 1),%ebx
	int	$0x30
	hlt
