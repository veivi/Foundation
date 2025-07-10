#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "StaP.h"
#include "NVStore.h"
#include "Console.h"
#include "CRC16.h"

#define STARTUP_DELAY  10

#define NVSTORE_BLOCK_PAYLOAD(p) ((p)->device->pageSize - sizeof(NVBlockHeader_t))
#define NVSTORE_BLOB_PAYLOAD(p) (NVSTORE_BLOCK_PAYLOAD(p) - sizeof(NVBlobHeader_t))
#define NVSTORE_ADDR(p, i) ((p->start + (i)) * (p)->device->pageSize)
#define NVSTORE_DELTA(p, delta) ((p->index + p->size - 1 - (delta)) % p->size)

static bool readBlock(NVStorePartition_t *p, uint32_t index, uint8_t *buffer)
{
  bool status = false;
  
  if(index < p->size) {
    if((*p->device->deviceRead)(NVSTORE_ADDR(p, index), buffer, p->device->pageSize))
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

  if(header->type != nvb_blob_c && header->type != nvb_data_c) {
    //    consoleNotefLn("block header weird type %d", header->type);
    return false;
  }
  
  if(header->crc != crc16OfRecord(0xFFFF, buffer, pageSize)) {
    //    consoleNotefLn("block header crc fail %#x", header->crc);
    return false;
  }

  return true;
}
  
static bool startup(NVStorePartition_t *p)
{
  if(!p->device->buffer || !p->device->pageSize)
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
    
    if(!readBlock(p, ptr, p->device->buffer)) {
      consoleNotefLn("NVStore %s startup readBlock() fail", p->name);
      return false;
    }

    if(validateBlock(p->device->buffer, p->device->pageSize, &header)) {      
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

    consoleNotefLn("NVStoreBlock %s count %U index %U", p->name, header.count, p->index);

    memcpy(p->device->buffer, (const uint8_t*) &header, sizeof(header));
    memcpy(&p->device->buffer[sizeof(header)], data, NVSTORE_BLOCK_PAYLOAD(p));
	   
    if(p->device->deviceWrite(NVSTORE_ADDR(p, p->index), p->device->buffer,
			      p->device->pageSize)) {
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

static bool recallBlock(NVStorePartition_t *p, uint32_t index, NVBlockHeader_t *header, uint8_t *buffer)
{
  return readBlock(p, index, buffer)
    && validateBlock(buffer, p->device->pageSize, header);
}

static NVStore_Status_t recallBlob(NVStorePartition_t *p, uint32_t index, NVBlobHeader_t *blob, uint8_t *data)
{
  NVStore_Status_t status = NVStore_Status_NotConnected;
  NVBlockHeader_t header;

  if(blob->size > NVSTORE_BLOB_PAYLOAD(p)) {
    uint8_t *ptr = data;
    size_t remaining = blob->size;

    // Extract the part in the blob block
    memcpy(ptr, &p->device->buffer[NVSTORE_BLOB_OVERHEAD], NVSTORE_BLOB_PAYLOAD(p));

    ptr += NVSTORE_BLOB_PAYLOAD(p);
    remaining -= NVSTORE_BLOB_PAYLOAD(p);

    // Read the needed data blocks
	    
    while(remaining > 0 && index != p->index) {
      index = (index + 1) % p->size;
	      
      if(recallBlock(p, index, &header, p->device->buffer)
	 && header.type == nvb_data_c) {
	size_t segment = remaining;
		
	if(segment > NVSTORE_BLOCK_PAYLOAD(p))
	  segment = NVSTORE_BLOCK_PAYLOAD(p);

	memcpy(ptr, &p->device->buffer[sizeof(header)], segment);
		
	ptr += segment;
	remaining -= segment;
      }
    }
	    
    if(remaining > 0) {
      // Ran out of data blocks
      consoleNotefLn("NVStore %s ReadBlob(%s) data block(s) missing",
		     p->name, blob->name);
      status = NVStore_Status_ReadFailed;
    } else {
      uint16_t crc =
	crc16(crc16OfRecord(0xFFFF, (const uint8_t*) blob, sizeof(*blob)),
	      (const uint8_t*) data, blob->size);
	      
      if(crc == blob->crc) {
	status = NVStore_Status_OK;
      }  else {
	consoleNotefLn("NVStore %s ReadBlob(%s) CRC fail (%#X vs %#X)",
		       p->name, blob->name, (uint32_t) crc, (uint32_t) blob->crc);
	status = NVStore_Status_CRCFail;
      }
    }
  } else {
    if(blob->crc == crc16OfRecord(0xFFFF,
				 (const uint8_t*) &p->device->buffer[sizeof(header)],
				 sizeof(*blob) + blob->size)) {
      memcpy(data, &p->device->buffer[NVSTORE_BLOB_OVERHEAD], blob->size);
      status = NVStore_Status_OK;
    } else {
      consoleNotefLn("NVStore %s ReadBlob(%s) CRC fail", p->name, blob->name);
      status = NVStore_Status_CRCFail;
    }
  }
  
  return status;
}

NVStore_Status_t NVStoreWriteBlob(NVStorePartition_t *p, const char *name, const uint8_t *data, size_t size)
{
  NVStore_Status_t status = NVStore_Status_NotConnected;

  SHARED_ACCESS_BEGIN(*(p->device));
  
  if(startup(p)) {
    NVBlobHeader_t header = { .crc = 0, .size = size };

    memset(header.name, 0, sizeof(header.name));
    strncpy(header.name, name, NVSTORE_NAME_MAX);
    
    header.crc =
      crc16(crc16OfRecord(0xFFFF, (const uint8_t*) &header, sizeof(header)), data, size);

    if(size > NVSTORE_BLOB_PAYLOAD(p)) {
      // Write the blob block with the start of the data

      memcpy(p->device->buffer, &header, sizeof(header));
      memcpy(&p->device->buffer[sizeof(header)], data, NVSTORE_BLOB_PAYLOAD(p));
      
      data += NVSTORE_BLOB_PAYLOAD(p);
      size -= NVSTORE_BLOB_PAYLOAD(p);

      if(!storeBlock(p, nvb_blob_c, p->device->buffer)) {
	consoleNotefLn("NVStore WriteBlob blob write (2) fail", p->name);
	status = NVStore_Status_WriteFailed;
      } else {
	// Write the remaining data as data blocks

	while(size > 0) {
	  size_t segment = size;
	  
	  if(segment > NVSTORE_BLOCK_PAYLOAD(p))
	    segment = NVSTORE_BLOCK_PAYLOAD(p);
	  else
	    memset(p->device->buffer, 0, p->device->pageSize);
	  
	  memcpy(p->device->buffer, data, segment);
	    
	  if(!storeBlock(p, nvb_data_c, p->device->buffer)) {
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
      
      memset(p->device->buffer, 0, p->device->pageSize);
      memcpy(p->device->buffer, &header, sizeof(header));
      memcpy(&p->device->buffer[sizeof(header)], data, size);
      
      if(!storeBlock(p, nvb_blob_c, p->device->buffer)) {
	consoleNotefLn("NVStore WriteBlob blob write (1) fail", p->name);
	status = NVStore_Status_WriteFailed;
      } else
	status = NVStore_Status_OK;
    }
  }

  if(status == NVStore_Status_OK && p->device->deviceDrain && !p->device->deviceDrain()) {
    consoleNotefLn("NVStore WriteBlob device drain fail", p->name);    
    status = NVStore_Status_WriteFailed;
  }

  SHARED_ACCESS_END(*(p->device));

  STAP_DEBUG(0, "BLOB");
  
  return status;
}

NVStore_Status_t NVStoreReadBlob(NVStorePartition_t *p, const char *name, uint8_t *data, size_t size)
{
  NVStore_Status_t status = NVStore_Status_NotConnected;

  SHARED_ACCESS_BEGIN(*(p->device));
  
  if(startup(p)) {
    uint32_t delta = 0;

    while(delta < p->size) {
      NVBlockHeader_t header;
      
      if(recallBlock(p, NVSTORE_DELTA(p, delta), &header, p->device->buffer)
	 && header.type == nvb_blob_c) {
    	  // Found a blob header

    	  NVBlobHeader_t blob;
    	  memcpy(&blob, &p->device->buffer[sizeof(header)], sizeof(blob));

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
	    status = recallBlob(p, NVSTORE_DELTA(p, delta), &blob, data);
	    
	    /*
	    if(size > NVSTORE_BLOB_PAYLOAD(p)) {
	    uint8_t *ptr = data;
	    size_t remaining = size;

	    // Extract the part in the blob block
	    memcpy(ptr, &p->device->buffer[sizeof(header) + sizeof(blob)], NVSTORE_BLOB_PAYLOAD(p));

	    ptr += NVSTORE_BLOB_PAYLOAD(p);
	    remaining -= NVSTORE_BLOB_PAYLOAD(p);

	    // Read the needed data blocks
	    
	    while(remaining > 0 && delta > 0) {
	      delta--;
	      
	      if(recallBlock(p, NVSTORE_DELTA(p, delta), &header, p->device->buffer)
		 && header.type == nvb_data_c) {
		size_t segment = remaining;
		
		if(segment > NVSTORE_BLOCK_PAYLOAD(p))
		  segment = NVSTORE_BLOCK_PAYLOAD(p);

		memcpy(ptr, &p->device->buffer[sizeof(header)], segment);
		
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
	    if(blob.crc == crc16OfRecord(0xFFFF, (const uint8_t*) &p->device->buffer[sizeof(header)],
					   sizeof(blob) + blob.size)) {
	      memcpy(data, &p->device->buffer[sizeof(header) + sizeof(blob)], size);
	      status = NVStore_Status_OK;
	    } else {
	      consoleNotefLn("NVStore %s ReadBlob(%s) CRC fail", p->name, blob.name);
	      status = NVStore_Status_CRCFail;
	    }
	  }
	    */
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

  SHARED_ACCESS_END(*(p->device));

  if(status != NVStore_Status_OK)
    // Error returns all zeros
    memset(data, 0, size);
  
  return status;
}

NVStore_Status_t NVStoreScanStart(NVStoreScanState_t *s, NVStorePartition_t *p, const char *name)
{
  NVStore_Status_t status = NVStore_Status_NotConnected;

  SHARED_ACCESS_BEGIN(*(p->device));

  s->partition = p;
  strncpy(s->name, name, NVSTORE_NAME_MAX);
  
  if(startup(p)) {
    uint32_t delta = p->size;

    do {
      NVBlockHeader_t header;
      
      delta--;
		  
      if(recallBlock(p, NVSTORE_DELTA(p, delta), &header, p->device->buffer) && header.type == nvb_blob_c) {
    	  // Found a blob header

    	  NVBlobHeader_t blob;
    	  memcpy(&blob, &p->device->buffer[sizeof(header)], sizeof(blob));

          if(!strncmp(blob.name, name, NVSTORE_NAME_MAX)) {
	    consoleNotefLn("NVStore %s ScanStart(%s) delta = %#x",
			   p->name, blob.name, delta);
	    s->count = delta + 1;
	    s->index = NVSTORE_DELTA(p, delta);
	    status = NVStore_Status_OK;
	    break;
	  }
      } 
    } while(delta > 0);

    if(status != NVStore_Status_OK) {
      consoleNotefLn("NVStore %s ScanStart(%s) blob not found", p->name, name);
      status = NVStore_Status_NotFound;
    }
  }

  SHARED_ACCESS_END(*(p->device));
  
  return status;
}

NVStore_Status_t NVStoreScan(NVStoreScanState_t *s, uint8_t *data, size_t size)
{
  NVStore_Status_t status = NVStore_Status_NotConnected;

  SHARED_ACCESS_BEGIN(*(s->partition->device));

  if(startup(s->partition)) {
    while(s->count > 0) {
      NVBlockHeader_t header;
      		  
      if(recallBlock(s->partition, s->index, &header, s->partition->device->buffer)
	 && header.type == nvb_blob_c) {
    	  // It's a blob header

    	  NVBlobHeader_t blob;
    	  memcpy(&blob, &s->partition->device->buffer[sizeof(header)], sizeof(blob));

          if(!strncmp(blob.name, s->name, NVSTORE_NAME_MAX) && blob.size == size) {
	    consoleNotefLn("NVStore %s Scan(%s) index = %#x",
			   s->partition->name, blob.name, s->index);
	    status = recallBlob(s->partition, s->index, &blob, data);
	    break;
	  }
      } 

      s->index = (s->index + 1) % s->partition->size;
      s->count--;
    }

    if(status != NVStore_Status_OK) {
      consoleNotefLn("NVStore %s Scan(%s) finished", s->partition->name, s->name);
      status = NVStore_Status_NotFound;
    }
  }

  SHARED_ACCESS_END(*(s->partition->device));
  
  return status;
}


  
