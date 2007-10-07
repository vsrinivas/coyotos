#ifndef I386_HAL_CPU_H
#define I386_HAL_CPU_H

#include <coyotos/i386/pagesize.h>

struct CPU;

// static inline struct CPU *current_cpu() __attribute__((always_inline));

/** @brief Return pointer to CPU structure corresponding to currently
 * executing CPU.
 */
static inline struct CPU *current_cpu()
{
  register uint32_t sp asm ("esp");
  sp &= ~((KSTACK_NPAGES*COYOTOS_PAGE_SIZE)-1);
  return *((struct CPU **) sp);
}

#endif /* I386_HAL_CPU_H */
