#include "framework.h"
#include "Scripting/Internal/TEN/Properties/PropertyMap.h"

using namespace TEN::Utils;

namespace TEN::Scripting::Properties
{
	const PropertyValue* PropertyMap::GetRaw(const std::string& name) const
	{
		return GetRaw(GetHash(name));
	}

	const PropertyValue* PropertyMap::GetRaw(int hash) const
	{
		auto it = _values.find(hash);
		return (it != _values.end()) ? &it->second : nullptr;
	}

	void PropertyMap::Set(const std::string& name, const PropertyValue& value)
	{
		auto hash = GetHash(name);
		_values[hash] = value;
		_names[hash] = name;
	}

	void PropertyMap::Set(int hash, const PropertyValue& value)
	{
		auto hash = GetRaw(hash);
		_values[hash] = value;
		_names[hash] = hash;
	}

	bool PropertyMap::Has(const std::string& name) const
	{
		return Has(GetHash(name));
	}

	bool PropertyMap::Has(int hash) const
	{
		return _values.find(hash) != _values.end();
	}

	bool PropertyMap::Remove(const std::string& name)
	{
		return Remove(GetHash(name));
	}

	bool PropertyMap::Remove(int hash)
	{
		_names.erase(hash);
		return _values.erase(hash) > 0;
	}

	void PropertyMap::Clear()
	{
		_values.clear();
		_names.clear();
	}

	bool PropertyMap::IsEmpty() const
	{
		return _values.empty();
	}

	size_t PropertyMap::GetCount() const
	{
		return _values.size();
	}

	std::vector<std::string> PropertyMap::GetNames() const
	{
		auto names = std::vector<std::string>{};
		names.reserve(_names.size());

		for (const auto& [hash, name] : _names)
			names.push_back(name);

		return names;
	}

	const std::string* PropertyMap::GetName(int hash) const
	{
		auto it = _names.find(hash);
		return (it != _names.end()) ? &it->second : nullptr;
	}
}
