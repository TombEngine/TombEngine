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
		// Members

		bool _isActive			= false;
		bool _checkDirection	= false;

		Vector3 _position		= {};
		float _fade				= 0.0f;

		InteractionType _type	= InteractionType::Undefined;

		// Utilities

		bool TestInteractionConditions(ItemInfo& actor, ItemInfo& item, InteractionMode mode);

	public:
		// Utilities

		void Test(ItemInfo& actor, ItemInfo& item, InteractionMode type = InteractionMode::Always);
		void SetAttributes(ItemInfo& item, InteractionType type = InteractionType::Undefined);
		void Draw() const;
		void Update();
		void Clear();
	};
}