#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include "Console.h"
#include "StaP.h"
#include "PRNG.h"
#include "Scheduler.h"
#include "FreeRTOS.h"
#include "task.h"

#define SERIAL_BLOCKSIZE   (1<<5)
// #define STAP_TEST_TASK     1

TaskHandle_t signalOwner[StaP_NumOfSignals];
extern int FreeRTOSUp;
static volatile VP_TIME_MICROS_T idleMicros;

#if configTASK_NOTIFICATION_ARRAY_ENTRIES > 1
#define OSKernelNotify(t, s, v)             xTaskNotifyIndexed(t, 1, s, v)
#define OSKernelNotifyFromISR(t, s, v, y)   xTaskNotifyIndexedFromISR(t, 1, s, v, y)
#define OSKernelNotifyWait(a, b, c, t)      xTaskNotifyWaitIndexed(1, a, b, c, t)
#else
#define OSKernelNotify(t, s, v)             xTaskNotify(t, s, v)
#define OSKernelNotifyFromISR(t, s, v, y)   xTaskNotifyFromISR(t, s, v, y)
#define OSKernelNotifyWait(a, b, c, t)      xTaskNotifyWait(a, b, c, t)
#endif

void STAP_Indicate(uint8_t code)
{
  int i = 0;

  ForbidContext_T c = STAP_FORBID_SAFE;
  
  while(i++ < 8) {
    STAP_LED1_ON;
    STAP_DelayMillisFromISR((code & 0x80) ? 300 : 100);
    STAP_LED1_OFF;
    STAP_DelayMillisFromISR(150);
    code <<= 1;
  }

  STAP_DelayMillisFromISR(2000);
    
  STAP_PERMIT_SAFE(c);
}

void STAP_Panic(uint8_t reason)
{
  STAP_FailSafe;
  
  while(1) {
    STAP_Indicate(reason);
  }
}

void STAP_Panicf(uint8_t reason, const char *format, ...)
{
  va_list argp;
  
  STAP_FailSafe;

  while(1) {
    va_start(argp, format);
  
    consoleNotef("PANIC(%#x) ", reason);
    consolevPrintf(format, argp);
    consoleNL();
    consoleFlush();

    va_end(argp);
    
    STAP_Indicate(reason);
  }
}

void STAP_MutexObtain(STAP_MutexRef_T m)
{ 
  if(xSemaphoreTake(m, portMAX_DELAY) != pdPASS)
    STAP_Panic(STAP_ERR_MUTEX);
} 

void STAP_MutexInit(STAP_MutexRef_T *m)
{ 
  STAP_FORBID;

  if(!*m && !(*m = STAP_MutexCreate))
    STAP_Panic(STAP_ERR_MUTEX_CREATE);

  STAP_PERMIT;

  if(xSemaphoreTake(*m, portMAX_DELAY) != pdPASS)
    STAP_Panic(STAP_ERR_MUTEX);
} 

void STAP_DelayMillis(VP_TIME_MILLIS_T d)
{
  if(d > 0)
    vTaskDelay(pdMS_TO_TICKS(d));
}

void STAP_DelayUntil(STAP_NativeTime_T *start, VP_TIME_MILLIS_T d)
{
  if(d > 0)
    vTaskDelayUntil(start, pdMS_TO_TICKS(d));
}

uint16_t STAP_CPUIdlePermille(void)
{
#if INCLUDE_ulTaskGetIdleRunTimePercent
  return ulTaskGetIdleRunTimePercent()*10;
#else
  return 0xFA17;
#endif
}

void STAP_Signal(StaP_Signal_T sig)
{
  if(signalOwner[sig])
	  OSKernelNotify(signalOwner[sig], STAP_SignalSet(sig), eSetBits);

}

bool STAP_SignalFromISR(StaP_Signal_T sig)
{
  BaseType_t yield = false;
  
  if(signalOwner[sig])
	  OSKernelNotifyFromISR(signalOwner[sig], STAP_SignalSet(sig), eSetBits, &yield);

  return yield;
}

StaP_SignalSet_T STAP_SignalWaitTimeout(StaP_SignalSet_T mask,
					  VP_TIME_MILLIS_T timeout)
{
  StaP_SignalSet_T status = 0;

  //  status = ulTaskNotifyValueClearIndexed(xTaskGetCurrentTaskHandle(), 1, mask);

  while(!(mask & status)) {
    if(OSKernelNotifyWait(0, mask, &status,
			    VP_MILLIS_FINITE(timeout)
			    ? pdMS_TO_TICKS(timeout) : portMAX_DELAY)
      == pdFALSE)
      // Timed out
      break;
  }

  randomEntropyInput(STAP_ENTROPY_SRC);
  
  return status & mask;
}

StaP_SignalSet_T STAP_SignalWait(StaP_SignalSet_T mask)
{
  return STAP_SignalWaitTimeout(mask, VP_TIME_MILLIS_MAX);
}

