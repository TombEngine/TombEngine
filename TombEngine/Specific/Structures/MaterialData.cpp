#include "framework.h"
#include "Specific/Structures/MaterialData.h"

#include <unordered_map>

#include "Math/Utils.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Math;
using namespace TEN::Utils;

static std::unordered_map<MaterialShaderType, MaterialPropertyDefinitions> MaterialPropertyDefinitionMap = {};

void MaterialPropertyDefinition::SetName(const std::string& name)
{
	Name = name;
	NameHash = GetHash(name);
}

bool MaterialPropertyDefinition::IsPresent() const
{
	return (Type != MaterialPropertyType::None);
}

void MaterialPropertyData::Reset()
{
	Value = PrevValue = DefaultValue;
}

void MaterialPropertyData::StoreInterpolationData()
{
	PrevValue = Value;
}

Vector4 MaterialPropertyData::GetInterpolatedValue(MaterialPropertyType type, float alpha) const
{
	switch (type)
	{
	case MaterialPropertyType::Bool:
		return Vector4((float)std::lround(std::clamp(Lerp(PrevValue.x, Value.x, alpha), 0.0f, 1.0f)), 0.0f, 0.0f, 0.0f);

	case MaterialPropertyType::Int:
		return Vector4((float)std::lround(Lerp(PrevValue.x, Value.x, alpha)), 0.0f, 0.0f, 0.0f);

	case MaterialPropertyType::None:
		return Vector4::Zero;

	default:
		return Vector4::Lerp(PrevValue, Value, alpha);
	}
}

void UpdateMaterials()
{
	for (auto& material : g_Level.Materials)
		material.StoreInterpolationData();
}

void ResetMaterialPropertyDefinitions()
{
	MaterialPropertyDefinitionMap.clear();
}

void SetMaterialPropertyDefinitions(MaterialShaderType shaderType, const MaterialPropertyDefinitions& definitions)
{
	MaterialPropertyDefinitionMap[shaderType] = definitions;
}

const MaterialPropertyDefinitions& GetMaterialPropertyDefinitions(MaterialShaderType shaderType)
{
	auto found = MaterialPropertyDefinitionMap.find(shaderType);
	if (found != MaterialPropertyDefinitionMap.end())
		return found->second;

	static const MaterialPropertyDefinitions EmptyDefinitions = {};
	return EmptyDefinitions;
}

void MaterialData::SetName(const std::string& name)
{
	Name = name;
	NameHash = GetHash(name);
}

void MaterialData::ResetProperties()
{
	for (auto& property : Properties)
		property.Reset();
}

void MaterialData::StoreInterpolationData()
{
	for (auto& property : Properties)
		property.StoreInterpolationData();
}

std::array<Vector4, MaterialData::PropertyCount> MaterialData::GetProperties() const
{
	std::array<Vector4, PropertyCount> result = {};

	for (int i = 0; i < PropertyCount; i++)
		result[i] = Properties[i].Value;

	return result;
}

std::array<Vector4, MaterialData::PropertyCount> MaterialData::GetInterpolatedProperties(float alpha) const
{
	std::array<Vector4, PropertyCount> result = {};

	for (int i = 0; i < PropertyCount; i++)
	{
		auto* definition = GetPropertyDefinition(i);
		result[i] = (definition != nullptr) ? Properties[i].GetInterpolatedValue(definition->Type, alpha) : Vector4::Zero;
	}

	return result;
}

void MaterialData::SetCurrentProperties(const std::array<Vector4, PropertyCount>& properties)
{
	for (int i = 0; i < PropertyCount; i++)
		Properties[i].Value = properties[i];
}

void MaterialData::SetPrevProperties(const std::array<Vector4, PropertyCount>& properties)
{
	for (int i = 0; i < PropertyCount; i++)
		Properties[i].PrevValue = properties[i];
}

MaterialPropertyData* MaterialData::GetProperty(int index)
{
	if (index < 0 || index >= PropertyCount)
		return nullptr;

	return &Properties[index];
}

const MaterialPropertyData* MaterialData::GetProperty(int index) const
{
	if (index < 0 || index >= PropertyCount)
		return nullptr;

	return &Properties[index];
}

const MaterialPropertyDefinition* MaterialData::GetPropertyDefinition(int index) const
{
	if (index < 0 || index >= MaterialData::PropertyCount)
		return nullptr;

	const auto& definitions = GetMaterialPropertyDefinitions(Type);
	return &definitions[index];
}

int MaterialData::GetPropertyIndex(const std::string& name) const
{
	if (name.empty())
		return NO_VALUE;

	auto hash = GetHash(name);
	const auto& definitions = GetMaterialPropertyDefinitions(Type);
	for (int i = 0; i < (int)definitions.size(); i++)
	{
		const auto& definition = definitions[i];
		if (definition.NameHash == hash && definition.Name == name)
			return i;
	}

	return NO_VALUE;
}