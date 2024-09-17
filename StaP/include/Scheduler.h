#ifndef STAP_SCHEDULER_H
#define STAP_SCHEDULER_H

#include "VPTime.h"
#include "StaP.h"

//
// Task structure
//

typedef enum { StaP_Task_Period, StaP_Task_Signal, StaP_Task_Serial } StaP_TaskType_T;

struct TaskDecl {
  StaP_TaskType_T type;
  const char *name;
  uint8_t priority;
  uint16_t stackSize;
  VP_TIME_MICROS_T (*code)(void);
  void *handle;
  VP_TIME_MICROS_T runTime;
  VP_TIME_MILLIS_T lastInvoked;
  
  union {
    VP_TIME_MILLIS_T period;

    struct {
      VP_TIME_MILLIS_T timeOut;
      StaP_Signal_T id;
    } signal;

    struct {
      StaP_LinkId_T link;      
    } serial;
  } typeSpecific;
};

extern struct TaskDecl StaP_TaskList[];
extern const int StaP_NumOfTasks;

#define HZ_TO_PERIOD(f) ((VP_TIME_MILLIS_T) (1.0e3f/(f)))

#define TASK_BY_PERIOD(N, P, C, PER, ST)		\
  { .type = StaP_Task_Period, .name = N, .code = C, .typeSpecific.period = PER, .priority = P, .stackSize = ST }
#define TASK_BY_SIGNAL(N, P, C, SIG, T, ST)	\
  { .type = StaP_Task_Signal, .name = N, .code = C, .typeSpecific.signal.id = SIG, .typeSpecific.signal.timeOut = T, .priority = P, .stackSize = ST }
#define TASK_BY_SIGNAL_NOTO(N, P, C, SIG, ST) \
  TASK_BY_SIGNAL(N, P, C, SIG, VP_TIME_MILLIS_MAX, ST)
#define TASK_BY_SERIAL(N, P, C, LINK, ST)	\
  { .type = StaP_Task_Serial, .name = N, .code = C, .typeSpecific.serial.link = LINK, .priority = P, .stackSize = ST }
#define TASK_BY_FREQ(name, pri, code, freq, stack) TASK_BY_PERIOD(name, pri, code, HZ_TO_PERIOD(freq), stack)
#define END_OF_TASKS TASK_BY_PERIOD(NULL, 0, NULL, 0, 0)

void StaP_SchedulerStart( void );
struct TaskDecl *StaP_CurrentTask(void);

#endif
