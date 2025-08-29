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

	enum class InteractiveObjectType
	{
		Generic,
		Door,
		StartPosOnly
	};

	class InteractionHighlighterController
	{
	private:
		// Members

		bool _isActive;
		Vector3 _position;
		float _fade;
		InteractionType _type;

		// Utilities

		bool TestHardcodedSetup(ItemInfo& actor, ItemInfo& item, InteractiveObjectType type);

	public:
		// Utilities

		void Test(ItemInfo& actor, ItemInfo& item, InteractiveObjectType type = InteractiveObjectType::Generic);
		void Show(ItemInfo& item, InteractionType type = InteractionType::Undefined);
		void Draw() const;
		void Update();
		void Clear();
	};
}