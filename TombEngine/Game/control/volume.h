#pragma once
#include "Game/control/event.h"
#include "Game/room.h"
#include "Game/Setup.h"
#include "Renderer/Renderer.h"

struct CollisionSetupData;

namespace TEN::Control::Volumes
{
	constexpr auto VOLUME_BUSY_TIMEOUT	= 10;
	constexpr auto VOLUME_LEAVE_TIMEOUT = 5;

	constexpr auto VOLUME_STATE_QUEUE_SIZE = 16;

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

	void TestVolumes(int roomNumber, const BoundingOrientedBox& box, ActivatorFlags activatorFlag, Activator activator);
	void TestVolumes(int itemNumber, const CollisionSetupData* coll = nullptr);
	void TestVolumes(int roomNumber, StaticMesh* mesh);
	void TestVolumes(CAMERA_INFO* camera);

	bool HandleEvent(Event& event, Activator& activator);
	bool HandleEvent(const std::string& name, EventType eventType, Activator activator);
	void HandleAllGlobalEvents(EventType type, Activator& activator);
	bool SetEventState(const std::string& name, EventType eventType, bool enabled);
	void InitializeNodeScripts();
}

// TODO: Move into namespace and deal with errors.
struct TriggerVolume
{
	bool Enabled = true;
	bool DetectInAdjacentRooms = false;
	int	 EventSetIndex = 0;

	std::string Name = {};
	VolumeType	Type = VolumeType::Box;

	BoundingOrientedBox Box	   = BoundingOrientedBox();
	BoundingSphere		Sphere = BoundingSphere();

	std::vector<VolumeState> StateQueue = {};
};