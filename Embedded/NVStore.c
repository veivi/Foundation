#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "StaP.h"
#include "NVStore.h"
#include "Console.h"
#include "CRC16.h"

#define STARTUP_DELAY  50

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

#define NVSTORE_BLOCK_PAYLOAD(p) ((p)->pageSize - sizeof(NVBlockHeader_t))
#define NVSTORE_BLOB_PAYLOAD(p) (NVSTORE_BLOCK_PAYLOAD(p) - sizeof(NVBlobHeader_t))
#define NVSTORE_ADDR(p, i) ((p->start + (i)) * (p)->pageSize)

static bool readBlock(NVStorePartition_t *p, uint32_t index, uint8_t *buffer)
{
  bool status = false;
  
  if(index < p->size) {
    if((*p->device->deviceRead)(NVSTORE_ADDR(p, index), buffer, p->pageSize))
      status = true;
    else
      consoleNotefLn("NVStore %s readBlock(%#x) read fail", p->name, index);
  } else
    consoleNotefLn("NVStore %s readBlock(%#x) illegal index", p->name, index);

  return status;
}

static uint16_t crc16OfRecord(uint16_t initial, const uint8_t *record, int size)
{
  return crc16(initial, &record[sizeof(uint16_t)], size - sizeof(uint16_t));
}

static bool validateBlock(const uint8_t *buffer, size_t pageSize, NVBlockHeader_t *header)
{
  memcpy(header, buffer, sizeof(*header));
  return (header->type == nvb_blob_c || header->type == nvb_data_c)
    && header->crc == crc16OfRecord(0xFFFF, buffer, pageSize);
}
  
static bool startup(NVStorePartition_t *p)
{
  if(!p->buffer || !p->pageSize)
    STAP_Panicf(0xFF, "NVStore(%s) buffer invalid", p->name);
  
  if(p->running)
    return true;

  if(vpTimeMillis() < STARTUP_DELAY)
    STAP_DelayMillis (STARTUP_DELAY);

  uint32_t ptr = 0, index = 0xFFFFFFFFUL;
  uint32_t count = 0;
  bool valid = false;

  consoleNotefLn("NVStore %s being initialized", p->name);
	
  while(ptr < p->size) {
    NVBlockHeader_t header;
    
    if(!readBlock(p, ptr, p->buffer)) {
      consoleNotefLn("NVStore %s startup readBlock() fail", p->name);
      return false;
    }

    if(validateBlock(p->buffer, p->pageSize, &header)) {      
      if(!valid) {
	count = header.count;
	index = ptr;
	consoleNotefLn("  NVStore %s first valid block at %#x, count = %#x",
		       p->name, ptr, count);
	valid = true;
	
      } else if(header.count > count || (header.count < 0x10 && count > 0xFFFFFFF0UL)) {
	count = header.count;
	index = ptr;
      }
    }
    
    ptr++;
  }

  p->index = (index + 1) % p->size;
  p->count = count;
  
  consoleNotefLn("  NVStore %s head at %#x, count = %#x", p->name, p->index, p->count);
  
  p->running = true;

  return true;
}

static bool storeBlock(NVStorePartition_t *p, uint16_t type, const uint8_t *data)
{
  bool status = false;
  
  if(p->index < p->size) {
    NVBlockHeader_t header = { .crc = 0, .count = p->count + 1, .type = type };

    header.crc = crc16OfRecord(0xFFFF, (const uint8_t*) &header, sizeof(header));
    header.crc = crc16(header.crc, (const uint8_t*) data, NVSTORE_BLOCK_PAYLOAD(p));

    if(p->device->deviceWrite(NVSTORE_ADDR(p, p->index),
    		(const uint8_t*) &header, sizeof(header))
       && p->device->deviceWrite(NVSTORE_ADDR(p, p->index) + sizeof(header),
				 (const uint8_t*) data, NVSTORE_BLOCK_PAYLOAD(p))) {
      // Success
      
      p->index = (p->index + 1) % p->size;
      p->count++;
      
      status = true;
    } else
      consoleNotefLn("NVStore %s writeBlock() write fail", p->name);
  } else
    consoleNotefLn("NVStore %s writeBlock(%#x) illegal index", p->name, p->index);

  return status;
}

static bool recallBlock(NVStorePartition_t *p, uint32_t delta, NVBlockHeader_t *header, uint8_t *buffer)
{
  return readBlock(p, (p->index + p->size - 1 - delta) % p->size, buffer)
    && validateBlock(buffer, p->pageSize, header);
}

