#pragma once

#include <array>

#include "Game/items.h"

namespace TEN::Multiplayer
{
	constexpr auto MAX_REMOTE_PLAYERS  = 20;
	constexpr auto MAX_REMOTE_VEHICLES = 16;
	constexpr auto MAX_REMOTE_ITEMS	   = MAX_REMOTE_PLAYERS + MAX_REMOTE_VEHICLES;

	// Isolated pool for remote players. Items here are never in level items, room lists, the active or free chains, or savegames.
	// Pool items use renderer indices past the level items; the renderer array already reserves that range. -- divezzi, 20.11.26
	extern std::array<ItemInfo, MAX_REMOTE_ITEMS> g_RemoteItems;
	extern bool									  g_RemoteItemsOccupied[MAX_REMOTE_ITEMS];
	extern int									  g_RemoteItemRendererOffset;

	// Counts the allocations of each slot, so a log line can tell a new item from the previous one in the same slot.
	extern int g_RemoteItemSerial[MAX_REMOTE_ITEMS];

	void	  InitializeRemoteItemPool();
	short	  AllocRemotePlayerItem();
	short	  AllocRemoteVehicleItem();
	void	  FreeRemoteItem(short slotIndex);
	bool	  IsRemoteVehicleSlot(int slotIndex);
	void	  ReleaseRemoteItemScriptName(ItemInfo& item);
	ItemInfo* GetItemByRendererIndex(int rendererIndex);
	ItemInfo* GetPoolItem(int rendererIndex);
}
