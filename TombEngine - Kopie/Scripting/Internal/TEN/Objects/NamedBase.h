#pragma once

#include "Scripting/Internal/ScriptAssert.h"

template <typename S> using callbackSetName = std::function<bool(const std::string&, S identifier)>;
using callbackRemoveName = std::function<bool(const std::string&)>;

// Use the "curiously recurring template pattern" to allow classes to inherit static members and functions.
// TLuaObj: Lua object that derives and instantiates this base class.
// TEngineObj: Engine object referenced by the Lua object.
template <typename TLuaObj, class TEngineObj> class NamedBase
{
protected:
	static callbackSetName<TEngineObj> _callbackSetName;
	static callbackRemoveName		   _callbackRemoveName;

public:
	static void SetNameCallbacks(callbackSetName<TEngineObj> cbSetName, callbackRemoveName cbRemoveName)
	{
		_callbackSetName = cbSetName;
		_callbackRemoveName = cbRemoveName;
	}
};

// Default callbacks.
template <typename TLuaObj, typename TEngineObj> callbackSetName<TEngineObj>
NamedBase<TLuaObj, TEngineObj>::_callbackSetName = [](const std::string& name, TEngineObj identifier)
{
	auto err = std::string("\"Set Name\" callback is not set.");
	throw TENScriptException(err);
	return false;
};

// NOTE: Could potentially be called by GameScriptItemInfo destructor and thus cannot throw.
template <typename TLuaObj, typename TEngineObj> callbackRemoveName
NamedBase<TLuaObj, TEngineObj>::_callbackRemoveName = [](const std::string& name)
{
	TENLog("\"Remove Name\" callback is not set.", LogLevel::Error);
	std::terminate();
	return false;
};
