#pragma once

#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/TEN/Strings/DisplayString/DisplayString.h"
#include "Scripting/Include/Strings/ScriptInterfaceStringsHandler.h"

using DisplayStringMap = std::unordered_map<DisplayStringID, UserDisplayString>;

class StringsHandler : public LuaHandler, public ScriptInterfaceStringsHandler
{
private:
	DisplayStringMap   m_userDisplayStrings{};
	CallbackDrawString m_callbackDrawSring;

public:
	StringsHandler(sol::state* lua, sol::table& parent);
	void SetCallbackDrawString(CallbackDrawString cb) override;
	void ProcessDisplayStrings(float deltaTime) override;
	void ClearDisplayStrings() override;
	bool SetDisplayString(DisplayStringID id, UserDisplayString const& ds);

	std::optional<std::reference_wrapper<UserDisplayString>> GetDisplayString(DisplayStringID id);
	bool ScheduleRemoveDisplayString(DisplayStringID id);

	void ShowString(DisplayString const&, sol::optional<float> nSeconds, sol::optional<bool> autoDelete);

	bool IsStringDisplaying(DisplayString const& str);

	void Register(sol::state* state);
};
