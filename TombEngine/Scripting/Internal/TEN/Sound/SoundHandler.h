#pragma once

#include <sol/sol.hpp>

#include "Sound/sound.h"

namespace TEN::Scripting::Sound
{
	void Register(sol::state* lua, sol::table& parent);

	SoundProperties ParseSoundProperties(const sol::table& table);
	SoundProperties GetEffectiveSoundProperties(const SoundSourceInfo& soundSource);
	sol::table MakeSoundPropertiesTable(sol::this_state state, const SoundProperties& properties);
};
