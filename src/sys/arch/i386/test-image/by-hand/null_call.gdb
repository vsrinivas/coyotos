source common.gdb

b *0x100000
commands
  silent
  echo \nNow sitting At first kernel instruction.\n
  d 1
  echo \ \ Deleting initial breakpoint.\n
  echo \ \ Adding breakpoints on halt(),IdleThisProcessor():\n
  b halt
  b IdleThisProcessor

  echo \ \ Adding breakpoint on proc_invoke_cap\n
  b proc_invoke_cap
end

continue

b maybe_kernel_iret
commands
  p/x *((uint32_t*)$sp)@16
end

b irq_UserFault if inProc==0
