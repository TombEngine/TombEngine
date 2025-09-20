#include "framework.h"
#include "Scripting/Internal/ScriptUtil.h"

sol::table MakeSpecialTableBase(sol::state* state, const std::string& name)
{
	std::string metaName{ name + "Meta" };
	auto meta = sol::table{ *state, sol::create };
	state->set(metaName, meta);

	meta.set("__metatable", "\"metatable is protected\"");
	state->safe_script("rawset(_G, \"" + name + "\", {})");

	auto tab = (*state)[name];
	tab[sol::metatable_key] = meta;
	state->set(metaName, sol::nil);

	return meta;
}
