/*
 * Copyright (C) 2007, The EROS Group, LLC
 *
 * This file is part of the EROS Operating System.
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
 * @brief Process management.
 */

#include <kerninc/ccs.h>
#include <kerninc/Process.h>
#include <kerninc/printf.h>
#include <kerninc/Sched.h>
#include <kerninc/MemWalk.h>
#include <kerninc/Endpoint.h>
#include <kerninc/GPT.h>
#include <kerninc/assert.h>
#include <coyotos/syscall.h>
#include <kerninc/pstring.h>
#include <kerninc/InvParam.h>
#include <kerninc/ReadyQueue.h>
#include <kerninc/util.h>
#include <hal/transmap.h>
#include <hal/irq.h>
#include <hal/syscall.h>
#include <coyotos/machine/registers.h>
#include <coyotos/machine/pagesize.h>
#include <idl/coyotos/ProcessHandler.h>
#include <idl/coyotos/MemoryHandler.h>

// Temporary:
#include <hal/machine.h>

DEFINE_CPU_PRIVATE(Process *, current);

#define DEFCAP(nm, ft, ty) extern void cap_##nm(InvParam_t *);
#include <obstore/captype.def>

#define DEBUG_DISPATCH if (0)

static void cap_UndefinedType(InvParam_t *iParam);

typedef void (*CapHandlerProc)(InvParam_t *);
CapHandlerProc capHandler[] = {
#define DEFCAP(nm, ft, ty) cap_##nm,
#define NODEFCAP(nm, ft, ty) cap_UndefinedType,
#include <obstore/captype.def>
} ;

static void
cap_UndefinedType(InvParam_t *iParam)
{
  fatal("Invocation of unimplemented capability type %d\n", 
	iParam->iCap.cap->type);
}


/* Forward declaration */
static bool
proc_marshall_dest_cap(Process *invokee, caploc_t cap,
		       DestCap *dc,
		       bool nonBlock);


bool
proc_do_upcall(InvParam_t *iParam)
{
  Endpoint *ep = 
    cap_prepare_for_invocation(iParam, iParam->iCap.cap, true, false);

  if (ep == 0)
    return false;

  uintptr_t invokee_ipw0 = get_icw(iParam->invokee);
  size_t invokee_lrc = IPW0_LRC(invokee_ipw0);

  if (invokee_ipw0 & IPW0_AC) {
    for (size_t i = 0; i <= invokee_lrc; i++)
      proc_marshall_dest_cap(iParam->invokee, 
			     get_rcv_cap(iParam->invokee, i),
			     &iParam->destCap[i], true);
  }

  obhdr_dirty(&iParam->invokee->hdr);

  /* Note this drops the RP and SP bits, leaving only the CO phase
   * for the receiver to complete. */
  uintptr_t opw0 = iParam->opw[0] | (invokee_ipw0 & IPW0_PRESERVE);

  for (size_t i = 1; i <= IPW0_LDW(iParam->opw[0]); i++)
    set_pw(iParam->invokee, i, iParam->opw[i]);

  set_pw(iParam->invokee, OPW_SNDLEN, 0);

  // receiver AC must be set, but no need to check receiver LRC,
  // since cannot be less than zero:
  if (opw0 & IPW0_AC) {
    size_t nCap = min(IPW0_LSC(iParam->opw[0])+1, invokee_lrc+1);

    for (size_t i = 0; i < nCap; i++)
      cap_set(iParam->destCap[i].cap, &iParam->srcCap[i].theCap);
  }

  if (ep->state.pm)
    ep->state.protPayload++;

  /* Transfer PP of invoked capability. */
  set_pw(iParam->invokee, OPW_PP, iParam->iCap.cap->u1.protPayload);

  /* Transfer endpoint ID of invoked endpoint. */
  set_epID(iParam->invokee, ep->state.endpointID);

  /* Send and receive phases are done. Note that opw0.SP and
     opw0.RP are zero because we never set them above. */
  set_icw(iParam->invokee, opw0);

  /* Invocation is complete. Set invokee running and donate our slice. */
  iParam->invokee->state.runState = PRS_RUNNING;
  rq_add(&mainRQ, iParam->invokee, true);

  return true;
}

/** @brief If a process has pending software interrupts, issue an
 * upcall to deliver them.
 *
 * This is called just before a process enters an open receive state,
 * so there ought to be no pending faults on the process (or it would
 * not have gotten far enough to enter the receive state.
 *
 * The process is in the PRS_RECEIVING state, but has not yet awakened
 * any sleepers. All outgoing parameters will go to registers. The
 * combination of these facts means that we do not need to do most of
 * the checks in the cap_prepare_for_invocation() path. This is good,
 * because we do not have an entry capability to use here.
 */
static void
proc_DeliverSoftInts(Process *p)
{
  assert (p->state.faultCode == coyotos_Process_FC_NoFault);

  InvParam_t invParam = { p, p };
  invParam.next_odw = (IPW_DW0 == IPW_ICW) ? (IPW_DW0 + 1) : IPW_DW0;

  /* We were running, so we should be dirty already. */
  assert(p->hdr.dirty);

  uintptr_t icw = get_icw(p);

  /* Note this drops the RP and SP bits, leaving only the CO phase
   * for the receiver to complete. */
  icw &= IPW0_PRESERVE;

  /* No send caps, so LSC should be zero. */
  icw |= IPW0_MAKE_LDW(2)|IPW0_MAKE_LSC(0);

  set_epID(p, -1ll);
  set_pw(p, OPW_PP, 0);
  set_pw(p, OPW_SNDLEN, 0);	/* no string */

  set_icw(p, icw);
  set_pw(p, IPW_DW0+1, /* SOME_OPCODE */0);
  set_pw(p, IPW_DW0+2, p->state.softInts);

  p->state.softInts = 0;

  /* Invocation is complete. Set invokee running and donate our slice. */
  p->state.runState = PRS_RUNNING;
  rq_add(&mainRQ, p, true);

  sched_abandon_transaction();
}

