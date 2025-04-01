#ifndef STAP_SCHEDULER_H
#define STAP_SCHEDULER_H

#include "VPTime.h"
#include "Datagram.h"
#include "StaP.h"

//
// Task structure
//

typedef enum { StaP_Task_Period, StaP_Task_Signal, StaP_Task_Serial, StaP_Task_Datagram } StaP_TaskType_T;

struct TaskDecl {
  StaP_TaskType_T type;
  const char *name;
  uint8_t priority;
  uint16_t stackSize;
  union {
    VP_TIME_MICROS_T (*task)(void);
    void (*handler)(void *context, uint8_t, const uint8_t *data, size_t size);
  } code;
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

    struct {
      StaP_LinkId_T link;
      DgLink_t *state;
    } datagram;
  } typeSpecific;
};

extern struct TaskDecl StaP_TaskList[];
extern const int StaP_NumOfTasks;

#define HZ_TO_PERIOD(f) (f) > 0 ? ((VP_TIME_MILLIS_T) (1.0e3f/(f))) : VP_TIME_MILLIS_MAX

#define PERIODIC_TASK(N, P, C, PER, ST)		\
  (struct TaskDecl) { .type = StaP_Task_Period, .name = N, .code.task = C, .typeSpecific.period = PER, .priority = P, .stackSize = ST }
#define SYNCHRONOUS_TASK_TO(N, P, C, SIG, TO, ST)	\
  (struct TaskDecl) { .type = StaP_Task_Signal, .name = N, .code.task = C, .typeSpecific.signal.id = SIG, .typeSpecific.signal.timeOut = TO, .priority = P, .stackSize = ST }
#define SYNCHRONOUS_TASK(N, P, C, SIG, ST) \
  SYNCHRONOUS_TASK_TO(N, P, C, SIG, VP_TIME_MILLIS_MAX, ST)
#define RECEIVER_TASK(N, P, C, LINK, ST)	\
  (struct TaskDecl) { .type = StaP_Task_Serial, .name = N, .code.task = C, .typeSpecific.serial.link = LINK, .priority = P, .stackSize = ST }
#define PERIODIC_TASK_F(name, pri, code, freq, stack) PERIODIC_TASK(name, pri, code, HZ_TO_PERIOD(freq), stack)
#define DATAGRAM_TASK(N, P, C, DG, LINK, ST)	\
  (struct TaskDecl) { .type = StaP_Task_Datagram, .name = N, .code.handler = C, .typeSpecific.datagram.link = LINK, .typeSpecific.datagram.state = DG, .priority = P, .stackSize = ST }

// Old names for backward compat

#define TASK_BY_PERIOD       PERIODIC_TASK
#define TASK_BY_FREQ         PERIODIC_TASK_F
#define TASK_BY_SIGNAL       SYNCHRONOUS_TASK
#define TASK_BY_SIGNAL_NOTO  SYNCHRONOUS_TASK_NOTO
#define TASK_BY_SERIAL       RECEIVER_TASK
#define TASK_BY_DATAGRAM     DATAGRAM_TASK

void StaP_SchedulerInit( void );
void StaP_SchedulerStart( void );
void StaP_SchedulerReport(void);
struct TaskDecl *StaP_CurrentTask(void);

#endif
