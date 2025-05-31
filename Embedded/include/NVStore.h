#ifndef AP_NVSTORE_H
#define AP_NVSTORE_H

#include <stdint.h>
#include <stddef.h>
#include "StaP.h"
#include "SharedObject.h"

#define NVSTORE_NAME_MAX      ((1<<4) - 1)

typedef enum { NVStore_Status_OK = 0,
	       NVStore_Status_NotConnected,
	       NVStore_Status_NotFound,
	       NVStore_Status_SizeMismatch,
	       NVStore_Status_ReadFailed ,
	       NVStore_Status_CRCFail,
	       NVStore_Status_WriteFailed } NVStore_Status_t;

typedef struct {
  struct SharedObject header;
  bool (*deviceRead)(uint32_t addr, uint8_t *data, size_t size);
  bool (*deviceWrite)(uint32_t addr, const uint8_t *data, size_t size);
  bool (*deviceDrain)(void);
  size_t pageSize;
  uint8_t *buffer;
} NVStoreDevice_t;

#define NVSTORE_DEVICE(name, store) { .deviceRead = name ## Read, .deviceWrite = name ## Write, .deviceDrain = name ## Drain, .pageSize = sizeof(store), .buffer = store }

#define NVSTORE_PAGEDEVICE(name, store) { .deviceRead = name ## ReadPage, .deviceWrite = name ## WritePage, .deviceDrain = NULL, .pageSize = sizeof(store), .buffer = store }

typedef struct {
  const char *name;
  NVStoreDevice_t *device;
  uint32_t start, size;
  uint32_t index, count;
  bool running;
} NVStorePartition_t;

NVStore_Status_t NVStoreReadBlob(NVStorePartition_t *p, const char *name, uint8_t *data, size_t size);
NVStore_Status_t NVStoreWriteBlob(NVStorePartition_t *p, const char *name, const uint8_t *data, size_t size);

#endif
