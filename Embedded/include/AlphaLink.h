#ifndef ALPHALINK_H
#define ALPHALINK_H

#include <stdint.h>
#include "Datagram.h"
#include "Spatial.h"

// AlphaLink bitrate

#define AL_BITRATE           133000UL

// AlphaLink node type declarations, the LSB is used for ID

#define ALN_ALPHABOX         (0)
#define ALN_ALPHA            (1<<1)
#define ALN_PITOT            (2<<1)
#define ALN_BAROMETER        (3<<1)
#define ALN_COMPASS          (4<<1)
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

#define DG_ALPHA_BROADCAST        (DG_ALPHALINK+0)
#define DG_ALPHA_QUERY            (DG_ALPHALINK+1)
#define DG_ALPHA_RESPONSE         (DG_ALPHALINK+2)
#define DG_ALPHA_COMMAND          (DG_ALPHALINK+3)
#define DG_ALPHA_CONFIGURE        (DG_ALPHALINK+4)
#define DG_ALPHA_CALIBRATE        (DG_ALPHALINK+5)
#define DG_ALPHA_RESTORE          (DG_ALPHALINK+6)
#define DG_ALPHA_DEBUG            (DG_ALPHALINK+7)

// Broadcast message types

#define ALBC_GROUND_TIMEDATE   1
#define ALBC_GROUND_POSITION   2
#define ALBC_GROUND_ALMANAC    3
#define ALBC_GROUND_EPHEMERIS  4
#define ALBC_ORIENTATION       5
#define ALBC_MAGNETIC_FIELD    6
#define ALBC_GRAVITY           7   // Estimated gravity
#define ALBC_ACCELERATION      8   // Expected acceleration (so local acceleration used for estimated gravity)

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
#define IMU_ORIENT_LOST    0x80    // Warning flag

#define IMU_DATA_VERSION   0

struct IMUState {
  FloatQuat_t orientation;
  FloatVector_t acceleration;
  FloatVector_t rate;
  uint8_t version, flags;
  uint8_t _pad[2];
};

//
// SERVO data structures
//

struct ServoState {
  float controlRate, jitter;
  float supplyVolt0, supplyVolt1;
  float servoVolt, servoCurr;
};

#endif
