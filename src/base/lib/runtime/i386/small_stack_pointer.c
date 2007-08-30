#include <stdint.h>
#include <coyotos/machine/pagesize.h>

/* Default stack pointer at top of single GPT. */
uintptr_t __rt_stack_pointer __attribute__((weak, section(".data"))) = 
  2 * COYOTOS_PAGE_SIZE;
