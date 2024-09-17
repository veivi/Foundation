#ifndef HOSTLINK_H
#define HOSTLINK_H

#include "Datagram.h"

#define HL_PING          0
#define HL_HEARTBEAT     1
#define HL_CONSOLE       2
#define HL_LOGDATA       3
#define HL_LOGINFO       4
#define HL_PARAMS        5
#define HL_SIMLINK       8
#define HL_STATUS        9
#define HL_DISCONNECT    10
#define HL_PONG          11
#define HL_LOGNAME       12
#define HL_LOGTXT        13

struct SimLinkSensor {
  float alpha, alt, ias;
  float roll, pitch, heading;
  float rrate, prate, yrate;
  float accx, accy, accz;
};

struct SimLinkControl {
  float aileron, elevator, throttle, rudder;
};

#endif

