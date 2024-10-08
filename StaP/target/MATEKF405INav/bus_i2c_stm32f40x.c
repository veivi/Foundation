/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <platform.h>

#include "build/atomic.h"

#include "common/utils.h"

#include "drivers/io.h"
#include "drivers/time.h"

#include "drivers/bus_i2c.h"
#include "drivers/nvic.h"
#include "io_impl.h"
#include "rcc.h"
#include "drivers/light_led.h"

#include <AlphaPilot/StaP.h>

#ifndef SOFT_I2C

static void i2cUnstick(IO_t scl, IO_t sda);

#define GPIO_AF_I2C GPIO_AF_I2C1

#ifdef STM32F4

#if defined(USE_I2C_PULLUP)
#define IOCFG_I2C IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_OD, GPIO_PuPd_UP)
#else
#define IOCFG_I2C IOCFG_AF_OD
#endif

#ifndef I2C1_SCL
#define I2C1_SCL PB8
#endif
#ifndef I2C1_SDA
#define I2C1_SDA PB9
#endif

#else

#ifndef I2C1_SCL
#define I2C1_SCL PB6
#endif
#ifndef I2C1_SDA
#define I2C1_SDA PB7
#endif
#define IOCFG_I2C   IO_CONFIG(GPIO_Mode_AF_OD, GPIO_Speed_50MHz)

#endif

#ifndef I2C2_SCL
#define I2C2_SCL PB10
#endif

#ifndef I2C2_SDA
#define I2C2_SDA PB11
#endif

#ifdef STM32F4
#ifndef I2C3_SCL
#define I2C3_SCL PA8
#endif
#ifndef I2C3_SDA
#define I2C3_SDA PB4
#endif
#endif

typedef enum {
    I2C_STATE_STOPPED = 0,
    I2C_STATE_STOPPING,
    I2C_STATE_STARTING,
    I2C_STATE_STARTING_WAIT,

    I2C_STATE_R_ADDR,
    I2C_STATE_R_ADDR_WAIT,
    I2C_STATE_R_REGISTER,
    I2C_STATE_R_REGISTER_WAIT,
    I2C_STATE_R_RESTARTING,
    I2C_STATE_R_RESTARTING_WAIT,
    I2C_STATE_R_RESTART_ADDR,
    I2C_STATE_R_RESTART_ADDR_WAIT,
    I2C_STATE_R_TRANSFER_EQ1,
    I2C_STATE_R_TRANSFER_EQ2,
    I2C_STATE_R_TRANSFER_GE2,

    I2C_STATE_W_ADDR,
    I2C_STATE_W_ADDR_WAIT,
    I2C_STATE_W_REGISTER,
    I2C_STATE_W_TRANSFER_WAIT,
    I2C_STATE_W_TRANSFER,

    I2C_STATE_NACK,
    I2C_STATE_BUS_ERROR,
} i2cState_t;

typedef enum {
    I2C_TXN_READ,
    I2C_TXN_WRITE
} i2cTransferDirection_t;

typedef struct i2cBusState_s {
    I2CDevice       device;
    bool            initialized;
    i2cState_t      state;
    uint32_t        timeout;

    /* Active transfer */
    bool                        allowRawAccess;
    uint8_t                     addr;   // device address
    i2cTransferDirection_t      rw;     // direction
    uint8_t                     reg;    // register
    uint32_t                    len;    // buffer length
    uint8_t                    *buf;    // buffer
    bool                        txnOk;
} i2cBusState_t;

static volatile uint16_t i2cErrorCount = 0, i2cErrorCode = 0;

static i2cDevice_t i2cHardwareMap[] = {
    { .dev = I2C1, .scl = IO_TAG(I2C1_SCL), .sda = IO_TAG(I2C1_SDA), .rcc = RCC_APB1(I2C1), .speed = I2C_SPEED_400KHZ },
    { .dev = I2C2, .scl = IO_TAG(I2C2_SCL), .sda = IO_TAG(I2C2_SDA), .rcc = RCC_APB1(I2C2), .speed = I2C_SPEED_400KHZ },
#ifdef STM32F4
    { .dev = I2C3, .scl = IO_TAG(I2C3_SCL), .sda = IO_TAG(I2C3_SDA), .rcc = RCC_APB1(I2C3), .speed = I2C_SPEED_400KHZ }
#endif
};

static i2cBusState_t busState[I2CDEV_COUNT] = { { 0 } };

static void i2cResetInterface(i2cBusState_t * i2cBusState)
{
    const i2cDevice_t * i2c = &(i2cHardwareMap[i2cBusState->device]);
    IO_t scl = IOGetByTag(i2c->scl);
    IO_t sda = IOGetByTag(i2c->sda);

    i2cErrorCount++;
    i2cUnstick(scl, sda);
    i2cInit(i2cBusState->device);
}

