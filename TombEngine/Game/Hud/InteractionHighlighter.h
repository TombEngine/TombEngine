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

	class InteractionHighlighterController
	{
	private:
		// Members

		bool _isActive;
		Vector3 _position;
		float _fade;
		InteractionType _type;

	public:
		// Utilities

		void Test(ItemInfo& actor, ItemInfo& item);
		void Show(ItemInfo& item, InteractionType type = InteractionType::Undefined);
		void Draw() const;
		void Update();
		void Clear();
	};
}