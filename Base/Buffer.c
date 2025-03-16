#include "Buffer.h"
#include "StaP.h"
#include <string.h>
#include "cmsis_compiler.h"

#define CRITICAL_SECTIONS    0    // We don't believe in critical sectoins.

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

#if CRITICAL_SECTIONS
  ForbidContext_T c = STAP_FORBID_SAFE;
#endif
  
  VPBufferIndex_t space = vpbuffer_space(i);

  if(s > space) {
    if(overwrite) {
      // Adjust the inPtr for overwriting

      vpbuffer_adjust(i, s - space + 3);
      vpbuffer_insert(i, " ~ ", 3, false);
      vpbuffer_insert(i, b, s, false);
      
      i->overrun = true;
    
#if CRITICAL_SECTIONS
      STAP_PERMIT_SAFE(c);
#endif
      
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
  
//  __DMB();

  i->inPtr = VPBUFFER_INDEX((*i), i->inPtr, s);

#if CRITICAL_SECTIONS
  STAP_PERMIT_SAFE(c);
#endif

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
  
#if CRITICAL_SECTIONS
  ForbidContext_T c = STAP_FORBID_SAFE;
#endif
  
  VPBufferIndex_t g = vpbuffer_gauge(i);
  
  if(s > g)
    s = g;

//  __DMB();

  if(VPBUFFER_INDEX((*i), i->outPtr, s) < i->outPtr) {
    VPBufferSize_t cut = i->mask + 1 - i->outPtr;
    memcpy(b, &i->storage[i->outPtr], cut);
    if(s > cut)
      memcpy(&b[cut], i->storage, s - cut);
  } else {
    memcpy(b, &i->storage[i->outPtr], s);
  }
  
  i->outPtr = VPBUFFER_INDEX((*i), i->outPtr, s);

#if CRITICAL_SECTIONS
  STAP_PERMIT_SAFE(c);
#endif
  
  return s;
}

void vpbuffer_insertChar(VPBuffer_t *i, char b)
{
  if(!i->storage)
    return;
  
#if CRITICAL_SECTIONS
  ForbidContext_T c = STAP_FORBID_SAFE;
#endif
  
  VPBufferIndex_t ptrNew = VPBUFFER_INDEX((*i), i->inPtr, 1);

  i->storage[i->inPtr] = b;
  
  if(ptrNew != i->outPtr)
    i->inPtr = ptrNew;
  else
    i->overrun = true;

#if CRITICAL_SECTIONS
  STAP_PERMIT_SAFE(c);
#endif
}

char vpbuffer_extractChar(VPBuffer_t *i)
{
  char b = 0xFE;
  
#if CRITICAL_SECTIONS
  ForbidContext_T c = STAP_FORBID_SAFE;
#endif
  
  if(i->storage && i->outPtr != i->inPtr) {
    b = i->storage[i->outPtr];
    i->outPtr = VPBUFFER_INDEX((*i), i->outPtr, 1);      
  }

#if CRITICAL_SECTIONS
  STAP_PERMIT_SAFE(c);
#endif
  
  return b;
}

