#include "framework.h"
#include "tr4_burningfloor.h"
#include "Specific/level.h"
#include "Game/items.h"

void InitializeBurningFloor(short itemNumber)
{
	g_Level.Items[itemNumber].Animation.RequiredState = 127;
}

void BurningFloorControl(short itemNumber)
{

}
