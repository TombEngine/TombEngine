#pragma once

#include <sol/sol.hpp>

#include "Math/Math.h"
#include "Scripting/Internal/TEN/Objects/NamedBase.h"

struct SoundSourceInfo;

class Vec3;

class SoundSource : public NamedBase<SoundSource, SoundSourceInfo &>
{
public:
	using IdentifierType = std::reference_wrapper<SoundSourceInfo>;
	SoundSource(SoundSourceInfo& ref);
	~SoundSource() = default;

	SoundSource& operator=(SoundSource const& other) = delete;
	SoundSource(SoundSource const& other) = delete;

	static void Register(sol::table &);
	Vec3 GetPos() const;
	void SetPos(Vec3 const& pos);

	int GetSoundID() const;
	void SetSoundID(int soundID);

	bool GetEnabled() const;
	void SetEnabled(bool enabled);

	sol::table GetProperties(sol::this_state state) const;
	void SetProperties(sol::optional<sol::table> properties);

	std::string GetName() const;
	void SetName(std::string const &);

private:
	SoundSourceInfo & m_soundSource;
};

