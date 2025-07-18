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

typedef struct {
  NVStorePartition_t *partition;
  char name[NVSTORE_NAME_MAX+1];
  uint32_t index, count;
} NVStoreScanState_t;

NVStore_Status_t NVStoreInit(NVStorePartition_t *p);
NVStore_Status_t NVStoreReadBlob(NVStorePartition_t *p, const char *name, uint8_t *data, size_t size);
NVStore_Status_t NVStoreWriteBlob(NVStorePartition_t *p, const char *name, const uint8_t *data, size_t size);

NVStore_Status_t NVStoreScanStart(NVStoreScanState_t *s, NVStorePartition_t *p, const char *name);
NVStore_Status_t NVStoreScan(NVStoreScanState_t *s, uint8_t *data, size_t size);

typedef enum {
  nvb_invalid_c = 0,
  nvb_blob_c,
  nvb_data_c
} NVStoreBlockType_t;

typedef struct {
  uint16_t crc;
  uint16_t type;
  uint32_t count;
} NVBlockHeader_t;

typedef struct {
  uint16_t crc;
  uint16_t size;
  char name[NVSTORE_NAME_MAX+1];
} NVBlobHeader_t;

#define NVSTORE_BLOB_OVERHEAD   (sizeof(NVBlockHeader_t) + sizeof(NVBlobHeader_t))

#endif
