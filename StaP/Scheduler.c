#include <stddef.h>
#include "FreeRTOS.h"
#include "task.h"
#include "Console.h"
#include <string.h>
#include <ctype.h>
#include "StaP.h"

// #define ONLY_TASK_NAME   "Blink"

#ifdef USE_NATIVE_SCHEDULER

typedef struct {
  VP_TIME_MICROS_T latency;
  bool asserted;
} signalRecord_t;

static volatile signalRecord_t signalStore[StaP_NumOfSignals];
static int currentTask;

struct TaskDecl *StaP_CurrentTask(void)
{
  return &StaP_TaskList[currentTask];
}
  
void STAP_SignalLatent(StaP_Signal_T value, VP_TIME_MICROS_T latency)
{
  STAP_FORBID;
  STAP_SignalLatentFromISR(value, latency);
  STAP_PERMIT;
}

bool STAP_SignalLatentFromISR(StaP_Signal_T value, VP_TIME_MICROS_T latency)
{
  if(value != StaP_Sig_Invalid) {
    if(!signalStore[value].asserted || signalStore[value].latency > latency)
      signalStore[value].latency = latency;
    signalStore[value].asserted = true;
  }

  return false;
}

void STAP_Signal(StaP_Signal_T value)
{
  STAP_SignalLatent(value, 0);
}

bool STAP_SignalFromISR(StaP_Signal_T value)
{
  return STAP_SignalLatentFromISR(value, 0);
}

void STAP_SignalSelf(VP_TIME_MICROS_T latency)
{
  STAP_SignalLatent(StaP_TaskList[currentTask].typeSpecific.signal.signal, latency);
}

static VP_TIME_MICROS_T lastAged;

static void signalUpdateAge(void)
{
  VP_TIME_MICROS_T delta = VP_ELAPSED_MICROS(lastAged);
  int i = 0;

  lastAged = vpTimeMicrosApprox;
  
  for(i = 0; i < StaP_Sig_Invalid; i++) {
    if(signalStore[i].latency > delta)
      signalStore[i].latency -= delta;
    else
      signalStore[i].latency = 0;
  }
}

static bool signalReceive(StaP_Signal_T value)
{
  bool status = false;

  if(value != StaP_Sig_Invalid) {
    STAP_FORBID;
    
    if(signalStore[value].asserted && !signalStore[value].latency) {
      signalStore[value].asserted = false;
      status = true;
    }
    
    STAP_PERMIT;
  }

  return status;
}

//
// Scheduler
//

static bool scheduler(void)
{
  bool status = false;
  int i = 0;
    
  //  consoleNote("S ");

  for(i = 0; i < StaP_NumOfTasks; i++) {
    bool unBlocked = false;
    
    vpTimeAcquire();
    signalUpdateAge();

    VP_TIME_MILLIS_T elapsed
      = VP_ELAPSED_MILLIS(StaP_TaskList[i].lastInvoked);

    switch(StaP_TaskList[i].type) {
    case StaP_Task_Period:
      unBlocked = (elapsed >= StaP_TaskList[i].typeSpecific.period);
      break;

    case StaP_Task_Signal:
      unBlocked = signalReceive(StaP_TaskList[i].typeSpecific.signal.signal);
      
      if(StaP_TaskList[i].typeSpecific.signal.timeOut > 0)
	unBlocked |= (elapsed >= StaP_TaskList[i].typeSpecific.signal.timeOut);
      break;
      
    case StaP_Task_Serial:
      unBlocked = signalReceive(StaP_LinkTable[StaP_TaskList[i].typeSpecific.serial.link].signal) | (elapsed >= 300);
      break;
    }
      
    if(unBlocked) {
      StaP_TaskList[i].lastInvoked = vpTimeMillis();

      currentTask = i;
      
#ifdef ALP_PROFILING
      VP_TIME_MICROS_T startTime = vpTimeMicrosApprox;
#endif      

      StaP_TaskList[i].code();
      
#ifdef ALP_PROFILING
      StaP_TaskList[i].timesRun++;
      
      if(signal)
	StaP_TaskList[i].triggered++;
      
      if(vpStatus.consoleLink) 
	StaP_TaskList[i].runTime += vpTimeMicros() - startTime;
#endif      
      status = true; // We had something to do
    }
  }

  //  consoleNote("SX ");
  
  currentTask = -1;
  return status;
}

