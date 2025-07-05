#pragma once

#include "Game/Gui.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"

namespace sol { class state; }
using namespace TEN::Gui;

enum GAME_OBJECT_ID : short;

namespace TEN::Scripting
{
	static const auto ROTATION_AXES = std::unordered_map<std::string, RotationFlags>
	{
		{ "X", RotationFlags::INV_ROT_X },
		{ "Y", RotationFlags::INV_ROT_Y },
		{ "Z", RotationFlags::INV_ROT_Z }
	};

	struct InventoryItem
	{
		static void Register(sol::table& lua);

		InventoryItem(const std::string& name, GAME_OBJECT_ID objectID, float yOffset, float scale, const Rotation& rot, RotationFlags rotFlags, int meshBits, ItemOptions menuAction);

		void SetAction(ItemOptions action);

		std::string			 Name	  = {};
		InventoryObjectTypes ObjectID = INV_OBJECT_PISTOLS;

		float YOffset = 0.0f;
		float Scale	  = 1.0f;

		Rotation	  Rot		 = Rotation();
		RotationFlags RotFlags	 = RotationFlags::INV_ROT_X;
		int			  MeshBits	 = 0;
		ItemOptions	  MenuAction = ItemOptions::OPT_USE;
	};
}
