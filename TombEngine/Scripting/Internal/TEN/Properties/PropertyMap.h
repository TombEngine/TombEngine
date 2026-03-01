#pragma once

#include "Scripting/Internal/TEN/Properties/PropertyValue.h"
#include "Specific/trutils.h"

namespace TEN::Scripting::Properties
{
	class PropertyMap
	{
	private:
		// Hash-to-value storage for O(1) lookup.
		std::unordered_map<int, PropertyValue> _values = {};

		// Hash-to-name index for iteration and serialization.
		std::unordered_map<int, std::string> _names = {};

	public:
		// Get a typed property by name (hashes internally).
		template <typename T> std::optional<T> Get(const std::string& name) const
		{
			return Get<T>(TEN::Utils::GetHash(name));
		}

		// Get a typed property by pre-computed hash (fast path for C++ hot code).
		// Integral types (except bool) are read from the stored float and cast automatically.
		template <typename T> std::optional<T> Get(int hash) const
		{
			auto it = _values.find(hash);
			if (it == _values.end())
				return std::nullopt;

			return ExtractValue<T>(it->second);
		}

		// Get a typed property by name, returning a default if not found or type mismatch.
		template <typename T> T GetOr(const std::string& name, const T& defaultValue) const
		{
			return GetOr<T>(TEN::Utils::GetHash(name), defaultValue);
		}

		// Get a typed property by hash, returning a default if not found or type mismatch.
		// Integral types (except bool) are read from the stored float and cast automatically.
		template <typename T> T GetOr(int hash, const T& defaultValue) const
		{
			auto it = _values.find(hash);
			if (it == _values.end())
				return defaultValue;

			return ExtractValue<T>(it->second).value_or(defaultValue);
		}

		// Get raw variant by name.
		const PropertyValue* GetRaw(const std::string& name) const;

		// Get raw variant by hash.
		const PropertyValue* GetRaw(int hash) const;

		// Set a property by name. Creates the entry if it doesn't exist.
		void Set(const std::string& name, const PropertyValue& value);

		// Check if a property exists by name.
		bool Has(const std::string& name) const;

		// Check if a property exists by hash.
		bool Has(int hash) const;

		// Remove a property by name. Returns true if removed.
		bool Remove(const std::string& name);

		// Remove a property by hash. Returns true if removed.
		bool Remove(int hash);

		// Clear all properties.
		void Clear();

		// Check if empty.
		bool IsEmpty() const;

		// Get number of properties.
		size_t GetCount() const;

		// Get all property names. Useful for iteration and serialization.
		std::vector<std::string> GetNames() const;

		// Get name string for a given hash (returns nullptr if not found).
		const std::string* GetName(int hash) const;

		// Iteration support over the values map.
		auto begin() const { return _values.begin(); }
		auto end()   const { return _values.end(); }
	};
}
