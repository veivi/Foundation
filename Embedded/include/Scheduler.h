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
    void (*handler)(const uint8_t *data, size_t size);
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

#define TASK_BY_PERIOD(N, P, C, PER, ST)		\
  (struct TaskDecl) { .type = StaP_Task_Period, .name = N, .code.task = C, .typeSpecific.period = PER, .priority = P, .stackSize = ST }
#define TASK_BY_SIGNAL(N, P, C, SIG, T, ST)	\
  (struct TaskDecl) { .type = StaP_Task_Signal, .name = N, .code.task = C, .typeSpecific.signal.id = SIG, .typeSpecific.signal.timeOut = T, .priority = P, .stackSize = ST }
#define TASK_BY_SIGNAL_NOTO(N, P, C, SIG, ST) \
  TASK_BY_SIGNAL(N, P, C, SIG, VP_TIME_MILLIS_MAX, ST)
#define TASK_BY_SERIAL(N, P, C, LINK, ST)	\
  (struct TaskDecl) { .type = StaP_Task_Serial, .name = N, .code.task = C, .typeSpecific.serial.link = LINK, .priority = P, .stackSize = ST }
#define TASK_BY_FREQ(name, pri, code, freq, stack) TASK_BY_PERIOD(name, pri, code, HZ_TO_PERIOD(freq), stack)
#define TASK_BY_DATAGRAM(N, P, C, DG, LINK, ST)	\
  (struct TaskDecl) { .type = StaP_Task_Datagram, .name = N, .code.handler = C, .typeSpecific.datagram.link = LINK,.typeSpecific.datagram.state = DG, .priority = P, .stackSize = ST }


void StaP_SchedulerInit( void );
void StaP_SchedulerStart( void );
void StaP_SchedulerReport(void);
struct TaskDecl *StaP_CurrentTask(void);

#endif
