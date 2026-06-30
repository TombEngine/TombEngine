#pragma once

#include <unordered_map>
#include "Scripting/Internal/TEN/Properties/PropertyMap.h"

struct ItemInfo;
struct StaticMesh;

namespace TEN::Scripting::Properties
{
	// Global registry holding type-level properties (Layer 1).
	// Moveable properties are keyed by GAME_OBJECT_ID (int).
	// Static properties are keyed by static slot ID (int).
	class PropertyHandler
	{
	private:
		static std::unordered_map<int, PropertyMap> _moveableProperties;
		static std::unordered_map<int, PropertyMap> _staticProperties;

		// Shared typed extraction from a raw resolution result.
		template <typename T>
		static T ResolveTyped(const PropertyValue* val, const T& defaultValue)
		{
			return val ? ExtractValue<T>(*val).value_or(defaultValue) : defaultValue;
		}

	public:
		// Get or create the property map for a moveable type.
		static PropertyMap& GetMoveableProperties(int objectID);

		// Find the property map for a moveable type (returns nullptr if none).
		static const PropertyMap* FindMoveableProperties(int objectID);

		// Get or create the property map for a static slot.
		static PropertyMap& GetStaticProperties(int slotID);

		// Find the property map for a static slot (returns nullptr if none).
		static const PropertyMap* FindStaticProperties(int slotID);

		// --- Two-layer resolution ---
		// Check instance properties first, then type defaults.
		// ObjectNumber / Slot is derived from the entity reference internally.

		// Raw resolution (returns nullptr if not found in either layer).
		static const PropertyValue* Get(const ItemInfo& item, const std::string& name);
		static const PropertyValue* Get(const ItemInfo& item, int hash);
		static const PropertyValue* Get(const StaticMesh& staticMesh, const std::string& name);
		static const PropertyValue* Get(const StaticMesh& staticMesh, int hash);

		// Typed resolution with optional default value fallback.
		// Checks instance first, then type defaults, then returns defaultValue.
		template <typename T> static T Get(const ItemInfo& item, const std::string& name, const T& defaultValue = T{})         { return ResolveTyped<T>(Get(item, name), defaultValue); }
		template <typename T> static T Get(const ItemInfo& item, int hash, const T& defaultValue = T{})                        { return ResolveTyped<T>(Get(item, hash), defaultValue); }
		template <typename T> static T Get(const StaticMesh& staticMesh, const std::string& name, const T& defaultValue = T{}) { return ResolveTyped<T>(Get(staticMesh, name), defaultValue); }
		template <typename T> static T Get(const StaticMesh& staticMesh, int hash, const T& defaultValue = T{})                { return ResolveTyped<T>(Get(staticMesh, hash), defaultValue); }

		// Pointer overloads (delegate to reference versions).
		static const PropertyValue* Get(const ItemInfo* item, const std::string& name)         { return Get(*item, name); }
		static const PropertyValue* Get(const ItemInfo* item, int hash)                        { return Get(*item, hash); }
		static const PropertyValue* Get(const StaticMesh* staticMesh, const std::string& name) { return Get(*staticMesh, name); }
		static const PropertyValue* Get(const StaticMesh* staticMesh, int hash)                { return Get(*staticMesh, hash); }

		template <typename T> static T Get(const ItemInfo* item, const std::string& name, const T& defaultValue = T{})         { return Get<T>(*item, name, defaultValue); }
		template <typename T> static T Get(const ItemInfo* item, int hash, const T& defaultValue = T{})                        { return Get<T>(*item, hash, defaultValue); }
		template <typename T> static T Get(const StaticMesh* staticMesh, const std::string& name, const T& defaultValue = T{}) { return Get<T>(*staticMesh, name, defaultValue); }
		template <typename T> static T Get(const StaticMesh* staticMesh, int hash, const T& defaultValue = T{})                { return Get<T>(*staticMesh, hash, defaultValue); }

		// --- Type-level only resolution (by object ID) ---
		// Looks up global type properties directly, without any instance context.

		// Raw resolution (returns nullptr if not found).
		static const PropertyValue* Get(int objectID, const std::string& name);
		static const PropertyValue* Get(int objectID, int hash);

		// Typed resolution with optional default value fallback.
		template <typename T> static T Get(int objectID, const std::string& name, const T& defaultValue = T{}) { return ResolveTyped<T>(Get(objectID, name), defaultValue); }
		template <typename T> static T Get(int objectID, int hash, const T& defaultValue = T{})                { return ResolveTyped<T>(Get(objectID, hash), defaultValue); }

		// Clear all type properties (call on level unload).
		static void Clear();

		// Serialization access.
		static const std::unordered_map<int, PropertyMap>& GetAllMoveableProperties();
		static const std::unordered_map<int, PropertyMap>& GetAllStaticProperties();

		// Non-const access for loading from savegame.
		static std::unordered_map<int, PropertyMap>& GetMutableMoveableProperties();
		static std::unordered_map<int, PropertyMap>& GetMutableStaticProperties();
	};
}
