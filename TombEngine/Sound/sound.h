#pragma once
#include <bass.h>
#include <bass_fx.h>

#include "Game/control/control.h"
#include "Sound/sound_effects.h"

constexpr auto SOUND_NO_CHANNEL              = -1;
constexpr auto SOUND_BASS_UNITS              = 1.0f / 1024.0f;	// TR->BASS distance unit coefficient
constexpr auto SOUND_MAXVOL_RADIUS           = 1024.0f;		// Max. volume hearing distance
constexpr auto SOUND_OMNIPRESENT_ORIGIN      = Vector3(1.17549e-038f, 1.17549e-038f, 1.17549e-038f);
constexpr auto SOUND_MAX_SAMPLES             = 8192;
constexpr auto SOUND_MAX_CHANNELS            = 32;
constexpr auto SOUND_LEGACY_SOUNDMAP_SIZE    = 450;
constexpr auto SOUND_NEW_SOUNDMAP_MAX_SIZE   = 4096;
constexpr auto SOUND_LEGACY_TRACKTABLE_SIZE  = 136;
constexpr auto SOUND_FLAG_NO_PAN             = (1<<12);	// Unused flag
constexpr auto SOUND_FLAG_RND_PITCH          = (1<<13);
constexpr auto SOUND_FLAG_RND_GAIN           = (1<<14);
constexpr auto SOUND_MAX_PITCH_CHANGE        = 0.09f;
constexpr auto SOUND_MAX_GAIN_CHANGE         = 0.0625f;
constexpr auto SOUND_32BIT_SILENCE_LEVEL     = 4.9e-04f;
constexpr auto SOUND_SAMPLE_FLAGS            = (BASS_SAMPLE_MONO | BASS_SAMPLE_FLOAT);
constexpr auto SOUND_MILLISECONDS_IN_SECOND  = 1000.0f;
constexpr auto SOUND_XFADETIME_BGM           = 5000;
constexpr auto SOUND_XFADETIME_BGM_START     = 1500;
constexpr auto SOUND_XFADETIME_ONESHOT       = 200;
constexpr auto SOUND_XFADETIME_LEVELJUMP     = 400;
constexpr auto SOUND_XFADETIME_CUTSOUND      = 100;
constexpr auto SOUND_XFADETIME_HIJACKSOUND   = 50;
constexpr auto SOUND_BGM_DAMP_COEFFICIENT    = 0.5f;
constexpr auto SOUND_MIN_PARAM_MULTIPLIER    = 0.05f;
constexpr auto SOUND_MAX_PARAM_MULTIPLIER    = 5.0f;

enum class SoundPauseMode
{
	Global,
	Inventory,
	Pause
};

enum class SoundTrackType
{
	OneShot,
	BGM,
	Voice,
	Count
};

enum class SoundEnvironment
{
	Always,
	Land,
	ShallowWater,
	Swamp,
	Underwater
};

enum class SoundPlayMode
{
	Normal,
	Wait,
	Restart,
	Looped
};

enum class ReverbType
{
	Outside,  // 0x00   no reverberation
	Small,	  // 0x01   little reverberation
	Medium,   // 0x02
	Large,	  // 0x03
	Pipe,	  // 0x04   highest reverberation, almost never used
	Count
};

enum class SoundState
{
	Idle,
	Ending,
	Ended
};

enum class SoundFilter
{
	Reverb,
	Compressor,
	Lowpass,
	Count
};

struct SoundEffectSlot
{
	SoundState State;
	short EffectID;
	float Gain;
	HCHANNEL Channel;
	Vector3 Origin;
};

struct SoundTrackSlot
{
	HSTREAM Channel { 0 };
	std::string Track {};
};

struct SampleInfo
{
	short Number;
	byte Volume;
	byte Radius;
	byte Randomness;
	signed char Pitch;
	short Flags;
};

struct SoundTrackInfo
{
	std::string Name {};
	SoundTrackType Mode { SoundTrackType::OneShot };
	int Mask { 0 };
};

struct SoundSourceInfo
{
	Vector3i	Position = Vector3i::Zero;
	int			SoundID	 = 0;
	int			Flags	 = 0;
	std::string	Name	 = {};

	SoundSourceInfo()
	{
	}

	SoundSourceInfo(int xPos, int yPos, int zPos)
	{
		Position = Vector3i(xPos, yPos, zPos);
	}

	SoundSourceInfo(int xPos, int yPos, int zPos, short soundID)
	{
		Position = Vector3i(xPos, yPos, zPos);
		SoundID = soundID;
	}

	SoundSourceInfo(int xPos, int yPos, int zPos, short soundID, short newflags)
	{
		Position = Vector3i(xPos, yPos, zPos);
		SoundID = soundID;
		Flags = newflags;
	}
};

extern std::map<std::string, int> SoundTrackMap;
extern std::unordered_map<int, SoundTrackInfo> SoundTracks;

bool SoundEffect(int soundID, Pose* pose, SoundEnvironment soundEnv = SoundEnvironment::Land, float pitchMult = 1.0f, float gainMult = 1.0f);
void StopSoundEffect(short effectID);
bool LoadSample(char *buffer, int compSize, int uncompSize, int currentIndex);
void FreeSamples();
void StopAllSounds();
void PauseAllSounds(SoundPauseMode mode);
void ResumeAllSounds(SoundPauseMode mode);
void SayNo();
void PlaySoundSources();
int  GetShatterSound(int shatterID);

void PlaySoundTrack(const std::string& trackName, SoundTrackType mode, std::optional<QWORD> pos = std::nullopt, int forceFadeInTime = 0);
void PlaySoundTrack(const std::string& trackName, short mask = 0);
void PlaySoundTrack(int index, short mask = 0);
void StopSoundTrack(SoundTrackType mode, int fadeoutTime);
void StopSoundTracks(int fadeoutTime = SOUND_XFADETIME_LEVELJUMP, bool excludeAmbience = false);
void ClearSoundTrackMasks();
void PlaySecretTrack();
void EnumerateLegacyTracks();
void LoadSubtitles(const std::string& path);
float GetSoundTrackLoudness(SoundTrackType mode);
std::optional<std::string> GetCurrentSubtitle();
std::pair<std::string, QWORD> GetSoundTrackNameAndPosition(SoundTrackType type);

static void CALLBACK Sound_FinishOneshotTrack(HSYNC handle, DWORD channel, DWORD data, void* userData);

void  SetVolumeTracks(int vol);
void  SetVolumeFX(int vol);

void  Sound_Init(const std::string& gameDirectory);
void  Sound_DeInit();
bool  Sound_CheckBASSError(const char* message, bool verbose, ...);
void  Sound_UpdateScene();
void  Sound_FreeSample(int index);
int   Sound_GetFreeSlot();
void  Sound_FreeSlot(int index, unsigned int fadeout = 0);
int   Sound_EffectIsPlaying(int effectID, Pose *position);
int   Sound_TrackIsPlaying(const std::string& fileName);
float Sound_DistanceToListener(Pose *position);
float Sound_DistanceToListener(Vector3 position);
float Sound_Attenuate(float gain, float distance, float radius);
bool  Sound_UpdateEffectPosition(int index, Pose *position, bool force = false);
bool  Sound_UpdateEffectAttributes(int index, float pitch, float gain);
