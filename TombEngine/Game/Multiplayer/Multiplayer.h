#pragma once

struct ItemInfo;
struct lua_State;

namespace TEN::Multiplayer
{
	// True when a script has called Flow.EnableMultiplayer() and the option of the player is on.
	bool IsRemotePlayerEnabled();

	// The option as the player has it now: the saved option after Flow.EnableMultiplayer(true), the choice of this session after false.
	bool GetMultiplayerOption();

	// Called by the Options page after it copied its switch into g_Configuration. With false the choice is kept for this session and the saved option is restored to previousSavedOption.
	void ApplyMultiplayerOption(bool previousSavedOption);

	// Removes the remote players and lets the next require of LuaSocket go through the gate again.
	void OnMultiplayerSwitchedOff();

	// Makes require refuse LuaSocket while multiplayer is not in effect.
	void InstallSocketGate(lua_State* luaStatePtr);

	// Reports a sound of the local player to the level script, which sends it to the other players. Does nothing while multiplayer is off.
	void NotifyLaraSound(const ItemInfo& item, int soundID, int frameNumber);
}