/** @brief If a process has a pending fault, deliver that fault to the
 * process's per-process fault handler.
 *
 * This routine is called when the @c current process is attempting to
 * run with a pending fault code. It is therefore in the PRS_RUNNING
 * state. The procedure either clears the fault trivially
 * (e.g. FC_Startup), or attempts to upcall the per-process fault
 * handler. If the handler is not receiving, current is enqueued on
 * it.
 *
 * Once the fault message is delivered to the handler, or if no
 * handler is present, the @c current process transitions to the
 * PRS_FAULTED state.
 */
static void
proc_handle_process_fault()
{
  Process *p = MY_CPU(current);

  assert(p);

  if (p->state.faultCode == coyotos_Process_FC_Startup) {
    /* SPECIAL CASE! This is the first-time startup fault for a
     * process that came to us out of an initial system image. It
     * isn't a real fault at all. Resolution is to ask the
     * architecture-dependent code to set up initially sane register
     * values, zero the actSaveArea pointer, and clear the fault code.
     *
     * We then yield, with the effect that this process will be
     * dispatched in activated mode and then fault back in due to
     * absence of any valid addresses. This is the correct outcome,
     * and the page fault handler will take it from there.
     */

    proc_arch_initregs(p->state.faultInfo);

    p->state.faultCode = coyotos_Process_FC_NoFault;
    p->state.faultInfo = 0;
    p->issues &= ~pi_Faulted;
    return;
  }

  if (p->state.faultCode == coyotos_Process_FC_NoFault) {
    assert(false);

    p->issues &= ~pi_Faulted;
    return;
  }

  /* Do it the hard way. Deliver this fault to the process keeper. */
  InvParam_t invParam = { p, 0 };
  invParam.iCap.cap = &p->state.handler;
  invParam.next_odw = (IPW_DW0 == IPW_ICW) ? (IPW_DW0 + 1) : IPW_DW0;

  // Sending one cap. A null cap will go out in slot zero replacing
  // the usual reply cap.
  cap_init(&invParam.srcCap[0].theCap);
  cap_init(&invParam.srcCap[1].theCap);

  invParam.srcCap[1].theCap.type = ct_Process;
  invParam.srcCap[1].theCap.swizzled = 1;
  invParam.srcCap[1].theCap.allocCount = p->hdr.allocCount;
  invParam.srcCap[1].theCap.u2.prepObj.target = &p->hdr;
  invParam.srcCap[1].theCap.u2.prepObj.otIndex = atomic_read_ptr(&p->hdr.otIndex);

  put_oparam32(&invParam, OC_coyotos_ProcessHandler_handle);
  put_oparam32(&invParam, p->state.faultCode);
  put_oparam64(&invParam, p->state.faultInfo);
  invParam.opw[0] = InvResult(&invParam, 2);

  if (proc_do_upcall(&invParam)) 
    /* This is here only until I debug through this path. */
    bug("Do not yet know how to deliver faults to process keepers\n");
  else
    /* Bad handler capability. Process ceases to execute instructions. */
    printf("Process oid=0x%016llx faults w/ no keeper (PC=0x%08p fc=%d, fi=0x%llx)\n",
	   p->hdr.oid, 
	   FIX_PC(p->state.fixregs),
	   p->state.faultCode, p->state.faultInfo);

  p->state.runState = PRS_FAULTED;
  sched_abandon_transaction();
}

