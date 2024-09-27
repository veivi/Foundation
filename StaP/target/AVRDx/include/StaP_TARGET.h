#ifndef STAP_TARGET_H
#define STAP_TARGET_H

#include <limits.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "VPTime.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "SPI.h"
#include "I2C.h"

typedef enum { StaP_Txcv_Invalid, AVRDx_Txcv_UART0, AVRDx_Txcv_UART1, AVRDx_Txcv_UART2, AVRDx_Txcv_UART3,  AVRDx_Txcv_UART4, StaP_NumOfTxcvs } StaP_Txcv_T;

#define STAP_STACK_SIZE_UNIT      1

typedef uint32_t StaP_ErrorStatus_T;
extern StaP_ErrorStatus_T StaP_ErrorState;

#define STAP_Error(e) StaP_ErrorState |= (e) < 32 ? (1UL<<(e)) : (1UL<<31)
StaP_ErrorStatus_T STAP_Status(bool clear);

typedef TickType_t STAP_NativeTime_T;
#define STAP_NativeTime xTaskGetTickCount()

#define STAP_ENTROPY_SRC (uint8_t) (TCB1.CNT & 0xF)

// #define STAP_FEATURE_SERIALRX
// #define STAP_FEATURE_SERIALTX
#define STAP_FEATURE_GPS

//
// Multitasking friendly delay
//

#define STAP_DelayMillis(d)         AVRDx_DelayMillis(d)
#define STAP_DelayUntil(start, d)   AVRDx_DelayUntil(start, d)

void AVRDx_DelayMillis(VP_TIME_MILLIS_T);
void AVRDx_DelayUntil(STAP_NativeTime_T*, VP_TIME_MILLIS_T);

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

//
// Mutex
//

typedef SemaphoreHandle_t    STAP_MutexRef_T;
#define STAP_MutexCreate     xSemaphoreCreateMutex()
#define STAP_MutexObtain(m)  if(xSemaphoreTake(m, portMAX_DELAY) != pdPASS) STAP_Panic(STAP_ERR_MUTEX)
#define STAP_MutexRelease(m) xSemaphoreGive(m)

#define STAP_LinkIsUsable(port) AVRDxSTAP_LinkIsUsable(port)
#define STAP_LinkStatus(port) AVRDxSTAP_LinkStatus(port)
#define STAP_LinkSuspend(port) AVRDxSTAP_LinkSuspend(port)
#define STAP_LinkResume(port) AVRDxSTAP_LinkResume(port)
#define STAP_LinkGetChar(port) AVRDxSTAP_LinkGetChar(port)
#define STAP_LinkGet(port, b, s) AVRDxSTAP_LinkGet(port, b, s)
#define STAP_LinkPutChar(port, c, t) AVRDxSTAP_LinkPutChar(port, c, t)
#define STAP_LinkPut(port, b, s, t) AVRDxSTAP_LinkPut(port, b, s, t)
#define STAP_LinkTalk(port) AVRDxSTAP_LinkTalk(port)
#define STAP_LinkDrain(port, timeout) AVRDxSTAP_LinkDrain(port, timeout)
#define STAP_LinkListen(port, timeout) AVRDxSTAP_LinkListen(port, timeout)
#define STAP_LinkSetRate(port, rate) AVRDxSTAP_LinkSetRate(port, rate)
#define STAP_NVStoreWrite(addr, buffer, size) AVRDxSTAP_NVStoreWrite(addr, buffer, size)
#define STAP_NVStoreRead(addr, buffer, size) AVRDxSTAP_NVStoreRead(addr, buffer, size)

int AVRDxSTAP_LinkStatus(uint8_t);
char AVRDxSTAP_LinkGetChar(uint8_t);
int AVRDxSTAP_LinkGet(uint8_t, char *, int);
int AVRDxSTAP_LinkPutChar(uint8_t, char, VP_TIME_MILLIS_T);
int AVRDxSTAP_LinkPut(uint8_t, const char *, int, VP_TIME_MILLIS_T);
bool AVRDxSTAP_NVStoreWrite(void *addr, const void *buffer, size_t size);
bool AVRDxSTAP_NVStoreRead(const void *addr, void *buffer, size_t size);
void AVRDxSTAP_LinkTalk(uint8_t);
void AVRDxSTAP_LinkListen(uint8_t, VP_TIME_MILLIS_T);
void AVRDxSTAP_LinkDrain(uint8_t, VP_TIME_MILLIS_T);
void AVRDxSTAP_LinkSetRate(uint8_t port, unsigned long rate);
void AVRDxSTAP_LinkSuspend(uint8_t);
void AVRDxSTAP_LinkResume(uint8_t);

#define STAP_PWMOutput(num, pulse) AVRDx_pwmOutput(num, pulse)
void AVRDx_pwmOutput(uint8_t num, const uint16_t value[]);

#define STAP_I2CTransfer(dev, seg, num) I2C_0_Transfer(dev, seg, num)

#define CS_QUALIFIER  // PROGMEM
#define CS_MEMCPY memcpy // memcpy_P
#define CS_STRNCPY strncpy // strncpy_P
#define CS_STRING(s) s // (__extension__({static const char __c[] PROGMEM = (s); &__c[0]; }))

typedef UBaseType_t ForbidContext_T; 

#define STAP_FORBID_SAFE       taskENTER_CRITICAL_FROM_ISR()
#define STAP_PERMIT_SAFE(c)    taskEXIT_CRITICAL_FROM_ISR(c)
#define STAP_FORBID            taskENTER_CRITICAL()
#define STAP_PERMIT            taskEXIT_CRITICAL()

#define STAP_EnterSystem       cli()

//
// LED control
//

#define STAP_LED0_SET(s)  STAP_LED0_PORT.OUT = ((!s) ? STAP_LED0_PORT.OUT | STAP_LED0_MASK : STAP_LED0_PORT.OUT & ~STAP_LED0_MASK)
#define STAP_LED1_SET(s)  STAP_LED1_PORT.OUT = ((!s) ? STAP_LED1_PORT.OUT | STAP_LED1_MASK : STAP_LED1_PORT.OUT & ~STAP_LED1_MASK)

#define STAP_LED0_OFF     STAP_LED0_SET(0)
#define STAP_LED0_ON      STAP_LED0_SET(1)

#define STAP_LED1_OFF     STAP_LED1_SET(0)
#define STAP_LED1_ON      STAP_LED1_SET(1)

#endif
