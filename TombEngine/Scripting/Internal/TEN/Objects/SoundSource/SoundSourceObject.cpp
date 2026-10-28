#include "framework.h"
#include "SoundSourceObject.h"

#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Sound/SoundHandler.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Sound/sound.h"

/***
Sound source

@tenclass Objects.SoundSource
@pragma nostrip
*/

static auto IndexError = IndexErrorMaker(SoundSource, ScriptReserved_SoundSource);
static auto NewIndexError = NewIndexErrorMaker(SoundSource, ScriptReserved_SoundSource);

SoundSource::SoundSource(SoundSourceInfo& ref) : m_soundSource{ref}
{};

void SoundSource::Register(sol::table& parent)
{
	parent.new_usertype<SoundSource>(ScriptReserved_SoundSource,
		sol::no_constructor, // ability to spawn new ones could be added later
		sol::meta_function::index, IndexError,
		sol::meta_function::new_index, NewIndexError,
		
		/// Get the sound source's position.
		// @function SoundSource:GetPosition
		// @treturn Vec3 Sound source's position.
		ScriptReserved_GetPosition, &SoundSource::GetPos,

		/// Set the sound source's position.
		// @function SoundSource:SetPosition
		// @tparam Vec3 position The new position of the sound source.
		ScriptReserved_SetPosition, &SoundSource::SetPos,

		/// Get the sound source's unique string identifier.
		// @function SoundSource:GetName
		// @treturn string The sound source's name.
		ScriptReserved_GetName, &SoundSource::GetName,

		/// Set the sound source's unique string identifier.
		// @function SoundSource:SetName
		// @tparam string name The sound source's new name.
		ScriptReserved_SetName, &SoundSource::SetName,

		/// Get the sound source's sound ID.
		// @function SoundSource:GetSoundID
		// @treturn int The ID of the sound.
		ScriptReserved_GetSoundID, &SoundSource::GetSoundID,

		/// Set the sound source's ID.
		// @function SoundSource:SetSoundID
		// @tparam int name The sound source's new sound ID.
		ScriptReserved_SetSoundID, &SoundSource::SetSoundID,

		/// Get whether the sound source is enabled.
		// @function SoundSource:GetEnabled
		// @treturn bool True if the sound source is enabled, false otherwise.
		ScriptReserved_GetEnabled, &SoundSource::GetEnabled,

		/// Set whether the sound source is enabled. Disabled sound sources do not play.
		// @function SoundSource:SetEnabled
		// @tparam bool enabled Whether the sound source is enabled.
		ScriptReserved_SetEnabled, &SoundSource::SetEnabled,

		/// Get the sound source's properties. Fields that were not overridden are read from the sound catalogue.
		// @function SoundSource:GetProperties
		// @treturn table Table of sound properties.
		ScriptReserved_GetProperties, &SoundSource::GetProperties,

		/// Set the sound source's property overrides. Properties not provided fall back to the sound catalogue.
		// @function SoundSource:SetProperties
		// @tparam table properties Table of sound properties to override. Supported fields are `volume` (0-255), `radius` (0-255 sectors), `randomness` (0-255), `pitch` (-127-127), `playMode` (a @{Sound.SoundPlayMode} value), `randomPitch` (bool) and `randomGain` (bool). An empty table clears all overrides.
		ScriptReserved_SetProperties, &SoundSource::SetProperties
	);
}

Vec3 SoundSource::GetPos() const
{
	return Vec3(m_soundSource.Position);
}

void SoundSource::SetPos(Vec3 const& pos)
{
	m_soundSource.Position = Vector3i(pos.x, pos.y, pos.z);
}

std::string SoundSource::GetName() const
{
	return m_soundSource.Name;
}

void SoundSource::SetName(std::string const& id) 
{
	if (!ScriptAssert(!id.empty(), "Name cannot be blank. Not setting name."))
	{
		return;
	}

	if (_callbackSetName(id, m_soundSource))
	{
		// remove the old name if we have one
		_callbackRemoveName(m_soundSource.Name);
		m_soundSource.Name = id;
	}
	else
	{
		ScriptAssertF(false, "Could not add name {} - does an object with this name already exist?", id);
		TENLog("Name will not be set", LogLevel::Warning, LogConfig::All);
	}
}

int SoundSource::GetSoundID() const
{
	return m_soundSource.SoundID;
}

void SoundSource::SetSoundID(int soundID)
{
	if (m_soundSource.SoundID == soundID)
		return;

	m_soundSource.SoundID = soundID;
	m_soundSource.Properties = {};
}

bool SoundSource::GetEnabled() const
{
	return m_soundSource.Enabled;
}

void SoundSource::SetEnabled(bool enabled)
{
	if (!enabled)
		StopSoundEffect((short)m_soundSource.SoundID);

	m_soundSource.Enabled = enabled;
}

sol::table SoundSource::GetProperties(sol::this_state state) const
{
	auto properties = TEN::Scripting::Sound::GetEffectiveSoundProperties(m_soundSource);
	return TEN::Scripting::Sound::MakeSoundPropertiesTable(state, properties);
}

void SoundSource::SetProperties(sol::optional<sol::table> properties)
{
	m_soundSource.Properties = properties.has_value() ?
		TEN::Scripting::Sound::ParseSoundProperties(properties.value()) : SoundProperties{};
}
