#include <coyotos/i386/asm.h>
#include <coyotos/syscall.h>
#include "SimpleProc.h"
	
	.text
GEXT(_start)
	mov	$(IPW0_MAKE_NR(sc_CopyCap)),%eax
	mov	$(SimpleProc_addr_cap + 1),%ebx  /* copy first cap in capPage*/
	mov	$(1 << 1),%esi                   /* to capReg 1 */
	mov	$1f, %edx
1:	int	$0x30
	hlt
