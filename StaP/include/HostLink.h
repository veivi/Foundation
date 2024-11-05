#ifndef HOSTLINK_H
#define HOSTLINK_H

#include "Datagram.h"

#define DG_HOST_PING          (DG_HOSTLINK+0)
#define DG_HOST_LOGDATA       (DG_HOSTLINK+1)
#define DG_HOST_LOGINFO       (DG_HOSTLINK+2)
#define DG_HOST_PARAMS        (DG_HOSTLINK+3)
#define DG_HOST_SIMLINK       (DG_HOSTLINK+4)
#define DG_HOST_STATUS        (DG_HOSTLINK+5)
#define DG_HOST_DISCONNECT    (DG_HOSTLINK+6)
#define DG_HOST_PONG          (DG_HOSTLINK+7)
#define DG_HOST_LOGNAME       (DG_HOSTLINK+8)
#define DG_HOST_LOGTXT        (DG_HOSTLINK+9)

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