static void periodTaskWrapper( void *pvParameters )
{
  struct TaskDecl *appTask = (struct TaskDecl*) pvParameters;
  TickType_t activation = 0;
  
  if(appTask->type != StaP_Task_Period)
    STAP_Panic(STAP_ERR_TASK_TYPE);
    
  if(!appTask->code.task)
    STAP_Panic(STAP_ERR_TASK_CODE);
      
  for(;;) {
    VP_TIME_MICROS_T callAgain = (*appTask->code.task)();
    
    if(!VP_MILLIS_FINITE(appTask->typeSpecific.period))
      // Period if infinite, wait forever
      for(;;)
	vTaskDelay(portMAX_DELAY);
    else if(appTask->typeSpecific.period)
      // Period is finite
      vTaskDelayUntil(&activation, pdMS_TO_TICKS(appTask->typeSpecific.period));
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
    
  if(!appTask->code.task)
    STAP_Panic(STAP_ERR_TASK_CODE);
      
  for(;;) {
    if(appTask->typeSpecific.signal.timeOut)
      // We have a timeout specification
      STAP_SignalWaitTimeout(STAP_SignalSet(appTask->typeSpecific.signal.id),
			     appTask->typeSpecific.signal.timeOut);
    else
      STAP_SignalWait(STAP_SignalSet(appTask->typeSpecific.signal.id));
    
    (*appTask->code.task)();
  }
}

static void serialTaskWrapper( void *pvParameters )
{
  struct TaskDecl *appTask = (struct TaskDecl*) pvParameters;
  StaP_LinkId_T link = 
    appTask->type == StaP_Task_Serial
    ? appTask->typeSpecific.serial.link : appTask->typeSpecific.datagram.link;
  
  VP_TIME_MICROS_T invokeAgain = 0;
  
  if(appTask->type != StaP_Task_Serial && appTask->type != StaP_Task_Datagram)
    STAP_Panic(STAP_ERR_TASK_TYPE);
    
  if(!link)
    STAP_Panic(STAP_ERR_NO_LINK);
    
  if(!appTask->code.task)
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

    if(appTask->type == StaP_Task_Datagram) {
      char buffer[SERIAL_BLOCKSIZE];

      datagramRxInputWithHandler(appTask->typeSpecific.datagram.state, 
				 appTask->code.handler,
				 (const uint8_t*) buffer,
				 STAP_LinkGet(link, buffer, sizeof(buffer)));
      
      invokeAgain = 0;
    } else
      invokeAgain = (*appTask->code.task)();
  }
}

#ifdef STAP_TEST_TASK

static void STAP_TestTask(void *params)
{
  for(;;) {
    (void) STAP_Status();

    consolePrintfLn("micros = %U", vpTimeMicros());
    STAP_DelayMillis(100);
  } 
} 

#endif

void StaP_SchedulerInit( void )
{
  TaskHandle_t handle = NULL;

  portDISABLE_INTERRUPTS();
  
  // Clear the signal owner table
  
  memset((void*) signalOwner, 0, sizeof(signalOwner));

#ifdef STAP_TEST_TASK
  if(xTaskCreate(STAP_TestTask, "Test Task",
		 configMINIMAL_STACK_SIZE + (1<<9),
		 NULL,
		 tskIDLE_PRIORITY + 1,
		 &handle) != pdPASS)
    STAP_Panic(STAP_ERR_TASK_CREATE);
#else
  int i = 0;
  
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
    case StaP_Task_Datagram:
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
  #endif
}

void StaP_SchedulerStart( void )
{
  FreeRTOSUp = true;
  vTaskStartScheduler();

  // Should never get here

  STAP_Panicf(STAP_ERR_FELLTHROUGH, "vTaskStartScheduler() failed (free mem %d)",
	      STAP_MemoryFree());
}

void StaP_SchedulerReport(void)
{
  static VP_TIME_MICROS_T prev;
  VP_TIME_MICROS_T delta = VP_ELAPSED_MICROS(prev);
  int i = 0;

  consolePrint("\033[2J\033[0;0H");
  consolePrintfLn("OS STATUS (CPU load %d%%) %u bytes free",
		  (1000 - STAP_CPUIdlePermille())/10,
		  STAP_MemoryFree());
  consolePrintLn("Task           Stack   CPU");
  consolePrintLn("---------------------------");

  while(i < StaP_NumOfTasks) {
    if(StaP_TaskList[i].handle) {
      configSTACK_DEPTH_TYPE watermark
	= uxTaskGetStackHighWaterMark(StaP_TaskList[i].handle);
      VP_TIME_MICROS_T runtime =
#if INCLUDE_ulTaskGetRunTimeCounter
        STAP_JiffiesToMicros(ulTaskGetRunTimeCounter(StaP_TaskList[i].handle));
#else
        0;
#endif
      consolePrintfLn("%s %t%u %t %.1f%%",
		      StaP_TaskList[i].name,
		      15, watermark,
		      22, 100.0f * (float) (runtime - StaP_TaskList[i].runTime) / delta);

      StaP_TaskList[i].runTime = runtime;
    }
    
    i++;
  }

  prev += delta;
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
