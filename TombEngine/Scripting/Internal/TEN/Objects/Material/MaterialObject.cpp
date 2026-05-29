#include "framework.h"
#include "Scripting/Internal/TEN/Objects/Material/MaterialObject.h"

#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Properties/PropertyLuaConverters.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Specific/trutils.h"

using namespace TEN::Scripting::Types;
using namespace TEN::Utils;
using namespace TEN::Scripting::Properties;

namespace TEN::Scripting::Objects
{
	/// Represents a material instance for the texture. To be used with @{Objects.GetMaterialByName}. Materials are managed in the Tomb Editor's material window.
	// This class is not in any way related to @{Collision.MaterialType}.
	// @tenclass Objects.Material
	// @pragma nostrip

	static auto IndexError = IndexErrorMaker(Material, ScriptReserved_Material);
	static auto NewIndexError = NewIndexErrorMaker(Material, ScriptReserved_Material);

	static const std::string GetPropertyTypeName(MaterialPropertyType type)
	{
		switch (type)
		{
		case MaterialPropertyType::Bool:
			return "bool";

		case MaterialPropertyType::Int:
			return "int";

		case MaterialPropertyType::Float:
			return "float";

		case MaterialPropertyType::Vec2:
			return "Vec2";

		case MaterialPropertyType::Vec3:
			return "Vec3";

		case MaterialPropertyType::Color:
			return "Color";

		case MaterialPropertyType::None:
		default:
			return "none";
		}
	}

	static void WarnMissingProperty(const MaterialData& material, const std::string& propertyName)
	{
		TENLog(fmt::format("Objects.Material '{}' does not contain property '{}'.", material.Name, propertyName), LogLevel::Warning);
	}

	static void WarnInvalidPropertyIndex(const MaterialData& material, int index)
	{
		TENLog(fmt::format("Objects.Material '{}' property index {} is out of range [0, {}].", material.Name, index, MaterialData::PropertyCount - 1), LogLevel::Warning);
	}

	static void WarnTypeMismatch(const MaterialData& material, const MaterialPropertyDefinition& property)
	{
		TENLog(fmt::format("Objects.Material '{}' property '{}' expects {}.", material.Name, property.Name, GetPropertyTypeName(property.Type)), LogLevel::Warning);
	}

	static void WarnOverflow(const MaterialData& material, const MaterialPropertyDefinition& property)
	{
		TENLog(fmt::format("Objects.Material '{}' property '{}' value is out of range for {}.", material.Name, property.Name, GetPropertyTypeName(property.Type)), LogLevel::Warning);
	}

	static std::optional<PropertyValue> GetPropertyValue(const MaterialPropertyDefinition& definition, const MaterialPropertyData& property)
	{
		switch (definition.Type)
		{
		case MaterialPropertyType::Bool:
			return property.Value.x != 0.0f;

		case MaterialPropertyType::Int:
			return (float)(int)property.Value.x;

		case MaterialPropertyType::Float:
			return property.Value.x;

		case MaterialPropertyType::Vec2:
			return Vec2(property.Value.x, property.Value.y);

		case MaterialPropertyType::Vec3:
			return Vec3(property.Value.x, property.Value.y, property.Value.z);

		case MaterialPropertyType::Color:
			return ScriptColor(property.Value);

		case MaterialPropertyType::None:
		default:
			return std::nullopt;
		}
	}

