#ifndef SHAREDOBJECT_H
#define SHAREDOBJECT_H

#include "StaP.h"
#include "Scheduler.h"

struct SharedObject {
  STAP_MutexRef_T mutex;
};

#define SHARED_ACCESS_BEGIN(o)        sharedAccessBegin(&((o).header))
#define SHARED_ACCESS_END(o)          sharedAccessEnd(&((o).header))

#define SHARED_OBJECT_REFERENCE(r, f, v) \
  (SHARED_ACCESS_BEGIN(r), v = (r).f, SHARED_ACCESS_END(r), v) 
#define SHARED_OBJECT_READ(r, f, v) \
  (void) (SHARED_ACCESS_BEGIN(r), v = (r).f, SHARED_ACCESS_END(r), v) 
#define SHARED_OBJECT_UPDATE(r, f, v) \
  (void) (SHARED_ACCESS_BEGIN(r), (r).f = v, SHARED_ACCESS_END(r), v) 


void sharedAccessBegin(struct SharedObject *obj);
void sharedAccessEnd(struct SharedObject *obj);

#endif
