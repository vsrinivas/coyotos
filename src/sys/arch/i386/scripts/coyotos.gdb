target extended-remote localhost:1234

define print-message-buffer
  set pagination off
  # Use printf to bypass string length limits
  if (message_buffer_wrapped)
    printf "%s", message_buffer_end+2
    printf "%s", message_buffer
  else
    printf "%s", message_buffer
  end
  set pagination on
end

set $done_debugger_wait = 0
define hook-stop
  if $done_debugger_wait == 0 && debugger_wait != 0
    echo Clearing debugger_wait:
    printf "  debugger_wait <- %d\n", (debugger_wait = 0)
    set $done_debugger_wait = 1
  end
end  

echo Coyotos Debugger Support loaded.\n
hook-stop

# continue if the process wasn't waiting for us...
if $done_debugger_wait == 0
  c
end
