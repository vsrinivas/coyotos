#!/bin/ksh
# run qemu in background so that it will not receive kbd ^C as INT
background_cmd() {
  "$@"&
}

background_cmd qemu -s -fda grubdisk
QEMU_PID=$!
#echo "QEMU_PID is ${QEMU_PID}"
sleep 1
if [ -r $1.gdb ]
then
    echo "Using $1.gdb"
    xterm -e gdb -x $1.gdb ../../kernel/BUILD/coyotos
else
    echo "Using testme.gdb"
    xterm -e gdb -x default.gdb ../../kernel/BUILD/coyotos
fi
kill -9 ${QEMU_PID}