void STAP_SchedulerReport(void)
{
#ifdef ALP_PROFILING
  static VP_TIME_MICROS_T lastReport;
  VP_TIME_MICROS_T period = vpTimeMicros() - lastReport;
  float load = 0.0f, cum = 0.0f;
  int i = 0;  
      
  consoleNoteLn_P(CS_STRING("Task statistics"));

  for(i = 0; StaP_TaskList[i].code != NULL; i++) {
    consoleNote("  ");
    consolePrintI(i);
    consoleTab(10);
    consolePrintF(StaP_TaskList[i].timesRun / (period / 1.0e6));
    consolePrint_P(CS_STRING(" Hz "));
    load = 100.0f * StaP_TaskList[i].runTime / period;
    cum += load;
    consoleTab(20);
    consolePrintF(load);
    consolePrint_P(CS_STRING(" %"));
    consoleTab(30);
    if(StaP_TaskList[i].signal != StaP_Sig_Invalid) {
      consolePrintF((float) StaP_TaskList[i].triggered / (period / 1.0e6));
      consolePrint_P(CS_STRING(" Hz "));
    }
    consoleTab(40);
    if(StaP_TaskList[i].realTime) {
      consolePrintF((float) StaP_TaskList[i].lagged / (period / 1.0e6));
      consolePrint_P(CS_STRING(" Hz "));
    }
    consoleNL();
    StaP_TaskList[i].timesRun = 0;
    StaP_TaskList[i].lagged = 0;
    StaP_TaskList[i].triggered = 0;
    StaP_TaskList[i].runTime = 0;
  }

  consoleTab(20);
  consolePrintF(cum);
  consolePrintLn(" %");

  lastReport = vpTimeMicros();
#endif  

  consoleNote_P(CS_STRING("Uptime "));
  consolePrintUL(uptimeMinutes);
  consolePrintLn_P(CS_STRING(" minutes"));
}

VPPeriodicTimer_t minuteTimer = VP_PERIODIC_TIMER_CONS(60.0e3);

void StaP_SchedulerStart() 
{
  bool idling = false;
  VP_TIME_MICROS_T idleStarted = 0, idleEnded = 0;

  while(true) {
    idleEnded = vpTimeMicros();

    if(vpPeriodicEvent(&minuteTimer)) {
      uptimeMinutes++;

      if(uptimeMinutes % 15 == 0) {
	consoleNote_P(CS_STRING("Uptime "));
	consolePrintUL(uptimeMinutes);
	consolePrintLn_P(CS_STRING(" minutes"));
	consoleFlush();
      }
    }

    if(scheduler()) {    
      // Had something to do
      if(idling) {
	// Not idling anymore
	idleMicros += idleEnded - idleStarted;
	idling = false;
      }
    } else if(!idling) {
      // Just started idling
      idling = true;
      
      if(vpControl.initState == it_done && !logReady(false))
	logInitialize(50);

      idleStarted = vpTimeMicros();
    }
}
}

#else

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
    VP_TIME_MICROS_T callAgain = (*appTask->code)();
    
    if(appTask->typeSpecific.period)
      vTaskDelay(pdMS_TO_TICKS(appTask->typeSpecific.period));
    else
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
      StaP_SignalSet_T sig = 0;

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

      //      if(sig) {
	// Now we know the buffer is not empty, we need to consider
	// the link-specific latency as a timeout as we wait for more
	
	if(StaP_LinkTable[link].latency/1000 < timeout)
	  timeout = StaP_LinkTable[link].latency/1000;

	StaP_LinkTable[link].buffer.watermark =
	  StaP_LinkTable[link].buffer.mask>>1;

	// Wait until we time out or see lots of stuff in the buffer
	
	do {
	  sig = STAP_SignalWaitTimeout
	    (STAP_SignalSet(StaP_LinkTable[link].signal), timeout);
	} while(sig && !VPBUFFER_GAUGE(StaP_LinkTable[link].buffer)
		<= StaP_LinkTable[link].buffer.watermark);
	//      } 	
    }
    
    // Invoke the code
    
    invokeAgain = (*appTask->code)();
  }
}

#ifdef BLOB

void blobTask(void *params)
{
  int i = 0;

  for(;;) {
    // USART_Transmit(&USART2, "Paska", 5);
    // STAP_LinkPut(ALP_Link_HostTX, "Paska ", 6, 1000);
#if BLOB > 0
    if(params)
      STAP_DEBUG(0, " Blobeti %d", vpTimeMillis());
    else
      STAP_DEBUG(0, " BlobBlob %d ", vpTimeMillis());
#endif

    if(i++ & 1) {
      if(params)
	STAP_LED0_ON;
      else
	STAP_LED1_ON;
    }  else {
      if(params)
	STAP_LED0_OFF;
      else
	STAP_LED1_OFF;
    }
    if(params)
      STAP_DelayMillis(250);
    else
      STAP_DelayMillis(300);
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

extern int FreeRTOSUp;

void StaP_SchedulerStart( void )
{
  TaskHandle_t handle = NULL;
  int i = 0;

  portDISABLE_INTERRUPTS();
  
  // Clear the signal owner table
  
  memset((void*) signalOwner, 0, sizeof(signalOwner));
	 
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

  while(i < StaP_NumOfTasks) {
#ifdef ONLY_TASK_NAME
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
#endif

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


#endif

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
  STAP_Panic(STAP_ERR_MALLOC_FAIL);
}
