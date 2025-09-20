#pragma once

namespace sol { class state; };

namespace TEN::Scripting::Effects
{
	void Register(sol::state* lua, sol::table& parent);
};
