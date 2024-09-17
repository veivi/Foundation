#include "FreeRTOS.h"
#include "task.h"
#include "Scheduler.h"
#include "Setup.h"
#include "StaP.h"

int main( void )
{
  STAP_Initialize();
  
  portDISABLE_INTERRUPTS();

  mainLoopSetup();

  StaP_SchedulerStart();

  // Should never get here

  STAP_Panic(STAP_ERR_FELLTHROUGH);
    
  return 0;
}