void i2cSetSpeed(uint8_t speed)
{
    for (unsigned int i = 0; i < sizeof(i2cHardwareMap) / sizeof(i2cHardwareMap[0]); i++) {
        i2cHardwareMap[i].speed = speed;
    }
}

uint32_t i2cTimeoutUserCallback(void)
{
    i2cErrorCount++;
    return false;
}

void i2cInit(I2CDevice device)
{
    if (device == I2CINVALID)
        return;

    i2cDevice_t *i2c = &(i2cHardwareMap[device]);

    IO_t scl = IOGetByTag(i2c->scl);
    IO_t sda = IOGetByTag(i2c->sda);

    RCC_ClockCmd(i2c->rcc, ENABLE);

    IOInit(scl, OWNER_I2C, RESOURCE_I2C_SCL, RESOURCE_INDEX(device));
    IOInit(sda, OWNER_I2C, RESOURCE_I2C_SDA, RESOURCE_INDEX(device));

#ifdef STM32F4
    IOConfigGPIOAF(scl, IOCFG_I2C, GPIO_AF_I2C);
    IOConfigGPIOAF(sda, IOCFG_I2C, GPIO_AF_I2C);
#else
    IOConfigGPIO(scl, IOCFG_I2C);
    IOConfigGPIO(sda, IOCFG_I2C);
#endif

    I2C_DeInit(i2c->dev);

    I2C_InitTypeDef i2cInit;
    I2C_StructInit(&i2cInit);

    i2cInit.I2C_Mode = I2C_Mode_I2C;
    i2cInit.I2C_DutyCycle = I2C_DutyCycle_2;
    i2cInit.I2C_OwnAddress1 = 0x00;
    i2cInit.I2C_Ack = I2C_Ack_Enable;
    i2cInit.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;

    switch (i2c->speed) {
        case I2C_SPEED_400KHZ:
        default:
            i2cInit.I2C_ClockSpeed = 400000;
            break;

        case I2C_SPEED_800KHZ:
            i2cInit.I2C_ClockSpeed = 800000;
            break;

        case I2C_SPEED_100KHZ:
            i2cInit.I2C_ClockSpeed = 100000;
            break;

        case I2C_SPEED_200KHZ:
            i2cInit.I2C_ClockSpeed = 200000;
            break;
    }

    I2C_Init(i2c->dev, &i2cInit);
    I2C_StretchClockCmd(i2c->dev, ENABLE);
    I2C_Cmd(i2c->dev, ENABLE);

    busState[device].device = device;
    busState[device].initialized = true;
    busState[device].state = I2C_STATE_STOPPED;
}

static void i2cUnstick(IO_t scl, IO_t sda)
{
    int i;

    IOHi(scl);
    IOHi(sda);

    IOConfigGPIO(scl, IOCFG_OUT_OD);
    IOConfigGPIO(sda, IOCFG_OUT_OD);

    // Analog Devices AN-686
    // We need 9 clock pulses + STOP condition
    for (i = 0; i < 9; i++) {
        // Wait for any clock stretching to finish
        int timeout = 100;
        while (!IORead(scl) && timeout) {
            delayMicroseconds(5);
            timeout--;
        }

        // Pull low
        IOLo(scl); // Set bus low
        delayMicroseconds(5);
        IOHi(scl); // Set bus high
        delayMicroseconds(5);
    }

    // Generate a stop condition in case there was none
    IOLo(scl);
    delayMicroseconds(5);
    IOLo(sda);
    delayMicroseconds(5);

    IOHi(scl); // Set bus scl high
    delayMicroseconds(5);
    IOHi(sda); // Set bus sda high
}

void i2cGenerateGlitch(I2CDevice device, int pin)
{
  if (device == I2CINVALID)
    return;

  i2cDevice_t *i2c = &(i2cHardwareMap[device]);

  IO_t io = pin ? IOGetByTag(i2c->sda) : IOGetByTag(i2c->scl);
  
  IOConfigGPIO(io, IOCFG_OUT_OD);
    
  IOLo(io);
  delayMicroseconds(5);
  IOHi(io);
  /*
#ifdef STM32F4
  IOConfigGPIOAF(io, IOCFG_I2C, GPIO_AF_I2C);
#else
  IOConfigGPIO(io, IOCFG_I2C);
#endif
  */
}
    
static bool i2cWaitForFlagGeneric(I2C_TypeDef *I2Cx, int num, const uint32_t *flag, FlagStatus status)
{
  VP_TIME_MICROS_T startTime = vpTimeMicros();

  while(VP_ELAPSED_MICROS(startTime) < I2C_TIMEOUT) {
    for(int i = 0; i < num; i++)
      if(I2C_GetFlagStatus(I2Cx, flag[i]) == status)
	return true;
  }

  return false;
}

