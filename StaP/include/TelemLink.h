#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "Datagram.h"
#include "AlphaLink.h"

#define DG_TELEM_AIRDATA           (DG_TELEMLINK+0)
#define DG_TELEM_CONFIG            (DG_TELEMLINK+1)
#define DG_TELEM_ANNUNCIATOR       (DG_TELEMLINK+2)
#define DG_TELEM_IMUDATA           (DG_TELEMLINK+3)
#define DG_TELEM_GPSDATA           (DG_TELEMLINK+4)
#define DG_TELEM_BARODATA          (DG_TELEMLINK+5)
#define DG_TELEM_POSITION          (DG_TELEMLINK+6)
#define DG_TELEM_GPSVIEW           (DG_TELEMLINK+7)
#define DG_TELEM_GROUNDDATA        (DG_TELEMLINK+8)
#define DG_TELEM_GROUNDTIMEDATE    (DG_TELEMLINK+9)
#define DG_TELEM_ALMANAC           (DG_TELEMLINK+10)
#define DG_TELEM_EPHEMERIS         (DG_TELEMLINK+11)

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