	static bool TryConvertPropertyValue(const MaterialData& material, const MaterialPropertyDefinition& property, const sol::object& luaValue, const PropertyValue& value, Vector4& outValue)
	{
		switch (property.Type)
		{
		case MaterialPropertyType::Bool:
			if (auto convertedValue = ExtractValue<bool>(value); convertedValue.has_value())
			{
				outValue = Vector4(*convertedValue ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
				return true;
			}
			else
			{
				WarnTypeMismatch(material, property);
				return false;
			}

		case MaterialPropertyType::Int:
		case MaterialPropertyType::Float:
			if (luaValue.get_type() != sol::type::number)
			{
				WarnTypeMismatch(material, property);
				return false;
			}
			else
			{
				double number = luaValue.as<double>();
				if (!std::isfinite(number))
				{
					WarnTypeMismatch(material, property);
					return false;
				}

				auto lowerLimit = property.Type == MaterialPropertyType::Int ? (double)INT_MIN : -(double)FLT_MAX;
				auto upperLimit = property.Type == MaterialPropertyType::Int ? (double)INT_MAX : (double)FLT_MAX;

				if (number < lowerLimit || number > upperLimit)
				{
					WarnOverflow(material, property);
					return false;
				}

				if (property.Type == MaterialPropertyType::Int)
					number = std::round(number);

				outValue = Vector4((float)number, 0.0f, 0.0f, 0.0f);
				return true;
			}

		case MaterialPropertyType::Vec2:
			if (auto vector = ExtractValue<Vec2>(value); vector.has_value())
			{
				outValue = Vector4(vector->x, vector->y, 0.0f, 0.0f);
				return true;
			}
			else
			{
				WarnTypeMismatch(material, property);
				return false;
			}

		case MaterialPropertyType::Vec3:
			if (auto vector = ExtractValue<Vec3>(value); vector.has_value())
			{
				outValue = Vector4(vector->x, vector->y, vector->z, 0.0f);
				return true;
			}
			else
			{
				WarnTypeMismatch(material, property);
				return false;
			}

		case MaterialPropertyType::Color:
			if (auto color = ExtractValue<ScriptColor>(value); color.has_value())
			{
				outValue = Vector4(*color);
				return true;
			}
			else
			{
				WarnTypeMismatch(material, property);
				return false;
			}

		case MaterialPropertyType::None:
		default:
			return false;
		}
	}

	Material::Material(MaterialData& material) : _material(material) { }

	int Material::GetPropertyIndex(const std::string& name, bool silent) const
	{
		auto index = _material.GetPropertyIndex(name);
		if (index > NO_VALUE)
			return index;

		if (!silent)
			WarnMissingProperty(_material, name);

		return NO_VALUE;
	}

	const MaterialPropertyDefinition* Material::GetPropertyDefinition(int index, bool warn) const
	{
		auto* definition = _material.GetPropertyDefinition(index);
		if (definition == nullptr)
		{
			if (warn)
				WarnInvalidPropertyIndex(_material, index);

			return nullptr;
		}

		if (!definition->IsPresent())
		{
			if (warn)
				WarnInvalidPropertyIndex(_material, index);

			return nullptr;
		}

		return definition;
	}

	void Material::Register(sol::table& parent)
	{
		parent.new_usertype<Material>(
			ScriptReserved_Material,
			sol::no_constructor,
			sol::meta_function::index, IndexError,
			sol::meta_function::new_index, NewIndexError,
			ScriptReserved_GetProperty,
			sol::overload(
				static_cast<sol::object (Material::*)(sol::this_state, const std::string&) const>(&Material::GetProperty),
				static_cast<sol::object (Material::*)(sol::this_state, int) const>(&Material::GetProperty)),
			ScriptReserved_SetProperty,
			sol::overload(
				static_cast<void (Material::*)(const std::string&, const sol::object&)>(&Material::SetProperty),
				static_cast<void (Material::*)(int, const sol::object&)>(&Material::SetProperty)),
			ScriptReserved_ResetProperty,
			sol::overload(
				static_cast<void (Material::*)(const std::string&)>(&Material::ResetProperty),
				static_cast<void (Material::*)(int)>(&Material::ResetProperty)),
			ScriptReserved_IsPropertyPresent, &Material::IsPropertyPresent);
	}

	/// Get a material property value.
	// Property can be addressed by its name or zero-based slot index.
	// @function Material:GetProperty
	// @tparam string name Property name.
	// @treturn any Property value, or nil if property is not present. Return type depends on property definition and can be bool, number, @{Vec2}, @{Vec3}, or @{Color}.
	sol::object Material::GetProperty(sol::this_state state, const std::string& name) const
	{
		auto index = GetPropertyIndex(name);
		if (index <= NO_VALUE)
			return sol::nil;

		return GetProperty(state, index);
	}

	sol::object Material::GetProperty(sol::this_state state, int index) const
	{
		auto* definition = GetPropertyDefinition(index);
		if (definition == nullptr)
			return sol::nil;

		auto* property = _material.GetProperty(index);
		if (property == nullptr)
		{
			WarnInvalidPropertyIndex(_material, index);
			return sol::nil;
		}

		auto value = GetPropertyValue(*definition, *property);
		if (!value.has_value())
			return sol::nil;

		return PropertyValueToLua(sol::state_view(state), *value);
	}

	/// Set a material property value.
	// Property can be addressed by its name or zero-based slot index.
	// @function Material:SetProperty
	// @tparam string name Property name.
	// @tparam any value Value matching the property's declared type: bool, number, @{Vec2}, @{Vec3}, or @{Color}. Provided value type must match the property type.
	void Material::SetProperty(const std::string& name, const sol::object& value)
	{
		auto index = GetPropertyIndex(name);
		if (index <= NO_VALUE)
			return;

		SetProperty(index, value);
	}

	void Material::SetProperty(int index, const sol::object& value)
	{
		auto* definition = GetPropertyDefinition(index);
		if (definition == nullptr)
			return;

		auto* property = _material.GetProperty(index);
		if (property == nullptr)
		{
			WarnInvalidPropertyIndex(_material, index);
			return;
		}

		if (value == sol::nil)
		{
			WarnTypeMismatch(_material, *definition);
			return;
		}

		if (value.get_type() == sol::type::userdata && !value.is<Vec2>() && !value.is<Vec3>() && !value.is<ScriptColor>())
		{
			WarnTypeMismatch(_material, *definition);
			return;
		}

		auto propertyValue = PropertyValueFromLua(value);
		if (!propertyValue.has_value())
		{
			WarnTypeMismatch(_material, *definition);
			return;
		}

		auto converted = Vector4::Zero;
		if (TryConvertPropertyValue(_material, *definition, value, *propertyValue, converted))
			property->Value = converted;
	}

	/// Reset a material property to its default value.
	// Property can be addressed by its name or zero-based slot index.
	// @function Material:ResetProperty
	// @tparam string name Property name.
	void Material::ResetProperty(const std::string& name)
	{
		auto index = GetPropertyIndex(name);
		if (index <= NO_VALUE)
			return;

		ResetProperty(index);
	}

	void Material::ResetProperty(int index)
	{
		if (GetPropertyDefinition(index) == nullptr)
			return;

		auto* property = _material.GetProperty(index);
		if (property == nullptr)
		{
			WarnInvalidPropertyIndex(_material, index);
			return;
		}

		property->Reset();
	}

	/// Check if this material type has a property with the given name.
	// @function Material:IsPropertyPresent
	// @tparam string name Property name.
	// @treturn bool True if the property with a given name exists for this material.
	bool Material::IsPropertyPresent(const std::string& name) const
	{
		if (name.empty())
		{
			WarnMissingProperty(_material, name);
			return false;
		}

		auto index = GetPropertyIndex(name);
		return (index > NO_VALUE);
	}
}