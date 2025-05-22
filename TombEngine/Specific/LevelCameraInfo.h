#pragma once
#include <string>

#include "Math/Math.h"

using namespace TEN::Math;

enum class LevelCameraFlags
{
	None = 0,
	Locked = 1 << 0,
	GlideOut = 1 << 1 // Unused for now but exists in the editor.
};

struct LevelCameraInfo
{
	std::string Name  = {};
	short		Index = -1;

	Vector3i Position	= Vector3i::Zero;
	int		 RoomNumber = 0;
	int		 Flags		= 0;
	int		 Speed		= 1;

	LevelCameraInfo() {}

	LevelCameraInfo(int xPos, int yPos, int zPos)
	{
		Position = Vector3i(xPos, yPos, zPos);
	}

	LevelCameraInfo(int xPos, int yPos, int zPos, short roomNumber)
	{
		Position = Vector3i(xPos, yPos, zPos);
		RoomNumber = roomNumber;
	}

	// Use isFlags to use flag instead of new data.
	LevelCameraInfo(int xPos, int yPos, int zPos, short flags, bool isFlags)
	{
		Position = Vector3i(xPos, yPos, zPos);
		Flags = flags;
	}

	LevelCameraInfo(int xPos, int yPos, int zPos, short roomNumber, short newflags)
	{
		Position = Vector3i(xPos, yPos, zPos);
		RoomNumber = roomNumber;
		Flags = newflags;
	}
};
