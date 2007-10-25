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

echo Coyotos utility functions loaded.\n
