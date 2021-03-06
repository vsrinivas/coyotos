#ifndef I386_HAL_ATOMIC_H
#define I386_HAL_ATOMIC_H

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
		       : "r" (newval), "m" (*((uint32_t *)(a))), "0" (oldval)
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
		       : "r" (newval), "m" (*((uintptr_t *)(a))), "0" (oldval)
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

/** @brief Atomic OR bits into word. */
static inline void
atomic_set_bits(TARGET_HAL_ATOMIC32_T *a, uint32_t mask)
{ 
  __asm__ __volatile__("lock orl %[mask],%[wval]"
		       : [wval] "+m" (*((uint32_t *)(a)))
		       : [mask] "r" (mask)
		       : "cc");
}

/** @brief Atomic AND bits into word. */
static inline void 
atomic_clear_bits(TARGET_HAL_ATOMIC32_T *a, uint32_t mask)
{
  __asm__ __volatile__("lock andl %[mask],%[wval]"
		       : [wval] "+m" (*((uint32_t *)(a)))
		       : [mask] "r" (~mask)
		       : "cc");
}


#endif /* I386_HAL_ATOMIC_H */
