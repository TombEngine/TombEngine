#include "framework.h"
#include "Math/Legacy.h"

float phd_sin(short x)
{
	return sin(TO_RAD(x));
}

float phd_cos(short x)
{
	return cos(TO_RAD(x));
}

// NOTE: Order of parameters is inverted!
int phd_atan(int y, int x)
{
	return FROM_RAD(atan2(x, y));
}
