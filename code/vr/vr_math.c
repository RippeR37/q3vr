#include "vr_math.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

float DegreesToRadians(float deg)
{
	return (deg * M_PI) / 180.0;
}

float RadiansToDegrees(float rad)
{
	return (rad * 180.0) / M_PI;
}
