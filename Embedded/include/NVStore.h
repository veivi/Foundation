#ifndef AP_NVSTORE_H
#define AP_NVSTORE_H

#include <stdint.h>
#include <stddef.h>
#include "StaP.h"
#include "Scheduler.h"

#define NVSTORE_BLOCKSIZE     (1<<6)
#define NVSTORE_NAME_MAX      ((1<<4) - 1)

typedef enum { NVStore_Status_OK = 0,
	       NVStore_Status_NotConnected,
	       NVStore_Status_NotFound,
	       NVStore_Status_SizeMismatch,
	       NVStore_Status_ReadFailed ,
	       NVStore_Status_CRCFail,
	       NVStore_Status_WriteFailed } NVStore_Status_t;

typedef struct {
  bool (*deviceRead)(uint32_t addr, uint8_t *data, size_t size);
  bool (*deviceWrite)(uint32_t addr, const uint8_t *data, size_t size);
  bool (*deviceDrain)(void);
} NVStoreDevice_t;

typedef struct {
  const char *name;
  const NVStoreDevice_t *device;
  uint32_t start, size;
  uint32_t index, count;
  bool running;
  STAP_MutexRef_T mutex;
} NVStorePartition_t;

NVStore_Status_t NVStoreReadBlob(NVStorePartition_t *p, const char *name, uint8_t *data, size_t size);
NVStore_Status_t NVStoreWriteBlob(NVStorePartition_t *p, const char *name, const uint8_t *data, size_t size);

#endif
