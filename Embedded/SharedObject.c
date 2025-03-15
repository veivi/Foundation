#include "SharedObject.h"

void sharedAccessBegin(struct SharedObject *obj)
{
  STAP_FORBID;
    
  if(!obj->mutex && !(obj->mutex = STAP_MutexCreate))
    STAP_Panic(STAP_ERR_MUTEX_CREATE);

  STAP_PERMIT;
    
  STAP_MutexObtain(obj->mutex);
}

void sharedAccessEnd(struct SharedObject *obj)
{
  STAP_MutexRelease(obj->mutex);
}
