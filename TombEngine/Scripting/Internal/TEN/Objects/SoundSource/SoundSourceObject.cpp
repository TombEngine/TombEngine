#include "framework.h"
#include "SoundSourceObject.h"

#include "Scripting/Internal/ReservedScriptNames.h"
#include "Sound/sound.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"

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
		ScriptReserved_SetSoundID, &SoundSource::SetSoundID
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
	m_soundSource.SoundID = soundID;
}
