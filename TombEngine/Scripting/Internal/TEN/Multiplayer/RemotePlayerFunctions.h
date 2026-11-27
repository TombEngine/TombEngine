#pragma once

#include "Game/items.h"
#include "Scripting/Internal/ScriptUtil.h"

namespace TEN::Scripting::Multiplayer
{
	void SetRemotePlayerState(ItemInfo& item, const sol::table& stateTable);
	sol::table GetRemotePlayerState(const ItemInfo& item, sol::this_state state);
	sol::table GetLocalPlayerState(const ItemInfo& item, sol::this_state state);
}
