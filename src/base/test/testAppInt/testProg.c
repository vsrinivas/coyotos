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

#include <inttypes.h>

#include "testAppInt.h"
#include <idl/coyotos/AppNotice.h>
#include <idl/coyotos/KernLog.h>
#include <idl/test/testAppInt.h>

#include <coyotos/runtime.h>
#include <string.h>

static void
log_message(const char *message)
{
  size_t len = strlen(message);
  if (len > 255) 
    len = 255;
  
  coyotos_KernLog_logString str = { 
    .max = 256, .len = len, .data = (char *)message
  };
  
  (void) coyotos_KernLog_log(testAppInt_APP_KERNLOG, str);
}

void
do_get_appnotice(bool andSendMessage)
{
  _INV_coyotos_AppNotice_postNotice inv;

  inv.pb.pw[1] = OC_test_testAppInt_testWhileOpenWait;
  inv.pb.rcvPtr = 0;
  inv.pb.rcvBound = 0;
  inv.pb.sndPtr = 0;
  inv.pb.sndLen = 0;
  inv.pb.u.invCap = testAppInt_APP_OTHER;
  inv.pb.sndCap[0] = CR_NULL;

  inv.pb.pw[0] = IPW0_MAKE_NR(sc_InvokeCap)|IPW0_RP|IPW0_NB|IPW0_CO;  
  if (andSendMessage) 
    inv.pb.pw[0] |= IPW0_SP | IPW0_MAKE_LDW(1) | IPW0_SC | IPW0_MAKE_LSC(0);

  invoke_capability(&inv.pb);

  if (inv.pb.epID != -(uint64_t)1) {
    log_message("bad EPID, expected appnotice");
    *(uint32_t *)0 = 0;
  }
  if (inv.pb.pw[1] != OC_coyotos_AppNotice_postNotice) {
    log_message("bad order code");
    *(uint32_t *)0 = 0;
  }

  char mess[256];
  strcpy(mess, "Received notices: 0x");
  size_t idx = strlen(mess) + 8;
  size_t offset = 0;
  uint32_t notices = inv.pb.pw[2];

  mess[idx + 1] = 0;
  mess[idx] = '\n';
  
  for (offset = 0; offset < 8; offset++) {
    uint8_t val = (notices & 0xf);
    notices >>= 4;

    /* ah, the wonders of hex decoding */
    mess[idx - offset - 1] =
      (val < 10) ? '0' + val : 'a' + val - 10;
  }
  log_message(mess);
}

int
main(int argc, char *argv[])
{
  volatile uint64_t i = 0;

  while (i < 0x1000000)
    i++;

  log_message("finished spinning.  Getting notices\n");
  
  do_get_appnotice(false);

  log_message("Doing open wait test\n");

  do_get_appnotice(true);

  log_message("Doing closed wait test\n");

  test_testAppInt_testWhileClosedWait(testAppInt_APP_OTHER);

  log_message("closed wait finished, checking for notices\n");

  do_get_appnotice(false);

  log_message("test complete.");

  *(uint32_t *)0 = 0;

  return 0;
}