void 
proc_deliver_memory_fault(Process *p,
			  coyotos_Process_FC fc, uintptr_t addr, 
			  const MemWalkResults * const mwr)
{
  bool isCurrent = (p == MY_CPU(current));

  assert ( isCurrent || (p->state.runState == PRS_RECEIVING));

  const MemWalkEntry * keptEntry = 0;

  /* Need to see if a memory keeper was found and conditionally
     deliver to that. */
  for (size_t i = 0; i < mwr->count; i++) {
    const MemWalkEntry * const mwe = &mwr->ents[i];

    /* Once we see a no-call bit, no need to search further. */
    if (mwe->restr & CAP_RESTR_NC)
      break;

    /* If the fault is an access violation, we want to stop at the
       first read-only entry. */
    if (fc == coyotos_Process_FC_AccessViolation && 
	(mwe->restr & (CAP_RESTR_RO|CAP_RESTR_WK)))
      break;

    if (mwe->l2v == 0)		/* This is an entry for a page, no
				   further keepers are possible. */
      break;

    if (mwe->window)		/* traversed window capability */
      continue;

    /* In all remaining cases, the mwe->header field names a GPT. */
    GPT *gpt = (GPT *) mwe->entry;
    if (gpt->state.ha)
      keptEntry = mwe;
  }

  if (keptEntry) {
    /* There is an applicable memory keeper. Attempt to deliver a fault
       message to the memory keeper, providing a restart capability to
       p. */
    GPT *gpt = (GPT *) keptEntry->entry;
    capability *handler = &gpt->state.cap[GPT_HANDLER_SLOT];

    InvParam_t invParam = { p, 0 };
    invParam.iCap.cap = handler;
    invParam.next_odw = (IPW_DW0 == IPW_ICW) ? (IPW_DW0 + 1) : IPW_DW0;

    // Sending two caps. A null cap will go out in slot zero replacing
    // the usual reply cap.
    cap_init(&invParam.srcCap[0].theCap);
    cap_init(&invParam.srcCap[1].theCap);
    cap_init(&invParam.srcCap[2].theCap);

    /* Restart capability to p */
    invParam.srcCap[1].theCap.type = ct_Process;
    invParam.srcCap[1].theCap.restr = CAP_RESTR_RESTART;
    invParam.srcCap[1].theCap.swizzled = 1;
    invParam.srcCap[1].theCap.allocCount = p->hdr.allocCount;
    invParam.srcCap[1].theCap.u2.prepObj.target = &p->hdr;
    invParam.srcCap[1].theCap.u2.prepObj.otIndex = atomic_read_ptr(&p->hdr.otIndex);

    /* GPT capability to the kept GPT */
    invParam.srcCap[2].theCap.type = ct_GPT;
    invParam.srcCap[2].theCap.swizzled = 1;
    invParam.srcCap[2].theCap.restr = 0; /* full strength! */
    invParam.srcCap[2].theCap.allocCount = gpt->mhdr.hdr.allocCount;
    invParam.srcCap[2].theCap.u1.mem.l2g = COYOTOS_SOFTADDR_BITS;
    invParam.srcCap[2].theCap.u1.mem.match = 0;
    invParam.srcCap[2].theCap.u2.prepObj.target = &gpt->mhdr.hdr;
    invParam.srcCap[2].theCap.u2.prepObj.otIndex = 
      atomic_read_ptr(&gpt->mhdr.hdr.otIndex);

    put_oparam32(&invParam, OC_coyotos_MemoryHandler_handle);
    put_oparam32(&invParam, fc);
    put_oparam64(&invParam, keptEntry->remAddr);
    invParam.opw[0] = InvResult(&invParam, 3);

    if (proc_do_upcall(&invParam)) {
      /* The keeper capability was valid, and the upcall has
       * succeeded, so the handler is now going to run. Set a fault
       * state in p that we hope the memory handler will clear. Migrate
       * p to the PRS_FAULTED state.
       */

      proc_SetFault(p, fc, addr);
      p->state.runState = PRS_FAULTED;

      /* If p == current, abandon the current transaction so that the
       * handler will run. If p != current, then restart the current
       * transaction so that current will end up queued in p's receive
       * wait queue. 
      */

      if (isCurrent)
	 sched_abandon_transaction();

      sched_restart_transaction();
    }

    /* Handler cap invalid. Fall through to the process handler
       delivery case. */
  }

  /* Inflict the fault as a process fault on the target process. Set
     target running */
  proc_SetFault(p, fc, addr);

  if (!isCurrent) {
    assert(p->state.runState == PRS_RECEIVING);

    /* This is a cross-process fault, but invokee had no applicable
       memory keeper. Invokee is in receiving state, so we know it is
       not presently on any queue. Migrate it to PRS_RUNNING state and
       get it onto the ready queue so that it can recognize that it
       has incurred a fault. */
    p->state.runState = PRS_RUNNING;
    rq_add(&mainRQ, p, true);
  }

  /* if p == current, restarting will cause it to self-deliver the
     fault via the proc_handle_process_fault() path.

     If p != current, then p has been made ready above, but we need
     to have current re-attempt the invocation so that it will end
     up on p's "waiting to receive" queue. */
  sched_restart_transaction();
}


void 
proc_dispatch_current()
{
  Process *p = MY_CPU(current);

  if (p->lastCPU != MY_CPU(curCPU)->id || p->mappingTableHdr == 0)
    proc_migrate_to_current_cpu();

  vm_switch_curcpu_to_map(p->mappingTableHdr);

  if (p->issues) {
    if (p->issues & pi_Schedule) {
      cap_prepare(&p->state.schedule);

      // ??? Set Priority ???

      p->issues &= ~pi_Schedule;
    }

    if (p->issues & pi_Faulted)
      proc_handle_process_fault();

    /** @bug what to do here? */
    if (p->issues & pi_Preempted) {
      assert(MY_CPU(curCPU)->hasPreempted);

      /* Stick current at back of ready queue. */
      rq_add(&mainRQ, p, false);
      sched_abandon_transaction();
    }

    if (p->issues)
      proc_clear_arch_issues(p);

    assert((p->issues & ~(pi_SysCallDone | pi_Preempted)) == 0);
  }

  DEBUG_DISPATCH {
    proc_dump_current_savearea();
    printf("Dispatch process 0x%lx, w/ oid 0x%llx\n",
	   MY_CPU(current), 
	   MY_CPU(current) ? MY_CPU(current)->hdr.oid : -1ull);
  }

  flags_t f = locally_disable_interrupts();

  // Last check for preemption interrupt:
  if (p->issues & pi_Preempted) {
    printf("  process was preempted at last gasp!\n");
    locally_enable_interrupts(f);

    /* Stick current at back of ready queue. */
    assert(MY_CPU(curCPU)->hasPreempted);
    rq_add(&mainRQ, p, false);
    sched_abandon_transaction();
  }

  MY_CPU(curCPU)->hasPreempted = false;

  // Last check for IPI response obligations
  // Set interval timer for preemption

  proc_resume();
}

static capability TheNullCapability;

coyotos_Process_FC
proc_findCapPageSlot(Process *p, uintptr_t addr, bool forWriting,
		     bool nonBlock,
		     struct FoundPage * /*OUT*/ results)
{
  MemWalkResults mwr;
  
  assert(mutex_isheld(&p->hdr.lock));

  coyotos_Process_FC fc =
    memwalk(&p->state.addrSpace,
	    addr,
	    forWriting,
	    &mwr);

  if (fc && nonBlock)
    return (fc);

  if (fc)
    proc_deliver_memory_fault(p, fc, addr, &mwr);

  MemWalkEntry *last = &mwr.ents[mwr.count - 1];

  /* If the walk completed, we hold the lock on the target CapPage
   * because we prepared a capability to it on the way down. It is
   * safe to read/write the target slot. */

  if (last->entry->hdr.ty != ot_CapPage)
    return coyotos_Process_FC_CapAccessTypeError;

  results->pgHdr = (Page *) last->entry;
  results->slot = (addr & COYOTOS_PAGE_ADDR_MASK) >> 4;
  results->restr = mwr.cum_restr;

  return (0);
}

