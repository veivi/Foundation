/* This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

#include "platform.h"
//#include "build/debug.h"
//#include "drivers/serial.h"
//#include "drivers/serial_softserial.h"

//#include "fc/fc_init.h"

//#include "scheduler/scheduler.h"

#ifdef SOFTSERIAL_LOOPBACK
//serialPort_t *loopbackPort;
#endif

#include <AlphaPilot/AlphaPilot.h>
#include <AlphaPilot/Setup.h>
#include <AlphaPilot/Scheduler.h>

int main(void)
{
  STAP_Initialize();

  mainLoopSetup();

  /*
  int i = 0;
  
  while(1) {
    if(i++ & 1)
      STAP_LED0_ON;
    else
      STAP_LED0_OFF;

    STAP_LinkTalk(ALP_Link_HostTX);
    STAP_LinkPut(ALP_Link_HostTX, "Paska", 5, 1000);
    //    consolePrint("paska ");
    STAP_DelayMillis(500);
  }
  */
  
  StaP_SchedulerStart();

  // Should never get here

  STAP_Panic(STAP_ERR_FELLTHROUGH);
    
  return 0;
}

