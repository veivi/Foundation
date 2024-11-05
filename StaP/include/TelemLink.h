#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "Datagram.h"
#include "AlphaLink.h"

#define TL_AIRDATA           (DG_TELEMLINK+0)
#define TL_CONFIG            (DG_TELEMLINK+1)
#define TL_ANNUNCIATOR       (DG_TELEMLINK+2)
#define TL_IMUDATA           (DG_TELEMLINK+3)
#define TL_GPSDATA           (DG_TELEMLINK+4)
#define TL_BARODATA          (DG_TELEMLINK+5)
#define TL_POSITION          (DG_TELEMLINK+6)
#define TL_GPSVIEW           (DG_TELEMLINK+7)
#define TL_GROUNDDATA        (DG_TELEMLINK+8)
#define TL_GROUNDTIMEDATE    (DG_TELEMLINK+9)
#define TL_ALMANAC           (DG_TELEMLINK+10)
#define TL_EPHEMERIS         (DG_TELEMLINK+11)

struct TelemetryAirData {
  float alpha;
  float IAS;
  uint16_t status;
};

struct TelemetryBaroData {
  float alt;
  //  float windSpeed, windDir;
};

struct TelemetryPositionData {
  FloatVector_t position;
  FloatVector_t velocity;
};

#define TELEM_NAME_LEN   10

struct TelemetryConfig {
  float trim;
  float maxAlpha, shakerAlpha, threshAlpha;
  float trimIAS, stallIAS, margin;
  float fuelFlow;
  uint8_t fuel, flap, gear, load, latency;
  char name[TELEM_NAME_LEN+1];
};

struct TelemetryIMUData {
  float bank, pitch, heading;
  float ball, loadFactor;
};

struct TelemetryGroundData {
  struct GPSFix pos;
  float pressure, temp;
};

struct TelemetryGroundTimeDate {
  struct GPSTime time;
  struct GPSDate date;
};

#endif


