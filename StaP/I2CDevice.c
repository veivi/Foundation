#include <stdlib.h>
#include <stdbool.h>
#include "I2CDevice.h"
#include "Console.h"
#include "PRNG.h"

#define BACKOFF_INITIAL    (0.1e3)
#define BACKOFF_MAX        (20e3)
#define BACKOFF_FRACTION   3
#define MAX_FAILS          3

static STAP_MutexRef_T i2cMutex = NULL;

void I2CDeviceReset(I2CDevice_t *device)
{
  device->offLine = false;
  device->failCount = 0;
}

void I2CDeviceSetId(I2CDevice_t *device, uint8_t id)
{
  device->id = id;
}

bool I2CDeviceIsOnline(I2CDevice_t *device)
{
  return !device->offLine;
}

bool I2CDeviceMaybeOnline(I2CDevice_t *device)
{
  return I2CDeviceIsOnline(device)
    || VP_ELAPSED_MILLIS(device->offLineAt) > device->backoff;
}

bool I2CDeviceIsStable(I2CDevice_t *device)
{
  return !device->offLine && device->failCount == 0;
}

static uint8_t I2CDeviceInvoke(I2CDevice_t *device, uint8_t subId, uint8_t status)
{
  if(status) {
    consoleNotefLn("I2C(%s:%d) ERROR %#X", device->name, subId, status);

    if(device->offLine) {
      device->backoff += device->backoff/BACKOFF_FRACTION;
      
      if(device->backoff > BACKOFF_MAX)
	device->backoff = BACKOFF_MAX;
      
      device->backoff += (randomUI16() & 0x1FF);
      
    } else if(device->failCount++ > MAX_FAILS) {
      consoleNotefLn("I2C(%s) is OFFLINE", device->name);
      device->offLine = true;
      device->backoff = BACKOFF_INITIAL;
    }
    
    device->offLineAt = vpTimeMillis();
  } else {    
    if(device->failCount > 0) {
      consoleNotefLn("I2C(%s:%d) RECOVERED", device->name, subId);
      device->failCount = 0;
      device->offLine;
    }
  }
  
  return status;
}

static void criticalBegin(void)
{
  STAP_FORBID;
    
  if(!i2cMutex && !(i2cMutex = STAP_MutexCreate))
    STAP_Panic(STAP_ERR_MUTEX_CREATE);

  STAP_PERMIT;
    
  STAP_MutexObtain(i2cMutex);
}

static void criticalEnd(void)
{
  STAP_MutexRelease(i2cMutex);
}

uint8_t I2CDeviceTransmit(I2CDevice_t *device, uint8_t subId, const StaP_TransferUnit_t *buffers, size_t numBuffers)
{
  uint8_t status = 0xFF;

  criticalBegin();
	
  if(I2CDeviceMaybeOnline(device))
    status = I2CDeviceInvoke(device, subId, STAP_I2CTransfer(device->id + subId, buffers, numBuffers, NULL, 0));

  criticalEnd();
	
  return status;
}

