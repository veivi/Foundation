#ifndef STAP_INAV_H
#define STAP_INAV_H

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "VPTime.h"
#include "drivers/light_led.h"
#include "sensors/gyro.h"

#define STAP_STACK_SIZE_UNIT      4

// #define USE_NATIVE_SCHEDULER    1

#define STAP_I2C_BUS              MAG_I2C_BUS

#define STAP_FEATURE_SERIALRX     // EXBUS/SRXL input, disables PPM.
#define STAP_FEATURE_SERIALTX     // EXBUS/SRXL output

typedef enum { StaP_Txcv_Invalid, StaP_Txcv_UART1, StaP_Txcv_UART2, StaP_Txcv_UART3, StaP_Txcv_UART4,  StaP_Txcv_UART5,  StaP_Txcv_UART6,  StaP_Txcv_VCP, StaP_NumOfTxcvs } StaP_Txcv_T;

void usbSerialPoll(uint8_t port);

#define STAP_HOST_POLL       usbSerialPoll(ALP_Link_HostRX)
#define STAP_HOST_POLL_HZ    100

typedef uint32_t StaP_ErrorStatus_T;
extern StaP_ErrorStatus_T StaP_ErrorState;

#define STAP_Error(e) StaP_ErrorState |= (e) < 32 ? (1UL<<(e)) : (1UL<<31)
StaP_ErrorStatus_T STAP_Status(bool clear);

typedef TickType_t STAP_NativeTime_T;
#define STAP_NativeTime xTaskGetTickCount()

// typedef VP_TIME_MICROS_T STAP_NativeTime_T;
// #define STAP_NativeTime vpTimeMicros()

// #define STAP_FEATURE_SERIALRX
// #define STAP_FEATURE_SERIALTX
#define STAP_FEATURE_GPS

//
// Multitasking friendly delay
//

#ifdef USE_NATIVE_SCHEDULER
void STAP_DelayMillis(VP_TIME_MILLIS_T value);
void STAP_DelayMillisUntil(VP_TIME_MILLIS_T *start, VP_TIME_MILLIS_T d);
#else
#define STAP_DelayMillis(d)         vTaskDelay(pdMS_TO_TICKS(d))
#define STAP_DelayUntil(start, d)   vTaskDelayUntil(start, pdMS_TO_TICKS(d))
#endif

//
// Signals
//

extern TaskHandle_t signalOwner[];

typedef uint32_t StaP_SignalSet_T;

#define STAP_SignalSet(sig) (1UL << sig)

StaP_SignalSet_T STAP_SignalWait(StaP_SignalSet_T mask);
StaP_SignalSet_T STAP_SignalWaitTimeout(StaP_SignalSet_T mask, VP_TIME_MILLIS_T timeout);
void STAP_Signal(StaP_Signal_T sig);
bool STAP_SignalFromISR(StaP_Signal_T sig);
void STAP_SignalLatent(StaP_Signal_T sig, VP_TIME_MICROS_T);
bool STAP_SignalLatentFromISR(StaP_Signal_T sig, VP_TIME_MICROS_T);

#define STAP_SENSOR_OVERSAMPLE    4
#define STAP_SENSOR_ASYNC

#define STAP_ENTROPY_SRC       (STAP_CPUClkTime() & 0x1F)

#define STAP_LinkQuiescent(port, time) inavStaP_LinkQuiescent(port, time)
#define STAP_LinkIsIdle(port) inavStaP_LinkIsIdle(port)
#define STAP_LinkIsUsable(port) inavStaP_LinkIsUsable(port)
#define STAP_LinkSetRate(port, rate) inavStaP_LinkSetRate(port, rate)
#define STAP_LinkStatus(port) inavStaP_LinkStatus(port)
#define STAP_LinkGetChar(port) inavStaP_LinkGetChar(port)
#define STAP_LinkGet(port, b, s) inavStaP_LinkGet(port, b, s)
#define STAP_LinkHasOverrun(port) inavStaP_LinkHasOverrun(port)
#define STAP_LinkPutChar(port, c, to) inavStaP_LinkPutChar(port, c, to)
#define STAP_LinkPut(port, b, s, to) inavStaP_LinkPut(port, b, s, to)
#define STAP_LinkDrain(port, timeout) inavStaP_LinkDrain(port, timeout)
#define STAP_LinkTalk(port) inavStaP_LinkTalk(port)
#define STAP_LinkListen(port, timeout) inavStaP_LinkListen(port, timeout)
#define STAP_LinkTxBegin(port) inavStaP_LinkTxBegin(port)
#define STAP_LinkTxEnd(port) inavStaP_LinkTxEnd(port)

#define STAP_I2CTransfer(dev, seg, num) 0xFF // inavStaP_I2CTransfer(dev, seg, num)

