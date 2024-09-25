#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include "StaP.h"
#include "HostLink.h"
#include "AlphaLink.h"
#include "Console.h"
#include "Scheduler.h"

bool hostConnected;
extern DgLink_t hostLink, telemLink;

#define COMM_BLOCKSIZE      (1<<5)

void blinkTask(void)
{
  static int i = 0;

  if(i & 1)
    STAP_LED1_ON;
  else
    STAP_LED1_OFF;

  i++;
}

int reportType = 0;

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

VP_TIME_MICROS_T serialTaskRadio(void)
{
  uint8_t buffer[COMM_BLOCKSIZE];

  randomEntropyInput(STAP_ENTROPY_SRC);
  
  uint8_t len = STAP_LinkGet(GS_Link_RadioRX, buffer, sizeof(buffer));
  
  datagramRxInput(&telemLink, (const uint8_t*) buffer, len);

  return 0;
}

#if TEST == 1
VP_TIME_MICROS_T textTask(void)
{
  static int i = 0;

  STAP_FailSafe;
  
  for(;;)
    consolePrintfLn("Serial TX FailSafe %d. ", i++);
}
#endif

#if !defined(TEST) || TEST == 2
VP_TIME_MICROS_T textTask(void)
{
  static int i = 0;

  consolePrintfLn("Serial TX from a task %d. ", i++);
}
#endif

#if !defined(TEST) || TEST == 3
VP_TIME_MICROS_T transmitTestTask(void)
{
  static uint64_t i = 0;

  consolePrintfLn("DG TX test %d", i);
  
  datagramTxStart(&telemLink, ALN_TELEMETRY);
  datagramTxOut(&telemLink, (const uint8_t*) &i, sizeof(i));
  datagramTxEnd(&telemLink);

  i++;
}
#endif

struct TaskDecl StaP_TaskList[] = {
  TASK_BY_FREQ("Blink", 0, blinkTask, 2, 1<<8)

#if !defined(TEST) || TEST == 1 || TEST == 2
  ,TASK_BY_FREQ("Text", 0, textTask, 1, 1<<8)
#endif

#if !defined(TEST) || TEST == 3
  ,TASK_BY_FREQ("TxTest", 0, transmitTestTask, 100, 1<<8)
#endif

#if TEST != 0 && TEST != 1
  ,TASK_BY_SERIAL("HostRX", 2, serialTaskHost, GS_Link_HostRX, 3<<8)
  ,TASK_BY_SERIAL("TelemRX", 2, serialTaskRadio, GS_Link_RadioRX, 3<<8)
#endif
};

const int StaP_NumOfTasks
  = sizeof(StaP_TaskList)/sizeof(struct TaskDecl);
