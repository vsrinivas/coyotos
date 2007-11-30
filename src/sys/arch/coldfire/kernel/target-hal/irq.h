#ifndef COLDFIRE_HAL_IRQ_H
#define COLDFIRE_HAL_IRQ_H
/*
 * Copyright (C) 2007, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/** @brief Number of distinct interrupt lines.
 *
 * Freescale reserves vectors 0-63. These cover CPU exceptions,
 * including the system call trap architecture. They also cover the
 * level 1-7 auto vectored interrupts. Vector entries 64 and above are
 * reserved for user-defined interrupts.
 *
 * The vector table must be aligned at a 1M boundary in memory, which
 * is a mild pain in the neck.
 *
 * @issue It is not clear (to shap) if fetches from the hardware
 * vector table proceed through the translation unit or not. My guess
 * is that they do, because the processor asserts the address onto the
 * internal bus, and the MMU sits outside that. The question is: what
 * happens if that address is invalid? The architecture does not
 * appear to define any machine check exception.
 *
 * Because the level 1-7 auto-vectored interrupts fall in the middle
 * of the architecture-reserved vector entries, there is no clean
 * relationship between NUM_IRQ, NUM_TRAP, and NUM_VECTOR. The
 * hardware makes provision for a 256 entry vector table, thus
 * NUM_VECTOR is 256. The first 64 vector entries are reserved for the
 * hardware, thus NUM_TRAP is 64. It @em appears that the total
 * potential number of interrupt entries is (256-64)+7, or 199. I'm
 * not sure I have that part right. I defined NUM_IRQ to 200 here
 * purely for rounding reasons.
 */
/** @brief User-defined interrupt vectors. */
#define TARGET_HAL_NUM_IRQ         200

/** @brief Number of hardware-defined trap entries.
 *
 * Freescale reserves the first 64 entries for architecture-defined
 * vectors. These <em>include</em> the level 1-7 auto-vectored
 * interrupts. These also include the system call vectors.
 */
#define TARGET_HAL_NUM_TRAP        64

/** @brief Number of hardware-defined vectors */
#define TARGET_HAL_NUM_VECTOR      256

#ifndef __ASSEMBLER__

#include <stdbool.h>
#include <stdint.h>

/**@brief Type used for IRQ indices.
 *
 * IA-32 platforms admit the possibility of interrupts getting
 * remapped. In particular, interrupt pin N on the ISA
 * bus may not be mapped to global IRQ N. My initial hope had been to
 * express the entire kernel-external interface strictly in terms of
 * global interrupt numbers, but there does not appear to be any
 * particularly good way for an older driver to consult the interrupt
 * re-routing tables. This is particularly true given that future
 * versions of the PC may introduce new re-routing strategies, and we
 * would like to avoid the need to change the drivers at that time.
 *
 * We therefore reserve the uppermost 8 bits of the target IRQ value
 * to indicate a "bus identifier". At present the defined bus
 * identifiers are:
 *
 * <table>
 * <thead>
 * <tr valign="top">
 *   <td><b>Identifier</b></td>
 *   <td><b>Number</b></td>
 *   <td><b>Meaning</b></td>
 * </tr>
 * </thead>
 * <tbody>
 * <tr valign="top">
 *  <td>IBUS_GLOBAL</td>
 *  <td>0</td>
 *  <td>System global IRQ namespace/td>
 * </tr>
 * <tr valign="top">
 *  <td>IBUS_ISA</td>
 *  <td>1</td>
 *  <td>ISA interrupt namespace (not used)/td>
 * </tr>
 * <tr valign="top">
 *  <td>IBUS_PCI</td>
 *  <td>2</td>
 *  <td>PCI interrupt namespace/td>
 * </tr>
 * </tbody>
 * </table>
 *
 * @bug This idea probably needs to become part of the public kernel
 * interface, at which point it will need to move into an IDL file.
 */
typedef uint32_t TARGET_IRQ_T;

#define IBUS_GLOBAL 0x0
//#define IBUS_ISA    0x01000000
#define IBUS_PCI    0x02000000
//#define IBUS_LAPIC  0x03000000

#define IRQ(bus,pin) ((bus) | (pin))
#define IRQ_PIN(irq) ((irq) & 0x00ffffff)
#define IRQ_BUS(irq) ((irq) & 0xff000000)

/** @brief Opaque flags type definition. 
 *
 * Target-specific HAL must define TARGET_FLAGS_T
*/
typedef uint16_t TARGET_FLAGS_T;


/** @brief Return the current status register value. */
static inline TARGET_FLAGS_T local_get_flags()
{
  uint16_t oldSR;
  __asm__ __volatile__("movew %%sr,%[oldsr]"
		       : [oldsr] "=d" (oldSR)
		       : /* no inputs */);
  return oldSR;
}

/** @brief Set the  status register value. */
static inline void local_set_flags(TARGET_FLAGS_T flags)
{
  __asm__ __volatile__("movew %[newsr],%%sr"
		       : /* no inputs */
		       : [newsr] "d" (flags)
		       : "cc");
}

/** @brief Disable interrupts on the current processor.
 * 
 * The convoluted implementation is necessitated by the fact that the
 * non-word forms of OR were eliminated on the coldfire processors. By
 * doing it this way we can let the compiler figure out the necessary
 * code generation.
 */
static inline TARGET_FLAGS_T locally_disable_interrupts()
{
  uint16_t oldSR = local_get_flags();
  local_set_flags(oldSR | 0x700);

  return oldSR;
}

static inline void locally_enable_interrupts(TARGET_FLAGS_T oldFlags)
{
  local_set_flags(oldFlags);
}

static inline bool local_interrupts_enabled()
{
  return ((local_get_flags() & 0x700) != 0x700) ? true : false;
}

#endif /* __ASSEMBLER__ */

#endif /* COLDFIRE_HAL_IRQ_H */
