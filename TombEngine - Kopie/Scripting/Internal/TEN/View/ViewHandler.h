#pragma once

namespace sol { class state; };

namespace TEN::Scripting::View
{
	void Register(sol::state* lua, sol::table& parent);
};
