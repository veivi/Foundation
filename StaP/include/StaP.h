#ifndef STAP_H
#define STAP_H

//
// Common StaP definitions
//

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "VPTime.h"
#include "Buffer.h"

//
// SW configuration related stuff
//

#include "StaP_CONFIG.h"

#if defined(STAP_DEBUG_LEVEL) && STAP_DEBUG_LEVEL > 0
#define STAP_DEBUG  consoleDebugf
#else
#define STAP_DEBUG  (void)
#endif

//
// System mode
//

extern bool failSafeMode;  // Interrupts disabled

#define STAP_FailSafe          { STAP_EnterSystem; failSafeMode = true; }

uint16_t STAP_CPUIdlePermille(void);

typedef uint32_t StaP_ErrorStatus_T;
extern StaP_ErrorStatus_T StaP_ErrorState;

#define STAP_Error(e) StaP_ErrorState |= (e) < 32 ? (1UL<<(e)) : (1UL<<31)
StaP_ErrorStatus_T STAP_Status(bool clear);

//
// Common error code definitions
//

#define STAP_ERR_HALT              0
#define STAP_ERR_MALLOC_FAIL       1
#define STAP_ERR_MUTEX_CREATE      2
#define STAP_ERR_MUTEX             3
#define STAP_ERR_ISR_USARTRX_HW    4
#define STAP_ERR_ISR_USARTRX_LINK  5
#define STAP_ERR_ISR_USARTTX_HW    6
#define STAP_ERR_ISR_USARTTX_LINK  7
#define STAP_ERR_SPURIOUS_INT      8
#define STAP_ERR_TX_TOO_BIG        9
#define STAP_ERR_NO_UART           10
#define STAP_ERR_TASK_INVALID      11
#define STAP_ERR_THREADING         12
#define STAP_ERR_ASSERT_KERNEL     13
#define STAP_ERR_NO_SIGNAL         14
#define STAP_ERR_NO_LINK           15
#define STAP_ERR_TASK_TYPE         16
#define STAP_ERR_TASK_CODE         17
#define STAP_ERR_TX_TIMEOUT0       18
#define STAP_ERR_DATAGRAM          19
#define STAP_ERR_RX_OVERRUN_H      20
#define STAP_ERR_RX_OVERRUN_S      21
#define STAP_ERR_TIME              22
#define STAP_ERR_I2C               23
#define STAP_ERR_TX_TIMEOUT1       24
#define STAP_ERR_TX_TIMEOUT2       25
#define STAP_ERR_APPLICATION       28
#define STAP_ERR_TASK_CREATE       0x20
#define STAP_ERR_STACK_OVF         0x40
#define STAP_ERR_STACK_OVF_IDLE    0x5F
#define STAP_ERR_FELLTHROUGH       0xF0

// Protocols

#define ALPHAPILOT_EXBUS     0
#define ALPHAPILOT_SRXL      1

//
// I2C/SPI interface
//

typedef enum { transfer_dir_transmit, transfer_dir_receive } StaP_TransferDir_t;

typedef struct {
    union {
        const uint8_t *tx;
        uint8_t *rx;
    } data;
    size_t size;
    StaP_TransferDir_t dir;
} StaP_TransferUnit_t;

//
// System interface
//

void STAP_Initialize(void);
VP_TIME_MICROS_T STAP_TimeMicros(void);
VP_TIME_SECS_T STAP_TimeSecs(void);
void STAP_Reboot(bool bootloader);
void STAP_Panic(uint8_t reason);
void STAP_Panicf(uint8_t reason, const char *format, ...);
void STAP_Indicate(uint8_t code);
size_t STAP_MemoryFree(void);
void STAP_DelayMillis(uint16_t);
void STAP_SchedulerReport(void);
uint8_t STAP_CPUIdlePercent(void);

//
// HW timer for generating kernel stats
//

void STAP_TimerInit(void);
VP_TIME_MICROS_T STAP_TimerCount(void);


//
// Gyro interface
//

typedef struct stap_Vector3f {
  float x, y, z;
} stap_Vector3f_t;

bool stap_gyroUpdate(void);
bool stap_attiUpdate(void);
bool stap_accUpdate(void);
bool stap_sensorRead(stap_Vector3f_t *acc, stap_Vector3f_t *atti, stap_Vector3f_t *rot);

//
// Serial link definition
//

#define LINK_MODE_RX     1
#define LINK_MODE_TX     2
#define LINK_MODE_RXTX   (LINK_MODE_RX | LINK_MODE_TX)

typedef struct StaP_LinkRecord {
  uint8_t txcv;
  uint32_t rate;
  uint8_t mode;
  volatile VPBuffer_t buffer;
  StaP_Signal_T signal;
  VP_TIME_MICROS_T latency, lastReceived;
} StaP_LinkRecord_T;

extern StaP_LinkRecord_T StaP_LinkTable[];

//
// Target hardware dependent stuff
//

#ifndef __APPLE__
#include "StaP_TARGET.h"
#endif


#endif

