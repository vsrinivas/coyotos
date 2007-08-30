#include <stdint.h>
#include <coyotos/machine/pagesize.h>

/** @file
 *
 * This is a default declaration that works for most platforms.  If
 * there is a target-specific file of the same name, that version will
 * be compiled in preference to this one.
 */

/**
 * Default stack pointer just below text section. Leave a one page gap
 * for stack underflow.
 */
uintptr_t __rt_stack_pointer __attribute__((weak, section(".data")))
  = 0x07fff000u;
