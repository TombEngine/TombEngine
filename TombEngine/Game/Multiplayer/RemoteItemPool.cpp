#include "framework.h"
#include "Game/Multiplayer/RemoteItemPool.h"

#include <algorithm>

#include "Game/debug/debug.h"
#include "Game/Multiplayer/Multiplayer.h"
#include "Scripting/Include/Objects/ScriptInterfaceObjectsHandler.h"
#include "Specific/level.h"

namespace TEN::Multiplayer
{
	std::array<ItemInfo, MAX_REMOTE_ITEMS> g_RemoteItems;
	bool								   g_RemoteItemsOccupied[MAX_REMOTE_ITEMS] = {};
	int									   g_RemoteItemRendererOffset = 0;
	int									   g_RemoteItemSerial[MAX_REMOTE_ITEMS] = {};

	void InitializeRemoteItemPool()
	{
		// Set at level load to the level item count.
		g_RemoteItemRendererOffset = (int)g_Level.Items.size();
		std::fill(std::begin(g_RemoteItemsOccupied), std::end(g_RemoteItemsOccupied), false);
	}

	short AllocRemotePlayerItem()
	{
		if (!IsRemotePlayerEnabled())
		{
			TENLog("[REMOTE] Cannot allocate a remote player: multiplayer is disabled.", LogLevel::Warning);
			return NO_VALUE;
		}

		for (short i = 0; i < MAX_REMOTE_PLAYERS; i++)
		{
			if (!g_RemoteItemsOccupied[i])
			{
				g_RemoteItemsOccupied[i] = true;
				g_RemoteItems[i] = ItemInfo{};
				g_RemoteItems[i].Index = g_RemoteItemRendererOffset + i;
				g_RemoteItemSerial[i]++;
				return i;
			}
		}

		TENLog("[REMOTE] All " + std::to_string(MAX_REMOTE_PLAYERS) + " remote player slots are occupied.", LogLevel::Error);
		return NO_VALUE;
	}

	short AllocRemoteVehicleItem()
	{
		if (!IsRemotePlayerEnabled())
		{
			TENLog("[REMVEH] Cannot allocate a remote vehicle: multiplayer is disabled.", LogLevel::Warning);
			return NO_VALUE;
		}

		for (short i = MAX_REMOTE_PLAYERS; i < MAX_REMOTE_ITEMS; i++)
		{
			if (!g_RemoteItemsOccupied[i])
			{
				g_RemoteItemsOccupied[i] = true;
				g_RemoteItems[i] = ItemInfo{};
				g_RemoteItems[i].Index = g_RemoteItemRendererOffset + i;
				g_RemoteItemSerial[i]++;
				return i;
			}
		}

		TENLog("[REMVEH] All " + std::to_string(MAX_REMOTE_VEHICLES) + " remote vehicle slots are occupied.", LogLevel::Error);
		return NO_VALUE;
	}

	void FreeRemoteItem(short slotIndex)
	{
		if (slotIndex >= 0 && slotIndex < MAX_REMOTE_ITEMS)
			g_RemoteItemsOccupied[slotIndex] = false;
	}

	bool IsRemoteVehicleSlot(int slotIndex)
	{
		return (slotIndex >= MAX_REMOTE_PLAYERS && slotIndex < MAX_REMOTE_ITEMS);
	}

	// Invalidates the Lua handles of a pooled item and frees its script name, so the name can be created again.
	// The name is freed only when the name map points at this item: a refused duplicate create also ends up here, and the name then belongs to another item. -- divezzi, 21.11.26
	void ReleaseRemoteItemScriptName(ItemInfo& item)
	{
		if (g_GameScriptEntities == nullptr)
			return;

		g_GameScriptEntities->NotifyKilled(&item);

		if (item.Name.empty() || g_GameScriptEntities->GetIndexByName(item.Name) != item.Index)
			return;

		g_GameScriptEntities->RemoveName(item.Name);
	}

	// Returns the level item or the pooled item behind a renderer index, or nullptr.
	ItemInfo* GetItemByRendererIndex(int rendererIndex)
	{
		if (rendererIndex < 0)
			return nullptr;

		if (rendererIndex < (int)g_Level.Items.size())
			return &g_Level.Items[rendererIndex];

		int slot = rendererIndex - g_RemoteItemRendererOffset;
		if (slot >= 0 && slot < MAX_REMOTE_ITEMS && g_RemoteItemsOccupied[slot])
			return &g_RemoteItems[slot];

		return nullptr;
	}

	// Returns the pool slot behind a renderer index past the level items, whether it is occupied or not, or nullptr.
	ItemInfo* GetPoolItem(int rendererIndex)
	{
		if (g_Level.Items.empty() || rendererIndex < (int)g_Level.Items.size())
			return nullptr;

		int slot = rendererIndex - g_RemoteItemRendererOffset;
		if (slot < 0 || slot >= MAX_REMOTE_ITEMS)
			return nullptr;

		return &g_RemoteItems[slot];
	}
}
