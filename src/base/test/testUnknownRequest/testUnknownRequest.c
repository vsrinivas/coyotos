/*
 * Copyright (C) 2007, The EROS Group, LLC
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

#include <string.h>
#include <stdlib.h>

#include <coyotos/captemp.h>
#include <coyotos/runtime.h>
#include <idl/coyotos/SpaceBank.h>
#include <idl/coyotos/Verifier.h>
#include <coyotos/kprintf.h>

int
main(int argc, char *argv[])
{
  bool result = false;

  kprintf(CR_APP0, "Attempting bad request\n");
  if (coyotos_Verifier_verifyYield(CR_SPACEBANK, CR_NULL, &result)) {
    kprintf(CR_APP0, "Bad request succeeded?!\n");
  } else {
    errcode_t errCode = IDL_exceptCode;
    if (errCode != RC_coyotos_Cap_UnknownRequest) {
      kprintf(CR_APP0,
              "unexpected error code: %d expected UnknownRequest: %d\n",
              errCode, RC_coyotos_Cap_UnknownRequest);
    } else {
      kprintf(CR_APP0,
              "Bad request failed as expected.\n");
    }
  }

  return 0;
}

