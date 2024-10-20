#ifndef ALPHALINK_H
#define ALPHALINK_H

#include <stdint.h>

// AlphaLink bitrate

#define AL_BITRATE           100000UL

// AlphaLink node type declarations, the LSB is used for ID

#define ALN_ALPHABOX         (0)
#define ALN_ALPHA            (1<<1)
#define ALN_PITOT            (2<<1)
#define ALN_BAROMETER        (3<<1)
#define ALN_MAGNETOMETER     (4<<1)
#define ALN_GNSS             (5<<1)
#define ALN_RANGEFINDER      (6<<1)
#define ALN_FORCE            (7<<1)
#define ALN_FUELFLOW         (8<<1)
#define ALN_GPIO             (9<<1)
#define ALN_SERVO            (10<<1)
#define ALN_GYRO             (11<<1)
#define ALN_TELEMETRY        (12<<1)
#define ALN_BROADCAST        ((1<<6)-1)

// AlphaLink actions

#define AL_IDENTIFY         0
#define AL_QUERY            1
#define AL_RESPONSE         2
#define AL_COMMAND          3
#define AL_SET              4
#define AL_GET              5
#define AL_DEBUG            6
#define AL_GROUND_TIMEDATE  7
#define AL_GROUND_POSITION  8

struct ALQueryHeader {
  uint16_t channel;
  uint16_t latencyTarget, latencyMax; // microsecs
  uint16_t idlePeriod;                // milliseconds
};

#endif
