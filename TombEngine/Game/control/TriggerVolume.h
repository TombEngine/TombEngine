#pragma once
#include "Game/control/event.h"

namespace TEN::Control::Volumes
{
	enum class VolumeStateStatus
	{
		Outside,
		Entering,
		Inside,
		Leaving
	};

	enum class VolumeType
	{
		Box,
		Sphere,
		Prism // TODO: Unsupported as of now.
	};

	struct VolumeState
	{
		VolumeStateStatus Status	= VolumeStateStatus::Outside;
		Activator		  Activator = nullptr;

		int Timestamp = 0;
	};
}

// TODO: Move into namespace and deal with errors.
struct TriggerVolume
{
	bool Enabled = true;
	bool DetectInAdjacentRooms = false;
	int	 EventSetIndex = 0;

	std::string Name = {};
	TEN::Control::Volumes::VolumeType Type = TEN::Control::Volumes::VolumeType::Box;

	BoundingOrientedBox Box	   = BoundingOrientedBox();
	BoundingSphere		Sphere = BoundingSphere();

	std::vector<TEN::Control::Volumes::VolumeState> StateQueue = {};
};
