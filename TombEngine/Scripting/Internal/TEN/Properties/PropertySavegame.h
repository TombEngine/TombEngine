#pragma once

#include "Scripting/Internal/TEN/Properties/PropertyMap.h"
#include "Scripting/Internal/TEN/Properties/PropertyValue.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Specific/Serialization/flatbuffers/ten_savegame_generated.h"

namespace TEN::Scripting::Properties
{
	namespace Save = TEN::Serialization::Save;

	// Build helpers (PropertyMap -> FlatBuffers).

	// Serialize a single PropertyValue into a FlatBuffers PropertyValueUnion.
	inline flatbuffers::Offset<void> BuildPropertyValue(flatbuffers::FlatBufferBuilder& fbb, const PropertyValue& value, Save::PropertyValueUnion& outType)
	{
		return std::visit([&](const auto& val) -> flatbuffers::Offset<void>
		{
			using T = std::decay_t<decltype(val)>;

			if constexpr (std::is_same_v<T, bool>)
			{
				outType = Save::PropertyValueUnion::prop_bool;
				return Save::CreatePropertyBool(fbb, val).Union();
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				outType = Save::PropertyValueUnion::prop_float;
				return Save::CreatePropertyFloat(fbb, val).Union();
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				outType = Save::PropertyValueUnion::prop_string;
				auto strOffset = fbb.CreateString(val);
				return Save::CreatePropertyString(fbb, strOffset).Union();
			}
			else if constexpr (std::is_same_v<T, Vec2>)
			{
				outType = Save::PropertyValueUnion::prop_vec2;
				return Save::CreatePropertyVec2(fbb, val.x, val.y).Union();
			}
			else if constexpr (std::is_same_v<T, Vec3>)
			{
				outType = Save::PropertyValueUnion::prop_vec3;
				return Save::CreatePropertyVec3(fbb, val.x, val.y, val.z).Union();
			}
			else if constexpr (std::is_same_v<T, ScriptColor>)
			{
				outType = Save::PropertyValueUnion::prop_color;
				return Save::CreatePropertyColor(fbb, val.GetR(), val.GetG(), val.GetB(), val.GetA()).Union();
			}
			else if constexpr (std::is_same_v<T, Rotation>)
			{
				outType = Save::PropertyValueUnion::prop_rotation;
				return Save::CreatePropertyRotation(fbb, val.x, val.y, val.z).Union();
			}
			else if constexpr (std::is_same_v<T, Time>)
			{
				outType = Save::PropertyValueUnion::prop_time;
				return Save::CreatePropertyTime(fbb, val.GetFrameCount()).Union();
			}
			else
			{
				outType = Save::PropertyValueUnion::NONE;
				return 0;
			}

		}, value);
	}

	// Serialize a PropertyMap into a FlatBuffers PropertyMapData offset.
	inline flatbuffers::Offset<Save::PropertyMapData> BuildPropertyMap(flatbuffers::FlatBufferBuilder& fbb, const PropertyMap& propMap)
	{
		auto entries = std::vector<flatbuffers::Offset<Save::PropertyEntry>>{};
		auto names = propMap.GetNames();

		for (const auto& name : names)
		{
			auto* val = propMap.GetRaw(name);
			if (val == nullptr)
				continue;

			auto nameOffset = fbb.CreateString(name);
			auto valueType = Save::PropertyValueUnion::NONE;
			auto valueOffset = BuildPropertyValue(fbb, *val, valueType);

			if (valueType == Save::PropertyValueUnion::NONE)
				continue;

			Save::PropertyEntryBuilder entry(fbb);
			entry.add_name(nameOffset);
			entry.add_value_type(valueType);
			entry.add_value(valueOffset);
			entries.push_back(entry.Finish());
		}

		auto entriesOffset = fbb.CreateVector(entries);
		return Save::CreatePropertyMapData(fbb, entriesOffset);
	}

