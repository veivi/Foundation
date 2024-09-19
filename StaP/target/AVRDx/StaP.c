#include "Scheduler.h"
#include "Buffer.h"
#include "Console.h"
#include "Objects.h"
#include "USART.h"
#include "SPI.h"
#include "Timer.h"
#include <stdarg.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "avr/cpufunc.h"

// #define SERIAL_TX_SYNC     1
#define MAX_SERVO          5
// #define SERIAL_DRAIN_DELAY    1
// #define STAP_ACTION_NO_UART return
#define STAP_ACTION_NO_UART  STAP_Panic(STAP_ERR_NO_UART)

int FreeRTOSUp;
volatile uint32_t ticks = 0, superTick = 0;
StaP_ErrorStatus_T StaP_ErrorState;
bool failSafeMode = false;

#ifdef STAP_DEBUG_LEVEL
uint8_t consoleDebugLevel = STAP_DEBUG_LEVEL;
#else
uint8_t consoleDebugLevel = 0;
#endif

#ifdef STAP_MutexCreate
static STAP_MutexRef_T mutex[StaP_NumOfLinks];
#endif

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

struct Transceiver {
  volatile USART_t *hw;
  StaP_LinkId_T txLink, rxLink;
  bool initialized;
};

volatile static struct Transceiver txcvTable[StaP_NumOfTxcvs] = {
  [AVRDx_Txcv_UART0] = { &USART0, 0, 0, false },
  [AVRDx_Txcv_UART1] = { &USART1, 0, 0, false },
  [AVRDx_Txcv_UART2] = { &USART2, 0, 0, false }
#if STAP_NUM_UARTS > 3
  ,[AVRDx_Txcv_UART3] = { &USART3, 0, 0, false }
#if STAP_NUM_UARTS > 4
  ,[AVRDx_Txcv_UART4] = { &USART4, 0, 0, false }
#endif
#endif
};

#define STAP_LINK_HAS_HW(link) \
  (StaP_LinkTable[link].txcv != StaP_Txcv_Invalid && txcvTable[StaP_LinkTable[link].txcv].initialized)
#define STAP_LINK_HW(link) (txcvTable[StaP_LinkTable[link].txcv].hw)

size_t STAP_MemoryFree(void)
{
  return xPortGetFreeHeapSize();
}