coyotos_Process_FC
proc_findDataPage(Process *p, uintptr_t addr, bool forWriting,
		  bool nonBlock,
		  struct FoundPage * /*OUT*/ results)
{
  MemWalkResults mwr;
  
  assert(mutex_isheld(&p->hdr.lock));

  coyotos_Process_FC fc =
    memwalk(&p->state.addrSpace,
	    addr,
	    forWriting,
	    &mwr);

  if (fc && nonBlock)
    return (fc);

  if (fc)
    proc_deliver_memory_fault(p, fc, addr, &mwr);

  MemWalkEntry *last = &mwr.ents[mwr.count - 1];

  /* If the walk completed, we hold the lock on the target CapPage
   * because we prepared a capability to it on the way down. It is
   * safe to read/write the target slot. */

  if (last->entry->hdr.ty != ot_Page)
    return coyotos_Process_FC_DataAccessTypeError;

  results->pgHdr = (Page *) last->entry;
  results->slot = 0;
  results->restr = mwr.cum_restr;

  return (0);
}

static void
proc_load_cap(Process *p, capreg_t reg, caploc_t from)
{
  /* Validate target register number before side-effecting memory.
   * This test is correct in all cases. A null reg should have a 0
   * location field, and the reserved types shouldn't be present at
   * all. */
  if (reg.ty != CAPLOC_REG || reg.loc >= NUM_CAP_REGS)
    proc_TakeFault(p, coyotos_Process_FC_MalformedSyscall, 0);

  capability *dest = 0;

  if (reg.loc)
    dest = &p->state.capReg[reg.loc];


  switch(from.ty) {
  case CAPLOC_REG:	/* register source */
    {
      uint8_t reg = from.loc;
      if (reg >= NUM_CAP_REGS)
	proc_TakeFault(p, coyotos_Process_FC_MalformedSyscall, 0);

      sched_commit_point();

      if (dest)
	cap_set(dest, &p->state.capReg[reg]);
      return;
    }

  case CAPLOC_MEM:	/* memory source */
    {
      struct FoundPage cps;

      uintptr_t addr = from.loc << 1;

      coyotos_Process_FC fc = 
	proc_findCapPageSlot(p, addr, false, false, &cps);

      assert(fc == 0);

      /* We have a valid address and a capability page, so we can
       * perform a read, but we need to honor any WEAK restriction */

      sched_commit_point();

      /* Ready to read from the cap page. */
      capability *capPg = TRANSMAP_MAP(cps.pgHdr->pa, capability *);

      if (dest) {
	cap_set(dest, &capPg[cps.slot]);

	if (cps.restr & CAP_RESTR_WK)
	  cap_weaken(dest);
      }

      TRANSMAP_UNMAP(capPg);

      return;
    }
  }
}

static void
proc_store_cap(Process *p, capreg_t reg, caploc_t to)
{
  /* Validate target register number before side-effecting memory.
   * This test is correct in all cases. A null reg should have a 0
   * location field, and the reserved types shouldn't be present at
   * all. */
  if (reg.ty != CAPLOC_REG || reg.loc >= NUM_CAP_REGS)
    proc_TakeFault(p, coyotos_Process_FC_MalformedSyscall, 0);

  capability *src = &TheNullCapability;

  /* If reg.ty == CAPLOC_NULL, the cap_init() has already done what we
     need to do. */
  if (reg.ty == CAPLOC_REG)
    src = &p->state.capReg[reg.loc];

  switch(to.ty) {
  case CAPLOC_REG:	/* register dest */
    {
      uint8_t reg = to.loc;
      if (reg >= NUM_CAP_REGS)
	proc_TakeFault(p, coyotos_Process_FC_MalformedSyscall, 0);

      sched_commit_point();

      if (reg)
	cap_set(&p->state.capReg[reg], src);
      return;
    }

  case CAPLOC_MEM:	/* memory dest */
    {
      struct FoundPage cps;

      uintptr_t addr = to.loc << 1;

      coyotos_Process_FC fc = 
	proc_findCapPageSlot(p, addr, true, false, &cps);

      assert(fc == 0);

      /* If the walk completed, we hold the lock on the target CapPage
       * because we prepared a capability to it on the way down. It is
       * safe to read/write the target slot. */

      /* We have a valid address and a capability page.  If the
       * path is RO or WK, we cannot modify the page.
       */

      obhdr_dirty(&cps.pgHdr->mhdr.hdr);

      sched_commit_point();

      /* Ready to write to the cap page. */
      capability *capPg = TRANSMAP_MAP(cps.pgHdr->pa, capability *);

      cap_set(&capPg[cps.slot], src);

      TRANSMAP_UNMAP(capPg);

      return;
    }
  }
}

void
proc_ensure_exclusive(Process *p)
{
  assert(mutex_isheld(&p->hdr.lock));
  if (p->onCPU != NULL) {
    assert(p->state.runState == PRS_RUNNING && p->onQ == NULL);
    if (p->onCPU == MY_CPU(curCPU)) {
      assert(MY_CPU(current) == p);
    } else {
      bug("do not know how to cross-call to get exclusive process access");
    }
  }
}

