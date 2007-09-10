These test cases were used for early bootstrap development. They are
run by hand under an emulator or a debugger. They should be run after
the kernel and the CCS tree are built.

We checked them in the following order:

HLT

   This test case simply executed a halt instruction, which is illegal
   in user mode. The purpose of this test case is to confirm that we
   actually make it all the way to user mode. If this test is
   "successful", the process will immediately execute a GP fault.

BPT

   Similar to halt, but executes a breakpoint instruction. Since this
   generates a different fault code, it helps to confirm that we
   really are executing something.

BPT_MIDPAGE

   Executes a breakpoint instruction after executing a few no-ops.
   This causes a fault at a different PC, so we know that we are
   running instructions.

The following cases are intended to be executed with breakpoints on
various bits of kernel code so that we can see them happen:

INC_EAX

   Infinite loop incrementing EAX. When the timer interrupt goes
   off we can see that EAX has changed.

LDCAP (STCAP)

   Performs a copy capability capability pseudo instruction, loading
   a capability from a capability page into a register (storing a
   capability to a capability page from a register). Lets us check that
   the system call path is operating correctly.

RECEIVE

   Tests entering a receive phase.

IDENTIFY

   Tests running the getAllegedType operation on the null capability.
