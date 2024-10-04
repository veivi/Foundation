#include <string.h>
#include "CRC16.h"
#include "Math.h"
#include "Setup.h"
#include "Buffer.h"
#include "RxInput.h"
#include "NVState.h"
#include "Objects.h"
#include "VPTime.h"
#include "Console.h"
#include "platform.h"
#include "drivers/system.h"
#include "drivers/time.h"
#include "drivers/serial.h"
#include "drivers/serial_usb_vcp.h"
#include "drivers/serial_uart.h"
#include "drivers/bus_i2c.h"
#include "drivers/pwm_output.h"
#include "drivers/rx_pwm.h"
#include "drivers/io.h"
#include "sensors/acceleration.h"
#include "drivers/light_led.h"
#include "io/serial.h"
#include "fc/runtime_config.h"
#include "StaP.h"
#include "stm32f4xx.h"

StaP_ErrorStatus_T StaP_ErrorState;
bool failSafeMode = false;

#ifdef STAP_DEBUG_LEVEL
uint8_t consoleDebugLevel = STAP_DEBUG_LEVEL;
#else
uint8_t consoleDebugLevel = 0;
#endif

static STAP_MutexRef_T mutex[StaP_NumOfLinks];

volatile uint8_t nestCount = 0;

struct Transceiver {
  void *hw;
  serialPort_t *ref;
};

struct Transceiver trans[StaP_NumOfTxcvs] = {
  [StaP_Txcv_VCP] = { NULL, NULL },
  [StaP_Txcv_UART1] = { (void*) USART1, NULL },
  [StaP_Txcv_UART2] = { (void*) USART2, NULL },
  [StaP_Txcv_UART3] = { (void*) USART3, NULL },
  [StaP_Txcv_UART4] = { (void*) UART4, NULL },
  [StaP_Txcv_UART5] = { (void*) UART5, NULL },
  [StaP_Txcv_UART6] = { (void*) USART6, NULL }
};

/*
	       ALP_Link_HostRX,
	       ALP_Link_HostTX,
	       ALP_Link_ALinkRX,
	       ALP_Link_ALinkTX,
	       ALP_Link_SRXLInA,
	       ALP_Link_SRXlOutA,
	       ALP_Link_SRXLInB,
	       ALP_Link_SRXlOutB,
	       ALP_Link_TelemRX,
	       ALP_Link_TelemTX,
	       ALP_Link_SRXLInA_TX,
	       ALP_Link_SRXLInB_TX,
	       ALP_Link_ALinkMon,
*/

StaP_ErrorStatus_T STAP_Status(bool clear)
{
  StaP_ErrorStatus_T value = StaP_ErrorState;

  if(!failSafeMode && value) {
    int i = 0;
    
    for(i = 0; i < 31; i++) {
      if(value & (1UL<<i))
	STAP_DEBUG(0, "StaP error %d", i);
    }
  }
  
  if(clear)
    StaP_ErrorState = 0UL;

  return value;
}

#ifdef USE_NATIVE_SCHEDULER

void STAP_Forbid(void)
{
  __disable_irq();
  nestCount++;
}

void STAP_Permit(void)
{ 
  if(!--nestCount)
    __enable_irq();
}		   

void STAP_DelayMillis(VP_TIME_MILLIS_T value)
{
  delay(value);
}

void STAP_DelayMillisUntil(VP_TIME_MILLIS_T *start, VP_TIME_MILLIS_T d)
{
  while(VP_ELAPSED_MILLIS(*start) < d);

  *start += d;
}

#endif

StaP_CPUClkTime_T STAP_CPUClkTime(void)
{
  return (StaP_CPUClkTime_T) DWT->CYCCNT; // (SysTick->VAL & 0xFFFF);
}

