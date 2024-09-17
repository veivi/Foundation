#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "Scheduler.h"
#include "Console.h"
#include "StaP.h"

// #define BLOB             2   // This defines the blobetiness level

TaskHandle_t signalOwner[StaP_NumOfSignals];

void STAP_Signal(StaP_Signal_T sig)
{
  if(signalOwner[sig])
    xTaskNotifyIndexed(signalOwner[sig], 1, STAP_SignalSet(sig), eSetBits);
}

bool STAP_SignalFromISR(StaP_Signal_T sig)
{
    UBaseType_t yield = false;
  
  if(signalOwner[sig])
    xTaskNotifyIndexedFromISR(signalOwner[sig], 1, STAP_SignalSet(sig), eSetBits, &yield);

  return yield;
}

StaP_SignalSet_T STAP_SignalWaitTimeout(StaP_SignalSet_T mask,
					  VP_TIME_MILLIS_T timeout)
{
  StaP_SignalSet_T status = 0;

  status = ulTaskNotifyValueClearIndexed(xTaskGetCurrentTaskHandle(), 1, mask);

  while(!(mask & status)) {
    if(xTaskNotifyWaitIndexed(1, 0, mask, &status,
			    VP_MILLIS_FINITE(timeout)
			    ? pdMS_TO_TICKS(timeout) : portMAX_DELAY)
      == pdFALSE)
      // Timed out
      break;
  }

  return status & mask;
}

StaP_SignalSet_T STAP_SignalWait(StaP_SignalSet_T mask)
{
  return STAP_SignalWaitTimeout(mask, VP_TIME_MILLIS_MAX);
}

static void periodTaskWrapper( void *pvParameters )
{
  struct TaskDecl *appTask = (struct TaskDecl*) pvParameters;
  
  if(appTask->type != StaP_Task_Period)
    STAP_Panic(STAP_ERR_TASK_TYPE);
    
  if(!appTask->code)
    STAP_Panic(STAP_ERR_TASK_CODE);
      
  for(;;) {
    (*appTask->code)();
    
    if(appTask->typeSpecific.period.period)
      vTaskDelay(pdMS_TO_TICKS(appTask->typeSpecific.period.period));
    else
      vTaskDelay(portMAX_DELAY);
  }
}

static void signalTaskWrapper( void *pvParameters )
{
  struct TaskDecl *appTask = (struct TaskDecl*) pvParameters;
  
  if(appTask->type != StaP_Task_Signal)
    STAP_Panic(STAP_ERR_TASK_TYPE);
    
  if(!appTask->typeSpecific.signal.signal)
    STAP_Panic(STAP_ERR_NO_SIGNAL);
    
  if(!appTask->code)
    STAP_Panic(STAP_ERR_TASK_CODE);
      
  for(;;) {
    if(appTask->typeSpecific.signal.timeOut)
      // We have a timeout specification
      STAP_SignalWaitTimeout(STAP_SignalSet(appTask->typeSpecific.signal.signal),
			     appTask->typeSpecific.signal.timeOut);
    else
      STAP_SignalWait(STAP_SignalSet(appTask->typeSpecific.signal.signal));
    
    (*appTask->code)();
  }
}

static void serialTaskWrapper( void *pvParameters )
{
  struct TaskDecl *appTask = (struct TaskDecl*) pvParameters;
  
  if(appTask->type != StaP_Task_Serial)
    STAP_Panic(STAP_ERR_TASK_TYPE);
    
  if(!appTask->typeSpecific.serial.link)
    STAP_Panic(STAP_ERR_NO_LINK);
    
  if(!appTask->code)
    STAP_Panic(STAP_ERR_TASK_CODE);
      
  for(;;) {
    if(!VPBUFFER_GAUGE(StaP_LinkTable[appTask->typeSpecific.serial.link].buffer)) {
      // Nothing in the buffer, need to wait

      STAP_SignalWait(STAP_SignalSet(StaP_LinkTable[appTask->typeSpecific.serial.link].signal));

      if(appTask->typeSpecific.serial.latency) {
	// We tolerate some latency, adjust watermark and wait until
	// it's met or latency exceeded

	StaP_LinkTable[appTask->typeSpecific.serial.link].buffer.watermark =
	  StaP_LinkTable[appTask->typeSpecific.serial.link].buffer.mask>>1;
      
	STAP_SignalWaitTimeout(STAP_SignalSet(StaP_LinkTable[appTask->typeSpecific.serial.link].signal), appTask->typeSpecific.serial.latency);

	StaP_LinkTable[appTask->typeSpecific.serial.link].buffer.watermark = 0;
      }
    }
    
    (*appTask->code)();
  }
}

