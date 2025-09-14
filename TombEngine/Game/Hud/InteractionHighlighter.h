#pragma once

struct ItemInfo;
struct ObjectCollisionBounds;

namespace TEN::Hud
{
	enum class InteractionType
	{
		Undefined,
		Use,
		Pickup,
		Talk
	};

	enum class InteractionMode
	{
		Always,		// Can be interacted with at any time.
		Activation,	// Can only be interacted when not activated (e.g. doors or switches).
		Custom		// Specific object types which may need additional checks based on object ID.
	};

	class InteractionHighlighterController
	{
	private:
		// Types

		struct HighlightState
		{
			Vector3 Position = Vector3::Zero;
			InteractionType Type = InteractionType::Undefined;
			float Fade = 0.0f;
		};

		// Members

		bool _isActive = false;
		HighlightState _current = {};
		HighlightState _previous = {};

		// Utilities

		bool TestInteractionConditions(ItemInfo& actor, ItemInfo& item, InteractionMode mode);

	public:
		// Utilities

		void Test(ItemInfo& actor, ItemInfo& item, InteractionMode type = InteractionMode::Always);
		void Draw() const;
		void Update();
		void Clear();
	};
}