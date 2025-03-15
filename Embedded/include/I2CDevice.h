#ifndef BASEI2CDEVICE_H
#define BASEI2CDEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "VPTime.h"
#include "StaP.h"

typedef struct I2CTarget {
  const char *name;
  int failCount;
  VP_TIME_MILLIS_T offLineAt, backoff;
  uint8_t id;
  bool offLine;
} I2CTarget_t;

#define I2CTARGET_CONS(d_name, d_id) { .name = d_name, .id = d_id, .failCount = 0, .offLineAt = 0, .backoff = 0, .offLine = false }

void I2CTargetReset(I2CTarget_t*);
void I2CTargetSetId(I2CTarget_t*, uint8_t);
bool I2CTargetIsOnline(I2CTarget_t*);
bool I2CTargetIsStable(I2CTarget_t*);
bool I2CTargetMaybeOnline(I2CTarget_t *);

// Generic bidirectional transmit from multiple buffers

uint8_t I2CTargetTransfer(I2CTarget_t *device, uint8_t subId, const StaP_TransferUnit_t *buffers, size_t numBuffers);

// Generic read and write with arbitrary address and data sizes

uint8_t I2CTargetRead(I2CTarget_t *device, uint8_t subId, const uint8_t *addr, size_t addrSize, uint8_t *value, size_t valueSize);
uint8_t I2CTargetWrite(I2CTarget_t *device, uint8_t subId, const uint8_t *addr, size_t addrSize, const uint8_t *value, size_t valueSize);

// Convenience functions for 8 and 16 bit addresses and generic data

uint8_t I2CTargetReadByUInt8(I2CTarget_t *device, uint8_t subId, uint8_t addr, uint8_t *value, size_t valueSize);
uint8_t I2CTargetWriteByUInt8(I2CTarget_t *device, uint8_t subId, uint8_t addr, const uint8_t *value, size_t valueSize);
uint8_t I2CTargetReadByUInt16(I2CTarget_t *device, uint8_t subId, uint16_t addr, uint8_t *value, size_t valueSize);
uint8_t I2CTargetWriteByUInt16(I2CTarget_t *device, uint8_t subId, uint16_t addr, const uint8_t *value, size_t valueSize);

// Convenience functions for 8 and 16 bit addresses and immediate 8 or 16 bit data

uint8_t I2CTargetWriteUInt8ByUInt8(I2CTarget_t *device, uint8_t subId, uint8_t addr, uint8_t value);
uint8_t I2CTargetWriteUInt16ByUInt8(I2CTarget_t *device, uint8_t subId, uint8_t addr, uint16_t value);
uint8_t I2CTargetWriteUInt8ByUInt16(I2CTarget_t *device, uint8_t subId, uint16_t addr, uint8_t value);
uint8_t I2CTargetWriteUInt16ByUInt16(I2CTarget_t *device, uint8_t subId, uint16_t addr, uint16_t value);

uint8_t I2CTargetReadUInt8ByUInt8(I2CTarget_t *device, uint8_t subId, uint8_t addr, uint8_t *status);
uint16_t I2CTargetReadUInt16ByUInt8(I2CTarget_t *device, uint8_t subId, uint8_t addr, uint8_t *status);
uint8_t I2CTargetReadUInt8ByUInt16(I2CTarget_t *device, uint8_t subId, uint16_t addr, uint8_t *status);
uint16_t I2CTargetReadUInt16ByUInt16(I2CTarget_t *device, uint8_t subId, uint16_t addr, uint8_t *status);

#endif
