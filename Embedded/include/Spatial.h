#ifndef DRIVER_SPATIAL_H
#define DRIVER_SPATIAL_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

// Constants

#define PI_F             ((float) M_PI)
#define RAD_F            (180.0f/PI_F)

// FP vector

typedef struct {
  float elem[3];
} FloatVector_t;

#define FLOAT_VECTOR(x,y,z)     (FloatVector_t) { .elem = {(float) (x), (float) (y), (float) (z) } }

// FP quaternion

typedef struct {
  float elem[4];
} FloatQuat_t;

#define FLOAT_QUAT(i,x,y,z)     (FloatQuat_t) { .elem = {(float) (i), (float) (x), (float) (y), (float) (z) } }

// Fixed-point (decimeter) vector

typedef int16_t DeciScalar_t;

typedef struct {
  DeciScalar_t elem[3];
} DeciVector_t;

#define DECI_VECTOR(x,y,z)     (DeciVector_t) { .elem = {(DeciScalar_t) (x), (DeciScalar_t) (y), (DeciScalar_t) (z) } }

// Sensor orientation

typedef enum { orient_0 = 0, orient_1, orient_2, orient_3, orient_4, orient_5, orient_6, orient_7 } SensorOrient_t;

// Orientation to local frame

#define ORIENTATION_0(v)   {  v[0],  v[1],  v[2] }
#define ORIENTATION_1(v)   { v[1],  -v[0],  v[2] }
#define ORIENTATION_2(v)   { -v[0], -v[1],  v[2] }
#define ORIENTATION_3(v)   { -v[1],  v[0],  v[2] }
#define ORIENTATION_4(v)   { v[0],  -v[1], -v[2] }
#define ORIENTATION_5(v)   { -v[1], -v[0], -v[2] }
#define ORIENTATION_6(v)   { -v[0],  v[1], -v[2] }
#define ORIENTATION_7(v)   {  v[1],  v[0], -v[2] }

// Vector arithmetic

void floatVectorFromDeci(FloatVector_t *result, const DeciVector_t *a);
void floatVectorAdd(FloatVector_t *result, const FloatVector_t *a, const FloatVector_t *b);
void floatVectorSubtract(FloatVector_t *result, const FloatVector_t *a, const FloatVector_t *b);
void floatVectorScale(FloatVector_t *result, const FloatVector_t *a, float b);
float floatVectorDot(const FloatVector_t *a, const FloatVector_t *b);
void floatVectorCross(FloatVector_t *result, const FloatVector_t *a, const FloatVector_t *b);
float floatVectorNormSquared(const FloatVector_t *a);
float floatQuatNormSquared(const FloatQuat_t *a);
float floatQuatDot(const FloatQuat_t *a, const FloatQuat_t *b);

bool deciVectorFromFloat(DeciVector_t *result, const FloatVector_t *a);
void deciVectorAdd(DeciVector_t *result, const DeciVector_t *a, const DeciVector_t *b);
void deciVectorScale(DeciVector_t *result, const DeciVector_t *a, float b);

float orientationEulerRoll(const FloatQuat_t *atti);
float orientationEulerPitch(const FloatQuat_t *atti);
float orientationEulerHeading(const FloatQuat_t *atti);

#endif