void STAP_DelayMillisFromISR(VP_TIME_MILLIS_T value)
{
  while(value-- > 0) {
    StaP_CPUClkTime_T start = STAP_CPUClkTime();
    
    while(STAP_CPUClkTime() - start < (StaP_CPUClkTime_T) SystemCoreClock/1000);
  }
}

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
    /*
    va_start(argp, format);
  
    consoleNotef("PANIC(%#x) ", reason);
    consolevPrintf(format, argp);
    consoleNL();
    consoleFlush();

    va_end(argp);
    */
    
    STAP_Indicate(reason);
  }
}

void STAP_Reboot(bool bootloader)
{
  if(bootloader)
    systemResetToBootloader();
  else
    systemReset();

  // We should not get here
  STAP_Panic(STAP_ERR_FELLTHROUGH+1);
}

uint16_t inavStaP_I2CErrorCount(void)
{
  return i2cGetErrorCounterReset();
}

uint16_t inavStaP_I2CErrorCode(void)
{
  return i2cGetErrorCodeReset();
}

#define MAX_I2C_TRANSMIT    (1<<7)

uint8_t inavStaP_I2CTransfer(uint8_t addr, const StaP_TransferUnit_t *seg, int num)
{
  uint8_t txBuffer[MAX_I2C_TRANSMIT], txSize = 0;
  bool success = false;
  int i = 0;

  while(i < num && seg[i].dir == transfer_dir_transmit) {
    if(txSize+seg[i].size < MAX_I2C_TRANSMIT) {
      memcpy((void*) &txBuffer[txSize], seg[i].data.tx, seg[i].size);
      txSize += seg[i].size;
    } else
      return 0xFE;
	
    i++;
  }
  
  if(i < num) {
    // We encountered a receive segment so it's a read

    success = i2cReadGeneric(STAP_I2C_BUS, addr, 0, NULL, //txSize, txBuffer,
			       seg[i].size, seg[i].data.rx); 

  } else {
    // It's a write
    
    success = i2cWriteGeneric(STAP_I2C_BUS, addr, 0, NULL, txSize, txBuffer);
  }
  
  return success ? 0 : i2cGetErrorCode();
}

#include "sensors/acceleration.h"
#include "sensors/gyro.h"
#include "flight/imu.h"

static bool gyroCalibStarted = false;

bool inavStaP_GyroIsGood(void)
{
  return gyroCalibStarted && gyroIsCalibrationComplete()
    ;//    && STATE(ACCELEROMETER_CALIBRATED);
}

void inavStaP_GyroCalibrate(void)
{
  gyroStartCalibration();
  gyroCalibStarted = true;
}

void inavStaP_Arm(void)
{
  ENABLE_ARMING_FLAG(ARMED);
  ENABLE_STATE(FIXED_WING_LEGACY);

  consoleNote("  INAV FIXED_WING_LEGACY = ");
  consolePrint(STATE(FIXED_WING_LEGACY) ? "TRUE" : "FALSE");
  consolePrint(" INAV ARMING_FLAG = ");
  consolePrintLn(ARMING_FLAG(ARMED) ? "TRUE" : "FALSE");
  consoleNote("   acc_ignore_rate/slope = ");
  consolePrintUI(imuConfig()->acc_ignore_rate);
  consolePrint("/");
  consolePrintLnUI(imuConfig()->acc_ignore_slope);  
  consoleNote("   DCM acc kp/ki = ");
  consolePrintUI(imuConfig()->dcm_kp_acc);
  consolePrint("/");
  consolePrintLnUI(imuConfig()->dcm_ki_acc);  
}

void inavStaP_GyroUpdate(void)
{
  fpVector3_t value = { .v = { 0, 0, 0} };
  
  gyroUpdate();
  // gyroFilter();
  imuUpdateAccelerometer();
  imuUpdateAttitude(micros());

  // Attitude, earth frame, Euler angles in decidegrees
  
  vpFlight.bank = attitude.values.roll/RADIAN/10;
  vpFlight.pitch = -attitude.values.pitch/RADIAN/10;
  vpFlight.heading = attitude.values.yaw/10;
  
  // Rotation rate, body frame, rad/s

  gyroGetMeasuredRotationRate(&value);

  vpFlight.rollR = value.v[X];
  vpFlight.pitchR = -value.v[Y];
  vpFlight.yawR = -value.v[Z];

  // Acceleration, body frame, cm/s^2

  accGetMeasuredAcceleration(&value);  // Acceleration in cm/s2

  const float cm = 0.01f;
  
  vpFlight.accX = value.v[X]*cm;
  vpFlight.accY = -value.v[Y]*cm;
  vpFlight.accZ = value.v[Z]*cm;
}

