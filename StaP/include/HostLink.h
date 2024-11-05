#ifndef HOSTLINK_H
#define HOSTLINK_H

#include "Datagram.h"

#define HL_PING          (DG_HOSTLINK+0)
#define HL_LOGDATA       (DG_HOSTLINK+1)
#define HL_LOGINFO       (DG_HOSTLINK+2)
#define HL_PARAMS        (DG_HOSTLINK+3)
#define HL_SIMLINK       (DG_HOSTLINK+4)
#define HL_STATUS        (DG_HOSTLINK+5)
#define HL_DISCONNECT    (DG_HOSTLINK+6)
#define HL_PONG          (DG_HOSTLINK+7)
#define HL_LOGNAME       (DG_HOSTLINK+8)
#define HL_LOGTXT        (DG_HOSTLINK+9)

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

