#ifndef BASEI2CDEVICE_H
#define BASEI2CDEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "VPTime.h"
#include "StaP.h"

typedef struct I2CDevice {
  const char *name;
  int failCount;
  VP_TIME_MILLIS_T offLineAt, backoff;
  uint8_t id;
  bool offLine;
} I2CDevice_t;

#define I2CDEVICE_CONS(d_name, d_id) { .name = d_name, .id = d_id, .failCount = 0, .offLineAt = 0, .backoff = 0, .offLine = false }

void I2CDeviceReset(I2CDevice_t*);
void I2CDeviceSetId(I2CDevice_t*, uint8_t);
bool I2CDeviceIsOnline(I2CDevice_t*);
bool I2CDeviceIsStable(I2CDevice_t*);
bool I2CDeviceMaybeOnline(I2CDevice_t *);

// Generic unidirectional transmit from multiple buffers

uint8_t I2CDeviceTransmit(I2CDevice_t *device, uint8_t subId, const StaP_TransferUnit_t *buffers, size_t numBuffers);

// Generic read and write with arbitrary address and data sizes

uint8_t I2CDeviceRead(I2CDevice_t *device, uint8_t subId, const uint8_t *addr, size_t addrSize, uint8_t *value, size_t valueSize);
uint8_t I2CDeviceWrite(I2CDevice_t *device, uint8_t subId, const uint8_t *addr, size_t addrSize, const uint8_t *value, size_t valueSize);

// Convenience functions for 8 and 16 bit addresses and generic data

uint8_t I2CDeviceReadByUInt8(I2CDevice_t *device, uint8_t subId, uint8_t addr, uint8_t *value, size_t valueSize);
uint8_t I2CDeviceWriteByUInt8(I2CDevice_t *device, uint8_t subId, uint8_t addr, const uint8_t *value, size_t valueSize);
uint8_t I2CDeviceReadByUInt16(I2CDevice_t *device, uint8_t subId, uint16_t addr, uint8_t *value, size_t valueSize);
uint8_t I2CDeviceWriteByUInt16(I2CDevice_t *device, uint8_t subId, uint16_t addr, const uint8_t *value, size_t valueSize);

// Convenience functions for 8 and 16 bit addresses and immediate 8 or 16 bit data

uint8_t I2CDeviceWriteUInt8ByUInt8(I2CDevice_t *device, uint8_t subId, uint8_t addr, uint8_t value);
uint8_t I2CDeviceWriteUInt16ByUInt8(I2CDevice_t *device, uint8_t subId, uint8_t addr, uint16_t value);
uint8_t I2CDeviceWriteUInt8ByUInt16(I2CDevice_t *device, uint8_t subId, uint16_t addr, uint8_t value);
uint8_t I2CDeviceWriteUInt16ByUInt16(I2CDevice_t *device, uint8_t subId, uint16_t addr, uint16_t value);

uint8_t I2CDeviceReadUInt8ByUInt8(I2CDevice_t *device, uint8_t subId, uint8_t addr, uint8_t *status);
uint16_t I2CDeviceReadUInt16ByUInt8(I2CDevice_t *device, uint8_t subId, uint8_t addr, uint8_t *status);
uint8_t I2CDeviceReadUInt8ByUInt16(I2CDevice_t *device, uint8_t subId, uint16_t addr, uint8_t *status);
uint16_t I2CDeviceReadUInt16ByUInt16(I2CDevice_t *device, uint8_t subId, uint16_t addr, uint8_t *status);

#endif
