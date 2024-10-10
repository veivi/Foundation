#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include "Console.h"
#include "StaP.h"
#include "Scheduler.h"
#include "FreeRTOS.h"
#include "task.h"

TaskHandle_t signalOwner[StaP_NumOfSignals];
extern int FreeRTOSUp;
volatile static VP_TIME_MICROS_T idleMicros;

uint16_t STAP_CPUIdlePermille(void)
{
  static bool initialized = false;
  static VP_TIME_MILLIS_T prev = 0;
  uint16_t result = 0;

  if(!initialized) {
    prev = vpTimeMillis();
    idleMicros = 0;
    initialized = true;
    result = 0;
  } else {
    result = idleMicros / ((VP_TIME_MICROS_T) VP_ELAPSED_MILLIS(prev));
    prev = vpTimeMillis();
    idleMicros = 0;
  }
  
  return result;  
}

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
    VP_TIME_MICROS_T callAgain = (*appTask->code)();
    
    if(!VP_MILLIS_FINITE(appTask->typeSpecific.period))
      // Period if infinite, wait forever
      for(;;)
	vTaskDelay(portMAX_DELAY);
    else if(appTask->typeSpecific.period)
      // Period is finite
      vTaskDelay(pdMS_TO_TICKS(appTask->typeSpecific.period));
    else
      // Period is zero, we go by what the code returned
      vTaskDelay(pdMS_TO_TICKS(callAgain / 1000));
  }
}

static void signalTaskWrapper( void *pvParameters )
{
  struct TaskDecl *appTask = (struct TaskDecl*) pvParameters;
  
  if(appTask->type != StaP_Task_Signal)
    STAP_Panic(STAP_ERR_TASK_TYPE);
    
  if(!appTask->typeSpecific.signal.id)
    STAP_Panic(STAP_ERR_NO_SIGNAL);
    
  if(!appTask->code)
    STAP_Panic(STAP_ERR_TASK_CODE);
      
  for(;;) {
    if(appTask->typeSpecific.signal.timeOut)
      // We have a timeout specification
      STAP_SignalWaitTimeout(STAP_SignalSet(appTask->typeSpecific.signal.id),
			     appTask->typeSpecific.signal.timeOut);
    else
      STAP_SignalWait(STAP_SignalSet(appTask->typeSpecific.signal.id));
    
    (*appTask->code)();
  }
}

static void serialTaskWrapper( void *pvParameters )
{
  struct TaskDecl *appTask = (struct TaskDecl*) pvParameters;
  StaP_LinkId_T link = appTask->typeSpecific.serial.link;
  
  VP_TIME_MICROS_T invokeAgain = 0;
  
  if(appTask->type != StaP_Task_Serial)
    STAP_Panic(STAP_ERR_TASK_TYPE);
    
  if(!link)
    STAP_Panic(STAP_ERR_NO_LINK);
    
  if(!appTask->code)
    STAP_Panic(STAP_ERR_TASK_CODE);

  // Wait until the link becomes usable
  
  while(!STAP_LinkIsUsable(link))
    STAP_DelayMillis(100);
    
  for(;;) {
    if(VPBUFFER_GAUGE(StaP_LinkTable[link].buffer)
       < (StaP_LinkTable[link].buffer.mask>>2)) {
      // Buffer near empty, might want to wait for a while

      VP_TIME_MILLIS_T timeout = VP_TIME_MILLIS_MAX;
      StaP_SignalSet_T sig = STAP_SignalSet(StaP_LinkTable[link].signal);

      if(invokeAgain) 
	// The task wants be unconditionally re-invoked at a specific time
	timeout = invokeAgain < 1000 ? 1 : (invokeAgain/1000);

      if(!VPBUFFER_GAUGE(StaP_LinkTable[link].buffer)) {
	// The buffer is empty, wait for the first char or the task-
	// specified timeout

	StaP_LinkTable[link].buffer.watermark = 0;

	// Wait until we time out or see something in the buffer
	
	do {
	  sig = STAP_SignalWaitTimeout
	    (STAP_SignalSet(StaP_LinkTable[link].signal), timeout);
	} while(sig && !VPBUFFER_GAUGE(StaP_LinkTable[link].buffer));
      } 
      
      if(sig) {
	// We didn't time out and now the buffer is no longer empty,
	// we need to consider the link-specific latency as a timeout
	// as we wait for more
	
	if(StaP_LinkTable[link].latency/1000 < timeout)
	  timeout = StaP_LinkTable[link].latency/1000;

	StaP_LinkTable[link].buffer.watermark =
	  StaP_LinkTable[link].buffer.mask>>1;

	// Wait until we time out or see lots of stuff in the buffer
	
	do {
	  sig = STAP_SignalWaitTimeout
	    (STAP_SignalSet(StaP_LinkTable[link].signal), timeout);
	} while(sig && VPBUFFER_GAUGE(StaP_LinkTable[link].buffer)
		<= StaP_LinkTable[link].buffer.watermark);
      } 	
    }
    
    // Invoke the code
    
    invokeAgain = (*appTask->code)();
  }
}

