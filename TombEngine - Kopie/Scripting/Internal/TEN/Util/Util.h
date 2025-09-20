#pragma once

namespace sol { class state; };

namespace TEN::Scripting::Util
{
	void Register(sol::state* lua, sol::table& parent);
};
