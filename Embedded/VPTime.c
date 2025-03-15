#include "VPTime.h"
#include "StaP.h"

VP_TIME_MICROS_T vpTimeMicrosApprox;
VP_TIME_MILLIS_T vpTimeMillisApprox;
VP_TIME_SECS_T vpTimeSecsApprox;

void vpTimeAcquire(void)
{
  vpTimeMicrosApprox = (VP_TIME_MICROS_T) STAP_TimeMicros();
  vpTimeMillisApprox = (VP_TIME_MILLIS_T) (vpTimeMicrosApprox>>10);
  vpTimeSecsApprox = (VP_TIME_SECS_T) STAP_TimeSecs();
}

VP_TIME_MICROS_T vpTimeMicros(void)
{
  vpTimeAcquire();
  return vpTimeMicrosApprox;
}

VP_TIME_MILLIS_T vpTimeMillis(void)
{
  vpTimeAcquire();
  return vpTimeMillisApprox;  
}

void vpDelayMillis(VP_TIME_MILLIS_T x)
{
  VP_TIME_MILLIS_T start = vpTimeMillis();
  
  while(VP_ELAPSED_MILLIS(start) < x)
    vpTimeAcquire();
}

bool vpPeriodicEvent(VPPeriodicTimer_t *timer)
{
  if(VP_ELAPSED_MILLIS(timer->previousEvent) >= timer->period) {
    timer->previousEvent = vpTimeMillisApprox;
    return true;
  } else
    return false;
}

void vpEventTimerReset(VPEventTimer_t *timer)
{
  timer->startTime = vpTimeMillisApprox;
}

bool vpEventTimerElapsed(VPEventTimer_t *timer)
{
  return VP_ELAPSED_MILLIS(timer->startTime) >= *(timer->delay);
}

static bool vpInertiaOnOff(VPInertiaTimer_t *timer, bool state, bool force)
{
  bool status = false;

  if(*timer->state == state)
    timer->lastStable = vpTimeMillisApprox;

  else if(VP_ELAPSED_MILLIS(timer->lastStable) >= timer->inertia || force) {
    *timer->state = state;
    timer->lastStable = vpTimeMillisApprox;
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