static bool i2cWaitForFlag(I2C_TypeDef *I2Cx, uint32_t flag, FlagStatus status)
{
  return i2cWaitForFlagGeneric(I2Cx, 1, &flag, status);
}

static bool i2cFailure(I2CDevice device, uint16_t code)
{
  if(code) {
    //    STAP_TRACE("ERR ");
    //    STAP_TRACE_T(code, uint);

    i2cErrorCode = code;
    i2cErrorCount++;

    return false;
  }
  return true;
}

static bool i2cFailureReset(I2CDevice device, uint16_t code)
{
  if(code) {
    // i2cInit(device);
    i2cResetInterface(&busState[device]);
    delay(1);
  }
  
  return i2cFailure(device, code);
}

static bool i2cSync(I2CDevice device)
{
  I2C_TypeDef *I2Cx = i2cHardwareMap[device].dev;
  
  return i2cWaitForFlag(I2Cx, I2C_FLAG_BUSY, RESET);
}

static bool i2cTransmitStart(I2CDevice device, uint8_t target)
{
  I2C_TypeDef *I2Cx = i2cHardwareMap[device].dev;

  I2C_GenerateSTART(I2Cx, ENABLE);
  
  if(!i2cWaitForFlag(I2Cx, I2C_FLAG_SB, SET))  // Wait for START
    return i2cFailureReset(device, 0x10);
  
  I2C_ClearFlag(I2Cx, I2C_FLAG_AF);
  I2C_Send7bitAddress(I2Cx, target<<1, I2C_Direction_Transmitter);

  const uint32_t flags[] = { I2C_FLAG_ADDR, I2C_FLAG_AF };
  
  if(!i2cWaitForFlagGeneric(I2Cx, 2, flags, SET))  // Wait for ADDR (or AF)
    return i2cFailureReset(device, 0x11);

  (void)I2Cx->SR2;  // Clear ADDR flag

  return true;
}

static bool i2cTransmitLoop(I2CDevice device, uint8_t size, const uint8_t *message)
{
  I2C_TypeDef *I2Cx = i2cHardwareMap[device].dev;
  
  for(uint8_t i = 0; i < size; i++) {
    if(!i2cWaitForFlag(I2Cx, I2C_FLAG_TXE, SET))
      return false;
    
    I2C_SendData(I2Cx, message[i]);
  }

  return true;
}

static bool i2cReceiveStart(I2CDevice device, uint8_t target, uint8_t size)
{
  I2C_TypeDef *I2Cx = i2cHardwareMap[device].dev;
  
  I2C_GenerateSTART(I2Cx, ENABLE);
  
  if(!i2cWaitForFlag(I2Cx, I2C_FLAG_SB, SET))  // Wait for START
    return i2cFailureReset(device, 0x20);
  
  I2C_ClearFlag(I2Cx, I2C_FLAG_AF);
  I2C_Send7bitAddress(I2Cx, target<<1, I2C_Direction_Receiver);

  const uint32_t flags[] = { I2C_FLAG_ADDR, I2C_FLAG_AF };
  
  if(!i2cWaitForFlagGeneric(I2Cx, 2, flags, SET))  // Wait for ADDR (or AF)
    return i2cFailureReset(device, 0x21);

  I2C_NACKPositionConfig
    (I2Cx, size != 2 ? I2C_NACKPosition_Current : I2C_NACKPosition_Next);

  I2C_AcknowledgeConfig(I2Cx, size < 3 ? DISABLE : ENABLE);
  
  (void)I2Cx->SR2;  // Clear ADDR flag

  return true;
}

bool i2cReceiveLoop(I2CDevice device, uint8_t size, uint8_t *message)
{
  I2C_TypeDef *I2Cx = i2cHardwareMap[device].dev;
  
  if(size < 3) {
    if(size < 2) {
      I2C_GenerateSTOP(I2Cx, ENABLE);

      if(!i2cWaitForFlag(I2Cx, I2C_FLAG_RXNE, SET))
	return i2cFailureReset(device, 0x45);
    } else {
      if(!i2cWaitForFlag(I2Cx, I2C_FLAG_BTF, SET))
	return i2cFailureReset(device, 0x46);

      I2C_GenerateSTOP(I2Cx, ENABLE);
    }
    
    for(uint8_t i = 0; i < size; i++)
      message[i] = I2C_ReceiveData(I2Cx);
  } else {
    for(uint8_t i = 0; i < size - 3; i++) {
      if(!i2cWaitForFlag(I2Cx, I2C_FLAG_RXNE, SET))
	return i2cFailureReset(device, 0x47);
      
      message[i] = I2C_ReceiveData(I2Cx);
    }

    if(!i2cWaitForFlag(I2Cx, I2C_FLAG_BTF, SET))
      return i2cFailureReset(device, 0x48);

    I2C_AcknowledgeConfig(I2Cx, DISABLE);

    message[size-3] = I2C_ReceiveData(I2Cx);

    if(!i2cWaitForFlag(I2Cx, I2C_FLAG_BTF, SET))
      return i2cFailureReset(device, 0x49);

    I2C_GenerateSTOP(I2Cx, ENABLE);
    
    message[size-2] = I2C_ReceiveData(I2Cx);
    message[size-1] = I2C_ReceiveData(I2Cx);
  }

  return true;
}  