void inavStaP_CalibrateAccel(void)
{
  accStartCalibration();
}

uint32_t stap_baudRate[StaP_NumOfLinks];

/*
int inavStaP_LinkStatus(int port)
{
  if(traceLen > 0) {
    char buffer[TRACE_BUFSIZE];
    bool overflow = traceOverflow > 0;
    
    STAP_FORBID;
    
    memcpy(buffer, traceBuf, traceLen+1);
    traceLen = 0;
    traceOverflow = 0;
    
    STAP_PERMIT;
    
    consoleNote_P(CS_STRING("TRACE : "));
    consolePrintLn(buffer);
    
    if(overflow > 0)
      consoleNoteLn_P(CS_STRING("TRACE OVERFLOW"));
  }
  
  if(stap_serialPort[port] && serialRxBytesWaiting(stap_serialPort[port]))
    return 1;
  
  return 0;
}
*/


bool inavStaP_LinkQuiescent(uint8_t port, VP_TIME_MICROS_T period)
{
  bool status = true;
  
  if(!trans[StaP_LinkTable[port].txcv].ref)
    return status;
  
  STAP_FORBID;
  status = VP_ELAPSED_MICROS(StaP_LinkTable[port].lastReceived) >= period;
  STAP_PERMIT;

  return status;
}

void inavStaP_LinkSetRate(uint8_t port, uint32_t baudrate)
{
  if(!trans[StaP_LinkTable[port].txcv].ref)
    return;
  
  if(stap_baudRate[port] != baudrate) {
    serialSetBaudRate(trans[StaP_LinkTable[port].txcv].ref, baudrate);
    
    vpbuffer_flush(&StaP_LinkTable[port].buffer);

    stap_baudRate[port] = baudrate;
  }
}

bool inavStaP_LinkHasOverrun(uint8_t port)
{
  if(!trans[StaP_LinkTable[port].txcv].ref)
    return false;
  
  return vpbuffer_hasOverrun(&StaP_LinkTable[port].buffer);
}

int inavStaP_LinkStatus(uint8_t port)
{
  int status = 0;
  
  if(!trans[StaP_LinkTable[port].txcv].ref)
    return status;
  
  if(StaP_LinkDir(port))
    status = serialTxBytesFree(trans[StaP_LinkTable[port].txcv].ref);
  else
    status = vpbuffer_gauge(&StaP_LinkTable[port].buffer);

  return status;
}

char inavStaP_LinkGetChar(uint8_t port)
{
  char buffer = 0xFF;
  
  if(!trans[StaP_LinkTable[port].txcv].ref)
    return buffer;

  inavStaP_LinkGet(port, &buffer, 1);

  return buffer;
}

int inavStaP_LinkGet(uint8_t port, char *buffer, int size)
{
  VPBufferSize_t num = 0;
  
  if(!trans[StaP_LinkTable[port].txcv].ref)
    return num;
  
  STAP_FORBID;
  num = vpbuffer_extract(&StaP_LinkTable[port].buffer, buffer, size);
  STAP_PERMIT;
  
  return num;
}

int inavStaP_LinkPutNB(uint8_t port, const char *buffer, int size)
{
  if(!trans[StaP_LinkTable[port].txcv].ref || failSafeMode) 
    return size;
  
  int len = inavStaP_LinkStatus(port);

  if(len > size)
    len = size;

  if(len > 0)
    serialWriteBuf(trans[StaP_LinkTable[port].txcv].ref,
		   (const uint8_t*) buffer, len);
  return len;
}

