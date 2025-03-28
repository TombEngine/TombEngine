#pragma once
#include <unordered_map>
#include <unordered_set>

#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Include/Objects/ScriptInterfaceObjectsHandler.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"
#include "Scripting/Internal/TEN/Objects/Static/StaticObject.h"
#include "Scripting/Internal/TEN/Objects/AIObject/AIObject.h"

class ObjectsHandler : public ScriptInterfaceObjectsHandler
{
public:
	ObjectsHandler::ObjectsHandler(sol::state* lua, sol::table& parent);

	bool NotifyKilled(ItemInfo* key) override;
	bool AddMoveableToMap(ItemInfo* key, Moveable* mov);
	bool RemoveMoveableFromMap(ItemInfo* key, Moveable* mov);

	bool TryAddColliding(int id) override
	{
		const auto& item = g_Level.Items[id];

		bool hasName = !(item.Callbacks.OnObjectCollided.empty() && item.Callbacks.OnRoomCollided.empty());
		if (hasName && item.Collidable && item.Status != ITEM_INVISIBLE)
			return _collidingItems.insert(id).second;

		return false;
	}

	bool TryRemoveColliding(int id, bool force = false) override
	{
		const auto& item = g_Level.Items[id];

		bool hasName = !(item.Callbacks.OnObjectCollided.empty() && item.Callbacks.OnRoomCollided.empty());
		if (!force && hasName && item.Collidable && item.Status != ITEM_INVISIBLE)
			return false;

		return _collidingItemsToRemove.insert(id).second;
	}

	void TestCollidingObjects() override;

private:
	LuaHandler _handler;

	// Map between Lua moveables and engine moveables. Needed so that when something is killed,
	// TEN can notify all corresponding Lua variables to become invalid.

	std::unordered_map<ItemInfo*, std::unordered_set<Moveable*>> _moveables	   = {};
	std::unordered_map<std::string, VarMapVal>					 _nameMap	   = {};
	std::unordered_map<std::string, int>	 					 _itemsMapName = {};

	// Map of moveables that are visible, collidable, and have Lua OnCollide callbacks.

	std::unordered_set<int> _collidingItems			= {};
	std::unordered_set<int> _collidingItemsToRemove = {};
	sol::table				_table_objects			= {};

	void AssignPlayer() override;

	template <typename R, const char* S>
	std::unique_ptr<R> GetByName(const std::string& name)
	{
		if (!ScriptAssertF(_nameMap.find(name) != _nameMap.end(), "{} name not found: {}", S, name))
			return nullptr;

		return std::make_unique<R>(std::get<R::IdentifierType>(_nameMap.at(name)));
	}

	template <typename R>
	std::vector <std::unique_ptr<R>> GetMoveablesBySlot(GAME_OBJECT_ID objectID)
	{
		auto movs = std::vector<std::unique_ptr<R>>{};
		for (const auto& [key, val] : _nameMap)
		{
			if (!std::holds_alternative<int>(val))
				continue;

			const auto& item = g_Level.Items[GetIndexByName(key)];
			if (objectID == item.ObjectNumber)
				movs.push_back(GetByName<Moveable, ScriptReserved_Moveable>(key));
		}

		return movs;
	}

	template <typename R>
	std::vector <std::unique_ptr<R>> GetStaticsBySlot(int slot)
	{
		auto items = std::vector<std::unique_ptr<R>>{};
		for (const auto& [key, value] : _nameMap)
		{
			if (!std::holds_alternative<std::reference_wrapper<MESH_INFO>>(value))
				continue;
			
			auto staticObj = std::get<std::reference_wrapper<MESH_INFO>>(value).get();

			if (staticObj.staticNumber == slot)
				items.push_back(GetByName<Static, ScriptReserved_Static>(key));
		}

		return items;
	}

	template <typename R>
	std::vector <std::unique_ptr<R>> GetRoomsByTag(std::string tag)
	{
		auto rooms = std::vector<std::unique_ptr<R>>{};
		for (const auto& [key, value] : _nameMap)
		{
			if (!std::holds_alternative<std::reference_wrapper<RoomData>>(value))
				continue;

			auto room = std::get<std::reference_wrapper<RoomData>>(value).get();
			
			if (std::any_of(room.Tags.begin(), room.Tags.end(), [&tag](const std::string& value) { return value == tag; }))
			{
				rooms.push_back(GetByName<Room, ScriptReserved_Room>(key));
			}
		}

		return rooms;
	}

	int GetIndexByName(std::string const& name) const override
	{
		return std::get<int>(_nameMap.at(name));
	}

	bool AddName(const std::string& key, VarMapVal val) override
	{
		if (key.empty())
			return false;

		auto p = std::pair<const std::string&, VarMapVal>(key, val);
		return _nameMap.insert(p).second;
	}

	bool RemoveName(const std::string& key)
	{
		return _nameMap.erase(key);
	}

	void FreeEntities() override
	{
		_nameMap.clear();
		_collidingItemsToRemove.clear();
		_collidingItems.clear();
	}
};
