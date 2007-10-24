/*
 * Copyright (C) 2007, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief CPU initialization logic.
 */

#include <stddef.h>
#include <kerninc/printf.h>
#include <kerninc/malloc.h>
#include <kerninc/CPU.h>
#include <kerninc/PhysMem.h>
#include <kerninc/string.h>
#include <kerninc/pstring.h>
#include <kerninc/util.h>
#include <kerninc/assert.h>
#include <hal/transmap.h>
#include "GDT.h"
#include "TSS.h"
#include "IRQ.h"
#include "cpu.h"
#include "acpi.h"
#include "hwmap.h"

extern kva_t TransientMap[];
extern uint32_t cpu0_kstack_hi[];

ArchCPU archcpu_vec[MAX_NCPU];


cpuid_t
cpu_getMyID(void)
{
  cpuid_regs_t regs;
  (void) cpuid(1u, &regs);
  uint8_t myApicID = FIELD(regs.ebx, 31, 24);

  for (size_t i = 0; i < cpu_ncpu; i++) 
    if (archcpu_vec[i].lapic_id == myApicID)
      return i;

  fatal("CPU ID of current CPU is unknown\n");
}

size_t
cpu_probe_cpus(void)
{
  /** @bug: On pre-ACPI, non-MP systems, we want to determine here
   * whether the primary CPU supports a local APIC, which means that
   * we want to call cpu_scan_features on CPU0 from here. We cannot
   * call cpu_scan_features on other CPUs until interrupts are
   * enabled.
   */

  size_t ncpu = acpi_probe_cpus();
  if (ncpu)
    printf("ACPI reports %d CPUs\n", ncpu);
#if 0
  if (ncpu == 0)
    ncpu = mptable_probe_cpus();
#endif

  if (ncpu == 0)
    ncpu = 1;			/* we ARE running on SOMETHING */

  /* Boot CPU is certainly present and started */
  cpu_vec[0].present = true;
  cpu_vec[0].active = true;
  cpu_vec[0].topOfStack = (kva_t) cpu0_kstack_hi;

  cpu_ncpu = ncpu;

  assert(cpu_ncpu <= MAX_NCPU);

  /* Allocate the per-CPU stack pages. */
  for (size_t i = 1; i < ncpu; i++) {
    kva_t stack_va = SMP_STACK_VA + i*(KSTACK_NPAGES * COYOTOS_PAGE_SIZE);
    kva_t stack_top = stack_va + (KSTACK_NPAGES * COYOTOS_PAGE_SIZE);
    cpu_vec[i].topOfStack = stack_top;

    for (size_t pg = 0; pg < KSTACK_NPAGES; pg++) {
      kva_t pg_va = stack_va + pg*COYOTOS_PAGE_SIZE;

      kmap_EnsureCanMap(pg_va, "SMP CPU stack");

      kpa_t pa = 
	pmem_AllocBytes(&pmem_need_pages, COYOTOS_PAGE_SIZE, 
			pmu_KERNEL, "SMP CPU stack");

      kmap_map(pg_va, pa, KMAP_W);
    }

    *((CPU **) stack_va) = &cpu_vec[i];
  }

  return cpu_ncpu;
}

/* Note that this spinlock violates the spinlock hold rules, because
   it is held for a long time. This is a special case, and the long
   hold is appropriate here to support AP CPU initialization. */
SpinHoldInfo smp_startlock_info;
spinlock_t smp_startlock = SPINLOCK_INIT;

/** @brief IPL all attached APs and get them to a sensible, quiescent
 * state.
 *
 * APs are halted in APIC INIT state. We need to issue a Startup IPI for the APs.
 * The Startup IPI conveys an 8-bit vector specified by the software that issues
 * the IPI to the APs. This vector provides the upper 8 bits of a 20-bit physical
 * address. Therefore, the AP startup code must reside in the lower 1Mbyte of
 * physical memory—with the entry point at offset 0 on that particular page.
 * 
 * In response to the Startup IPI, the APs start executing at the
 * specified location in 16-bit real mode. This AP startup code must
 * set up protections on each processor as determined by the SL or
 * SK. It must also set GIF to re-enable interrupts, and restore the
 * pre-SKINIT system context (as directed by the SL or SK executing on
 * the BSP), before resuming normal system operation.  The SL must
 * guarantee the integrity of the AP startup sequence, for example by
 * including the startup code in the hashed SL image and setting up
 * DEV protection for it before copying it to the desired area.  The
 * AP startup code does not need to (and should not) execute SKINIT.
*/
void 
cpu_init_all_aps()
{
  /* Start and end of 16-bit AP bootstrap code */
  extern uint32_t ap_boot[];
  extern uint32_t ap_boot_end[];

  smp_startlock_info = spinlock_grab(&smp_startlock);

  if (cpu_ncpu > 1) {
    hwmap_enable_low_map();

    size_t ap_boot_size = ((uint32_t)ap_boot_end - (uint32_t)ap_boot);

    memcpy_vtop(COYOTOS_PAGE_SIZE, ap_boot, ap_boot_size);

    for (size_t i = 1; i < cpu_ncpu; i++) {
      printf("Setting up CPU %d\n", i);

      // lapic_ipi_init(i);

      /* FIX: Add code here to kick off the APs one at a time. */
    }

    /* FIX: Cannot disable this until all APs are properly
     * started. How to know when that has happened? Perhaps add an
     * atomic "active" field in the CPU structure and check that as we
     * proceed? */
    hwmap_disable_low_map();
  }
}

void 
cpu_start_all_aps()
{
  spinlock_release(smp_startlock_info);
}

/** @brief Called from the low-level AP IPL code. When called, this AP
 * has a temporary GDT loaded and no IDT. We need to fix that up here.
 */
void
cpu_finish_ap_setup()
{
  load_gdtr_ldtr();
  load_tr();
  irq_init();			/* for CPU 0 */

  // lapic_ipi_wait();

  /* Eventually we need to enable interrupts, but we don't want to do
   * so until everything is fully initialized. Wait until we can get
   * the smp_startlock before we do that.
   */
  SpinHoldInfo shi = spinlock_grab(&smp_startlock);
  spinlock_release(shi);

  GNU_INLINE_ASM ("sti");
  printf("AP %x: Online\n", MY_CPU(id));
  sched_dispatch_something();
}
