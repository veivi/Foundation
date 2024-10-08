#ifndef SHAREDOBJECT_H
#define SHAREDOBJECT_H

#include "StaP.h"

struct SharedObject {
  StaP_MutexRef_T mutex;
};

#define SHARED_ACCESS_BEGIN(o)        sharedAccessBegin((struct SharedObject*) &(o))
#define SHARED_ACCESS_END(o)          sharedAccessEnd((struct SharedObject*) &(o))

#endif