int inavStaP_LinkPut(uint8_t port, const char *buffer, int size, VP_TIME_MILLIS_T timeout)
{
  if(!trans[StaP_LinkTable[port].txcv].ref)
    return 0;

  if(!mutex[port])
    STAP_Panic(STAP_ERR_MUTEX_CREATE);

  STAP_MutexObtain(mutex[port]);

  STAP_FORBID;
  signalOwner[StaP_LinkTable[port].signal] = xTaskGetCurrentTaskHandle();
  STAP_PERMIT;

  VP_TIME_MILLIS_T startTime = vpTimeMillis();
  int left = size;
  
  STAP_LED1_ON;
  
  do {
    int len = inavStaP_LinkPutNB(port, buffer, left);

    buffer += len;
    left -= len;

    if(len > 0)
      STAP_DelayMillis(1);    
  } while(left > 0 && VP_ELAPSED_MILLIS(startTime) < timeout);

  STAP_LED1_OFF;
  
  STAP_FORBID;
  signalOwner[StaP_LinkTable[port].signal] = NULL;  
  STAP_PERMIT;

  STAP_MutexRelease(mutex[port]);
    
  return left;
}

int inavStaP_LinkPutChar(uint8_t port, char c, VP_TIME_MILLIS_T timeout)
{
  return inavStaP_LinkPut(port, &c, 1, timeout);
}

bool inavStaP_LinkIsUsable(uint8_t port)
{
  return StaP_LinkTable[port].txcv != StaP_Txcv_Invalid
    && trans[StaP_LinkTable[port].txcv].ref != NULL;
}

bool inavStaP_LinkIsIdle(uint8_t port)
{
  return trans[StaP_LinkTable[port].txcv].ref == NULL
    || isSerialTransmitBufferEmpty(trans[StaP_LinkTable[port].txcv].ref);
}

void inavStaP_LinkDrain(uint8_t port,  VP_TIME_MILLIS_T timeout)
{
  while(!inavStaP_LinkIsIdle(port));
}

void inavStaP_LinkSetMode(uint8_t port, uint8_t mode)
{
  if(!trans[StaP_LinkTable[port].txcv].ref)
    return;
  
  serialSetMode(trans[StaP_LinkTable[port].txcv].ref, mode);
}

void inavStaP_LinkTalk(uint8_t port)
{
  if(!(StaP_LinkTable[port].mode & LINK_MODE_TX))
    // The default mode is non-TX, change the mode
    
    inavStaP_LinkSetMode(port, MODE_TX);

  serialBeginWrite(trans[StaP_LinkTable[port].txcv].ref);
}

void inavStaP_LinkListen(uint8_t port, VP_TIME_MILLIS_T timeout)
{
  if(!(StaP_LinkTable[port].mode & LINK_MODE_TX)) {
    // Restore the previous (non-TX) mode after draining
    
    STAP_LinkDrain(port, timeout);
    serialEndWrite(trans[StaP_LinkTable[port].txcv].ref);
    inavStaP_LinkSetMode(port, StaP_LinkTable[port].mode);
  }
}

/*
void inavStaP_LinkTalk(uint8_t port)
{
  if(!trans[linkMap[port]].valid || !trans[linkMap[port]].ref)
    return;
  
  serialBeginWrite(trans[linkMap[port]].ref);
}

void inavStaP_LinkListen(uint8_t port)
{
  if(!trans[linkMap[port]].valid || !trans[linkMap[port]].ref)
    return;
  
  serialEndWrite(trans[linkMap[port]].ref);
}
*/

VP_TIME_MICROS_T STAP_TimeMicros(void)
{
  return (VP_TIME_MICROS_T) micros();
}

VP_TIME_SECS_T STAP_TimeSecs(void)
{
  return (uint32_t) (millis()>>10);
}

