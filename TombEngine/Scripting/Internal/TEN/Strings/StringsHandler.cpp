#include "framework.h"
#include "Scripting/Internal/TEN/Strings/StringsHandler.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/TEN/Flow/FlowHandler.h"
#include "Renderer/RendererEnums.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/TEN/Flow/FlowHandler.h"

/***
Display strings.
@tentable Strings 
@pragma nostrip
*/

StringsHandler::StringsHandler(sol::state* lua, sol::table& parent) :
	LuaHandler(lua)
{
	auto table = sol::table(_lua->lua_state(), sol::create);
	parent.set(ScriptReserved_Strings, table);

/***
Show some text on-screen.
@function ShowString
@tparam DisplayString string The string object to draw.
@tparam[opt] float time The time in seconds for which to show the string. If not given, the string will have an "infinite" life, and will show
until @{HideString} is called or until the level is finished.
@tparam[opt=true] bool autoDelete Should be string automatically deleted after timeout is reached. If not given, the string will remain
allocated even after timeout is reached, and can be shown again without re-initialization.
*/
	table.set_function(ScriptReserved_ShowString, &StringsHandler::ShowString, this);

/***
Hide some on-screen text.
@function HideString
@tparam DisplayString string The string object to hide. Must previously have been shown with a call to @{ShowString}, or this function will have no effect.
*/
	table.set_function(ScriptReserved_HideString, [this](const DisplayString& string) { ShowString(string, 0.0f, false); });

/***
Checks if the string is shown
@function IsStringDisplaying
@tparam DisplayString string The string object to be checked.
@treturn bool true if it is shown, false if it is hidden
*/
	table.set_function(ScriptReserved_IsStringDisplaying, &StringsHandler::IsStringDisplaying, this);

	DisplayString::Register(table);
	DisplayString::SetCallbacks(
		[this](auto&& ... param) { return SetDisplayString(std::forward<decltype(param)>(param)...); },
		[this](auto&& ... param) { return ScheduleRemoveDisplayString(std::forward<decltype(param)>(param)...); },
		[this](auto&& ... param) { return GetDisplayString(std::forward<decltype(param)>(param)...); });
	
	MakeReadOnlyTable(table, ScriptReserved_DisplayStringOption, DISPLAY_STRING_OPTION_NAMES);
}

std::optional<std::reference_wrapper<UserDisplayString>> StringsHandler::GetDisplayString(DisplayStringID id)
{
	auto it = m_userDisplayStrings.find(id);
	if (std::cend(m_userDisplayStrings) == it)
		return std::nullopt;

	return std::ref(m_userDisplayStrings.at(id));
}

bool StringsHandler::ScheduleRemoveDisplayString(DisplayStringID id)
{
	auto it = m_userDisplayStrings.find(id);
	if (std::cend(m_userDisplayStrings) == it)
		return false;

	it->second._deleteWhenZero = true;
	return true;
}

void StringsHandler::SetCallbackDrawString(CallbackDrawString cb)
{
	m_callbackDrawSring = cb;
}

bool StringsHandler::SetDisplayString(DisplayStringID id, const UserDisplayString& displayString)
{
	return m_userDisplayStrings.insert_or_assign(id, displayString).second;
}

void StringsHandler::ShowString(const DisplayString& str, sol::optional<float> numSeconds, sol::optional<bool> autoDelete)
{
	auto it = m_userDisplayStrings.find(str.GetID());
	it->second._timeRemaining = numSeconds.value_or(0.0f);
	it->second._isInfinite = !numSeconds.has_value();
	it->second._deleteWhenZero = autoDelete.value_or(true);
}

bool StringsHandler::IsStringDisplaying(const DisplayString& displayString)
{
	auto it = m_userDisplayStrings.find(displayString.GetID());
	bool isAtEndOfLife = (0.0f >= it->second._timeRemaining);
	return (it->second._isInfinite ? isAtEndOfLife : !isAtEndOfLife);
}

void StringsHandler::ProcessDisplayStrings(float deltaTime)
{
	auto it = std::begin(m_userDisplayStrings);
	while (it != std::end(m_userDisplayStrings))
	{
		auto& str = it->second;
		bool endOfLife = 0.0f >= str._timeRemaining;
		if (!str._isInfinite && str._deleteWhenZero && endOfLife)
		{
			ScriptAssertF(!str._isInfinite, "The infinite string {} (key \"{}\") went out of scope without being hidden.", it->first, str._key);
			it = m_userDisplayStrings.erase(it);
		}
		else
		{
			if ((!endOfLife || str._isInfinite) && str._owner == g_GameFlow->CurrentFreezeMode)
			{
				auto cstr = str._isTranslated ? g_GameFlow->GetString(str._key.c_str()) : str._key;
				int flags = 0;

				if (str._flags[(size_t)DisplayStringOptions::Center])
					flags |= (int)PrintStringFlags::Center;

				if (str._flags[(size_t)DisplayStringOptions::Right])
					flags |= (int)PrintStringFlags::Right;

				if (str._flags[(size_t)DisplayStringOptions::Outline])
					flags |= (int)PrintStringFlags::Outline;

				if (str._flags[(size_t)DisplayStringOptions::Blink])
					flags |= (int)PrintStringFlags::Blink;

				m_callbackDrawSring(cstr, str._color, str._position, str._scale, flags);

				str._timeRemaining -= deltaTime;
			}

			++it;
		}
	}
}

void StringsHandler::ClearDisplayStrings()
{
	m_userDisplayStrings.clear();
}