#define STAP_I2CErrorCount inavStaP_I2CErrorCount()
#define STAP_I2CErrorCode  inavStaP_I2CErrorCode()
#define STAP_pwmOutput(num, pulse) inavStaP_pwmOutput(num, pulse)

#define STAP_GyroCalibrate inavStaP_GyroCalibrate()
#define STAP_GyroIsGood inavStaP_GyroIsGood()
#define STAP_GyroUpdate inavStaP_GyroUpdate()
#define STAP_BaroUpdate inavStaP_BaroUpdate()
#define STAP_BaroRead inavStaP_BaroRead()
 
#define STAP_NVStoreRead     M24XXRead
#define STAP_NVStoreWrite    M24XXWrite
#define STAP_SensorCalib     inavStaP_CalibrateAccel()
// #define STAP_FuelFlowSensor  inavStaP_SwitchRead(0)
// #define STAP_CanopySwitch    inavStaP_SwitchRead(1)

#define STAP_Arm         inavStaP_Arm()

//
// Mutex
//

#ifdef USE_NATIVE_SCHEDULER
typedef uint8_t STAP_MutexRef_T;
#define STAP_MutexCreate     1
#define STAP_MutexObtain(m)  
#define STAP_MutexRelease(m) 
#else
typedef SemaphoreHandle_t    STAP_MutexRef_T;
#define STAP_MutexCreate     xSemaphoreCreateMutex()
#define STAP_MutexObtain(m)  if(xSemaphoreTake(m, portMAX_DELAY) != pdPASS) STAP_Panic(STAP_ERR_MUTEX)
#define STAP_MutexRelease(m) xSemaphoreGive(m)
#endif

void inavStaP_Arm(void);
void inavStaP_CalibrateAccel(void);
bool inavStaP_LinkQuiescent(uint8_t, VP_TIME_MICROS_T);
bool inavStaP_LinkIsIdle(uint8_t);
bool inavStaP_LinkIsUsable(uint8_t);
void inavStaP_LinkSetRate(uint8_t, uint32_t);
void inavStaP_LinkTalk(uint8_t);
void inavStaP_LinkListen(uint8_t, VP_TIME_MILLIS_T);
void inavStaP_LinkTxBegin(uint8_t);
void inavStaP_LinkTxEnd(uint8_t);
int inavStaP_LinkStatus(uint8_t);
bool inavStaP_LinkHasOverrun(uint8_t);
char inavStaP_LinkGetChar(uint8_t);
int inavStaP_LinkGet(uint8_t, char*, int);
int inavStaP_LinkPutChar(uint8_t, char, VP_TIME_MILLIS_T);
int inavStaP_LinkPut(uint8_t, const char*, int, VP_TIME_MILLIS_T);
void inavStaP_LinkDrain(uint8_t, VP_TIME_MILLIS_T);

uint8_t inavStaP_I2CWrite(uint8_t, const uint8_t*, uint8_t, const StaP_TransferUnit_t*, int);
uint8_t inavStaP_I2CRead(uint8_t, const uint8_t*, uint8_t, uint8_t*, uint8_t);
uint8_t inavStaP_I2CWait(uint8_t);
uint16_t inavStaP_I2CErrorCount(void);
uint16_t inavStaP_I2CErrorCode(void);
void inavStaP_pwmOutput(int, const uint16_t []);
void inavStaP_GyroUpdate(void);
bool inavStaP_GyroIsGood(void);
void inavStaP_GyroCalibrate(void);
void inavStaP_BaroUpdate(void);
float inavStaP_BaroRead(void);
uint8_t inavStaP_SwitchRead(uint8_t);

// #define STAP_PERIOD_GYRO            gyro.targetLooptime
// #define STAP_PERIOD_GYRO_STATIC     HZ_TO_PERIOD(400)

typedef UBaseType_t ForbidContext_T; 

#define STAP_FORBID_SAFE       taskENTER_CRITICAL_FROM_ISR()
#define STAP_PERMIT_SAFE(c)    taskEXIT_CRITICAL_FROM_ISR(c)
#define STAP_FORBID            taskENTER_CRITICAL()
#define STAP_PERMIT            taskEXIT_CRITICAL()
#define STAP_EnterSystem       // _disable_interrupt_()

typedef uint32_t StaP_CPUClkTime_T;
StaP_CPUClkTime_T STAP_CPUClkTime(void);
#define STAP_CPUClockSpeed SystemCoreClock

/*

*/

#define STAP_LED0_ON      ledSet(1, true)
#define STAP_LED0_OFF     ledSet(1, false)
#define STAP_LED1_ON      ledSet(0, true)
#define STAP_LED1_OFF     ledSet(0, false)

//
// Constant storage access (alias nasty AVR hack called "progmem")
//

#define CS_QUALIFIER  
#define CS_MEMCPY memcpy
#define CS_READCHAR(s) (*((const char *)s))
#define CS_STRNCPY(dst, src, s) strncpy(dst, src, s)
#define CS_STRING(s) s

#endif