size_t STAP_MemoryFree(void)
{
#ifdef USE_NATIVE_SCHEDULER
  return hal.util->available_memory();
#else
  return xPortGetFreeHeapSize();
#endif
}

void inavStaP_pwmOutput(int num, const uint16_t value[])
{
  int i = 0;

  for(i = 0; i < num; i++)
    pwmWriteServo(i, value[i]);
}

void serialInputCallback(short unsigned int data, void *context)
{
  StaP_LinkRecord_T
    *link = &StaP_LinkTable[(StaP_LinkId_T) context];

  if(data & ~0xFF)
    link->buffer.overrun = true;
  
  vpbuffer_insertChar(&link->buffer, (uint8_t) (data & 0xFF));

  link->lastReceived = vpTimeMicros();
  
  if(VPBUFFER_GAUGE(link->buffer) > link->buffer.watermark)
    STAP_SignalFromISR(link->signal);
}

void usbSerialPoll(uint8_t port)
{
  StaP_LinkRecord_T *link = &StaP_LinkTable[port];

  if(!link->txcv || !trans[link->txcv].ref)
    STAP_Panic(STAP_ERR_NO_LINK);

  uint32_t size = serialRxBytesWaiting(trans[link->txcv].ref);

  if(size > 0) {
    while(size-- > 0)
      vpbuffer_insertChar(&link->buffer,
			  serialRead(trans[link->txcv].ref));
  
    link->lastReceived = vpTimeMicros();
  }   
}

static void serialLinkInit(uint8_t link, uint32_t baudRate, uint8_t mode)
{
  if(StaP_LinkTable[link].txcv != StaP_Txcv_Invalid
     && !trans[StaP_LinkTable[link].txcv].ref) {
    if(trans[StaP_LinkTable[link].txcv].hw) {
      // An actual pointer to UART hardware

      uint8_t modeEnc = 0, options = SERIAL_UNIDIR;

      switch(mode) {
      case LINK_MODE_RX:
	modeEnc = MODE_RX;
	break;
      case LINK_MODE_TX:
	modeEnc = MODE_TX;
	break;
      case LINK_MODE_RXTX:
	modeEnc = MODE_RXTX;
	break;
      }

      if(link == ALP_Link_SRXLInA || link == ALP_Link_SRXLInA_TX)
	options = SERIAL_BIDIR;

      trans[StaP_LinkTable[link].txcv].ref
	= uartOpen(trans[StaP_LinkTable[link].txcv].hw,
		   serialInputCallback, (void*) link,
		   baudRate, modeEnc, options | SERIAL_NOT_INVERTED);

    } else
      // It's the VCP
      
      trans[StaP_LinkTable[link].txcv].ref = usbVcpOpen();
  }
}

// cycles per microsecond, this is deliberately uint32_t to avoid type conversions
// This is not static so system.c can set it up for us.
uint32_t usTicks = 0;
// current uptime for 1kHz systick timer. will rollover after 49 days. hopefully we won't care.
STATIC_UNIT_TESTED volatile timeMs_t sysTickUptime = 0;
STATIC_UNIT_TESTED volatile uint32_t sysTickValStamp = 0;

// Return system uptime in milliseconds (rollover in 49 days)
timeMs_t millis(void)
{
    return sysTickUptime;
}

// SysTick

static volatile int sysTickPending = 0;

int FreeRTOSUp;

void SysTick_Handler(void)
{
  // ATOMIC_BLOCK(NVIC_PRIO_MAX) {
        sysTickUptime++;
        sysTickValStamp = SysTick->VAL;
        sysTickPending = 0;
        (void)(SysTick->CTRL);
	// }
#ifdef USE_HAL_DRIVER
    // used by the HAL for some timekeeping and timeouts, should always be 1ms
    HAL_IncTick();
#endif

    if(FreeRTOSUp)
      SysTick_HandlerFR();
}

uint32_t ticks(void)
{
#ifdef UNIT_TEST
    return 0;
#else
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    return DWT->CYCCNT;
#endif
}

