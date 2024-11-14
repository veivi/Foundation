#include "Spatial.h"

void floatVectorFromDeci(FloatVector_t *result, const DeciVector_t *a)
{
  *result = FLOAT_VECTOR((float) a->elem[0]/10.0f,
			 (float) a->elem[1]/10.0f,
			 (float) a->elem[2]/10.0f);
}

void floatVectorAdd(FloatVector_t *result, const FloatVector_t *a, const FloatVector_t *b)
{
  *result = FLOAT_VECTOR(a->elem[0]+b->elem[0], a->elem[1]+b->elem[1], a->elem[2]+b->elem[2]);
}

void floatVectorScale(FloatVector_t *result, const FloatVector_t *a, float b)
{
  *result = FLOAT_VECTOR(b*a->elem[0], b*a->elem[1], b*a->elem[2]);
}

float floatVectorDot(const FloatVector_t *a, const FloatVector_t *b)
{
  return a->elem[0]*b->elem[0] + a->elem[1]*b->elem[1] + a->elem[2]*b->elem[2];
}

void floatVectorCross(FloatVector_t *result, const FloatVector_t *a, const FloatVector_t *b)
{
  *result = FLOAT_VECTOR(a->elem[1] * b->elem[2] - a->elem[2] * b->elem[1],
                         a->elem[2] * b->elem[0] - a->elem[0] * b->elem[2],
                         a->elem[0] * b->elem[1] - a->elem[1] * b->elem[0]);
}

float floatVectorNormSquared(const FloatVector_t *a)
{
  return floatVectorDot(a, a);
}

#define DECI_RANGE         ((1<<15)/10)

bool deciVectorFromFloat(DeciVector_t *result, const FloatVector_t *a)
{
  if(a->elem[0] < -DECI_RANGE || a->elem[0] > DECI_RANGE
    || a->elem[1] < -DECI_RANGE || a->elem[1] > DECI_RANGE
    || a->elem[2] < -DECI_RANGE || a->elem[2] > DECI_RANGE)
    return false;
 
  *result = DECI_VECTOR(a->elem[0]*10, a->elem[1]*10, a->elem[2]*10);
  return true;
}

void deciVectorAdd(DeciVector_t *result, const DeciVector_t *a, const DeciVector_t *b)
{
  *result = DECI_VECTOR(a->elem[0]+b->elem[0], a->elem[1]+b->elem[1], a->elem[2]+b->elem[2]);
}

void deciVectorScale(DeciVector_t *result, const DeciVector_t *a, float b)
{
  *result = DECI_VECTOR(b*a->elem[0], b*a->elem[1], b*a->elem[2]);
}

float orientationEulerRoll(const FloatQuat_t *atti)
{
  return atan2f(2.0f*(atti->elem[0]*atti->elem[1] + atti->elem[2]*atti->elem[3]),
		1.0f - 2.0f*(atti->elem[1]*atti->elem[1] + atti->elem[2]*atti->elem[2]));
}

float orientationEulerPitch(const FloatQuat_t *atti)
{
  return 2.0f*atan2f(sqrtf(1.0f + 2.0f*(atti->elem[0]*atti->elem[2] - atti->elem[1]*atti->elem[3])),
		  sqrtf(1.0f - 2.0f*(atti->elem[0]*atti->elem[2] - atti->elem[1]*atti->elem[3])))
    - PI_F/2;
}

float orientationEulerHeading(const FloatQuat_t *atti)
{ 
  return atan2f(2.0f*(atti->elem[0]*atti->elem[3]+ atti->elem[1]*atti->elem[2]),
		1.0f - 2.0f*(atti->elem[2]*atti->elem[2] + atti->elem[3]*atti->elem[3]));
}

