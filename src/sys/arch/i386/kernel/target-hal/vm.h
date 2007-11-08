#ifndef I386_HAL_VM_H
#define I386_HAL_VM_H

#include <stdbool.h>
#include <hal/kerntypes.h>
#include <kerninc/set.h>

union IA32_PTE {
  struct {
    uint32_t V       :  1;	/**< present */
    uint32_t W       :  1;	/**< writable  */
    uint32_t USER    :  1;	/**< user-accessable */
    uint32_t PWT     :  1;	/**< page write through */
    uint32_t PCD     :  1;	/**< page cache disable */
    uint32_t ACC     :  1;	/**< accessed */
    uint32_t DIRTY   :  1;	/**< dirty */
    uint32_t PGSZ    :  1;	/**< large page */
    uint32_t GLBL    :  1;	/**< global mapping */
    uint32_t SW0     :  1;	/**< software use */
    uint32_t SW1     :  1;	/**< software use */
    uint32_t SW2     :  1;	/**< software use */
    uint32_t frameno : 20;	/**< frame number */
  } bits;

  uint32_t value;
};
typedef union IA32_PTE IA32_PTE;

/** @brief IA32 PAE PTE data structure. */
union IA32_PAE {
  struct {
    uint64_t V       :  1;	/**< valid */
    uint64_t W       :  1;	/**< writable  */
    uint64_t USER    :  1;	/**< user-accessable */
    uint64_t PWT     :  1;	/**< page write through */
    uint64_t PCD     :  1;	/**< page cache disable */
    uint64_t ACC     :  1;	/**< accessed */
    uint64_t DIRTY   :  1;	/**< dirty */
    uint64_t PGSZ    :  1;	/**< large page */
    uint64_t GLBL    :  1;	/**< global mapping */
    uint64_t SW0     :  1;	/**< software use */
    uint64_t SW1     :  1;	/**< software use */
    uint64_t SW2     :  1;	/**< software use */
    uint64_t frameno : 40;	/**< frame number */
    uint64_t resvd   : 11;      /**< must be zero */
    uint64_t NX      :  1;      /**< No-execute */
  } bits;

  uint64_t value;
};
typedef union IA32_PAE IA32_PAE;

/* IA32 has two PTE sizes in 32-bit mode. What we need to do here is
   define PTE type having the property that the machine independent
   code can declare a pointer to it, and get some degree of pointer
   type checking, but still not be able to use sizeof(*PTE) anywhere! */
typedef struct PTE TARGET_HAL_PTE_T;

/** @brief Invalidate a low-level page table entry.
 *
 * This routine ensures that an in-memory page table entry is marked
 * invalid. It does <em>not</em> attempt to invalidate the
 * corresponding TLB entry.
 *
 * @todo I am not sure that the model here still works. The EROS
 * version of this routine set a global to indicated that a PTE was
 * invalidated somewhere. This global was checked at the commit
 * point. The problem is that in a multiprocessor we may be shooting
 * down a PTE that is loaded by another processor. I'm not sure what
 * coordination we may need to do in that case.
 */
inline static void
pte_invalidate(TARGET_HAL_PTE_T *thePTE)
{
  /* Thankfully, we can invalidate *either* size of PTE by zeroing the
     low-order bit. Regrettably, this is NOT true for the page
     directory pointer in the process structure (which becomes the CR3
     value), because the least significant bit of that register really
     does need to be zero in any case. 

     However, in that case we also get lucky, because the register
     itself is only a 32-bit register and in consequence we can zero
     it using the same trick for invalidation.
  */
  *((uint32_t *) thePTE) = 0;
}

/** @brief Returns TRUE iff the specified VA is a valid user-mode VA
 * for the referencing process according to the address map of the current
 * architecture.
 */
struct Process;
static inline bool vm_valid_uva(struct Process *p, uva_t uva)
{
  return ((uva >= 0) && (uva < KVA));
}


#if 0
static inline bool
pte_is(TARGET_HAL_PTE_T *pte, unsigned flg)
{
  return WSET_IS(pte->value, flg);
}

static inline bool
pte_isnot(TARGET_HAL_PTE_T *pte, unsigned flg)
{
  return WSET_ISNOT(pte->value, flg);
}

static inline void
pte_set(TARGET_HAL_PTE_T *pte, unsigned flg)
{
  WSET_SET(pte->value, flg);
}

static inline void
pte_clr(TARGET_HAL_PTE_T *pte, unsigned flg)
{
  WSET_CLR(pte->value, flg);
}
#endif

#endif /* I386_HAL_VM_H */