timeDelta_t ticks_diff_us(uint32_t begin, uint32_t end)
{
    return (end - begin) / usTicks;
}

// Return system uptime in microseconds
timeUs_t microsISR(void)
{
    register uint32_t ms, pending, cycle_cnt;

    ForbidContext_T c = STAP_FORBID_SAFE;
    
    //ATOMIC_BLOCK(NVIC_PRIO_MAX) {
        cycle_cnt = SysTick->VAL;

        if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) {
            // Update pending.
            // Record it for multiple calls within the same rollover period
            // (Will be cleared when serviced).
            // Note that multiple rollovers are not considered.

            sysTickPending = 1;

            // Read VAL again to ensure the value is read after the rollover.

            cycle_cnt = SysTick->VAL;
        }

        ms = sysTickUptime;
        pending = sysTickPending;
	//}

	STAP_PERMIT_SAFE(c);
	
    // XXX: Be careful to not trigger 64 bit division
    const uint32_t partial = (usTicks * 1000U - cycle_cnt) / usTicks;
    return ((timeUs_t)(ms + pending) * 1000LL) + ((timeUs_t)partial);
}

timeUs_t micros(void)
{
    register uint32_t ms, cycle_cnt;

    // Call microsISR() in interrupt and elevated (non-zero) BASEPRI context

    if ((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) || (__get_BASEPRI())) {
        return microsISR();
    }

    do {
        ms = sysTickUptime;
        cycle_cnt = SysTick->VAL;
    } while (ms != sysTickUptime || cycle_cnt > sysTickValStamp);

    // XXX: Be careful to not trigger 64 bit division
    const uint32_t partial = (usTicks * 1000U - cycle_cnt) / usTicks;
    return ((timeUs_t)ms * 1000LL) + ((timeUs_t)partial);
}

void delayMicroseconds(timeUs_t us)
{
    timeUs_t now = micros();
    while (micros() - now < us);
}

void delay(timeMs_t ms)
{
    while (ms--)
        delayMicroseconds(1000);
}

void STAP_Initialize(void)
{
  int i = 0;
  
  //
  // Configure IMU
  //
  
  imuConfigMutable()->dcm_kp_acc = 100;             // / 10000
  imuConfigMutable()->dcm_ki_acc = 3;               // / 10000
  imuConfigMutable()->acc_ignore_rate = 1;
  imuConfigMutable()->acc_ignore_slope = 1;

  // Initialize INAV drivers
  
  init();

  //  STAP_Indicate(5);

  //
  // Initialize serial ports
  //

  for(i = 0; i < StaP_NumOfLinks; i++) {
    if(StaP_LinkTable[i].txcv != StaP_Txcv_Invalid) {
      if(StaP_LinkDir(i)) {
	//        UART_Table[STAP_LINK_HW(i)].txLink = i;

	if(!(mutex[i] = STAP_MutexCreate))
	  STAP_Panic(STAP_ERR_MUTEX_CREATE);
      } else
        ;// UART_Table[STAP_LINK_HW(i)].rxLink = i;

      serialLinkInit(i, StaP_LinkTable[i].rate, StaP_LinkTable[i].mode);
    }
  }
}

#define NUM_SWITCHES   3

static bool switchInitialized = false;
static ioTag_t switchTags[] = { IO_TAG(PC5), IO_TAG(PA15), IO_TAG(PC15) };
static IO_t switchIO[NUM_SWITCHES];

static void inavStaP_SwitchInit(void)
{
  if(!switchInitialized) {
    int i = 0;
    
    for(i = 0; i < NUM_SWITCHES; i++) {
      switchIO[i] = IOGetByTag(switchTags[i]);
      IOConfigGPIO(switchIO[i], IOCFG_IPU);
    }
    
    switchInitialized = true;
  }
}

uint8_t inavStaP_SwitchRead(uint8_t index)
{
  if(!switchInitialized)
    inavStaP_SwitchInit();

  return IORead(switchIO[index]);
}
    