void StaP_SchedulerStart( void )
{
  TaskHandle_t handle = NULL;
  int i = 0;

  portDISABLE_INTERRUPTS();
  
  // Clear the signal owner table
  
  memset((void*) signalOwner, 0, sizeof(signalOwner));
	 
  while(i < StaP_NumOfTasks) {
#ifdef STAP_ONLY_TASK_NAME
    if(strcmp(ONLY_TASK_NAME, StaP_TaskList[i].name)) {
      i++;
      continue;
    }
#endif

    uint16_t userStack = StaP_TaskList[i].stackSize;

    if(userStack < STAP_TASK_MIN_STACK)
      userStack = STAP_TASK_MIN_STACK;

    userStack /= STAP_STACK_SIZE_UNIT;
    
    switch(StaP_TaskList[i].type) {
    case StaP_Task_Period:
      if(xTaskCreate(periodTaskWrapper, StaP_TaskList[i].name,
		     configMINIMAL_STACK_SIZE + userStack,
		     (void*) &StaP_TaskList[i],
		     tskIDLE_PRIORITY + 1 + StaP_TaskList[i].priority,
		     &handle) != pdPASS)
	STAP_Panic(STAP_ERR_TASK_CREATE+i);
      break;
      
    case StaP_Task_Signal:
      if(xTaskCreate(signalTaskWrapper, StaP_TaskList[i].name,
		     configMINIMAL_STACK_SIZE + userStack,
		     (void*) &StaP_TaskList[i],
		     tskIDLE_PRIORITY + 1 + StaP_TaskList[i].priority,
		     &handle) != pdPASS)
	STAP_Panic(STAP_ERR_TASK_CREATE+i);

      signalOwner[StaP_TaskList[i].typeSpecific.signal.id] = handle;    
      break;
      
    case StaP_Task_Serial:
      if(xTaskCreate(serialTaskWrapper, StaP_TaskList[i].name,
		     configMINIMAL_STACK_SIZE + userStack,
		     (void*) &StaP_TaskList[i],
		     tskIDLE_PRIORITY + 1 + StaP_TaskList[i].priority,
		     &handle) != pdPASS)
	STAP_Panic(STAP_ERR_TASK_CREATE+i);

      signalOwner[StaP_LinkTable[StaP_TaskList[i].typeSpecific.serial.link].signal]
	= handle;    
      break;

    default:
      STAP_Panic(STAP_ERR_TASK_TYPE);
      break;
    }

    if(!handle)
      STAP_Panic(STAP_ERR_TASK_CREATE+i);

    StaP_TaskList[i++].handle = handle;
  }

  FreeRTOSUp = true;
  vTaskStartScheduler();
}

void StaP_SchedulerReport(void)
{
  static VP_TIME_MICROS_T prev;
  VP_TIME_MICROS_T delta = VP_ELAPSED_MICROS(prev);
  int i = 0;

  consolePrint("\033[2J\033[0;0H");
  consolePrintfLn("OS STATUS (CPU load %.1f%%) %u bytes free",
		  (1000 - STAP_CPUIdlePermille()) / 10.0f,
		  STAP_MemoryFree());
  consolePrintLn("Task           Stack   CPU");
  consolePrintLn("---------------------------");

  while(i < StaP_NumOfTasks) {
    if(StaP_TaskList[i].handle) {
      configSTACK_DEPTH_TYPE watermark
	= uxTaskGetStackHighWaterMark2(StaP_TaskList[i].handle);
      VP_TIME_MICROS_T runtime
#if ulTaskGetRunTimeCounter
	= ulTaskGetRunTimeCounter(StaP_TaskList[i].handle);
#else
      = 0;
#endif
      
      consolePrintfLn("%s %t%u %t %.1f%%",
		      StaP_TaskList[i].name,
		      15, watermark,
		      22, 100.0f * (float) (runtime - StaP_TaskList[i].runTime)
		      / delta);

      StaP_TaskList[i].runTime = runtime;
    }
    
    i++;
  }

  prev += delta;
}

// Idle hook

void vApplicationIdleHook( void )
{
  VP_TIME_MICROS_T prev = vpTimeMicros(), curr = 0;
  
  for(;;) {
    curr = vpTimeMicros();
    
    if(curr > prev) 
      // This way we don't get screwed by wrap-around
      idleMicros += curr - prev;
    
    prev = curr;
  }
}

/* vApplicationStackOverflowHook is called when a stack overflow occurs.
This is usefull in application development, for debugging.  To use this
hook, uncomment it, and set configCHECK_FOR_STACK_OVERFLOW to 1 in
"FreeRTOSConfig.h" header file. */

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName )
{
  int i = 0;

  STAP_FailSafe;
  
  pxTask = xTaskGetCurrentTaskHandle();
  
  while(i < StaP_NumOfTasks && StaP_TaskList[i].handle != pxTask)
    i++;

  if(i < StaP_NumOfTasks)
    STAP_Panicf(STAP_ERR_STACK_OVF + i,
		"Stack overflow! Task = \"%s\"", pcTaskName);
  else
    STAP_Panicf(STAP_ERR_STACK_OVF_IDLE,
		"Stack overflow! Task = \"%s\"", pcTaskName);
}

/* vApplicationMallocFailedHook is called when memorry allocation fails.
This is usefull in application development, for debugging.  To use this
hook, uncomment it, and set configUSE_MALLOC_FAILED_HOOK to 1 in
"FreeRTOSConfig.h" header file. */

void vApplicationMallocFailedHook( void )
{
  STAP_Panicf(STAP_ERR_MALLOC_FAIL, "malloc() out of mem");
}
