#ifndef VP_TIME_H
#define VP_TIME_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t VP_TIME_MICROS_T;
typedef uint16_t VP_TIME_MILLIS_T;
typedef uint32_t VP_TIME_SECS_T;
typedef uint64_t VP_TIME_JIFFIES_T;

#define VP_TIME_MICROS_MAX 0xFFFFFFFFUL
#define VP_TIME_MILLIS_MAX 0xFFFFU

#define VP_MILLIS_FINITE(t) ((t) != VP_TIME_MILLIS_MAX)

#define VP_ELAPSED_JIFFIES(start) (STAP_TimeJiffies() - start)
#define VP_ELAPSED_MILLIS(start) (VP_TIME_MILLIS_T) ((vpTimeMillis() - start) & 0xFFFF)
#define VP_ELAPSED_MICROS(start) (VP_TIME_MICROS_T) ((vpTimeMicros() - start) & 0xFFFFFFFFUL)
#define VP_ELAPSED_SECS(start) (VP_TIME_SECS_T) ((vpTimeSecs() - start) & 0xFFFFFFFFUL)

typedef struct VPInertiaTimer {
  bool *state;
  VP_TIME_MILLIS_T inertia;
  VP_TIME_MILLIS_T lastStable;
} VPInertiaTimer_t;

#define VP_INERTIA_TIMER_CONS(s, i) { .state = s, .inertia = i, .lastStable = 0 }

bool vpInertiaOn(VPInertiaTimer_t*);  // Is it okay to turn on?
bool vpInertiaOff(VPInertiaTimer_t*); // Okay to turn off?
bool vpInertiaOnForce(VPInertiaTimer_t*);
bool vpInertiaOffForce(VPInertiaTimer_t*);

typedef struct VPPeriodicTimer {
  VP_TIME_MILLIS_T period;
  VP_TIME_MILLIS_T previousEvent;
} VPPeriodicTimer_t;

#define VP_PERIODIC_TIMER_CONS(p) { .period = p, .previousEvent = 0 }

bool vpPeriodicEvent(VPPeriodicTimer_t*);

typedef struct VPEventTimer {
  const uint16_t *delay;
  VP_TIME_MILLIS_T startTime;
} VPEventTimer_t;

#define VP_EVENT_TIMER_CONS(d) { .delay = d, .startTime = 0 }

void vpEventTimerReset(VPEventTimer_t*);
bool vpEventTimerElapsed(VPEventTimer_t*);

VP_TIME_JIFFIES_T STAP_TimeJiffies(void);
VP_TIME_SECS_T STAP_TimeSecs(void);

VP_TIME_MICROS_T vpTimeMicros(void);
VP_TIME_MILLIS_T vpTimeMillis(void);
VP_TIME_SECS_T vpTimeSecs(void);

VP_TIME_MICROS_T vpApproxMicros(void);
VP_TIME_MILLIS_T vpApproxMillis(void);
VP_TIME_MICROS_T vpApproxMicrosFromISR(void);

void vpDelayMillis(VP_TIME_MILLIS_T);

#endif


