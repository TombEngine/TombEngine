#pragma once

#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Objects/NamedBase.h"
#include "Specific/Structures/MaterialData.h"

namespace TEN::Scripting::Objects
{
	class Material : public NamedBase<Material, MaterialData&>
	{
	public:
		static void Register(sol::table& parent);

		using IdentifierType = std::reference_wrapper<MaterialData>;

		Material(MaterialData& material);
		Material(const Material& other) = delete;
		~Material() = default;

		sol::object GetProperty(sol::this_state state, const std::string& name) const;
		sol::object GetProperty(sol::this_state state, int index) const;
		void SetProperty(const std::string& name, const sol::object& value);
		void SetProperty(int index, const sol::object& value);
		void ResetProperty(const std::string& name);
		void ResetProperty(int index);
		bool IsPropertyPresent(const std::string& name) const;

		Material& operator=(const Material& other) = delete;

	private:
		MaterialData& _material;
		int GetPropertyIndex(const std::string& name, bool silent = false) const;
		const MaterialPropertyDefinition* GetPropertyDefinition(int index, bool warn = true) const;
	};
}