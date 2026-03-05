#pragma once
#include "Game/control/volume_types.h"
#include "Game/room.h"
#include "Game/Setup.h"
#include "Renderer/Renderer.h"

struct CollisionSetupData;

namespace TEN::Control::Volumes
{
	void TestVolumes(int roomNumber, const BoundingOrientedBox& box, ActivatorFlags activatorFlag, Activator activator);
	void TestVolumes(int itemNumber, const CollisionSetupData* coll = nullptr);
	void TestVolumes(int roomNumber, StaticMesh* mesh);
	void TestVolumes(CAMERA_INFO* camera);

	bool HandleEvent(Event& event, Activator& activator);
	bool HandleEvent(const std::string& name, EventType eventType, Activator activator);
	void HandleAllGlobalEvents(EventType type, Activator activator);
	bool SetEventState(const std::string& name, EventType eventType, bool enabled);
	void InitializeNodeScripts();
}
