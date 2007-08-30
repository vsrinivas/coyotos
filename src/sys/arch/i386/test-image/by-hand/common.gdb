target remote localhost:1234

define printarg
  print $arg0
end

define print-ipw0
  # invoke capability:
  if (($arg0 & 0xf) == 0)
    printf "invoke ldw=%d lsc=%d lrc=%d", (($arg0 >> 3) & 0x7), \
       (($arg0 >> 6) & 0x3),  (($arg0 >> 8) & 0x3)
    if ($arg0 & 0x800)
      printf " SG"
    end
    if ($arg0 & 0x1000)
      printf " AS"
    end
    if ($arg0 & 0x2000)
      printf " NB"
    end
    if ($arg0 & 0x4000)
      printf " CW"
    end
    if ($arg0 & 0x8000)
      printf " RP"
    end
    if ($arg0 & 0x10000)
      printf " SP"
    end
    if ($arg0 & 0x20000)
      printf " RC"
    end
    if ($arg0 & 0x40000)
      printf " SC"
    end
    if ($arg0 & 0x80000)
      printf " AC"
    end
    if ($arg0 & 0x100000)
      printf " CO"
    end
    if ($arg0 & 0x200000)
      printf " EX"
    end
    printf "\n"
  end

  # ldcap
  if (($arg0 & 0xf) == 2)
    printf "ldcap %d <- <src>\n", ($arg0 >> 8))
  end

  # stcap
  if (($arg0 & 0xf) == 3)
    printf "stcap %d -> <dest>\n", ($arg0 >> 8)
  end

  # yield
  if (($arg0 & 0xf) == 4)
    printf "yield\n"
  end
end

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

echo Utility functions loaded\n
