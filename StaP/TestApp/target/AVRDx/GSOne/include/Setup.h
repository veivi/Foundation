#ifndef MAINLOOP_H
#define MAINLOOP_H

void mainLoopSetup(void);
void mainLoop(void);
void hostLoopback(void);
void schedulerReport(void);

extern struct Task *currentTask;

#endif