#ifdef BLOB

void blobTask(void *params)
{
  int i = 0;

  for(;;) {
    if(params)
      consolePrintf(" Blobeti %d", vpTimeMillis());
    else
      consolePrintf(" BlobBlob %d ", vpTimeMillis());
    
    // STAP_DelayMillis(1);
   }    
}

void flushTask(void *params)
{
  for(;;) {
    consoleFlush();
    vTaskDelay(pdMS_TO_TICKS(35));
  }
}

#endif

void StaP_SchedulerStart( void )
{
  TaskHandle_t handle = NULL;
  int i = 0;

#ifdef BLOB

  if(xTaskCreate(blobTask, "Blib1", configMINIMAL_STACK_SIZE << 2,
		 (void*) 0, tskIDLE_PRIORITY + 1, &handle) != pdPASS)
    STAP_Panic(STAP_ERR_TASK_CREATE);
#if BLOB > 1
  if(xTaskCreate(blobTask, "Blob2", configMINIMAL_STACK_SIZE << 2,
		 (void*) 1, tskIDLE_PRIORITY + 1, &handle) != pdPASS)
    STAP_Panic(STAP_ERR_TASK_CREATE+1);
#endif

#if BLOB > 2
  if(xTaskCreate(flushTask, "Blob3", configMINIMAL_STACK_SIZE << 2,
		 (void*) 0, tskIDLE_PRIORITY + 2, &handle) != pdPASS)
    STAP_Panic(STAP_ERR_TASK_CREATE+2);
#endif
#else

  // Clear the signal owner table
  
  memset((void*) signalOwner, 0, sizeof(signalOwner));
	 
  while(taskList[i].name) {
#ifdef ONLY_TASK_NAME
    if(strcmp(ONLY_TASK_NAME, taskList[i].name)) {
      i++;
      continue;
    }
#endif

    uint16_t userStack = taskList[i].stackSize;

    if(userStack < STAP_TASK_MIN_STACK)
      userStack = STAP_TASK_MIN_STACK;

    switch(taskList[i].type) {
    case StaP_Task_Period:
      if(xTaskCreate(periodTaskWrapper, taskList[i].name,
		     configMINIMAL_STACK_SIZE + userStack,
		     (void*) &taskList[i],
		     tskIDLE_PRIORITY + 1 + taskList[i].priority,
		     &handle) != pdPASS)
	STAP_Panic(STAP_ERR_TASK_CREATE+i);
      break;
      
    case StaP_Task_Signal:
      if(xTaskCreate(signalTaskWrapper, taskList[i].name,
		     configMINIMAL_STACK_SIZE + userStack,
		     (void*) &taskList[i],
		     tskIDLE_PRIORITY + 1 + taskList[i].priority,
		     &handle) != pdPASS)
	STAP_Panic(STAP_ERR_TASK_CREATE+i);

      signalOwner[taskList[i].typeSpecific.signal.signal] = handle;    
      break;
      
    case StaP_Task_Serial:
      if(xTaskCreate(serialTaskWrapper, taskList[i].name,
		     configMINIMAL_STACK_SIZE + userStack,
		     (void*) &taskList[i],
		     tskIDLE_PRIORITY + 1 + taskList[i].priority,
		     &handle) != pdPASS)
	STAP_Panic(STAP_ERR_TASK_CREATE+i);

      signalOwner[StaP_LinkTable[taskList[i].typeSpecific.serial.link].signal]
	= handle;    
      break;

    default:
      STAP_Panic(STAP_ERR_TASK_TYPE);
      break;
    }

    if(!handle)
      STAP_Panic(STAP_ERR_TASK_CREATE+i);

    taskList[i++].handle = handle;
  }
#endif

  vTaskStartScheduler();
}

void STAP_SchedulerReport(void)
{
  int i = 0;

  while(taskList[i].code) {
    if(taskList[i].handle) {
      configSTACK_DEPTH_TYPE watermark
	= uxTaskGetStackHighWaterMark2(taskList[i].handle);

      consolePrint(taskList[i].name);
      consoleTab(12);
      consolePrintUI(watermark);
      /*      consoleTab(20);
      consolePrintUI(ulTaskGetRunTimeCounter(taskList[i].handle));
      consolePrint("%");*/
      consoleNL();      
    }
    
    i++;
  }
  /*
  char buffer[1<<9];

  vTaskGetRunTimeStatistics(buffer, sizeof(buffer));
  consoleOut(buffer, strlen(buffer));
  */
}

