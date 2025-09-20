#pragma once

namespace sol { class state; };

namespace TEN::Scripting::Input
{
	void Register(sol::state* lua, sol::table& parent);
};
