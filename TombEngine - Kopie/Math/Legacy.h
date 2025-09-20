#pragma once

#include "Math/Constants.h"

constexpr auto FP_SHIFT				   = 16;
constexpr auto W2V_SHIFT			   = 14;
constexpr auto PREDICTIVE_SCALE_FACTOR = 14;

constexpr auto SHORTS_TO_1_DEGREE = 65536.0f / 360.0f;
constexpr auto DEGREES_TO_1_SHORT = 360.0f / 65536.0f;

constexpr float ROUND(float value)
{
	return ((value > 0.0f) ? int(value + 0.5f) : int(value - 0.5f));
}

constexpr short ANGLE(float degrees)
{
	return (short)ROUND(degrees * SHORTS_TO_1_DEGREE);
}

constexpr short FROM_RAD(float radians)
{
	return (short)ROUND((radians / RADIAN) * SHORTS_TO_1_DEGREE);
}

constexpr float TO_DEGREES(short shortAngle)
{
	return (shortAngle * DEGREES_TO_1_SHORT);
}

constexpr float TO_RAD(short shortAngle)
{
	return ((shortAngle * DEGREES_TO_1_SHORT) * RADIAN);
}

constexpr float RAD_TO_DEG(float radians)
{
	return ((radians * 180.0f) / PI);
}

constexpr float DEG_TO_RAD(float degrees)
{
	return ((degrees * PI) / 180.0f);
}

float phd_sin(short x);
float phd_cos(short x);
int	  phd_atan(int y, int x);
