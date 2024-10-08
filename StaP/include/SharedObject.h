#ifndef SHAREDOBJECT_H
#define SHAREDOBJECT_H

#include "StaP.h"

struct SharedObject {
  StaP_MutexRef_T mutex;
};

#define SHARED_ACCESS_BEGIN(o)        sharedAccessBegin(&((o).header))
#define SHARED_ACCESS_END(o)          sharedAccessEnd(&((o).header))

void sharedAccessBegin(struct SharedObject *obj);
void sharedAccessEnd(struct SharedObject *obj);

#endif
