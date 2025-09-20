#pragma once

#include "Scripting/Internal/TEN/Objects/NamedBase.h"
#include "Math/Math.h"

struct SoundSourceInfo;

namespace sol
{
	class state;
}
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

	std::string GetName() const;
	void SetName(std::string const &);

private:
	SoundSourceInfo & m_soundSource;
};

