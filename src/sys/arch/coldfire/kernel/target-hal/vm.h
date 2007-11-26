#ifndef COLDFIRE_HAL_VM_H
#define COLDFIRE_HAL_VM_H

#include <stdbool.h>
#include <hal/kerntypes.h>
#include <kerninc/set.h>

/** @brief Coldfire PTE data structure.
 *
 * The MMU on this machine is soft-loaded, so we can use pretty much
 * any format that we want here as long as the soft-miss handler can
 * use it efficiently to set the values of the MMU Tag Register
 * (MMUTR) and the MMU Data Entry Register (MMUDR). Each of these
 * registers is 32 bits.
 *
 * For our purposes the MMUTR consists of:
 *
 * @verbatim
 31                 10 9         2  1  0
+---------------------+-----------+--+--+
|          VA         |   ASID    |SG| V|
+---------------------+-----------+--+--+
@endverbatim
 * 
 * Strictly speaking, the least three bits of the VA could be reused
 * for software purposes, because they will be ignored for translation
 * purposes. The architecture goes down to 1 Kbyte pages, but we are
 * using 8 Kbyte pages to ensure cache consistency.
 *
 * At present we do not use the ASID value. It is set to 1 for all
 * user mode mappings and 0 for all kernel-mode mappings. This will
 * certainly change in future implementations.
 *
 * The MMUDR consists of:
 * @verbatim
 31                 10  8  6  5  4  3  2  1  0
+---------------------+--+--+--+--+--+--+--+--+
|          PA         |SZ|CM|SP| R| W| X|LK| 0|
+---------------------+--+--+--+--+--+--+--+--+
@endverbatim
 *
 * Where:
 * - PA is the physical page address.
 * - SZ is the page size (in our case, always 0b10 indicating 8 Kbyte)
 * - CM is the cache mode
 * - SP is supervisor protect (1 => supervisor only)
 * - R, W, X are read, write, execute
 * - LK indicates entry should be locked in TLB.
 * bottom bit is reserved for future use.
 */

typedef struct COLDFIRE_PTE {
  union {
    struct {
      uint32_t V       :  1;	/**< valid */
      uint32_t SG      :  1;	/**< shared globally */
      uint32_t asid    :  8;	/**< address space ID  */
      uint32_t va      :  22;	/**< virtual address */
    } bits;
    uint32_t value;
  } tr;
  union {
    struct {
      uint32_t resvd   :  1;	/**< reserved */
      uint32_t LK      :  1;	/**< locked */
      uint32_t X       :  1;	/**< execute  */
      uint32_t W       :  1;	/**< write  */
      uint32_t R       :  1;	/**< read  */
      uint32_t SP      :  1;	/**< supervisor protect */
      uint32_t CM      :  2;	/**< cache mode */
      uint32_t SZ      :  2;	/**< page size */
      uint32_t pa      :  22;	/**< frame number */
    } bits;
    uint32_t value;
  } dr;
} COLDFIRE_PTE;

#define COLDFIRE_PAGE_SIZE_8K 2

#define COLDFIRE_CACHE_WRITETHROUGH 0
#define COLDFIRE_CACHE_WRITEBACK    1
#define COLDFIRE_NONCACHE_PRECISE   2
#define COLDFIRE_NONCACHE_IMPRECISE 3

/* IA32 has two PTE sizes in 32-bit mode. What we need to do here is
   define PTE type having the property that the machine independent
   code can declare a pointer to it, and get some degree of pointer
   type checking, but still not be able to use sizeof(*PTE) anywhere! */
typedef COLDFIRE_PTE TARGET_HAL_PTE_T;

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
  thePTE->tr.value = 0;
  thePTE->dr.value = 0;
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

#endif /* COLDFIRE_HAL_VM_H */