NVStore_Status_t NVStoreReadBlob(NVStorePartition_t *p, const char *name, uint8_t *data, size_t size)
{
  NVStore_Status_t status = NVStore_Status_NotConnected;

  STAP_MutexInit(&p->mutex);
  
  if(startup(p)) {
    uint32_t delta = 0;

    while(delta < p->size) {
      NVBlockHeader_t header;
      
      if(recallBlock(p, delta, &header, p->buffer) && header.type == nvb_blob_c) {
    	  // Found a blob header

    	  NVBlobHeader_t blob;
    	  memcpy(&blob, &p->buffer[sizeof(header)], sizeof(blob));

          if(strncmp(blob.name, name, NVSTORE_NAME_MAX)) {
        	 // The name doesn't match
        	  delta++;
        	  continue;
          }

          consoleNotefLn("NVStore %s ReadBlob(%s) delta = %#x, size = %d, crc = %#X",
			 p->name, blob.name, delta, blob.size, (uint32_t) blob.crc);
			 
          if(blob.size != size) {
        	  consoleNotefLn("NVStore %s ReadBlob(%s) size mismatch (%d vs %d)",
				 p->name, blob.name, blob.size, (uint16_t) size);
        	  status = NVStore_Status_SizeMismatch;
          } else {
	    if(size > NVSTORE_BLOB_PAYLOAD(p)) {
	    uint8_t *ptr = data;
	    size_t remaining = size;

	    // Extract the part in the blob block
	    memcpy(ptr, &p->buffer[sizeof(header) + sizeof(blob)], NVSTORE_BLOB_PAYLOAD(p));

	    ptr += NVSTORE_BLOB_PAYLOAD(p);
	    remaining -= NVSTORE_BLOB_PAYLOAD(p);

	    // Read the needed data blocks
	    
	    while(remaining > 0 && delta > 0) {
	      delta--;
	      
	      if(recallBlock(p, delta, &header, p->buffer)
		 && header.type == nvb_data_c) {
		size_t segment = remaining;
		
		if(segment > NVSTORE_BLOCK_PAYLOAD(p))
		  segment = NVSTORE_BLOCK_PAYLOAD(p);

		memcpy(ptr, &p->buffer[sizeof(header)], segment);
		
		ptr += segment;
		remaining -= segment;
	      }
	    }
	    
	    if(remaining > 0) {
	      // Ran out of data blocks
	      consoleNotefLn("NVStore %s ReadBlob(%s) data block(s) missing",
			     p->name, blob.name);
	      status = NVStore_Status_ReadFailed;
	    } else {
	      uint16_t crc =
		crc16(crc16OfRecord(0xFFFF, (const uint8_t*) &blob, sizeof(blob)),
		      (const uint8_t*) data, blob.size);
	      
	      if(crc == blob.crc) {
		status = NVStore_Status_OK;
	      }  else {
		consoleNotefLn("NVStore %s ReadBlob(%s) CRC fail (%#X vs %#X)",
			       p->name, blob.name, (uint32_t) crc, (uint32_t) blob.crc);
		status = NVStore_Status_CRCFail;
	      }
	    }
	  } else {
	    if(blob.crc == crc16OfRecord(0xFFFF, (const uint8_t*) &p->buffer[sizeof(header)],
					   sizeof(blob) + blob.size)) {
	      memcpy(data, &p->buffer[sizeof(header) + sizeof(blob)], size);
	      status = NVStore_Status_OK;
	    } else {
	      consoleNotefLn("NVStore %s ReadBlob(%s) CRC fail", p->name, blob.name);
	      status = NVStore_Status_CRCFail;
	    }
	  }
	}
	
	break;
      }

      delta++;
    }

    if(delta == p->size) {
      consoleNotefLn("NVStore %s ReadBlob(%s) blob not found", p->name, name);
      status = NVStore_Status_NotFound;
    }
  }

  STAP_MutexRelease(p->mutex);

  if(status != NVStore_Status_OK)
    // Error returns all zeros
    memset(data, 0, size);
  
  return status;
}

NVStore_Status_t NVStoreWriteBlob(NVStorePartition_t *p, const char *name, const uint8_t *data, size_t size)
{
  NVStore_Status_t status = NVStore_Status_NotConnected;

  STAP_MutexInit(&p->mutex);
  
  if(startup(p)) {
    NVBlobHeader_t header = { .crc = 0, .size = size };

    memset(header.name, 0, sizeof(header.name));
    strncpy(header.name, name, NVSTORE_NAME_MAX);
    
    header.crc =
      crc16(crc16OfRecord(0xFFFF, (const uint8_t*) &header, sizeof(header)), data, size);

    if(size > NVSTORE_BLOB_PAYLOAD(p)) {
      // Write the blob block with the start of the data

      memcpy(p->buffer, &header, sizeof(header));
      memcpy(&p->buffer[sizeof(header)], data, NVSTORE_BLOB_PAYLOAD(p));
      
      data += NVSTORE_BLOB_PAYLOAD(p);
      size -= NVSTORE_BLOB_PAYLOAD(p);

      if(!storeBlock(p, nvb_blob_c, p->buffer)) {
	consoleNotefLn("NVStore WriteBlob blob write (2) fail", p->name);
	status = NVStore_Status_WriteFailed;
      } else {
	// Write the remaining data as data blocks

	while(size > 0) {
	  size_t segment = size;
	  
	  if(segment > NVSTORE_BLOCK_PAYLOAD(p))
	    segment = NVSTORE_BLOCK_PAYLOAD(p);
	  else
	    memset(p->buffer, 0, p->pageSize);
	  
	  memcpy(p->buffer, data, segment);
	    
	  if(!storeBlock(p, nvb_data_c, p->buffer)) {
	    consoleNotefLn("NVStore WriteBlob data write fail", p->name);
	    status = NVStore_Status_WriteFailed;
	    break;
	  }

	  data += segment;
	  size -= segment;
	}

	if(size == 0)
	  // We're good
	  status = NVStore_Status_OK;
      }       
    } else {
      // The contents fit in the blob block
      
      memset(p->buffer, 0, p->pageSize);
      memcpy(p->buffer, &header, sizeof(header));
      memcpy(&p->buffer[sizeof(header)], data, size);
      
      if(!storeBlock(p, nvb_blob_c, p->buffer)) {
	consoleNotefLn("NVStore WriteBlob blob write (1) fail", p->name);
	status = NVStore_Status_WriteFailed;
      } else
	status = NVStore_Status_OK;
    }
  }

  if(status == NVStore_Status_OK && !p->device->deviceDrain()) {
    consoleNotefLn("NVStore WriteBlob device drain fail", p->name);    
    status = NVStore_Status_WriteFailed;
  }

  STAP_MutexRelease(p->mutex);

  STAP_DEBUG(0, "BLOB");
  
  return status;
}

  
