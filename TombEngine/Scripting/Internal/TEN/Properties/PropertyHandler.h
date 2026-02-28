#pragma once

#include <unordered_map>

#include "Scripting/Internal/TEN/Properties/PropertyMap.h"

// Forward declarations (avoid circular includes).
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

		// Raw pointer resolution (returns nullptr if not found in either layer).
		static const PropertyValue* Get(const ItemInfo& item, const std::string& name);
		static const PropertyValue* Get(const ItemInfo& item, int hash);
		static const PropertyValue* Get(const StaticMesh& staticMesh, const std::string& name);
		static const PropertyValue* Get(const StaticMesh& staticMesh, int hash);

		// Typed resolution with optional default value fallback.
		// Checks instance first, then type defaults, then returns defaultValue.
		template <typename T>
		static T Get(const ItemInfo& item, const std::string& name, const T& defaultValue = T{})
		{
			return Get<T>(item, GetHash(name), defaultValue);
		}

		template <typename T>
		static T Get(const ItemInfo& item, int hash, const T& defaultValue = T{})
		{
			auto* val = Get(item, hash);
			if (val == nullptr)
				return defaultValue;

			auto* ptr = std::get_if<T>(val);
			return ptr ? *ptr : defaultValue;
		}

		template <typename T>
		static T Get(const StaticMesh& staticMesh, const std::string& name, const T& defaultValue = T{})
		{
			return GetStaticProperty<T>(staticMesh, GetHash(name), defaultValue);
		}

		template <typename T>
		static T Get(const StaticMesh& staticMesh, int hash, const T& defaultValue = T{})
		{
			auto* val = Get(staticMesh, hash);
			if (val == nullptr)
				return defaultValue;

			auto* ptr = std::get_if<T>(val);
			return ptr ? *ptr : defaultValue;
		}

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
