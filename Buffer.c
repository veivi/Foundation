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

  if(s > i->mask)
    s = i->mask;
  
  ForbidContext_T c = STAP_FORBID_SAFE;
  
  VPBufferIndex_t space = vpbuffer_space(i);

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

  STAP_PERMIT_SAFE(c);
  
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
  
  if(s > g)
    s = g;

  ForbidContext_T c = STAP_FORBID_SAFE;
  
  VPBufferIndex_t g = vpbuffer_gauge(i);
  
  if(VPBUFFER_INDEX((*i), i->outPtr, s) < i->outPtr) {
    VPBufferSize_t cut = i->mask + 1 - i->outPtr;
    memcpy(b, &i->storage[i->outPtr], cut);
    if(s > cut)
      memcpy(&b[cut], i->storage, s - cut);
  } else {
    memcpy(b, &i->storage[i->outPtr], s);

  }
  
  i->outPtr = VPBUFFER_INDEX((*i), i->outPtr, s);

  STAP_PERMIT_SAFE(c);
  
  return s;
}

void vpbuffer_insertChar(VPBuffer_t *i, char c)
{
  if(!i->storage)
    return;
  
  ForbidContext_T c = STAP_FORBID_SAFE;
  
  VPBufferIndex_t ptrNew = VPBUFFER_INDEX((*i), i->inPtr, 1);

  i->storage[i->inPtr] = c;
  
  if(ptrNew != i->outPtr)
    i->inPtr = ptrNew;
  else
    i->overrun = true;

  STAP_PERMIT_SAFE(c);
}

char vpbuffer_extractChar(VPBuffer_t *i)
{
  char b = 0xFE;
  
  ForbidContext_T c = STAP_FORBID_SAFE;
  
  if(i->storage && i->outPtr != i->inPtr) {
    b = i->storage[i->outPtr];
    i->outPtr = VPBUFFER_INDEX((*i), i->outPtr, 1);      
  }

  STAP_PERMIT_SAFE(c);
  
  return b;
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
