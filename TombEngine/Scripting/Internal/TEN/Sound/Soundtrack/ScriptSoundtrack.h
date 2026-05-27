#pragma once

#include <memory>
#include <string>

namespace sol { class state; }
namespace sol { class table; }

namespace TEN::Scripting::Sound
{
	void RegisterSoundtrackType(sol::state* state, sol::table& soundTable);
}
