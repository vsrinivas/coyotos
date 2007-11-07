target extended-remote localhost:1234

source util.gdb

b *0x100000
commands
  silent
  echo \nNow sitting At first kernel instruction.\n
  d 1
  echo \ \ Deleting initial breakpoint.\n
  echo \ \ Adding breakpoint on halt():\n
  b halt
end

c

echo Coyotos Debugger Support loaded.\n
