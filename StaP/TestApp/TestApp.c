#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include "StaP.h"
#include "HostLink.h"
#include "Console.h"
#include "Scheduler.h"

#define HEARTBEAT_HZ            3

bool hostConnected;
extern DgLink_t hostLink;

#define COMM_BLOCKSIZE      (1<<5)

void blinkTask(void)
{
  static int i = 0;

  if(i & 1)
    STAP_LED1_ON;
  else
    STAP_LED1_OFF;
  
   if(i & 2)
    STAP_LED0_ON;
  else
    STAP_LED0_OFF;
  
  i++;
}

int reportType = 1;

void commandHandler(const uint8_t *data, size_t size)
{
  int i = 0, num = 0;

  while(i < size && isdigit(data[i]))
    num = num*10 + (data[i++] - '0');

  reportType = num;
}

void hostLinkHandler(const uint8_t *data, int size)
{
  if(!hostConnected) {
    hostConnected = true;
    consoleNoteLn("Host CONNECTED");
  }

  if(size < 1)
    return;
  
  switch(data[0]) {
  case HL_CONSOLE:
    commandHandler(&data[1], size - 1);
    break;

  case HL_HEARTBEAT:
    // gnssReport();
    break;
    
  default:
    break;
  }
}

VP_TIME_MICROS_T serialTaskHost(void)
{
  uint8_t buffer[COMM_BLOCKSIZE];

  randomEntropyInput(STAP_ENTROPY_SRC);
  
  uint8_t len = STAP_LinkGet(GS_Link_HostRX, buffer, sizeof(buffer));
  
  datagramRxInput(&hostLink, (const uint8_t*) buffer, len);

  return 0;
}

VP_TIME_MICROS_T textTask(void)
{
  consolePrintf("blobeti BLOB blob blob. ");
}

struct TaskDecl StaP_TaskList[] = {
  TASK_BY_FREQ("Blink", 0, blinkTask, 2, 1<<8),
  TASK_BY_FREQ("Text", 0, textTask, 30, 1<<8),
  TASK_BY_SERIAL("HostRX", 2, serialTaskHost, GS_Link_HostRX, 3<<8)
};

const int StaP_NumOfTasks
  = sizeof(StaP_TaskList)/sizeof(struct TaskDecl);
