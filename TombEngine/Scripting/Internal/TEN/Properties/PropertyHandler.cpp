#include "framework.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"

#include "Game/items.h"
#include "Game/StaticMesh.h"
#include "Specific/trutils.h"

using namespace TEN::Utils;

namespace TEN::Scripting::Properties
{
	std::unordered_map<GAME_OBJECT_ID, PropertyMap> PropertyHandler::_moveableProperties = {};
	std::unordered_map<int, PropertyMap> PropertyHandler::_staticProperties   = {};

	PropertyMap& PropertyHandler::GetMoveableProperties(GAME_OBJECT_ID objectID)
	{
		return _moveableProperties[objectID];
	}

	const PropertyMap* PropertyHandler::FindMoveableProperties(GAME_OBJECT_ID objectID)
	{
		auto it = _moveableProperties.find(objectID);
		return (it != _moveableProperties.end() && !it->second.IsEmpty()) ? &it->second : nullptr;
	}

	PropertyMap& PropertyHandler::GetStaticProperties(int slotID)
	{
		return _staticProperties[slotID];
	}

	const PropertyMap* PropertyHandler::FindStaticProperties(int slotID)
	{
		auto it = _staticProperties.find(slotID);
		return (it != _staticProperties.end() && !it->second.IsEmpty()) ? &it->second : nullptr;
	}

	const PropertyValue* PropertyHandler::Get(GAME_OBJECT_ID objectID, const std::string& name)
	{
		return Get(objectID, GetHash(name));
	}

	const PropertyValue* PropertyHandler::Get(GAME_OBJECT_ID objectID, int hash)
	{
		// Look up global type property directly (Layer 1 only, no instance context).
		auto* typeProps = FindMoveableProperties(objectID);
		return typeProps ? typeProps->GetRaw(hash) : nullptr;
	}

	const PropertyValue* PropertyHandler::Get(int staticMeshSlot, const std::string& name)
	{
		return Get(staticMeshSlot, GetHash(name));
	}

	const PropertyValue* PropertyHandler::Get(int staticMeshSlot, int hash)
	{
		// Look up global type property directly (Layer 1 only, no instance context).
		auto* typeProps = FindStaticProperties(staticMeshSlot);
		return typeProps ? typeProps->GetRaw(hash) : nullptr;
	}

	const PropertyValue* PropertyHandler::Get(const ItemInfo& item, const std::string& name)
	{
		return Get(item, GetHash(name));
	}

	const PropertyValue* PropertyHandler::Get(const StaticMesh& staticMesh, const std::string& name)
	{
		return Get(staticMesh, GetHash(name));
	}

	const PropertyValue* PropertyHandler::Get(const ItemInfo& item, int hash)
	{
		// Layer 2: Per-instance override takes priority.
		auto* val = item.Properties.GetRaw(hash);
		if (val != nullptr)
			return val;

		// Layer 1: Fall back to global type property.
		return Get(item.ObjectNumber, hash);
	}

	const PropertyValue* PropertyHandler::Get(const StaticMesh& staticMesh, int hash)
	{
		// Layer 2: Per-instance override takes priority.
		auto* val = staticMesh.Properties.GetRaw(hash);
		if (val != nullptr)
			return val;

		// Layer 1: Fall back to global type property.
		return Get(staticMesh.Slot, hash);
	}

	void PropertyHandler::Clear()
	{
		_moveableProperties.clear();
		_staticProperties.clear();
	}

	const std::unordered_map<GAME_OBJECT_ID, PropertyMap>& PropertyHandler::GetAllMoveableProperties()
	{
		return _moveableProperties;
	}

	const std::unordered_map<int, PropertyMap>& PropertyHandler::GetAllStaticProperties()
	{
		return _staticProperties;
	}

	std::unordered_map<GAME_OBJECT_ID, PropertyMap>& PropertyHandler::GetMutableMoveableProperties()
	{
		return _moveableProperties;
	}

	std::unordered_map<int, PropertyMap>& PropertyHandler::GetMutableStaticProperties()
	{
		return _staticProperties;
	}
}
