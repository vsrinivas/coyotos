#include <idl/coyotos/Cap.h>
#include <idl/coyotos/Cap.server.h>

int main(int argc, char *argv[])
{
  IDL_Environment idlenv = { 1 };
  coyotos_Cap_AllegedType ty;

  if (coyotos_Cap_getType(0, &ty, &idlenv))
    return 0;
  return -1;
}
