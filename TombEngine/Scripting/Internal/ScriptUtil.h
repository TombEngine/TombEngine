#pragma once

#include "Scripting/Internal/ScriptAssert.h"

#define IndexErrorMaker(CPP_TYPE, LUA_CLASS_NAME) [](CPP_TYPE& item, sol::object key) \
{ \
	std::string err = "Attempted to read missing variable \"" + key.as<std::string>() + "\" from " + LUA_CLASS_NAME; \
	ScriptAssert(false, err);\
}

#define NewIndexErrorMaker(CPP_TYPE, LUA_CLASS_NAME) [](CPP_TYPE& item, sol::object key) \
{ \
	std::string err = "Attempted to set missing variable \"" + key.as<std::string>() + "\" of " + LUA_CLASS_NAME; \
	ScriptAssert(false, err);\
}

// Helper type to allow specification of optional parameters.
// Sol currently doesn't have a mechanism to do optional arguments without drawbacks.
// 
//- sol::optional doesn't distinguish between nil values and values of the wrong type,
// meaning the user cannot be provided with a useful error message.
//
// - std::variant provides an error if the user passes an argument of the wrong type,
// but the error unhelpfully exposes C++ code.
//
// - sol::object checks if the user has provided the right type, but takes valuable type information
// away from the function's C++ signature, giving things like void func(sol::object, sol::object, sol::object)
// even if the function's expected parameter types are, for example, float, sol::table, SomeUserType.
// 
// This alias avoids the above problems.
template <typename ... Ts>
using TypeOrNil = std::variant<Ts..., sol::nil_t, sol::object>;

// Used with TypeOrNil to fill arguments with default values if arguments in script not provided.
template <typename T, typename... Ts>
T ValueOr(const std::variant<Ts...>& value, const T& defaultValue)
{
	if (std::holds_alternative<T>(value))
		return std::get<T>(value);

	return defaultValue;
}

template<typename T>
bool IsValidOptional(const TypeOrNil<T>& arg)
{
	return (std::holds_alternative<T>(arg) || std::holds_alternative<sol::nil_t>(arg));
}

sol::table MakeSpecialTableBase(sol::state* state, std::string const& name);

template <typename TFuncIndex, typename TFuncNewIndex>
sol::table MakeSpecialTable(sol::state* state, const std::string& name, const TFuncIndex& funcIndex, const TFuncNewIndex& funcNewIndex)
{
	auto meta = MakeSpecialTableBase(state, name);
	meta.set_function("__index", funcIndex);
	meta.set_function("__newindex", funcNewIndex);
	return (*state)[name];
}

template <typename TFuncIndex, typename TFuncNewIndex, typename TObjPtr>
sol::table MakeSpecialTable(sol::state* state, const std::string& name, const TFuncIndex& funcIndex, const TFuncNewIndex& funcNewIndex, TObjPtr objPtr)
{
	auto meta = MakeSpecialTableBase(state, name);
	meta.set_function("__index", funcIndex, objPtr);
	meta.set_function("__newindex", funcNewIndex, objPtr);
	return (*state)[name];
}
