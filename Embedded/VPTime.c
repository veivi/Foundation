#include "VPTime.h"
#include "StaP.h"

static volatile VP_TIME_MICROS_T vpTimeMicrosValue;
static volatile VP_TIME_MILLIS_T vpTimeMillisValue;
static volatile VP_TIME_SECS_T vpTimeSecsValue;

static void vpTimeAcquire(void)
{
  volatile static VP_TIME_JIFFIES_T prev = 0;
  ForbidContext_T c = STAP_FORBID_SAFE;
  VP_TIME_JIFFIES_T jiffies = STAP_TimeJiffies();

  if(jiffies > prev) {
    // Only update if monotonous
    
    vpTimeMicrosValue = STAP_JiffiesToMicros(jiffies);
    vpTimeMillisValue = vpTimeMicrosValue >> 10;  
    vpTimeSecsValue = (VP_TIME_SECS_T) STAP_TimeSecs();

    prev = jiffies;
  } else
    STAP_Error(STAP_ERR_TIME);
  
  STAP_PERMIT_SAFE(c);
}

VP_TIME_MICROS_T vpApproxMicrosFromISR(void)
{
  return vpTimeMicrosValue;
}
  
VP_TIME_MICROS_T vpApproxMicros(void)
{
  ForbidContext_T c = STAP_FORBID_SAFE;
  VP_TIME_MICROS_T now = vpTimeMicrosValue;
  STAP_PERMIT_SAFE(c);
  
  return now;
}

VP_TIME_MILLIS_T vpApproxMillis(void)
{
  ForbidContext_T c = STAP_FORBID_SAFE;
  VP_TIME_MILLIS_T now = vpTimeMillisValue;
  STAP_PERMIT_SAFE(c);
  
  return now;
}

VP_TIME_MICROS_T vpTimeMicros(void)
{
  vpTimeAcquire();
  return vpApproxMicros();
}

VP_TIME_MILLIS_T vpTimeMillis(void)
{
  vpTimeAcquire();
  return vpApproxMillis();  
}

VP_TIME_SECS_T vpTimeSecs(void)
{
  ForbidContext_T c = STAP_FORBID_SAFE;
  VP_TIME_SECS_T now = vpTimeSecsValue;
  STAP_PERMIT_SAFE(c);
  
  return now;
}

bool vpPeriodicEvent(VPPeriodicTimer_t *timer)
{
  if(VP_ELAPSED_MILLIS(timer->previousEvent) >= timer->period) {
    timer->previousEvent = vpTimeMillisValue;
    return true;
  } else
    return false;
}

void vpEventTimerReset(VPEventTimer_t *timer)
{
  timer->startTime = vpTimeMillisValue;
}

bool vpEventTimerElapsed(VPEventTimer_t *timer)
{
  return VP_ELAPSED_MILLIS(timer->startTime) >= *(timer->delay);
}

static bool vpInertiaOnOff(VPInertiaTimer_t *timer, bool state, bool force)
{
  bool status = false;

  if(*timer->state == state)
    timer->lastStable = vpTimeMillisValue;

  else if(VP_ELAPSED_MILLIS(timer->lastStable) >= timer->inertia || force) {
    *timer->state = state;
    timer->lastStable = vpTimeMillisValue;
    status = true;
  }

  return status;
}

bool vpInertiaOn(VPInertiaTimer_t *timer)
{
  return vpInertiaOnOff(timer, true, false);
}

bool vpInertiaOff(VPInertiaTimer_t *timer)
{
  return vpInertiaOnOff(timer, false, false);
}

bool vpInertiaOnForce(VPInertiaTimer_t *timer)
{
  return vpInertiaOnOff(timer, true, true);
}

bool vpInertiaOffForce(VPInertiaTimer_t *timer)
{
  return vpInertiaOnOff(timer, false, true);
}