bool i2cWriteGeneric(I2CDevice device, uint8_t target, uint8_t addrSize, const uint8_t *addr, uint8_t dataSize, const uint8_t *data)
{
  I2C_TypeDef *I2Cx = i2cHardwareMap[device].dev;
  
  if(!i2cSync(device))
    return i2cFailureReset(device, 0x30);
    
  if(!i2cTransmitStart(device, target))
    return false;

  if(I2C_GetFlagStatus(I2Cx, I2C_FLAG_AF) == SET) {
    I2C_GenerateSTOP(I2Cx, ENABLE);
    return i2cFailure(device, 0x31);
  }
    
  if(addrSize > 0 && !i2cTransmitLoop(device, addrSize, addr))
    return i2cFailureReset(device, 0x32);

  if(dataSize > 0 && !i2cTransmitLoop(device, dataSize, data))
    return i2cFailureReset(device, 0x33);
  
  if(!i2cWaitForFlag(I2Cx, I2C_FLAG_TXE, SET))
    return i2cFailureReset(device, 0x34);
      
  I2C_GenerateSTOP(I2Cx, ENABLE);

  return true;
}

bool i2cReadGeneric(I2CDevice device, uint8_t target, uint8_t addrSize, const uint8_t *addr, uint8_t dataSize, uint8_t *data)
{
  I2C_TypeDef *I2Cx = i2cHardwareMap[device].dev;

  if(!i2cSync(device))
    return i2cFailureReset(device, 0x40);
  
  if(addrSize > 0) {
    if(!i2cTransmitStart(device, target))
      return false;
    
    if(I2C_GetFlagStatus(I2Cx, I2C_FLAG_AF) == SET) {
      I2C_GenerateSTOP(I2Cx, ENABLE);
      return i2cFailure(device, 0x41);
    }
    
    if(!i2cTransmitLoop(device, addrSize, addr))
      return i2cFailureReset(device, 0x42);

    if(!i2cWaitForFlag(I2Cx, I2C_FLAG_BTF, SET))
      return i2cFailureReset(device, 0x43);
  }

  if(!i2cReceiveStart(device, target, dataSize))
    return false;

  if(I2C_GetFlagStatus(I2Cx, I2C_FLAG_AF) == SET) {
    I2C_GenerateSTOP(I2Cx, ENABLE);
    return i2cFailure(device, 0x44);
  }
    
  if(!i2cReceiveLoop(device, dataSize, data))
    return false;

  return true;
}

uint16_t i2cGetErrorCounter(void)
{
    return i2cErrorCount;
}

uint16_t i2cGetErrorCode(void)
{
  if(i2cErrorCount > 0)
    return i2cErrorCode;
  else
    return 0;
}

uint16_t i2cGetErrorCounterReset(void)
{
  uint16_t value = i2cErrorCount;
  i2cErrorCount = 0;
  return value;
}

uint16_t i2cGetErrorCodeReset(void)
{
  uint16_t value = i2cErrorCode;
  i2cErrorCode = 0;
  return value;
}

bool i2cWriteBuffer(I2CDevice device, uint8_t addr, uint8_t reg, uint8_t len, const uint8_t * data, bool allowRawAccess)
{
#if 0
  if(reg == 0xFF && allowRawAccess)
    return i2cWriteGeneric(device, addr, 0, NULL, len, data);
  else
    return i2cWriteGeneric(device, addr, sizeof(reg), &reg, len, data);
#endif
}

bool i2cWrite(I2CDevice device, uint8_t addr_, uint8_t reg, uint8_t data, bool allowRawAccess)
{
#if 0
  if(reg == 0xFF && allowRawAccess)
    return i2cWriteGeneric(device, addr_, 0, NULL, sizeof(data), &data);
  else
    return i2cWriteGeneric(device, addr_, sizeof(reg), &reg, sizeof(data), &data);
#endif
}
  
bool i2cRead(I2CDevice device, uint8_t addr_, uint8_t reg, uint8_t len, uint8_t* buf, bool allowRawAccess)
{
#if 0
  if(reg == 0xFF && allowRawAccess)
    return i2cReadGeneric(device, addr_, 0, NULL, len, buf);
  else
    return i2cReadGeneric(device, addr_, sizeof(reg), &reg, len, buf);
#endif
}
#endif