static void 
proc_marshall_src_cap(Process *p, caploc_t from, SrcCap *sc, 
		      bool andPrepare)
{
  switch(from.ty) {
  case CAPLOC_REG:		/* register source */
    {
      uint8_t reg = from.loc;
      if (reg >= NUM_CAP_REGS)
	proc_TakeFault(p, coyotos_Process_FC_MalformedSyscall, 0);

      if (andPrepare) cap_prepare(&p->state.capReg[reg]);

      sc->cap = &p->state.capReg[reg];

      return;
    }
  case CAPLOC_MEM:		/* memory source */
    {
      struct FoundPage cps;

      uintptr_t addr = from.loc << 1;

      if (! (vm_valid_uva(p, addr) && 
	     vm_valid_uva(p, addr+sizeof(capability)-1)) )
	proc_TakeFault(p, coyotos_Process_FC_InvalidAddr, addr);

      coyotos_Process_FC fc = 
	proc_findCapPageSlot(p, addr, false, false, &cps);

      assert(fc == 0);

      /* We have a valid address and a capability page, so we can
       * perform a read, but we need to honor any WEAK restriction */

      /* Ready to read from the cap page. */
      capability *capPg = TRANSMAP_MAP(cps.pgHdr->pa, capability *);

      if (andPrepare) cap_prepare(&capPg[cps.slot]);

      sc->cap = &sc->theCap;
      cap_set(sc->cap, &capPg[cps.slot]);

      if (cps.restr & CAP_RESTR_WK)
	cap_weaken(sc->cap);

      TRANSMAP_UNMAP(capPg);

      return;
    }
  }
}

/** @brief Per-CPU pointer to the current CPU. */
DEFINE_CPU_PRIVATE(capability, NullTargetCap);

/** @brief Marshall @em locations of output capabilities.
 */
static bool
proc_marshall_dest_cap(Process *invokee, caploc_t to,
		       DestCap *dc,
		       bool nonBlock)
{
  /* This is a garbage capability. It is never sourced. It can be
   * overwritten inconsistently if you like.  It exists so that the
   * capability target pointer in a DestCap always points to *something*
   */
  dc->capPg = 0;		/* until otherwise proven */

  /* We assume that the dest capitem_t is well-formed, because we
     checked that on input to the invoke system call. What we need to
     do here is ensure that if it is a memory location that memory
     location is well defined. */
  switch(to.ty) {
  case CAPLOC_REG:	/* register dest */
    {
      uint8_t reg = to.loc;
      if (reg >= NUM_CAP_REGS)
	proc_TakeFault(invokee, coyotos_Process_FC_MalformedSyscall, 0);

      if (reg)
	dc->cap = &invokee->state.capReg[reg];
      else 
	dc->cap = &MY_CPU(NullTargetCap);

      break;
    }

  case CAPLOC_MEM:	/* memory dest */
    {
      struct FoundPage cps;

      uintptr_t addr = to.loc << 1;

      coyotos_Process_FC fc = 
	proc_findCapPageSlot(invokee, addr, true, nonBlock, &cps);

      if (fc) {
	assert(nonBlock);
	return false;
      }

      obhdr_dirty(&cps.pgHdr->mhdr.hdr);

      /* We have a valid address and a writable capability page. Transmap
       * the page and stash a capability pointer: */

      dc->capPg = TRANSMAP_MAP(cps.pgHdr->pa, capability *);
      dc->cap = &dc->capPg[cps.slot];

      break;
    }
  }

  return true;
}