	// Serialize global type properties into FlatBuffers vectors.
	inline flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Save::TypePropertyMap>>>
	BuildTypeProperties(flatbuffers::FlatBufferBuilder& fbb, const std::unordered_map<int, PropertyMap>& typeProps)
	{
		auto entries = std::vector<flatbuffers::Offset<Save::TypePropertyMap>>{};

		for (const auto& [typeID, propMap] : typeProps)
		{
			if (propMap.IsEmpty())
				continue;

			auto propsOffset = BuildPropertyMap(fbb, propMap);

			Save::TypePropertyMapBuilder tpm(fbb);
			tpm.add_type_id(typeID);
			tpm.add_properties(propsOffset);
			entries.push_back(tpm.Finish());
		}

		return fbb.CreateVector(entries);
	}

	// Parse helpers (FlatBuffers -> PropertyMap).

	// Deserialize a single FlatBuffers PropertyEntry into a PropertyValue.
	inline std::optional<PropertyValue> ParsePropertyEntry(const Save::PropertyEntry* entry)
	{
		if (entry == nullptr || entry->value() == nullptr)
			return std::nullopt;

		switch (entry->value_type())
		{
		case Save::PropertyValueUnion::prop_bool:
		{
			auto* v = static_cast<const Save::PropertyBool*>(entry->value());
			return PropertyValue(v->value());
		}
		case Save::PropertyValueUnion::prop_float:
		{
			auto* v = static_cast<const Save::PropertyFloat*>(entry->value());
			return PropertyValue(v->value());
		}
		case Save::PropertyValueUnion::prop_string:
		{
			auto* v = static_cast<const Save::PropertyString*>(entry->value());
			return PropertyValue(std::string(v->value() ? v->value()->str() : ""));
		}
		case Save::PropertyValueUnion::prop_vec2:
		{
			auto* v = static_cast<const Save::PropertyVec2*>(entry->value());
			return PropertyValue(Vec2(v->x(), v->y()));
		}
		case Save::PropertyValueUnion::prop_vec3:
		{
			auto* v = static_cast<const Save::PropertyVec3*>(entry->value());
			return PropertyValue(Vec3(v->x(), v->y(), v->z()));
		}
		case Save::PropertyValueUnion::prop_color:
		{
			auto* v = static_cast<const Save::PropertyColor*>(entry->value());
			return PropertyValue(ScriptColor(v->r(), v->g(), v->b(), v->a()));
		}
		case Save::PropertyValueUnion::prop_rotation:
		{
			auto* v = static_cast<const Save::PropertyRotation*>(entry->value());
			return PropertyValue(Rotation(v->x(), v->y(), v->z()));
		}
		case Save::PropertyValueUnion::prop_time:
		{
			auto* v = static_cast<const Save::PropertyTime*>(entry->value());
			return PropertyValue(Time(v->frame_count()));
		}
		default:
			return std::nullopt;
		}
	}

	// Deserialize a FlatBuffers PropertyMapData into a PropertyMap.
	inline void ParsePropertyMap(const Save::PropertyMapData* data, PropertyMap& outMap)
	{
		outMap.Clear();

		if (data == nullptr || data->entries() == nullptr)
			return;

		for (unsigned int i = 0; i < data->entries()->size(); i++)
		{
			auto* entry = data->entries()->Get(i);
			if (entry == nullptr || entry->name() == nullptr)
				continue;

			auto val = ParsePropertyEntry(entry);
			if (val.has_value())
				outMap.Set(entry->name()->str(), val.value());
		}
	}

	// Deserialize type properties from a FlatBuffers vector.
	inline void ParseTypeProperties(const flatbuffers::Vector<flatbuffers::Offset<Save::TypePropertyMap>>* typePropsVec, std::unordered_map<int, PropertyMap>& outMap)
	{
		outMap.clear();

		if (typePropsVec == nullptr)
			return;

		for (unsigned int i = 0; i < typePropsVec->size(); i++)
		{
			auto* entry = typePropsVec->Get(i);
			if (entry == nullptr)
				continue;

			auto& propMap = outMap[entry->type_id()];
			ParsePropertyMap(entry->properties(), propMap);
		}
	}
}
