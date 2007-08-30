#ifndef I386_HAL_ATOMIC_H
#define I386_HAL_ATOMIC_H

/** @brief Number of distinct interrupt lines.
 *
 * In APIC-based designs, the maximum hypothetical number of available
 * interrupt vectors at the APIC hardware is 256. Of these, NUM_TRAP
 * (32) are used for hardware trap vectors. We do not assign hardware
 * interrupt numbers to those because it is not possible to
 * differentiate them in software. This leaves 224 entries for
 * hardware interrupts. If you find a machine with a need for that
 * many interrupt lines on a single CPU, call us.
 *
 * Note that 224 corresponds conveniently to the Linux NR_IRQ value.
 *
 * Some of the "interrupt" lines are in fact used for system call
 * entry points. Notably, the $0x80 interrupt value (which is vector
 * 96) is used for a legacy system call entry point.
 */

/** @file
 *
 * @brief Structure declarations for atomic words and their
 * manipulators. 
*/

typedef struct {
  uint32_t  w;
} TARGET_HAL_ATOMIC32_T;

typedef struct {
  void * vp;
} TARGET_HAL_ATOMICPTR_T;

/** @brief Atomic compare and swap.
 *
 * If current word value is @p oldval, replace with @p
 * newval. Regardless, return value of target word prior to
 * operation. */
static inline uint32_t 
compare_and_swap(TARGET_HAL_ATOMIC32_T *a, uint32_t oldval, uint32_t newval)
{
  uint32_t result;

  __asm__ __volatile__("lock cmpxchgl %1,%2"
		       : "=a" (result)
		       : "r" (newval), "m"(*((uint32_t *)(a))), "0" (oldval)
		       : "memory", "cc");
  return result;

}

/** @brief Atomic compare and swap for pointers */
static inline void *
compare_and_swap_ptr(TARGET_HAL_ATOMICPTR_T *a, void *oldval, void *newval)
{
  void * result;

  __asm__ __volatile__("lock cmpxchgl %1,%2"
		       : "=a" (result)
		       : "r" (newval), "m"(*((uintptr_t *)(a))), "0" (oldval)
		       : "memory", "cc");
  return result;

}

/** @brief Atomic word read. */
static inline uint32_t 
atomic_read(TARGET_HAL_ATOMIC32_T *a)
{
  return a->w;
}

/** @brief Atomic word write. */
static inline void
atomic_write(TARGET_HAL_ATOMIC32_T *a, uint32_t u)
{
  a->w = u;
}

/** @brief Atomic ptr read. */
static inline void *
atomic_read_ptr(TARGET_HAL_ATOMICPTR_T *a)
{
  return a->vp;
}

/** @brief Atomic ptr write. */
static inline void
atomic_write_ptr(TARGET_HAL_ATOMICPTR_T *a, void *vp)
{
  a->vp = vp;
}


#endif /* I386_HAL_ATOMIC_H */
