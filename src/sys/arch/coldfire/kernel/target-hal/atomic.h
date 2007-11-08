#ifndef COLDFIRE_HAL_ATOMIC_H
#define COLDFIRE_HAL_ATOMIC_H

/** @file
 *
 * @brief Structure declarations for atomic words and their
 * manipulators. 
 *
 * This version is nearly generic for any uniprocessor. The only
 * machine-specific code here is the OR and AND instructions in the
 * bit set operations. These are present only to prevent the optimizer
 * from combining those operations in some way that causes them to
 * require multiple instructions.
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
  volatile uint32_t *pVal = &a->w;

  uint32_t result = *pVal;
  if (result == oldval)
    *pVal = newval;

  return result;
}

/** @brief Atomic compare and swap for pointers */
static inline void *
compare_and_swap_ptr(TARGET_HAL_ATOMICPTR_T *a, void *oldval, void *newval)
{
  void * volatile *pVal = &a->vp;

  void *result = *pVal;

  if (result == oldval)
    *pVal = newval;

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

/** @brief Atomic OR bits into word. */
static inline void
atomic_set_bits(TARGET_HAL_ATOMIC32_T *a, uint32_t mask)
{ 
  __asm__ __volatile__("or.l %[mask],%[wval]"
		       : [wval] "+m" (*((uint32_t *)(a)))
		       : [mask] "r" (mask)
		       : "cc");
}

/** @brief Atomic AND bits into word. */
static inline void 
atomic_clear_bits(TARGET_HAL_ATOMIC32_T *a, uint32_t mask)
{
  __asm__ __volatile__("and.l %[mask],%[wval]"
		       : [wval] "+m" (*((uint32_t *)(a)))
		       : [mask] "r" (~mask)
		       : "cc");
}


#endif /* COLDFIRE_HAL_ATOMIC_H */