void STAP_DelayMillisFromISR(uint16_t value)
{
  int i = 0;

  while(value-- > 0) {
    for(i = 0; i < F_CPU/2/1000000; i++) {
      TCB1.CNT = 0;
      while(TCB1.CNT < 1000);
    }
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

void STAP_Reboot(bool loader)
{
  (void) STAP_FORBID_SAFE;
  _PROTECTED_WRITE(RSTCTRL.SWRR, RSTCTRL_SWRST_bm);
  STAP_DelayMillisFromISR(1000);
  // We should not get here
  STAP_Panic(STAP_ERR_FELLTHROUGH+1);
}

bool AVRDxSTAP_NVStoreWrite(void *addr, const void *buffer, size_t size)
{
  // EEPROM_Write((uint16_t) addr, (const uint8_t*) buffer, size);
  return false;
}

bool AVRDxSTAP_NVStoreRead(const void *addr, void *buffer, size_t size)
{
  // EEPROM_Read((uint16_t) addr, (uint8_t*) buffer, size);
  return false;
}

void AVRDxSTAP_LinkTalk(uint8_t port)
{
  if(!STAP_LINK_HAS_HW(port))
    STAP_ACTION_NO_UART;

  if(!(StaP_LinkTable[port].mode & LINK_MODE_TX))
    // The default mode is non-TX, change the mode
    USART_SetMode(STAP_LINK_HW(port), LINK_MODE_TX);
}

void AVRDxSTAP_LinkListen(uint8_t port, VP_TIME_MILLIS_T timeout)
{
  if(!STAP_LINK_HAS_HW(port))
    STAP_ACTION_NO_UART;

  if(!(StaP_LinkTable[port].mode & LINK_MODE_TX)) {
    // Restore the previous (non-TX) mode after draining
    
    STAP_LinkDrain(port, timeout);
    USART_SetMode(STAP_LINK_HW(port), StaP_LinkTable[port].mode);
  }
}

static bool bufferDrainPrim(uint8_t port, uint8_t watermark, VP_TIME_MILLIS_T timeout)
{
  VP_TIME_MILLIS_T started = vpTimeMillisApprox;
  bool status = true;

  StaP_LinkTable[port].buffer.watermark = watermark;

  while(vpbuffer_gauge(&StaP_LinkTable[port].buffer) > watermark) {
    if(VP_MILLIS_FINITE(timeout) && VP_ELAPSED_MILLIS(started) > timeout) {
      status = false;
      break;
    }

#ifdef SERIAL_DRAIN_DELAY
    STAP_DelayMillis(SERIAL_DRAIN_DELAY);
#else
    if(StaP_LinkTable[port].signal) {
      STAP_SignalWaitTimeout(STAP_SignalSet(StaP_LinkTable[port].signal),
			     timeout);
    } else
      STAP_Panic(STAP_ERR_NO_SIGNAL);
#endif
  }
  
  return status;
}

bool AVRDxSTAP_LinkIsUsable(uint8_t port)
{
  return STAP_LINK_HW(port) != StaP_Txcv_Invalid;
  //    && trans[STAP_LINK_HW(port)].ref != NULL;
}
int AVRDxSTAP_LinkStatus(uint8_t port)
{
  if(!STAP_LINK_HAS_HW(port))
    STAP_ACTION_NO_UART;
  
  if(StaP_LinkDir(port))
    return vpbuffer_space(&StaP_LinkTable[port].buffer);
  else
    return vpbuffer_gauge(&StaP_LinkTable[port].buffer);
}

void AVRDxSTAP_LinkSuspend(uint8_t port)
{
  if(!STAP_LINK_HAS_HW(port))
    STAP_ACTION_NO_UART;
  
  if(!StaP_LinkDir(port))
    USART_SetMode(STAP_LINK_HW(port), StaP_LinkTable[port].mode & ~LINK_MODE_RX);
}

void AVRDxSTAP_LinkResume(uint8_t port)
{
  if(!STAP_LINK_HAS_HW(port))
    STAP_ACTION_NO_UART;
  
  if(!StaP_LinkDir(port))
    USART_SetMode(STAP_LINK_HW(port), StaP_LinkTable[port].mode);
}

char AVRDxSTAP_LinkGetChar(uint8_t port)
{
  if(!STAP_LINK_HAS_HW(port))
    STAP_ACTION_NO_UART;
  
    char buf = 0;
    AVRDxSTAP_LinkGet(port, &buf, 1);
    return buf;
}
  
int AVRDxSTAP_LinkGet(uint8_t port, char *buffer, int size)
{
  if(!STAP_LINK_HAS_HW(port))
    STAP_ACTION_NO_UART;
  
  VPBufferIndex_t num = 0;
  
  num = vpbuffer_extract(&StaP_LinkTable[port].buffer, buffer, size);

  if(vpbuffer_hasOverrun(&StaP_LinkTable[port].buffer))
    STAP_Error(STAP_ERR_RX_OVERRUN_S);
     
  return num;
}

int AVRDxSTAP_LinkPutSync(uint8_t port, const char *buffer, int size, VP_TIME_MILLIS_T timeout, bool sync)
{
  if(!STAP_LINK_HAS_HW(port))
    STAP_ACTION_NO_UART;
  
  if(size > 0x1000)
    STAP_Panic(STAP_ERR_TX_TOO_BIG);

  STAP_MutexObtain(mutex[port]);
  
#ifndef SERIAL_TX_SYNC
  if(failSafeMode) {
#endif
    USART_Transmit(STAP_LINK_HW(port), buffer, size);
  
    if(sync && !USART_Drain(STAP_LINK_HW(port), timeout))
      STAP_Error(STAP_ERR_TX_TIMEOUT);

#ifndef SERIAL_TX_SYNC
    return 0;
  }
      
  if(signalOwner[StaP_LinkTable[port].signal])
    STAP_Panicf(STAP_ERR_TASK_INVALID, "Vittu!");
  
  STAP_FORBID;
  signalOwner[StaP_LinkTable[port].signal] = xTaskGetCurrentTaskHandle();
  STAP_PERMIT;

  VPBufferIndex_t space = 0;

  while(size > 0) {
    VPBufferIndex_t progress =
      vpbuffer_insert(&StaP_LinkTable[port].buffer, buffer, size, false);

    if(progress > 0) {
      USART_TransmitStart(STAP_LINK_HW(port), &StaP_LinkTable[port].buffer);
      buffer += progress;
      size -= progress;

      //      if(port == ALP_Link_ALinkTX)
      //      	consolePrintf("%d ", size);
    }
    
    if(size > 0) {
      //    while((space = vpbuffer_space(&StaP_LinkTable[port].buffer)) == 0) {
      // We need to wait until more space becomes available
	
      VPBufferSize_t watermark
	= StaP_LinkTable[port].buffer.mask - size;

      if(watermark < 2)
	// Never drain completely empty so we maximize bandwidth
	watermark = 2;

      if(!bufferDrainPrim(port, watermark, timeout)) {
	// Timed out
	STAP_Error(STAP_ERR_TX_TIMEOUT);
	break;
      }
    }
  }

    /*
    if(space == 0) {
      // We have timed out
      break;
    }
    if(space > size)
      space = size;

    vpbuffer_insert(&StaP_LinkTable[port].buffer, buffer, space, false);
    
    USART_TransmitStart(STAP_LINK_HW(port), &StaP_LinkTable[port].buffer);

    buffer += space;
    size -= space;
    }*/

  if(sync) {
    if(!bufferDrainPrim(port, 0, timeout)) {
      STAP_Error(STAP_ERR_TX_TIMEOUT);
      vpbuffer_flush(&StaP_LinkTable[port].buffer);
    }
    
    if(!USART_Drain(STAP_LINK_HW(port), timeout))
      STAP_Error(STAP_ERR_TX_TIMEOUT);
  }
  
  STAP_FORBID;
  signalOwner[StaP_LinkTable[port].signal] = NULL;  
  STAP_PERMIT;
#else
  size = 0;
#endif

  STAP_MutexRelease(mutex[port]);
    
  return size;
}

int AVRDxSTAP_LinkPut(uint8_t port, const char *buffer, int size, VP_TIME_MILLIS_T timeout)
{
  return AVRDxSTAP_LinkPutSync(port, buffer, size, timeout, false);
}

void AVRDxSTAP_LinkDrain(uint8_t port, VP_TIME_MILLIS_T timeout)
{
  AVRDxSTAP_LinkPutSync(port, NULL, 0, timeout, true);
}

int AVRDxSTAP_LinkPutChar(uint8_t port, char c, VP_TIME_MILLIS_T timeout)
{
  return AVRDxSTAP_LinkPut(port, &c, 1, timeout);
}

void AVRDxSTAP_LinkSetRate(uint8_t port, ulong rate)
{
  USART_SetRate(STAP_LINK_HW(port), rate);
}
  
static void timerCallback(TCB_t *instance)
{
  ticks++;
  if(ticks == 0)
    superTick++;
}

#if F_CPU == 24000000UL
#define JIFFYS_TO_MICROS(v)  (((v)>>4)+((v)>>5))
#define TICKS_TO_MICROS(v)   (((v)<<12)+((v)<<11))
#elif F_CPU == 16000000UL
#define JIFFYS_TO_MICROS(v)  ((v)>>3)
#define TICKS_TO_MICROS(v)   ((v)<<13)
#else
#error "Clock speed must be 16M or 24M!!"
#endif

static VP_TIME_MICROS_T primitiveMicros(void)
{
  uint16_t count = TCB1.CNT + 1;
  uint32_t tickValue = ticks;

  return TICKS_TO_MICROS(tickValue)
    + JIFFYS_TO_MICROS((VP_TIME_MICROS_T) count);
}

VP_TIME_MICROS_T STAP_TimeMicros(void)
{
  ForbidContext_T c = STAP_FORBID_SAFE;
  
  VP_TIME_MICROS_T time0 = primitiveMicros(), time1 = primitiveMicros();

  STAP_PERMIT_SAFE(c);
  
  if(time1 < time0)
    time1 += JIFFYS_TO_MICROS((VP_TIME_MICROS_T) (1UL<<16));

  return time1;
}

VP_TIME_SECS_T STAP_TimeSecs(void)
{
    ForbidContext_T c = STAP_FORBID_SAFE;
    uint64_t now = (superTick<<32) | ticks;
    STAP_PERMIT_SAFE(c);

    return (VP_TIME_SECS_T) ((now>>4) / (F_CPU/2/1000000UL));
}

uint16_t STAP_CPUIdlePermille(void)
{
#ifdef HAVE_RUNTIME_STATS
  static VP_TIME_MICROS_T prevTime, prevIdle;
  VP_TIME_MICROS_T cycle = VP_ELAPSED_MICROS(prevTime),
    idleTime = ulTaskGetIdleRunTimeCounter() - prevIdle;
  
  prevTime += cycle;;
  prevIdle += idleTime;

  return (uint16_t) (1000UL*idleTime / cycle);
#else
  return 0;
#endif
}

void CPUINT_Initialize()
{
    /* IVSEL and CVT are Configuration Change Protected */

    //IVSEL disabled; CVT disabled; LVL0RR disabled; 
    ccp_write_io((void*)&(CPUINT.CTRLA),0x00);
    
    //LVL0PRI 0; 
    CPUINT.LVL0PRI = 0x00;
    
    //LVL1VEC 0; 
    CPUINT.LVL1VEC = 0x00;

    // ENABLE_INTERRUPTS;
}

/* This function initializes the CLKCTRL module */
void CLKCTRL_init(void)
{
#if F_CPU > 20000000UL
  // Assume 24 MHz external oscillator
  ccp_write_io((void*)&(CLKCTRL.MCLKCTRLA), (CLKCTRL.MCLKCTRLA | CLKCTRL_CLKSEL_EXTCLK_gc));
#else
  // Use 16MHz on-chip oscillator
  ccp_write_io((void*)&(CLKCTRL.OSCHFCTRLA), (CLKCTRL.OSCHFCTRLA | CLKCTRL_FRQSEL_16M_gc));
#endif
}

/*
extern const struct PortDescriptor portTable[];
  
extern const portName_t pcIntPort[];
extern const uint8_t pcIntMask[];
const struct PinDescriptor led = { PortA, 5 };
const struct PinDescriptor latch = { PortF, 0 };

const struct PortDescriptor portTable[] = {
  [PortA] = { &PINA, &PORTA, &DDRA },
  [PortB] = { &PINB, &PORTB, &DDRB, &PCMSK0, 0 },
  [PortC] = { &PINC, &PORTC, &DDRC },
  [PortD] = { &PIND, &PORTD, &DDRD },
  [PortE] = { &PINE, &PORTE, &DDRE },
  [PortF] = { &PINF, &PORTF, &DDRF },
  [PortG] = { &PING, &PORTG, &DDRG },
  [PortH] = { &PINH, &PORTH, &DDRH },
  [PortK] = { &PINK, &PORTK, &DDRK, &PCMSK2, 2 },
  [PortL] = { &PINL, &PORTL, &DDRL }
};

const portName_t pcIntPort[] = {
  PortB, PortF, PortK
};

const uint8_t pcIntMask[] = {
  1<<PCIE0, 1<<PCIE1, 1<<PCIE2
};

void pinOutputEnable(const struct PinDescriptor *pin, bool output)
{
  if(output)
    *(portTable[pin->port].ddr) |= 1<<(pin->index);
  else
    *(portTable[pin->port].ddr) &= ~(1<<(pin->index));
}  

void setPinState(const struct PinDescriptor *pin, uint8_t state)
{
  if(state > 0)
    *(portTable[pin->port].port) |= 1<<(pin->index);
  else
    *(portTable[pin->port].port) &= ~(1<<(pin->index));
}

uint8_t getPinState(const struct PinDescriptor *pin)
{
  return (*(portTable[pin->port].pin)>>(pin->index)) & 1;
}

void configureInput(const struct PinDescriptor *pin, bool pullup)
{
  pinOutputEnable(pin, false);
  setPinState(pin, pullup ? 1 : 0);
}

void configureOutput(const struct PinDescriptor *pin)
{
  pinOutputEnable(pin, true);
}

typedef enum { COMnA = 0, COMnB = 1, COMnC = 2 } PWM_Ch_t;

struct HWTimer {
  volatile uint8_t *TCCRA, *TCCRB;
  volatile uint16_t *ICR;
  volatile uint16_t *OCR[3]; // 0... 2 = A ... C
  volatile uint16_t *TCNT;
  bool sync;
  int8_t log2scale;
};

struct PWMOutput {
  struct PinDescriptor pin;
  const struct HWTimer *timer;
  PWM_Ch_t pwmCh; // COMnA / COMnB / COMnC
  bool active;
};

static const uint8_t outputModeMask[] = { 1<<COM1A1, 1<<COM1B1, 1<<COM1C1 };

//
// HW timer declarations
//

const struct HWTimer hwTimer1 =
  { &TCCR1A, &TCCR1B, &ICR1, { &OCR1A, &OCR1B, &OCR1C }, &TCNT1, SYNC_PWM_OUTPUT, -2 };
  // const struct HWTimer hwTimer3 =
  // { &TCCR3A, &TCCR3B, &ICR3, { &OCR3A, &OCR3B, &OCR3C }, &TCNT3, false, 1 };
const struct HWTimer hwTimer3 =
  { &TCCR3A, &TCCR3B, &ICR3, { &OCR3A, &OCR3B, &OCR3C }, &TCNT3, SYNC_PWM_OUTPUT, -2 };
const struct HWTimer hwTimer4 =
  { &TCCR4A, &TCCR4B, &ICR4, { &OCR4A, &OCR4B, &OCR4C }, &TCNT4, SYNC_PWM_OUTPUT, -2 };

const struct HWTimer *hwTimersOwn[] = 
  { &hwTimer1, &hwTimer3, &hwTimer4 };

const struct HWTimer hwTimer5 =
  { &TCCR5A, &TCCR5B, &OCR5A, { &OCR5A, &OCR5B, &OCR5C }, &TCNT5, false, 1 };

struct PWMOutput pwmOutput[MAX_SERVO] = {
  { { PortB, 6 }, &hwTimer1, COMnB },
  { { PortB, 5 }, &hwTimer1, COMnA },
  { { PortH, 5 }, &hwTimer4, COMnC },
  { { PortH, 4 }, &hwTimer4, COMnB },
  { { PortH, 3 }, &hwTimer4, COMnA },
  { { PortE, 5 }, &hwTimer3, COMnC },
  { { PortE, 4 }, &hwTimer3, COMnB },
  { { PortE, 3 }, &hwTimer3, COMnA },
  { { PortB, 7 }, &hwTimer1, COMnC },
  { { PortL, 4 }, &hwTimer5, COMnB },
  { { PortL, 5 }, &hwTimer5, COMnC }
};

static void pwmTimerInit(const struct HWTimer *timer[], int num)
{
  int i = 0, j = 0;
  
  for(i = 0; i < num; i++) { 
    // WGM, prescaling

    *(timer[i]->TCCRA) = 1<<WGM11;
    *(timer[i]->TCCRB) = (1<<WGM13) | (1<<WGM12) | (1<<CS11) | (timer[i]->sync ? (1<<CS10) : 0);
    
   // PWM frequency

    if(timer[i]->sync)
      *(timer[i]->ICR) = 0xFFFF;
    else
      *(timer[i]->ICR) = MICROS_TO_CNT(timer[i], 1000000UL/PWM_HZ) - 1;
    
   // Output set to 1.5 ms by default

    for(j = 0; j < 3; j++)
      *(timer[i]->OCR[j]) = MICROS_TO_CNT(timer[i], 1500U);
  }
}

static void pwmTimerSync(const struct HWTimer *timer[], int num)
{
  int i = 0;

  for(i = 0; i < num; i++) {
    if(*(timer[i]->TCNT) > MICROS_TO_CNT(timer[i], 5000U))
      *(timer[i]->TCNT) = 0xFFFF;
  }
}

static void pwmEnable(const struct PWMOutput *output)
{
   *(output->timer->TCCRA) |= outputModeMask[output->pwmCh];
}

static void pwmDisable(const struct PWMOutput *output)
{
   *(output->timer->TCCRA) &= ~outputModeMask[output->pwmCh];
}
*/

struct PWMOutput {
  volatile TCA_t *TCA;
  volatile uint16_t *CMP;
};

struct PWMOutput pwmTable[] = {
  { &TCA0, &TCA0.SINGLE.CMP0 },
  { &TCA0, &TCA0.SINGLE.CMP1 },
  { &TCA0, &TCA0.SINGLE.CMP2 }
};

#define RC_OUTPUT_MIN_PULSEWIDTH 500
#define RC_OUTPUT_MAX_PULSEWIDTH 2500

#if F_CPU > 20000000UL
#define MICROS_TO_CNT(x) ((x)+(x>>1))   // Assume 24MHz and prescaler 16
#else
#define MICROS_TO_CNT(x) (x)            // Assume 16MHz and prescaler 16
#endif

static uint16_t constrain_period(uint16_t p) {
    if (p > RC_OUTPUT_MAX_PULSEWIDTH)
       return RC_OUTPUT_MAX_PULSEWIDTH;
    else if (p < RC_OUTPUT_MIN_PULSEWIDTH)
       return RC_OUTPUT_MIN_PULSEWIDTH;
    else return p;
}

void AVRDxSTAP_pwmOutput(uint8_t num, const uint16_t value[])
{
  int i = 0;

  for(i = 0; i < num && i < sizeof(pwmTable)/sizeof(struct PWMOutput); i++) {
    if(value[i])
      *pwmTable[i].CMP = MICROS_TO_CNT(constrain_period(value[i]));
    else
      *pwmTable[i].CMP = 0;
  }
  
  TCA0.SINGLE.CNT = ~0;
}
  
void STAP_Initialize(void)
{
  int i = 0;

  // Initialize clocks

  CLKCTRL_init();

  // Initialize interrupt handling

  //  CPUINT_Initialize();

  // Serial I/O outputs

#if STAP_MCU_PINS > 28
  PORTB.DIRSET = 1<<0;
  PORTE.DIRSET = 1<<0;
#endif
  
  PORTC.DIRSET = 1<<0;
  PORTF.DIRSET = 1<<0;

  // Initialize LED control

  STAP_LED0_PORT.DIRSET = STAP_LED0_MASK;
  STAP_LED1_PORT.DIRSET = STAP_LED1_MASK;
  STAP_LED0_OFF;
  STAP_LED1_OFF;

  // SPI outputs: CS, MOSI, CLK

#ifdef STAP_SPI_PORT
  STAP_SPI_PORT.DIRSET = STAP_SPI_MASK_MOSI | STAP_SPI_MASK_CLK;
#endif
  
#ifdef STAP_SPI_CS0_PORT
  STAP_SPI_CS0_PORT.DIRSET = STAP_SPI_CS0_MASK;
#endif
#ifdef STAP_SPI_CS1_PORT
  STAP_SPI_CS1_PORT.DIRSET = STAP_SPI_CS1_MASK;
#endif
#ifdef STAP_SPI_CS2_PORT
  STAP_SPI_CS2_PORT.DIRSET = STAP_SPI_CS2_MASK;
#endif

  // Initialize timer1

  TCB_Initialize(&TCB1);
  TCB1.CTRLA |= TCB_CLKSEL_DIV2_gc;
  
  TCB_SetCaptIsrCallback(timerCallback);

  // RTC is HW time for perf monitoring

  RTC.CTRLA |= 1;

  SPI_Initialize(&SPI0);
  
  // Serial I/O initialization

  for(i = 0; i < StaP_NumOfLinks; i++) {
    if(StaP_LinkTable[i].txcv != StaP_Txcv_Invalid) {
      if(StaP_LinkDir(i)) {
        txcvTable[StaP_LinkTable[i].txcv].txLink = i;

	if(StaP_LinkTable[i].signal) {
#ifndef STAP_MutexCreate
#error Must have MUTEX support for signaling serial transmission!
#endif
	  if(!(mutex[i] = STAP_MutexCreate))
	    STAP_Panic(STAP_ERR_MUTEX_CREATE);
	}
      } else
        txcvTable[StaP_LinkTable[i].txcv].rxLink = i;

      if(!txcvTable[StaP_LinkTable[i].txcv].initialized) {
        txcvTable[StaP_LinkTable[i].txcv].initialized = true;
	USART_Init(STAP_LINK_HW(i),
		     StaP_LinkTable[i].rate, StaP_LinkTable[i].mode);
      }
    }
  }

// PWM output

  PORTMUX.TCAROUTEA = 0x3;

  PORTD.DIRSET = (1<<5)-1;

  TCA0.SINGLE.CTRLA = (4<<1) | 1;  // EN + prescale 16
  TCA0.SINGLE.CTRLB = (7<<4) | 3;
  TCA0.SINGLE.PER = ~0;

}

//
// Interrupt Service Routines
//

#define AVRDxUSART_Receive_ISR(num, trans)	\
  ISR(USART ## num ## _RXC_vect) { \
    UBaseType_t yield = false;	       \
    if(txcvTable[trans].initialized) {  \
      if(txcvTable[trans].rxLink) { \
	USART_ReceiveWorker(txcvTable[trans].hw, \
			    StaP_LinkTable[txcvTable[trans].rxLink].buffer); \
	if(VPBUFFER_GAUGE(StaP_LinkTable[txcvTable[trans].rxLink].buffer) > \
	   StaP_LinkTable[txcvTable[trans].rxLink].buffer.watermark)	\
	  yield = STAP_SignalFromISR(StaP_LinkTable[txcvTable[trans].rxLink].signal); \
      } else								\
	STAP_Panic(STAP_ERR_ISR_USARTRX_LINK); \
    } else \
      STAP_Panic(STAP_ERR_ISR_USARTRX_HW); \
    if(yield)							\
      portYIELD_FROM_ISR();						\
  }

#define AVRDxUSART_Transmit_ISR(num, trans)		\
  ISR(USART ## num ## _DRE_vect) {  \
    UBaseType_t yield = false;	       \
    if(txcvTable[trans].initialized)	{ \
      if(txcvTable[trans].txLink) {					\
	USART_TransmitWorker(txcvTable[trans].hw, StaP_LinkTable[txcvTable[trans].txLink].buffer); \
	if(VPBUFFER_GAUGE(StaP_LinkTable[txcvTable[trans].txLink].buffer) <= \
	   StaP_LinkTable[txcvTable[trans].txLink].buffer.watermark \
	   && StaP_LinkTable[txcvTable[trans].txLink].signal) {		\
	  yield = STAP_SignalFromISR(StaP_LinkTable[txcvTable[trans].txLink].signal); \
        } \
      } else \
        STAP_Panic(STAP_ERR_ISR_USARTTX_LINK); \
    } else				 \
      STAP_Panic(STAP_ERR_ISR_USARTTX_HW);	\
    if(yield)							\
      portYIELD_FROM_ISR();						\
  }

AVRDxUSART_Receive_ISR(0, AVRDx_Txcv_UART0);
AVRDxUSART_Transmit_ISR(0, AVRDx_Txcv_UART0);
AVRDxUSART_Receive_ISR(1, AVRDx_Txcv_UART1);
AVRDxUSART_Transmit_ISR(1, AVRDx_Txcv_UART1);
AVRDxUSART_Receive_ISR(2, AVRDx_Txcv_UART2);
AVRDxUSART_Transmit_ISR(2, AVRDx_Txcv_UART2);
#if STAP_NUM_UARTS > 3
AVRDxUSART_Receive_ISR(3, AVRDx_Txcv_UART3);
AVRDxUSART_Transmit_ISR(3, AVRDx_Txcv_UART3);
#if STAP_NUM_UARTS > 4
AVRDxUSART_Receive_ISR(4, AVRDx_Txcv_UART4);
AVRDxUSART_Transmit_ISR(4, AVRDx_Txcv_UART4);
#endif
#endif

ISR(BADISR_vect)
{
  //  STAP_Panicf(STAP_ERR_SPURIOUS_INT, "Spurious interrupt!");
  STAP_Error(STAP_ERR_SPURIOUS_INT);
}

