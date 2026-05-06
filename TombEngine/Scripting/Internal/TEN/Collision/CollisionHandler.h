#pragma once

namespace sol { class state; };

namespace TEN::Scripting::Collision
{
	void Register(sol::state* lua, sol::table& parent);
};