uint8_t I2CDeviceWrite(I2CDevice_t *device, uint8_t subId, const uint8_t *addr, size_t addrSize, const uint8_t *value, size_t valueSize)
{
  StaP_TransferUnit_t buffer[] = { 
    { .data.tx = addr, .size = addrSize, .dir = transfer_dir_transmit },
    { .data.tx = value, .size = valueSize, .dir = transfer_dir_transmit } };
  uint8_t status = 0xFF;

  criticalBegin();
	
  if(I2CDeviceMaybeOnline(device))
    // status = I2CDeviceInvoke(device, subId, STAP_I2CTransfer(device->id + subId, buffer, sizeof(buffer)/sizeof(StaP_TransferUnit_t), NULL, 0));
    status = I2CDeviceInvoke(device, subId, STAP_I2CTransferGeneric(device->id + subId, buffer, sizeof(buffer)/sizeof(StaP_TransferUnit_t));

  criticalEnd();
	
  return status;
}

uint8_t I2CDeviceRead(I2CDevice_t *device, uint8_t subId, const uint8_t *addr, size_t addrSize, uint8_t *value, size_t valueSize)
{
  // StaP_TransferUnit_t buffer = { addr, addrSize };
    
    StaP_TransferUnit_t buffer[] = { 
	    { .data.tx = addr, .size = addrSize, .dir = transfer_dir_transmit },    // addrSize can be zero, will just be ignored then
        { .data.rx = value, .size = valueSize, .dir = transfer_dir_receive } };

    uint8_t status = 0xFF;

    criticalBegin();
	
    if(I2CDeviceMaybeOnline(device)) {
	// if(addrSize > 0) {
	//   status = I2CDeviceInvoke(device, subId, STAP_I2CTransfer(device->id + subId, &buffer, 1, value, valueSize));
	// else
	//   status = I2CDeviceInvoke(device, subId, STAP_I2CTransfer(device->id + subId, NULL, 0, value, valueSize));
	  
        status = I2CDeviceInvoke(device, subId, STAP_I2CTransferGeneric(device->id + subId, &buffer, sizeof(buffer)/sizeof(StaP_TransferUnit_t)));
    }
	
    criticalEnd();
	
    return status;
}

uint8_t I2CDeviceReadByUInt8(I2CDevice_t *device, uint8_t subId, uint8_t addr, uint8_t *data, size_t size)
{
  return I2CDeviceRead(device, subId, &addr, sizeof(addr), data, size);
}

uint8_t I2CDeviceWriteByUInt8(I2CDevice_t *device, uint8_t subId, uint8_t addr, const uint8_t *data, size_t size)
{
  return I2CDeviceWrite(device, subId, &addr, sizeof(addr), data, size);
}

uint8_t I2CDeviceReadByUInt16(I2CDevice_t *device, uint8_t subId, uint16_t addr, uint8_t *data, size_t size)
{
  return I2CDeviceRead(device, subId, (const uint8_t*) &addr, sizeof(addr), data, size);
}

uint8_t I2CDeviceWriteByUInt16(I2CDevice_t *device, uint8_t subId, uint16_t addr, const uint8_t *data, size_t size)
{
  return I2CDeviceWrite(device, subId, (const uint8_t*) &addr, sizeof(addr), data, size);
}

// Convenience functions for 8 and 16 bit addresses and immediate 8 or 16 bit data

uint8_t I2CDeviceWriteUInt8ByUInt8(I2CDevice_t *device, uint8_t subId, uint8_t addr, uint8_t value)
{
   return I2CDeviceWrite(device, subId, &addr, sizeof(addr), &value, sizeof(value));
}

uint8_t I2CDeviceWriteUInt16ByUInt8(I2CDevice_t *device, uint8_t subId, uint8_t addr, uint16_t value)
{
   return I2CDeviceWrite(device, subId, &addr, sizeof(addr), &value, sizeof(value));
}

uint8_t I2CDeviceWriteUInt8ByUInt16(I2CDevice_t *device, uint8_t subId, uint16_t addr, uint8_t value)
{
  return I2CDeviceWrite(device, subId, (const uint8_t*) &addr, sizeof(addr), &value, sizeof(value));
}

uint8_t I2CDeviceWriteUInt16ByUInt16(I2CDevice_t *device, uint8_t subId, uint16_t addr, uint16_t value)
{
  return I2CDeviceWrite(device, subId, (const uint8_t*) &addr, sizeof(addr), &value, sizeof(value));
}

uint8_t I2CDeviceReadUInt8ByUInt8(I2CDevice_t *device, uint8_t subId, uint8_t addr, uint8_t *retCode)
{
  uint8_t buffer = 0;
  uint8_t status = I2CDeviceRead(device, subId, &addr, sizeof(addr), (uint8_t*) &buffer, sizeof(buffer));
  if(retCode)
    *retCode = status;
  return buffer;
}

uint16_t I2CDeviceReadUInt16ByUInt8(I2CDevice_t *device, uint8_t subId, uint8_t addr, uint8_t *retCode)
{
  uint16_t buffer = 0;
  uint8_t status = I2CDeviceRead(device, subId, &addr, sizeof(addr), (uint8_t*) &buffer, sizeof(buffer));
  if(retCode)
    *retCode = status;
  return buffer;
}

uint8_t I2CDeviceReadUInt8ByUInt16(I2CDevice_t *device, uint8_t subId, uint16_t addr, uint8_t *retCode)
{
  uint8_t buffer = 0;
  uint8_t status = I2CDeviceRead(device, subId, (const uint8_t*) &addr, sizeof(addr), (uint8_t*) &buffer, sizeof(buffer));
  if(retCode)
    *retCode = status;
  return buffer;
}

uint16_t I2CDeviceReadUInt16ByUInt16(I2CDevice_t *device, uint8_t subId, uint16_t addr, uint8_t *retCode)
{
  uint16_t buffer = 0;
  uint8_t status = I2CDeviceRead(device, subId, (const uint8_t*) &addr, sizeof(addr), (uint8_t*) &buffer, sizeof(buffer));
  if(retCode)
    *retCode = status;
  return buffer;
}
