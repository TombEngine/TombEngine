#pragma once

namespace sol { class state; };

namespace TEN::Scripting::Sound
{
	void Register(sol::state* lua, sol::table& parent);
};
