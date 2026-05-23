#pragma once

#include <array>
#include <string>

#include "Math/Math.h"
#include "Renderer/RendererEnums.h"

enum class MaterialPropertyType
{
	None,
	Bool,
	Int,
	Float,
	Vec2,
	Vec3,
	Color
};

struct MaterialPropertyDefinition
{
	std::string Name = {};
	int NameHash = 0;
	MaterialPropertyType Type = MaterialPropertyType::None;

	void SetName(const std::string& name);
	bool IsPresent() const;
};

struct MaterialPropertyData
{
	Vector4 DefaultValue = Vector4::Zero;
	Vector4 Value		 = Vector4::Zero;
	Vector4 PrevValue	 = Vector4::Zero;

	void Reset();
	void StoreInterpolationData();
	Vector4 GetInterpolatedValue(MaterialPropertyType type, float alpha) const;
};

struct MaterialData
{
	static constexpr int PropertyCount = 4;

	std::string Name = {};
	int NameHash = 0;
	MaterialShaderType Type = MaterialShaderType::Default;
	std::array<MaterialPropertyData, PropertyCount> Properties = {};

	bool HasNormalMap			= false;
	bool HasHeightMap			= false;
	bool HasAmbientOcclusionMap = false;
	bool HasRoughnessMap		= false;
	bool HasSpecularMap			= false;
	bool HasEmissiveMap			= false;

	void SetName(const std::string& name);
	void ResetProperties();
	void StoreInterpolationData();
	std::array<Vector4, PropertyCount> GetProperties() const;
	std::array<Vector4, PropertyCount> GetInterpolatedProperties(float alpha) const;
	void SetCurrentProperties(const std::array<Vector4, PropertyCount>& properties);
	void SetPrevProperties(const std::array<Vector4, PropertyCount>& properties);
	MaterialPropertyData* GetProperty(int index);
	const MaterialPropertyData* GetProperty(int index) const;
	const MaterialPropertyDefinition* GetPropertyDefinition(int index) const;
	int GetPropertyIndex(const std::string& name) const;
};

using MaterialPropertyDefinitions = std::array<MaterialPropertyDefinition, MaterialData::PropertyCount>;

void ResetMaterialPropertyDefinitions();
void SetMaterialPropertyDefinitions(MaterialShaderType shaderType, const MaterialPropertyDefinitions& definitions);
const MaterialPropertyDefinitions& GetMaterialPropertyDefinitions(MaterialShaderType shaderType);