static void
proc_invoke_cap(void)
{
  Process *p = MY_CPU(current);

  /* PW0 is in a hardware register on all architectures, so it is safe
     to fetch it before validating the incoming parameter block. */
  uintptr_t ipw0 = get_icw(p);

  /*******************************************************************
   *
   *                 PHASE 0
   *
   * In Phase 0, we validate all of the sender-side parameters that
   * might lead to a malformed system call fault. We must do this
   * before we proceed to the send OR receive phase.
   *
   *******************************************************************/

  /* On some platforms, the IN parameters may come partially from
   * memory. If the specified addresse range for those parameters is
   * illegal, the system call is malformed. 
   */
  if (!valid_param_block(p))
    proc_TakeFault(p, coyotos_Process_FC_MalformedSyscall, 0);

  /* Want to save the soft parameters, but must not clobber the
   * outgoing epID parameter if receive phase has already completed. */
  if (ipw0 & (IPW0_SP|IPW0_RP))
    save_soft_parameters(p);

  /* Note: We do NOT initialize invParam to zero, except for the
   * invokee word. By the time any of the other elements are used,
   * they will have been initialized below.
   *
   * Until otherwise proven, there is no invokee.
   */
  InvParam_t invParam = { p, 0 };

  /*******************************************************************
   *
   *                 PHASE 1: Send
   *
   * At this point, all capability params are in registers.
   *
   *******************************************************************/

  /* IPW0.ldw zero => no send phase or send phase done. */
  if (ipw0 & IPW0_SP) {
    /* Marshall the capability being invoked: */
    proc_marshall_src_cap(p, get_invoke_cap(p),
			  &invParam.iCap, false);

    if (ipw0 & IPW0_SC) {
      for (size_t i = 0; i <= IPW0_LSC(ipw0); i++)
	proc_marshall_src_cap(p, get_snd_cap(p, i),
			      &invParam.srcCap[i], false);

      if (ipw0 & IPW0_RC) {
	cap_prepare(invParam.srcCap[0].cap);

	if (invParam.srcCap[0].cap->type == ct_Endpoint) {
	  /* Need to fabricate an entry capability. Therefore need to
	   * ensure that this is a transient copy so that we do not
	   * whack the original endpoint cap.
	   *
	   * If this argument came from memory, a transient copy has
	   * already been made by proc_marshall_src_cap(). If from
	   * register, we need to make that copy here.
	   */
	  if (invParam.srcCap[0].cap != &invParam.srcCap[0].theCap) {
	    cap_set(&invParam.srcCap[0].theCap, invParam.srcCap[0].cap);
	    invParam.srcCap[0].cap = &invParam.srcCap[0].theCap;
	  }

	  /* Note that the reply entry cap, if any, is fabricated
	   * BEFORE the invoked capability is examined. This step is
	   * logically part of the message payload marshalling.
	   *
	   * There is a perverse input possibility. Suppose the user
	   * invoked
	   *
	   *   entry(x)->(endpoint(x)...) with IPW0_RC and x.pm=1
	   *
	   * Then endpoint(x) will get rewritten here to entry(x)
	   * PRIOR to bumping the x.pp field. When the fabricated
	   * reply cap is later invoked, it will be invalid.
	   *
	   * This is at least consistent, it is safe, and it only
	   * happens on intentionally perverse inputs.
	   */
	  Endpoint *replyEP = 
	    (Endpoint *)invParam.srcCap[0].cap->u2.prepObj.target;
	  invParam.srcCap[0].cap->u1.protPayload = replyEP->state.protPayload;
	  invParam.srcCap[0].cap->type = ct_Entry;
	}
      }
    }

    /* If we plan to send a string, entire send area must have
       valid virtual addresses. */
    uint32_t slen = get_pw(p, IPW_SNDLEN); /* xmit str len */
    if (slen > COYOTOS_MAX_SNDLEN)
      proc_TakeFault(p, coyotos_Process_FC_MalformedSyscall, 0);

    /* If we plan to receive a string, entire receive area must have
       valid virtual addresses. */
    uint32_t rbound = get_rcv_pw(p, IPW_RCVBOUND); /* xmit str len */
    if (rbound > COYOTOS_MAX_SNDLEN)
      proc_TakeFault(p, coyotos_Process_FC_MalformedSyscall, 0);
    if (rbound) {
      uva_t base = get_rcv_pw(p, IPW_RCVPTR);
      /** @bug doesn't catch wrapping.  How about:
       * !(vm_valid_uva(p, base + slen - 1) && base <= base + slen - 1)
       * or a vm_valid_uva_range(p, base, slen)?
       */
      if (! (vm_valid_uva(p, base) && vm_valid_uva(p, base + rbound - 1)) )
	proc_TakeFault(p, coyotos_Process_FC_MalformedSyscall, 0);
    }

    if (slen) {
      uva_t base = get_pw(p, IPW_SNDPTR);
      /** @bug doesn't catch wrapping.  How about:
       * !(vm_valid_uva(p, base + slen - 1) && base <= base + slen - 1)
       * or a vm_valid_uva_range(p, base, slen)?
       */
      if (! (vm_valid_uva(p, base) && vm_valid_uva(p, base + slen - 1)) )
	proc_TakeFault(p, coyotos_Process_FC_MalformedSyscall, 0);
    }

    /* There are no other in-bound parameters that can be bad by
       virtue of invalidity. It is still possible to fault while
       accessing them. */

    Endpoint *ep = 
      cap_prepare_for_invocation(&invParam, invParam.iCap.cap,
				 !(ipw0 & IPW0_NB), false);

    /** @bug Invoking an entry cap to ourselves ought to work, but
     * is not handled here. If we ever really care we can handle
     * that in the ct_Entry handler. */

    /* We cherry pick the IPC case here, because we really want that
       case to be straight-line in the I-cache. The alternative is
       to pass the EP returned by cap_prepare_for_invocation() into
       the handler along with the InvParam structure. */
    if (ep) {
      assert(invParam.invokee);

      uintptr_t invokee_ipw0 = get_icw(invParam.invokee);
      bool nonBlock = (ipw0 & IPW0_NB);

      /* Marshall receive capability locations. */
      if (invokee_ipw0 & IPW0_AC) {
	for (size_t i = 0; i <= IPW0_LRC(invokee_ipw0); i++)
	  if (!proc_marshall_dest_cap(invParam.invokee, 
				      get_rcv_cap(invParam.invokee, i),
				      &invParam.destCap[i], nonBlock))
	    goto receive_phase;
      }
      obhdr_dirty(&invParam.invokee->hdr);

      switch(IPW0_LDW(ipw0)) {
      case 7: set_pw(invParam.invokee, IPW_DW0+7, get_pw(p, IPW_DW0+7));
      case 6: set_pw(invParam.invokee, IPW_DW0+6, get_pw(p, IPW_DW0+6));
      case 5: set_pw(invParam.invokee, IPW_DW0+5, get_pw(p, IPW_DW0+5));
      case 4: set_pw(invParam.invokee, IPW_DW0+4, get_pw(p, IPW_DW0+4));
      case 3: set_pw(invParam.invokee, IPW_DW0+3, get_pw(p, IPW_DW0+3));
      case 2: set_pw(invParam.invokee, IPW_DW0+2, get_pw(p, IPW_DW0+2));
      case 1: set_pw(invParam.invokee, IPW_DW0+1, get_pw(p, IPW_DW0+1));
	break;
      }
      
      if (IPW_DW0 != IPW_ICW) 
	set_pw(invParam.invokee, IPW_DW0, get_pw(p, IPW_DW0));

      /* Receiver in proper state. IPC will succeed unless there is a
       * payload transfer fault during string transfer.
       */

      /* Assuming we make it out of this, the following is what we
	 will report to the invokee: */
      uintptr_t opw0 = get_icw(invParam.invokee) & IPW0_PRESERVE;
      opw0 |= ((ipw0 & IPW0_LDW_MASK) |
	       (ipw0 & IPW0_LSC_MASK) |
	       (ipw0 & (IPW0_SC|IPW0_EX)));


      /* String transfer. This part can have very puzzling results
	 when invokee==invoker, but those results are correct. */
      uint32_t slen = get_pw(p, IPW_SNDLEN); /* xmit str len */

      set_pw(invParam.invokee, OPW_SNDLEN, slen);
      if (slen) {
	uva_t src = get_pw(p, IPW_SNDPTR);

	uint32_t bound = get_rcv_pw(invParam.invokee, IPW_RCVBOUND);
	slen = min(slen, bound);

	uva_t dest = get_rcv_pw(invParam.invokee, IPW_RCVPTR);

	while (slen) {
	  uva_t target_page_va = dest & ~COYOTOS_PAGE_ADDR_MASK;
	  uva_t next_target_page_start = target_page_va + COYOTOS_PAGE_SIZE;
	  size_t count = min((next_target_page_start - target_page_va), slen);

	  FoundPage pgInfo;
	  coyotos_Process_FC fc = 
	    proc_findDataPage(invParam.invokee, target_page_va, true, false, &pgInfo);
	  assert(fc == 0);

	  obhdr_dirty(&pgInfo.pgHdr->mhdr.hdr);

	  if (fc) {
	    if (ipw0 & IPW0_NB) {
	      /* IPW0_NB set on exit means string was truncated. Set
		 output length to actual number of bytes
		 transferred. We cannot back out in this case,
		 because this is likely to be a server replying with
		 NB=1 to a client with CW=1, and if we back out
		 there is no way the client will ever run again. */
	      opw0 |= IPW0_NB;
	      set_pw(invParam.invokee, OPW_SNDLEN, 
		     (target_page_va > dest) ? target_page_va : 0);
	      goto xfer_caps;
	    }

	    proc_SetFault(invParam.invokee, fc, target_page_va);

	    /* Set the target process running so that it can clear
	       the fault. Insert them at front. In effect we are
	       donating our slice in order to let them clear their
	       fault. */
	    p->state.runState = PRS_RUNNING;
	    rq_add(&mainRQ, invParam.invokee, true);

	    /* Restart the transaction so that we will end up placed
	       on their rcvWaitQ queue. */
	    sched_restart_transaction();
	  }

	  kpa_t dest_pa = pgInfo.pgHdr->pa;
	  memcpy_vtop(dest_pa, (void *) src, count);

	  dest += count;
	  src += count;
	  slen -= count;
	}
      }

    xfer_caps:
      /* Transfer Capabilities. */

      /* Actual transfer. Order of operation matters, because
	 resulting permutation must be consistent when
	 invokee==invoker. Surprisingly, cap_set() handles this case
	 properly. */

      if (ipw0 & IPW0_SC) {
	if (invokee_ipw0 & IPW0_AC) {
	  size_t ncap = min(IPW0_LSC(ipw0)+1, IPW0_LRC(invokee_ipw0)+1);
	  for (size_t i = 0; i < ncap; i++)
	    cap_set(invParam.destCap[i].cap, invParam.srcCap[i].cap);
	}
      }

      /* At this point we have passed the last point where we might
       * back out. Check if we need to bump the PP on the endpoint we
       * invoked. 
       */
	
      if (ep->state.pm)
	ep->state.protPayload++;

      /* Transfer PP of invoked capability. */
      set_pw(invParam.invokee, OPW_PP, invParam.iCap.cap->u1.protPayload);

      /* Transfer endpoint ID of invoked endpoint. */
      set_epID(invParam.invokee, ep->state.endpointID);

      /* Mark the send phase done. Some bits of invokee ipw0 must
	 not be smashed yet. IPW0_RP must be clear, because invokee
	 has now finished their receive phase. It will get cleared
	 here because we did not set it above. */
      set_pw(invParam.invokee, 0, opw0);

      /* Careful! The local variable 'ipw0' is now stale if
	 invokee==p, but we still need it if invokee != p */

      /* Invocation is complete. Set invokee running. If
	 invokee==invoker, this is a no-op. */
      invParam.invokee->state.runState = PRS_RUNNING;

      assert((ipw0 & IPW0_RP) || (invParam.invokee != p));

      if ((ipw0 & IPW0_RP) == 0) {
	/* Sender is skipping receive phase, which means we wish to
	 * keep executing. Place invokee on ready queue, but do not
	 * donate our slice to them.
	 *
	 * Note we know in this case that invParam.invokee!=p */
	rq_add(&mainRQ, invParam.invokee, false);
	return;
      }
      else if (invParam.invokee == p) {
	return;
      }
      else {
	/* We are going to execute a receive phase. Donate our slice
	 * by placing invokee at FRONT of ready queue. Later, when
	 * we enter our receive phase, the invokee will resume.
	 *
	 * Note this is the ONLY case that actually goes to
	 * receive_phase, and in this case 'ipw0' is still good. */
	rq_add(&mainRQ, invParam.invokee, true);
	goto receive_phase;
      }
    }
    else {
      /* kernel capability invocation */

      /* Work out who the invokee is.
       *
       * We always say "non blocking" because the kernel always
       * returns as if with NB set.
       *
       * Allow return to self, because that is the overwhelmingly
       * common case.
       *
       * BUG: We have not fabricated the outgoing entry capability
       * yet, so the following test will fail if the endpoint PM bit
       * is set...
       */
      Endpoint *ep = 
	cap_prepare_for_invocation(&invParam, invParam.srcCap[0].cap,
				   !(ipw0 & IPW0_NB), true);

      uint32_t pp = 0;

      /* It is possible that there is no invokee, but if
       * there is, we need to check some things.
       *
       * Issue: pre-probing the reply string buffer */
      if (ep) {
	assert(invParam.invokee);

	pp = invParam.srcCap[0].cap->u1.protPayload;

	uintptr_t invokee_ipw0 = get_icw(invParam.invokee);
	if ((invokee_ipw0 & IPW0_CW) &&
	    (get_rcv_epID(invParam.invokee) != ep->state.endpointID)) {
	  invParam.invokee = 0;
	}
	else if (invokee_ipw0 & IPW0_AC) {
	  for (size_t i = 0; i <= IPW0_LRC(invokee_ipw0); i++) {
	    if (!proc_marshall_dest_cap(invParam.invokee, 
					get_rcv_cap(invParam.invokee, i),
					&invParam.destCap[i], 
					true /* Always nonBlock */))
	      goto receive_phase;
	  }
	}
      }
      else
	assert(invParam.invokee == 0);

      if ((IPW_DW0 == IPW_ICW) && (IPW0_LDW(ipw0) < 1)) {
	sched_commit_point();
	InvErrorMessage(&invParam, RC_coyotos_Cap_RequestError);
      }
      else {
	invParam.ipw0   = ipw0;
	invParam.sndLen = get_pw(invParam.invoker, IPW_SNDLEN);
	invParam.next_idw = (IPW_DW0 == IPW_ICW) ? (IPW_DW0 + 1) : IPW_DW0;
	invParam.next_odw = (IPW_DW0 == IPW_ICW) ? (IPW_DW0 + 1) : IPW_DW0;

	invParam.opCode = get_iparam32(&invParam);

	capHandler[invParam.iCap.cap->type](&invParam);
      }

      if (invParam.invokee) {
	/* Copy out the results to the invokee: */

	assert(ep);
	
	if (ep->state.pm)
	  ep->state.protPayload++;

	/* Transfer PP of invoked capability. */
	set_pw(invParam.invokee, OPW_PP, pp);

	/* Transfer endpoint ID of invoked endpoint. */
	set_epID(invParam.invokee, ep->state.endpointID);

	switch(IPW0_LDW(invParam.opw[0])) {
	case 7: set_pw(invParam.invokee, IPW_DW0+7, invParam.opw[7]);
	case 6: set_pw(invParam.invokee, IPW_DW0+6, invParam.opw[6]);
	case 5: set_pw(invParam.invokee, IPW_DW0+5, invParam.opw[5]);
	case 4: set_pw(invParam.invokee, IPW_DW0+4, invParam.opw[4]);
	case 3: set_pw(invParam.invokee, IPW_DW0+3, invParam.opw[3]);
	case 2: set_pw(invParam.invokee, IPW_DW0+2, invParam.opw[2]);
	case 1: set_pw(invParam.invokee, IPW_DW0+1, invParam.opw[1]);
	  break;
	}
	/* Must re-fetch, in case operation was SetRegs or something
	   else that changed the invokee register set. */
	uintptr_t opw0 = get_icw(invParam.invokee) & IPW0_PRESERVE;

	opw0 |= invParam.opw[0];

	if (opw0 & IPW0_SC) {
	  if (opw0 & IPW0_AC) {
	    size_t ncap = min(IPW0_LSC(invParam.opw[0])+1, IPW0_LRC(opw0)+1);
	    for (size_t i = 0; i < ncap; i++)
	      cap_set(invParam.destCap[i].cap, &invParam.srcCap[i].theCap);
	  }
	}

	set_pw(invParam.invokee, 0, opw0);

	/* If invokee == invoker, send phase and receive phase are
	 * already marked complete because we have just rewritten
	 * opw0. Otherwise, we need to mark the invoker's send phase
	 * done here. */

	if (invParam.invoker == invParam.invokee) /* implies invoker != 0 */
	  goto receive_phase;

	/* invoker 0 or not same as invokee */
	bool donate = true;

	if (invParam.invoker) {
	  /* Operation may have been a register set, in which case we
	     need to re-fetch ipw0. */
	  ipw0 = get_icw(invParam.invoker);
	  set_pw(invParam.invoker, 0, (ipw0 & ~IPW0_SP));
	  if ((ipw0 & IPW0_RP) == 0)
	    donate = false;
	}

	invParam.invokee->state.runState = PRS_RUNNING;
	rq_add(&mainRQ, invParam.invokee, donate);
      }

      if (invParam.invoker == 0)
	sched_abandon_transaction();
    }
  }

 receive_phase:
  /*******************************************************************
   *
   *                 PHASE 2: Receive
   *
   * This is executed if send phase completes without incident. Note
   * that some operations may either destroy the invoker or cause it
   * to become malformed. In these cases, invParam.invoker will have
   * been nullified. That is why we re-test it here.
   *
   * Because the operation may have been SetFixRegs(), the ipw0 value
   * aquired above may be stale. it is therefore necessary to re-fetch
   * ipw0 here to see if we are still going to do the receive phase.
   *
   *******************************************************************/

  if (invParam.invoker == 0)
    return;

  ipw0 = get_icw(invParam.invoker);

  if (ipw0 & IPW0_RP) {
    invParam.invoker->state.runState = PRS_RECEIVING;

    if (((ipw0 & IPW0_CW) == 0) && invParam.invoker->state.softInts)
      proc_DeliverSoftInts(invParam.invoker);

    sq_WakeAll(&invParam.invoker->rcvWaitQ, false);

    /* Let someone else run. */
    sched_abandon_transaction();
  }

  /*******************************************************************
   *
   *                 PHASE 3: Copyout
   *
   * The receive phase preserves the IPW0_CO bit, so it is not
   * necessary to re-fetch ipw0 here.
   *
   *******************************************************************/

  if (ipw0 & IPW0_CO)
    copyout_soft_parameters(p);

  /* Syscall has completed. It is therefore okay to return via fast
     system call return if this architecture has one. */
  p->issues |= pi_SysCallDone;

  return;
}

void
proc_syscall(void)
{
  Process *p = MY_CPU(current);
  uintptr_t ipw0 = get_icw(p);

  uint32_t syscall_nr = IPW0_NR(ipw0);

  switch(syscall_nr) {
  case sc_Yield:
    sched_abandon_transaction();
    /* Not reached */

  case sc_LoadCap:
    {
      capreg_t reg = to_capreg(IPW0_CAPREG(ipw0));
      uintptr_t ipw1 = get_pw(p, 1);
      caploc_t where = to_caploc(ipw1);
      proc_load_cap(p, reg, where);
      return;
    }
  case sc_StoreCap:
    {
      capreg_t reg = to_capreg(IPW0_CAPREG(ipw0));
      uintptr_t ipw1 = get_pw(p, 1);
      caploc_t where = to_caploc(ipw1);
      proc_store_cap(p, reg, where);
      return;
    }

  case sc_InvokeCap:
    return proc_invoke_cap();

  default:
    {
      proc_SetFault(p, coyotos_Process_FC_MalformedSyscall, 0);
      return;
    }
  }
}

bool proc_isReceiving(Process *p)
{
  bug("How the hell should I know if the process is receiving?\n");
}
