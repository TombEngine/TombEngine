#pragma once
#include <variant>

struct CAMERA_INFO;
struct StaticMesh;

namespace TEN::Control::Volumes
{
	using Activator = std::variant<
		std::nullptr_t,
		int,
		StaticMesh*,
		CAMERA_INFO*>;

	enum class ActivatorFlags
	{
		None = (0 << 0),
		Player = (1 << 0),
		NPC = (1 << 1),
		Moveable = (1 << 2),
		Static = (1 << 3),
		Flyby = (1 << 4),
		PhysicalObject = (1 << 5) // TODO: Future-proofing for Bullet.
	};

	enum class EventMode
	{
		LevelScript,
		Nodes
	};

	enum class EventType
	{
		Enter,
		Inside,
		Leave,
		Loop,
		Load,
		Save,
		Start,
		End,
		UseItem,
		Freeze,

		Count
	};

	struct Event
	{
		EventMode Mode = EventMode::LevelScript;
		std::string		Function = {};
		std::string		Data = {};

		bool Enabled = true;
		int  CallCounter = NO_VALUE;
	};

	struct EventSet
	{
		std::string	Name = {};
		ActivatorFlags Activators = ActivatorFlags::None;
		std::array<Event, int(EventType::Count)> Events = {};
	};
};
