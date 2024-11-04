#ifndef ALPHALINK_H
#define ALPHALINK_H

#include <stdint.h>
#include "Spatial.h"

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
#define ALN_BROADCAST        (DG_MAX_NODES-1)

// AlphaLink datagram types

#define AL_BROADCAST        0
#define AL_QUERY            1
#define AL_RESPONSE         2
#define AL_COMMAND          3
#define AL_SET              4
#define AL_GET              5
#define AL_DEBUG            6

// Broadcast message types

#define ALBC_GROUND_TIMEDATE   1
#define ALBC_GROUND_POSITION   2
#define ALBC_GROUND_ALMANAC    3
#define ALBC_GROUND_EPHEMERIS  4
#define ALBC_IMUDATA           5
#define ALBC_MAGNETIC_FIELD    6
#define ALBC_GRAVITY           7

struct ALQueryHeader {
  uint16_t channel;
  uint16_t latencyTarget, latencyMax; // microsecs
  uint16_t idlePeriod;                // milliseconds
};

//
// GNSS data structures
//

#define GPS_POS_VALID    1
#define GPS_ALT_VALID    2
#define GPS_TRACK_VALID  4
#define GPS_MAX_SATS     (1<<5)
#define GPS_MAX_VIEWS    5

struct GPSTime {
  uint8_t h, m, s, cs;    
};

struct GPSDate {
  uint16_t y;
  uint8_t m, d;
  uint8_t valid;
  uint8_t pad[3];
};

typedef struct {
  int32_t integer;
  uint32_t fraction;     // BCD coded, MSBs are aligned
} GPSLatLon_t;

struct GPSFix {
  GPSLatLon_t lat;       // Arc minutes, N positive
  GPSLatLon_t lon;       // Arc minutes, E positive
  struct GPSTime time;
  uint16_t alt;          // Decimeters
  uint8_t mask;
  uint8_t _pad;
};

struct GPSTrack {
  struct GPSTime time;
  uint16_t speed;    // Decimeters/s
  uint16_t heading;  // 0-360 deg
  uint8_t mask;
  uint8_t _pad[3];
};

struct GPSView {
  uint8_t numSats, type;
  uint8_t satPRN[GPS_MAX_SATS];
  uint8_t satSNR[GPS_MAX_SATS];  
};

//
// IMU data structures
//

#define IMU_ORIENT_VALID   1
#define IMU_ACCEL_VALID    2
#define IMU_RATE_VALID     4

#define IMU_DATA_VERSION   0

struct IMUState {
  FloatQuat_t orientation;
  FloatVector_t acceleration;
  FloatVector_t rate;
  uint8_t version, mask;
  uint8_t _pad[2];
};

#endif
