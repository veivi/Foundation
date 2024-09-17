#include "Buffer.h"
#include "StaP.h"
#include <string.h>

void vpbuffer_init(VPBuffer_t *i, VPBufferSize_t size, char *storage)
{
  i->inPtr = i->outPtr = 0;
  i->mask = size - 1;
  i->storage = storage;
}

void vpbuffer_flush(VPBuffer_t *i)
{
  i->outPtr = i->inPtr;
}

void vpbuffer_adjust(VPBuffer_t *i, VPBufferIndex_t delta)
{
  if(!i->storage)
    return;
  
  VPBufferSize_t g = vpbuffer_gauge(i);
  
  if(delta > g)
    delta = g;
  
  i->inPtr = VPBUFFER_INDEX((*i), i->inPtr, -delta);
}

VPBufferSize_t vpbuffer_insert(VPBuffer_t *i, const char *b, VPBufferSize_t s, bool overwrite)
{
  if(!i->storage || s < 1)
    return 0;

  VPBufferIndex_t space = vpbuffer_space(i);

  if(s > i->mask)
    s = i->mask;
  
  if(s > space) {
    if(overwrite) {
      // Adjust the inPtr for overwriting

      vpbuffer_adjust(i, s - space + 3);
      vpbuffer_insert(i, " ~ ", 3, false);
      vpbuffer_insert(i, b, s, false);
      
      i->overrun = true;
    
      return s;
    } else
      // Truncate
      s = space;
  }

  if(VPBUFFER_INDEX((*i), i->inPtr, s) < i->inPtr) {
    VPBufferSize_t cut = i->mask + 1 - i->inPtr;
    memcpy(&i->storage[i->inPtr], b, cut);
    
    if(s > cut)
      memcpy(i->storage, &b[cut], s - cut);
  } else {
    memcpy(&i->storage[i->inPtr], b, s);
  }
  
  i->inPtr = VPBUFFER_INDEX((*i), i->inPtr, s);

  return s;
}

bool vpbuffer_hasOverrun(VPBuffer_t *i)
{
  bool status = i->overrun;
  i->overrun = false;
  return status;
}

VPBufferSize_t vpbuffer_extract(VPBuffer_t *i, char *b, VPBufferSize_t s)
{
  if(!i->storage || s < 1)
    return 0;
  
  VPBufferIndex_t g = vpbuffer_gauge(i);
  
  if(s > g)
    s = g;

  if(VPBUFFER_INDEX((*i), i->outPtr, s) < i->outPtr) {
    VPBufferSize_t cut = i->mask + 1 - i->outPtr;
    memcpy(b, &i->storage[i->outPtr], cut);
    if(s > cut)
      memcpy(&b[cut], i->storage, s - cut);
  } else {
    memcpy(b, &i->storage[i->outPtr], s);

  }
  
  i->outPtr = VPBUFFER_INDEX((*i), i->outPtr, s);

  return s;
}

void vpbuffer_insertChar(VPBuffer_t *i, char c)
{
  VPBufferIndex_t ptrNew = VPBUFFER_INDEX((*i), i->inPtr, 1);

  if(!i->storage)
    return;
  
  i->storage[i->inPtr] = c;
  
  if(ptrNew != i->outPtr)
    i->inPtr = ptrNew;
  else
    i->overrun = true;
}

char vpbuffer_extractChar(VPBuffer_t *i)
{
  if(i->storage && i->outPtr != i->inPtr) {
    char c = i->storage[i->outPtr];
    i->outPtr = VPBUFFER_INDEX((*i), i->outPtr, 1);      
    return c;
  }

  return 0xFF;
}

/*

VPBufferIndex_t vpbuffer_space(VPBuffer_t *i)
{
  VPBufferIndex_t value = 0;
  STAP_FORBID;
  value = VPBUFFER_SPACE(*i);
  STAP_PERMIT;
  return value;  
}
    
VPBufferIndex_t vpbuffer_gauge(VPBuffer_t *i)
{
  VPBufferIndex_t value = 0;
  STAP_FORBID;
  value = VPBUFFER_GAUGE(*i);
  STAP_PERMIT;
  return value;  
}

*/
