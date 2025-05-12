#include "framework.h"
#include "Sound/sound.h"

#include <filesystem>
#include <regex>
#include <srtparser.h>

#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/gui.h"
#include "Game/Lara/lara.h"
#include "Game/room.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/configuration.h"
#include "Specific/level.h"
#include "Specific/trutils.h"
#include "Specific/Video/Video.h"
#include "Specific/winmain.h"

using namespace TEN::Gui;
using namespace TEN::Math;
using namespace TEN::Video;

enum SoundSourceFlags
{
	SS_FLAG_PLAY_FLIP_ROOM = 1 << 13,
	SS_FLAG_PLAY_BASE_ROOM = 1 << 14,
	SS_FLAG_PLAY_ALWAYS	   = 1 << 15
};

HSAMPLE BASS_SamplePointer[SOUND_MAX_SAMPLES];
HSTREAM BASS_3D_Mixdown;
HFX     BASS_FXHandler[(int)SoundFilter::Count];

HMODULE ADPCMLibrary = NULL; // Temporary hack for unexpected ADPCM codec unload on Win11 systems.

SoundEffectSlot SoundSlot[SOUND_MAX_CHANNELS];
SoundTrackSlot  SoundtrackSlot[(int)SoundTrackType::Count];

const BASS_BFX_FREEVERB BASS_ReverbTypes[(int)ReverbType::Count] =    // Reverb presets

{ // Dry Mix | Wet Mix |  Size   |  Damp   |  Width  |  Mode  | Channel
  {  1.0f,     0.20f,     0.05f,    0.90f,    0.7f,     0,      -1     },	// 0 = Outside
  {  1.0f,     0.20f,     0.35f,    0.15f,    0.8f,     0,      -1     },	// 1 = Small room
  {  1.0f,     0.25f,     0.55f,    0.20f,    1.0f,     0,      -1     },	// 2 = Medium room
  {  1.0f,     0.25f,     0.80f,    0.50f,    1.0f,     0,      -1     },	// 3 = Large room
  {  1.0f,     0.25f,     0.90f,    1.00f,    1.0f,     0,      -1     }	// 4 = Pipe
};

const  std::string TRACKS_EXTENSIONS[] = {".wav", ".ogg", ".mp3" };
const  std::string TRACKS_PATH = "Audio/";
static std::string FullAudioDirectory;

std::map<std::string, int> SoundTrackMap;
std::unordered_map<int, SoundTrackInfo> SoundTracks;
std::vector<SubtitleItem*> Subtitles;

constexpr int LegacyLoopingTrackMin = 98;
constexpr int LegacyLoopingTrackMax = 111;

static int SecretSoundIndex = 5;
static int GlobalMusicVolume;
static int GlobalFXVolume;

void SetVolumeTracks(int vol) 
{
	GlobalMusicVolume = vol;

	float fVol = static_cast<float>(vol) / 100.0f;
	for (int i = 0; i < (int)SoundTrackType::Count; i++)
	{
		if (BASS_ChannelIsActive(SoundtrackSlot[i].Channel))
			BASS_ChannelSetAttribute(SoundtrackSlot[i].Channel, BASS_ATTRIB_VOL, fVol);
	}
}

void SetVolumeFX(int vol)
{
	GlobalFXVolume = vol;
	g_VideoPlayer.SetVolume(vol);
}

bool LoadSample(char* pointer, int compSize, int uncompSize, int index)
{
	if (index >= SOUND_MAX_SAMPLES)
	{
		TENLog("Sample index " + std::to_string(index) + " is larger than max. amount of samples", LogLevel::Warning);
		return 0;
	}

	if (pointer == nullptr || compSize <= 0)
	{
		TENLog("Sample size or memory address is incorrect for index " + std::to_string(index), LogLevel::Warning);
		return 0;
	}

	// Load and uncompress sample to 32-bit float format.
	HSAMPLE sample = BASS_SampleLoad(true, pointer, 0, compSize, 1, SOUND_SAMPLE_FLAGS);

	if (!sample)
	{
		TENLog("Error loading sample " + std::to_string(index), LogLevel::Error);
		return false;
	}

	// Paranoid (c) TeslaRus
	// Try to free sample before allocating new one.
	Sound_FreeSample(index);

	BASS_SAMPLE info;
	BASS_SampleGetInfo(sample, &info);
	int finalLength = info.length + 44;	// uncompSize is invalid after 16->32 bit conversion

	if (info.freq != 22050 || info.chans != 1)
	{
		TENLog("Wrong sample parameters, must be 22050 Hz Mono", LogLevel::Error);
		return false;
	}

	// Generate RIFF/WAV header to simplify loading sample data to stream. In case if RIFF/WAV header
	// exists, stream could be completely created just by calling BASS_StreamCreateFile().
	char* uncompBuffer = new char[finalLength];
	ZeroMemory(uncompBuffer, finalLength);
	memcpy(uncompBuffer, "RIFF\0\0\0\0WAVEfmt \20\0\0\0", 20);
	memcpy(uncompBuffer + 36, "data\0\0\0\0", 8);

	WAVEFORMATEX *wf = (WAVEFORMATEX*)(uncompBuffer + 20);

	wf->wFormatTag = 3;
	wf->nChannels = info.chans;
	wf->wBitsPerSample = 32;
	wf->nSamplesPerSec = info.freq;
	wf->nBlockAlign = wf->nChannels * wf->wBitsPerSample / 8;
	wf->nAvgBytesPerSec = wf->nSamplesPerSec * wf->nBlockAlign;

	// Copy raw PCM data from temporary sample buffer to actual buffer which will be used by engine.
	BASS_SampleGetData(sample, uncompBuffer + 44);
	BASS_SampleFree(sample);

	// Cut off trailing silence from samples to prevent gaps in looped playback
	int cleanLength = info.length;
	for (DWORD i = 4; i < info.length; i += 4)
	{
		float *currentSample = reinterpret_cast<float*>(uncompBuffer + finalLength - i);
		if (*currentSample > SOUND_32BIT_SILENCE_LEVEL || *currentSample < -SOUND_32BIT_SILENCE_LEVEL)
		{
			int alignment = i % wf->nBlockAlign;
			cleanLength -= (i - alignment);
			break;
		}
	}

	// Put data size to header
	*(DWORD*)(uncompBuffer + 4) = cleanLength + 44 - 8;
	*(DWORD*)(uncompBuffer + 40) = cleanLength;

	// Create actual sample
	BASS_SamplePointer[index] = BASS_SampleLoad(true, uncompBuffer, 0, cleanLength + 44, 65535, SOUND_SAMPLE_FLAGS | BASS_SAMPLE_3D);
	delete[] uncompBuffer;

	return true;
}

// TODO: Use std::optional for pose argument.
bool SoundEffect(int soundID, Pose* pose, SoundEnvironment soundEnv, float pitchMult, float gainMult)
{
	if (!g_Configuration.EnableSound)
		return false;

	if (soundID >= g_Level.SoundMap.size())
		return false;

	if (BASS_GetDevice() == -1)
		return false;

	// Test if sound effect environment matches camera environment.
	if (soundEnv != SoundEnvironment::Always)
	{
		bool isCameraUnderwater = TestEnvironment(ENV_FLAG_WATER, Camera.pos.RoomNumber);
		if ((soundEnv == SoundEnvironment::Underwater && !isCameraUnderwater) ||
			(soundEnv != SoundEnvironment::Underwater && isCameraUnderwater))
		{
			return false;
		}
	}

	// Get actual sample index from sound map.
	int sampleIndex = g_Level.SoundMap[soundID];

	// -1 means no such effect exists in level file.Set to -2 afterwards to prevent further debug message firings.
	if (sampleIndex == -1)
	{
		TENLog("Missing sound effect " + std::to_string(soundID), LogLevel::Warning);
		g_Level.SoundMap[soundID] = -2;
		return false;
	}
	else if (sampleIndex == -2)
	{
		return false;
	}

	const auto& sample = g_Level.SoundDetails[sampleIndex];
	if (sample.Number < 0)
	{
		TENLog("No valid samples count for effect " + std::to_string(sampleIndex), LogLevel::Warning);
		return false;
	}

	// Assign common sample flags.
	DWORD sampleFlags = SOUND_SAMPLE_FLAGS;

	// Test play chance.
	if (sample.Randomness && ((GetRandomControl() & UCHAR_MAX) > sample.Randomness))
		return false;

	// Apply 3D attribute only to sound with position property.
	if (pose != nullptr)
		sampleFlags |= BASS_SAMPLE_3D;

	// Set and randomize volume (if needed).
	float gain = ((float)sample.Volume / UCHAR_MAX) * std::clamp(gainMult, SOUND_MIN_PARAM_MULTIPLIER, SOUND_MAX_PARAM_MULTIPLIER);
	if ((sample.Flags & SOUND_FLAG_RND_GAIN))
		gain -= Random::GenerateFloat(0.0f, 1.0f) * SOUND_MAX_GAIN_CHANGE;

	// Set and randomize pitch and additionally multiply by provided value (e.g. for vehicles).
	float pitch = (1.0f + ((float)sample.Pitch / 127.0f)) * std::clamp(pitchMult, SOUND_MIN_PARAM_MULTIPLIER, SOUND_MAX_PARAM_MULTIPLIER);

	// Randomize pitch (if needed)
	if ((sample.Flags & SOUND_FLAG_RND_PITCH))
		pitch += Random::GenerateFloat(-0.5f, 0.5f) * (SOUND_MAX_PITCH_CHANGE * 2);

	// Calculate sound radius and distance to sound.
	float radius = BLOCK(sample.Radius);
	float dist = Sound_DistanceToListener(pose);

	// Skip playing if too far from listener position.
	if (dist > radius)
		return false;

	// Get final sound volume.
	float volume = Sound_Attenuate(gain, dist, radius);

	// Get existing index, if any, of playing sound.
	int existingChannel = Sound_EffectIsPlaying(soundID, pose);

	// Select behaviour based on effect playback type (bytes 0-1 of flags field).
	auto playMode = (SoundPlayMode)(sample.Flags & 3);
	switch (playMode)
	{
	case SoundPlayMode::Normal:
		break;

	case SoundPlayMode::Wait:
		// Don't play until stopped.
		if (existingChannel != SOUND_NO_CHANNEL)
			return false;
		break;

	case SoundPlayMode::Restart:
		// Stop existing and continue.
		if (existingChannel != SOUND_NO_CHANNEL)
			Sound_FreeSlot(existingChannel, SOUND_XFADETIME_CUTSOUND); 
		break;

	case SoundPlayMode::Looped:
		// Update parameters and return if already playing.
		if (existingChannel != SOUND_NO_CHANNEL)
		{
			Sound_UpdateEffectPosition(existingChannel, pose);
			Sound_UpdateEffectAttributes(existingChannel, pitch, volume);
			return false;
		}

		sampleFlags |= BASS_SAMPLE_LOOP;
		break;
	}

	// Randomly select arbitrary sample from list if more than one is present.
	int sampleToPlay = 0;
	int sampleCount = (sample.Flags >> 2) & 15;
	if (sampleCount == 1)
	{
		sampleToPlay = sample.Number;
	}
	else
	{
		sampleToPlay = sample.Number + (int)((GetRandomControl() * sampleCount) >> 15);
	}

	// Get free channel to play sample.
	int freeSlot = Sound_GetFreeSlot();
	if (freeSlot == SOUND_NO_CHANNEL)
	{
		TENLog("No free channel slot available!", LogLevel::Warning);
		return false;
	}

	// Create sample's stream and reset buffer back to normal value.
	HSTREAM channel = BASS_SampleGetChannel(BASS_SamplePointer[sampleToPlay], true);

	if (Sound_CheckBASSError("Trying to create channel for sample %d", false, sampleToPlay))
		return false;

	// Ready to play sound; assign to sound slot.
	auto& soundSlot = SoundSlot[freeSlot];
	soundSlot.State = SoundState::Idle;
	soundSlot.EffectID = soundID;
	soundSlot.Channel = channel;
	soundSlot.Gain = gain;
	soundSlot.Origin = (pose != nullptr) ? pose->Position.ToVector3() : SOUND_OMNIPRESENT_ORIGIN;

	if (Sound_CheckBASSError("Applying pitch/gain attribs on channel %x, sample %d", false, channel, sampleToPlay))
		return false;

	// Set looped flag if necessary.
	if (playMode == SoundPlayMode::Looped)
		BASS_ChannelFlags(channel, BASS_SAMPLE_LOOP, BASS_SAMPLE_LOOP);

	// Play channel.
	BASS_ChannelPlay(channel, false);

	if (Sound_CheckBASSError("Queuing channel %x on sample mixer", false, freeSlot))
		return false;

	// Set attributes.
	BASS_ChannelSet3DAttributes(channel, pose ? BASS_3DMODE_NORMAL : BASS_3DMODE_OFF, SOUND_MAXVOL_RADIUS, radius, 360, 360, 0.0f);
	Sound_UpdateEffectPosition(freeSlot, pose, true);
	Sound_UpdateEffectAttributes(freeSlot, pitch, volume);

	if (Sound_CheckBASSError("Applying 3D attribs on channel %x, sound %d", false, channel, soundID))
		return false;

	return true;
}

void PauseAllSounds(SoundPauseMode mode)
{
	if (mode == SoundPauseMode::Global)
	{
		BASS_Pause();
		return;
	}

	for (const auto& slot : SoundSlot)
	{
		if ((slot.Channel != NULL) && (BASS_ChannelIsActive(slot.Channel) == BASS_ACTIVE_PLAYING))
			BASS_ChannelPause(slot.Channel);
	}

	for (int i = 0; i < (int)SoundTrackType::Count; i++)
	{
		if (mode == SoundPauseMode::Inventory && (SoundTrackType)i != SoundTrackType::Voice)
			continue;

		const auto& slot = SoundtrackSlot[i];
		if ((slot.Channel != NULL) && (BASS_ChannelIsActive(slot.Channel) == BASS_ACTIVE_PLAYING))
			BASS_ChannelPause(slot.Channel);
	}
}

void ResumeAllSounds(SoundPauseMode mode)
{
	if (mode == SoundPauseMode::Global)
		BASS_Start();

	if (g_Gui.GetInventoryMode() == InventoryMode::Pause || 
		g_Gui.GetInventoryMode() == InventoryMode::Statistics)
	{
		return;
	}

	for (const auto& slot : SoundtrackSlot)
	{
		if ((slot.Channel != NULL) && (BASS_ChannelIsActive(slot.Channel) == BASS_ACTIVE_PAUSED))
			BASS_ChannelStart(slot.Channel);
	}

	if (mode == SoundPauseMode::Global)
		return;

	for (const auto& slot : SoundSlot)
	{
		if ((slot.Channel != NULL) && (BASS_ChannelIsActive(slot.Channel) == BASS_ACTIVE_PAUSED))
			BASS_ChannelStart(slot.Channel);
	}
}

void StopSoundEffect(short effectID)
{
	for (int i = 0; i < SOUND_MAX_CHANNELS; i++)
	{
		if (SoundSlot[i].Channel != NULL && SoundSlot[i].EffectID == effectID && BASS_ChannelIsActive(SoundSlot[i].Channel) == BASS_ACTIVE_PLAYING)
			Sound_FreeSlot(i, SOUND_XFADETIME_CUTSOUND);
	}
}

void StopAllSounds()
{
	for (int i = 0; i < SOUND_MAX_CHANNELS; i++)
		Sound_FreeSlot(i, SOUND_XFADETIME_CUTSOUND);

	ZeroMemory(SoundSlot, (sizeof(SoundEffectSlot) * SOUND_MAX_CHANNELS));
}

void FreeSamples()
{
	StopAllSounds();
	for (int i = 0; i < SOUND_MAX_SAMPLES; i++)
		Sound_FreeSample(i);
}

void EnumerateLegacyTracks()
{
	auto dir = std::filesystem::path{ FullAudioDirectory };

    if (!std::filesystem::is_directory(dir))
    {
        TENLog("Folder \"" + dir.string() + "\" does not exist. ", LogLevel::Warning, LogConfig::All);
        return;
    }

	try 
	{
		// Capture three-digit filenames, or those which start with three digits.

		std::regex upToThreeDigits("((\\d{1,3})[^\\.]*)");
		std::smatch result;
		for (const auto& file : std::filesystem::directory_iterator{ dir })
		{
			std::string fileName = file.path().filename().string();
			auto bResult = std::regex_search(fileName, result, upToThreeDigits);
			if (!result.empty())
			{
				// result[0] is the whole match including the leading backslash, so ignore it
				// result[1] is the full file name, not including the extension
				int index = std::stoi(result[2].str());
				SoundTrackInfo s;

				// TRLE default looping tracks
				if (index >= LegacyLoopingTrackMin && index <= LegacyLoopingTrackMax)
				{
					s.Mode = SoundTrackType::BGM;
				}
				s.Name = result[1];
				SoundTracks.insert(std::make_pair(index, s));
				SecretSoundIndex = std::max(SecretSoundIndex, index);
			}
		}
	}
	catch (std::filesystem::filesystem_error const& e)
	{
		TENLog(e.what(), LogLevel::Error, LogConfig::All);
	}

}

float GetSoundTrackLoudness(SoundTrackType mode)
{
	float result = 0.0f;

	if (!g_Configuration.EnableSound)
		return result;

	if (!BASS_ChannelIsActive(SoundtrackSlot[(int)mode].Channel))
		return result;

	BASS_ChannelGetLevelEx(SoundtrackSlot[(int)mode].Channel, &result, 0.1f, BASS_LEVEL_MONO | BASS_LEVEL_RMS);
	return std::clamp(result * 2.0f, 0.0f, 1.0f);
}

std::optional<std::string> GetCurrentSubtitle()
{
	if (!g_Configuration.EnableSound || !g_Configuration.EnableSubtitles)
		return std::nullopt;

	auto channel = SoundtrackSlot[(int)SoundTrackType::Voice].Channel;

	if (!BASS_ChannelIsActive(channel))
		return std::nullopt;

	if (Subtitles.empty())
		return std::nullopt;

	long time = long(BASS_ChannelBytes2Seconds(channel, BASS_ChannelGetPosition(channel, BASS_POS_BYTE)) * SOUND_MILLISECONDS_IN_SECOND);

	for (auto* stringPtr : Subtitles)
	{
		if (time >= stringPtr->getStartTime() && time <= stringPtr->getEndTime())
			return stringPtr->getText();
	}

	return std::nullopt;
}

void PlaySoundTrack(const std::string& track, SoundTrackType mode, std::optional<QWORD> pos, int forceFadeInTime)
{
	if (!g_Configuration.EnableSound)
		return;

	if (track.empty())
		return;

	bool crossfade = false;
	DWORD crossfadeTime = 0;
	DWORD flags = BASS_STREAM_AUTOFREE | BASS_SAMPLE_FLOAT | BASS_ASYNCFILE;

	bool channelActive = BASS_ChannelIsActive(SoundtrackSlot[(int)mode].Channel);
	if (channelActive && SoundtrackSlot[(int)mode].Track.compare(track) == 0)
	{
		// Same track is incoming with different playhead; set it to new position.
		auto stream = SoundtrackSlot[(int)mode].Channel;
		if (pos.has_value() && BASS_ChannelGetLength(stream, BASS_POS_BYTE) > pos.value())
			BASS_ChannelSetPosition(stream, pos.value(), BASS_POS_BYTE);

		return;
	}

	switch (mode)
	{
	case SoundTrackType::OneShot:
	case SoundTrackType::Voice:
		crossfadeTime = SOUND_XFADETIME_ONESHOT;
		break;

	case SoundTrackType::BGM:
		crossfade = true;
		crossfadeTime = channelActive ? SOUND_XFADETIME_BGM : SOUND_XFADETIME_BGM_START;
		flags |= BASS_SAMPLE_LOOP;
		break;
	}

	auto fullTrackName = std::filesystem::path(FullAudioDirectory + track);
	if (!fullTrackName.has_extension() || !std::filesystem::is_regular_file(fullTrackName))
	{
		for (auto& extension : TRACKS_EXTENSIONS)
		{
			fullTrackName.replace_extension(extension);
			if (std::filesystem::is_regular_file(fullTrackName))
				break;
		}
	}

	if (!std::filesystem::is_regular_file(fullTrackName))
	{
		TENLog("No soundtrack files with name '" + fullTrackName.stem().string() + "' were found", LogLevel::Warning);
		return;
	}

	if (channelActive)
		BASS_ChannelSlideAttribute(SoundtrackSlot[(int)mode].Channel, BASS_ATTRIB_VOL, -1.0f, crossfadeTime);

	auto stream = BASS_StreamCreateFile(false, fullTrackName.c_str(), 0, 0, flags);

	if (Sound_CheckBASSError("Opening soundtrack '%s'", false, fullTrackName.filename().string().c_str()))
		return;

	float masterVolume = (float)GlobalMusicVolume / 100.0f;

	// Damp BGM track in case one-shot track is about to play.

	if (mode == SoundTrackType::OneShot)
	{
		if (BASS_ChannelIsActive(SoundtrackSlot[(int)SoundTrackType::BGM].Channel))
			BASS_ChannelSlideAttribute(SoundtrackSlot[(int)SoundTrackType::BGM].Channel, BASS_ATTRIB_VOL, masterVolume * SOUND_BGM_DAMP_COEFFICIENT, SOUND_XFADETIME_BGM_START);
		BASS_ChannelSetSync(stream, BASS_SYNC_FREE | BASS_SYNC_ONETIME | BASS_SYNC_MIXTIME, 0, Sound_FinishOneshotTrack, NULL);
	}

	// BGM tracks are crossfaded, and additionally shuffled a bit to make things more natural.
	// Think everybody are fed up with same start-up sounds of Caves ambience...

	if (forceFadeInTime > 0 || (crossfade && BASS_ChannelIsActive(SoundtrackSlot[(int)SoundTrackType::BGM].Channel)))
	{		
		// Crossfade...
		BASS_ChannelSetAttribute(stream, BASS_ATTRIB_VOL, 0.0f);
		BASS_ChannelSlideAttribute(stream, BASS_ATTRIB_VOL, masterVolume, (forceFadeInTime > 0) ? forceFadeInTime : crossfadeTime);

		// Shuffle...
		// Only activates if no custom position is passed as argument.
		if (!pos.has_value())
		{
			QWORD newPos = BASS_ChannelGetLength(stream, BASS_POS_BYTE) * (static_cast<float>(GetRandomControl()) / static_cast<float>(RAND_MAX));
			BASS_ChannelSetPosition(stream, newPos, BASS_POS_BYTE);
		}
	}
	else
	{
		BASS_ChannelSetAttribute(stream, BASS_ATTRIB_VOL, masterVolume);
	}

	BASS_ChannelPlay(stream, false);

	// Try to restore position, if specified.
	if (pos.has_value() && BASS_ChannelGetLength(stream, BASS_POS_BYTE) > pos.value())
		BASS_ChannelSetPosition(stream, pos.value(), BASS_POS_BYTE);

	if (Sound_CheckBASSError("Playing soundtrack '%s'", true, fullTrackName.filename().string().c_str()))
		return;

	SoundtrackSlot[(int)mode].Channel = stream;
	SoundtrackSlot[(int)mode].Track = track;

	// Additionally attempt to load subtitle file, if exists.
	if (mode == SoundTrackType::Voice)
		LoadSubtitles(track);
}

void LoadSubtitles(const std::string& name)
{
	Subtitles.clear();

	auto subtitleName = FullAudioDirectory + name + ".srt";

	if (!std::filesystem::is_regular_file(subtitleName))
		subtitleName = FullAudioDirectory + "/subtitles/" + name + ".srt";

	if (!std::filesystem::is_regular_file(subtitleName))
		return;

	auto factory = new SubtitleParserFactory(subtitleName);
	auto parser  = factory->getParser();
	Subtitles    = parser->getSubtitles();
	delete factory;

	for (auto& sub : Subtitles)
		sub->setText(ReplaceNewLineSymbols(sub->getText()));
}

void PlaySoundTrack(const std::string& track, short mask)
{
	// If track name was included in script, play it as registered track and take mask into account.
	// Otherwise, play it once without registering anywhere.

	if (SoundTrackMap.count(track))
		PlaySoundTrack(SoundTrackMap[track], mask);
	else
		PlaySoundTrack(track, SoundTrackType::OneShot);
}

void PlaySoundTrack(int index, short mask)
{
	if (SoundTracks.find(index) == SoundTracks.end())
	{
		static int lastAttemptedIndex = -1;
		if (lastAttemptedIndex != index)
		{
			TENLog("No track registered with index " + std::to_string(index), LogLevel::Error);
			lastAttemptedIndex = index;
		}
		return;
	}
	
	// Check and modify soundtrack map mask, if needed.
	// If existing mask is unmodified (same activation mask setup), track won't play.

	if (mask && !(SoundTracks[index].Mode == SoundTrackType::BGM))
	{
		int filteredMask = (mask >> 8) & 0x3F;
		if ((SoundTracks[index].Mask & filteredMask) == filteredMask)
			return;	// Mask is the same, don't play it.

		SoundTracks[index].Mask |= filteredMask;
	}

	PlaySoundTrack(SoundTracks[index].Name, SoundTracks[index].Mode);
}

void StopSoundTracks(int fadeoutTime, bool excludeAmbience)
{
	for (int i = 0; i < (int)SoundTrackType::Count; i++)
	{
		auto type = (SoundTrackType)i;
		if (excludeAmbience && type == SoundTrackType::BGM)
			continue;

		StopSoundTrack(type, fadeoutTime);
	}
}

void StopSoundTrack(SoundTrackType mode, int fadeoutTime)
{
	if (SoundtrackSlot[(int)mode].Channel == NULL)
		return;
	
	// Do fadeout.
	BASS_ChannelSlideAttribute(SoundtrackSlot[(int)mode].Channel, BASS_ATTRIB_VOL | BASS_SLIDE_LOG, -1.0f, fadeoutTime);

	SoundtrackSlot[(int)mode].Track = {};
	SoundtrackSlot[(int)mode].Channel = NULL;
}

void ClearSoundTrackMasks()
{
	for (auto& track : SoundTracks) { track.second.Mask = 0; }
}

// Returns specified soundtrack type's stem name and playhead position.
// To be used with savegames. To restore soundtrack, use PlaySoundtrack function with playhead position passed as 3rd argument.
std::pair<std::string, QWORD> GetSoundTrackNameAndPosition(SoundTrackType type)
{
	auto track = SoundtrackSlot[(int)type];

	if (track.Track.empty() || !BASS_ChannelIsActive(track.Channel))
		return std::pair<std::string, QWORD>();

	std::filesystem::path path = track.Track;
	return std::pair<std::string, QWORD>(path.string(), BASS_ChannelGetPosition(track.Channel, BASS_POS_BYTE));
}

static void CALLBACK Sound_FinishOneshotTrack(HSYNC handle, DWORD channel, DWORD data, void* userData)
{
	if (BASS_ChannelIsActive(SoundtrackSlot[(int)SoundTrackType::BGM].Channel))
		BASS_ChannelSlideAttribute(SoundtrackSlot[(int)SoundTrackType::BGM].Channel, BASS_ATTRIB_VOL, (float)GlobalMusicVolume / 100.0f, SOUND_XFADETIME_BGM_START);
}

void Sound_FreeSample(int index)
{
	if (BASS_SamplePointer[index] != NULL)
	{
		BASS_SampleFree(BASS_SamplePointer[index]);
		BASS_SamplePointer[index] = NULL;
	}
}

// Get first free (non-playing) sound slot.
// If no free slots found, now try to hijack slot which is as far from listener as possible
int Sound_GetFreeSlot()
{
	for (int i = 0; i < SOUND_MAX_CHANNELS; i++)
	{
		if (SoundSlot[i].Channel == NULL || !BASS_ChannelIsActive(SoundSlot[i].Channel))
			return i;
	}

	// No free slots, hijack now.

	float minDistance = 0;
	int farSlot = SOUND_NO_CHANNEL;

	for (int i = 0; i < SOUND_MAX_CHANNELS; i++)
	{
		float distance = Vector3(SoundSlot[i].Origin - Vector3(Camera.mikePos.x, Camera.mikePos.y, Camera.mikePos.z)).Length();
		if (distance > minDistance)
		{
			minDistance = distance;
			farSlot = i;
		}
	}

	TENLog("Hijacking sound effect slot " + std::to_string(farSlot), LogLevel::Info);
	Sound_FreeSlot(farSlot, SOUND_XFADETIME_HIJACKSOUND);
	return farSlot;
}

int Sound_TrackIsPlaying(const std::string& fileName)
{
	for (int i = 0; i < (int)SoundTrackType::Count; i++)
	{
		const auto& slot = SoundtrackSlot[i];

		if (!BASS_ChannelIsActive(slot.Channel))
			continue;

		auto name1 = TEN::Utils::ToLower(slot.Track);
		auto name2 = TEN::Utils::ToLower(fileName);

		if (name1.compare(name2) == 0)
			return true;
	}

	return false;
}

// Returns slot ID in which effect is playing, if found. If not found, returns -1.
// We use origin position as a reference, because in original TRs it's not possible to clearly
// identify what's the source of the producing effect.

int Sound_EffectIsPlaying(int effectID, Pose *position)
{
	for (int i = 0; i < SOUND_MAX_CHANNELS; i++)
	{
		if (SoundSlot[i].EffectID == effectID)
		{
			// Free channel.
			if (SoundSlot[i].Channel == NULL)
				continue;

			if (BASS_ChannelIsActive(SoundSlot[i].Channel))
			{
				// Only check position on 3D samples. 2D samples stop immediately.

				BASS_CHANNELINFO info;
				BASS_ChannelGetInfo(SoundSlot[i].Channel, &info);
				if (!(info.flags & BASS_SAMPLE_3D) || !position)
					return i;

				// Check if effect origin is equal OR in nearest possible hearing range.

				auto origin = Vector3(position->Position.x, position->Position.y, position->Position.z);
				if (Vector3::Distance(origin, SoundSlot[i].Origin) < SOUND_MAXVOL_RADIUS)
					return i;
			}
			else
			{
				SoundSlot[i].Channel = NULL; // WTF, let's clean this up
			}
		}
	}

	return SOUND_NO_CHANNEL;
}

// Gets the distance to the source.
float Sound_DistanceToListener(Pose *position)
{
	// Assume sound is 2D menu sound.
	if (!position)
		return 0.0f;

	return Sound_DistanceToListener(position->Position.ToVector3());
}
float Sound_DistanceToListener(Vector3 position)
{
	return Vector3(Vector3(Camera.mikePos.x, Camera.mikePos.y, Camera.mikePos.z) - position).Length();
}

// Calculate attenuated volume.
float Sound_Attenuate(float gain, float distance, float radius)
{
	float result = gain * (1.0f - (distance / radius));
	result = result < 0 ? 0.0f : (result > 1.0f ? 1.0f : result);
	return result * ((float)GlobalFXVolume / 100.0f);
}

// Stop and free desired sound slot.
void Sound_FreeSlot(int index, unsigned int fadeout)
{
	if (index >= SOUND_MAX_CHANNELS || index < 0)
		return;

	if (SoundSlot[index].Channel != NULL && BASS_ChannelIsActive(SoundSlot[index].Channel))
	{
		if (fadeout > 0)
			BASS_ChannelSlideAttribute(SoundSlot[index].Channel, BASS_ATTRIB_VOL, -1.0f, fadeout);
		else
			BASS_ChannelStop(SoundSlot[index].Channel);
	}

	SoundSlot[index].Channel = NULL;
	SoundSlot[index].State = SoundState::Idle;
	SoundSlot[index].EffectID = SOUND_NO_CHANNEL;
}

// Update sound position in a level.
bool Sound_UpdateEffectPosition(int index, Pose *position, bool force)
{
	if (index >= SOUND_MAX_CHANNELS || index < 0)
		return false;

	if (position)
	{
		BASS_CHANNELINFO info;
		BASS_ChannelGetInfo(SoundSlot[index].Channel, &info);
		if (info.flags & BASS_SAMPLE_3D)
		{
			SoundSlot[index].Origin.x = position->Position.x;
			SoundSlot[index].Origin.y = position->Position.y;
			SoundSlot[index].Origin.z = position->Position.z;

			auto pos = BASS_3DVECTOR(position->Position.x, position->Position.y, position->Position.z);
			auto rot = BASS_3DVECTOR(position->Orientation.x, position->Orientation.y, position->Orientation.z);
			BASS_ChannelSet3DPosition(SoundSlot[index].Channel, &pos, &rot, NULL);
			BASS_Apply3D();
		}
	}

	// Reset activity flag, important for looped samples
	if (BASS_ChannelIsActive(SoundSlot[index].Channel))
		SoundSlot[index].State = SoundState::Idle;

	return true;
}

// Update gain and pitch.
bool  Sound_UpdateEffectAttributes(int index, float pitch, float gain)
{
	if (index >= SOUND_MAX_CHANNELS || index < 0)
		return false;

	BASS_ChannelSetAttribute(SoundSlot[index].Channel, BASS_ATTRIB_FREQ, 22050.0f * pitch);
	BASS_ChannelSetAttribute(SoundSlot[index].Channel, BASS_ATTRIB_VOL, gain);

	return true;
}

// Update whole sound scene in a level.
// Must be called every frame to update camera position and 3D parameters.
void Sound_UpdateScene()
{
	if (!g_Configuration.EnableSound)
		return;

	// Apply environmental effects

	static int currentReverb = -1;
	auto roomReverb = g_Configuration.EnableReverb ? (int)g_Level.Rooms[Camera.pos.RoomNumber].reverbType : (int)ReverbType::Small;

	if (currentReverb == -1 || roomReverb != currentReverb)
	{
		currentReverb = roomReverb;
		if (currentReverb < (int)ReverbType::Count)
			BASS_FXSetParameters(BASS_FXHandler[(int)SoundFilter::Reverb], &BASS_ReverbTypes[(int)currentReverb]);
	}

	for (int i = 0; i < SOUND_MAX_CHANNELS; i++)
	{
		if ((SoundSlot[i].Channel != NULL) && (BASS_ChannelIsActive(SoundSlot[i].Channel) == BASS_ACTIVE_PLAYING))
		{
			SampleInfo* sampleInfo = &g_Level.SoundDetails[g_Level.SoundMap[SoundSlot[i].EffectID]];

			// Stop and clean up sounds which were in ending state in previous frame.
			// In case sound is looping, make it ending unless they are re-fired in next frame.

			if (SoundSlot[i].State == SoundState::Ending)
			{
				SoundSlot[i].State = SoundState::Ended;
				Sound_FreeSlot(i, SOUND_XFADETIME_CUTSOUND);
				continue;
			}
			else if ((SoundPlayMode)(sampleInfo->Flags & 3) == SoundPlayMode::Looped)
				SoundSlot[i].State = SoundState::Ending;

			// Calculate attenuation and clean up sounds which are out of listener range (only for 3D sounds).

			if (SoundSlot[i].Origin != SOUND_OMNIPRESENT_ORIGIN)
			{
				float radius = (float)(sampleInfo->Radius) * 1024.0f;
				float distance = Sound_DistanceToListener(SoundSlot[i].Origin);

				if (distance > radius)
				{
					Sound_FreeSlot(i);
					continue;
				}
				else
					BASS_ChannelSetAttribute(SoundSlot[i].Channel, BASS_ATTRIB_VOL, Sound_Attenuate(SoundSlot[i].Gain, distance, radius));
			}
		}
	}

	// Apply current listener position.

	Vector3 at = Vector3(Camera.target.x, Camera.target.y, Camera.target.z) -
		Vector3(Camera.mikePos.x, Camera.mikePos.y, Camera.mikePos.z);
	at.Normalize();
	auto mikePos = BASS_3DVECTOR(					// Pos
		Camera.mikePos.x,
		Camera.mikePos.y,
		Camera.mikePos.z);
	auto laraVel = BASS_3DVECTOR(					// Vel
		Lara.Context.WaterCurrentPull.x,
		Lara.Context.WaterCurrentPull.y,
		Lara.Context.WaterCurrentPull.z);
	auto atVec = BASS_3DVECTOR(at.x, at.y, at.z);	// At
	auto upVec = BASS_3DVECTOR(0.0f, 1.0f, 0.0f);	// Up
	BASS_Set3DPosition(&mikePos,
					   &laraVel,
					   &atVec,
					   &upVec);
	BASS_Apply3D();
}

// Initialize BASS engine and also prepare all sound data.
// Called once on engine start-up.
void Sound_Init(const std::string& gameDirectory)
{
	// Initialize and collect soundtrack paths.
	FullAudioDirectory = gameDirectory + TRACKS_PATH;
	EnumerateLegacyTracks();

	if (!g_Configuration.EnableSound)
		return;
	
	// HACK: Manually force-load ADPCM codec, because on Win11 systems it may suddenly unload otherwise.
	ADPCMLibrary = LoadLibrary("msadp32.acm");

	BASS_Init(g_Configuration.SoundDevice, 44100, BASS_DEVICE_3D, WindowsHandle, NULL);
	if (Sound_CheckBASSError("Initializing BASS sound device", true))
		return;

	// Initialize BASS_FX plugin.
	BASS_FX_GetVersion();
	if (Sound_CheckBASSError("Initializing FX plugin", true))
		return;

	// Set 3D world parameters.
	// Rolloff is lessened since we have own attenuation implementation.
	BASS_Set3DFactors(SOUND_BASS_UNITS, 1.5f, 0.5f);	
	BASS_SetConfig(BASS_CONFIG_3DALGORITHM, BASS_3DALG_FULL);

	// Set minimum latency and 2 threads for updating.
	// Most of modern PCs already have multi-core CPUs, so why not parallelize updating?
	BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 2);
	BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 10);

	// Create 3D mixdown channel and make it play forever.
	// For realtime mixer channels, we need minimum buffer latency. It shouldn't affect reliability.
	BASS_SetConfig(BASS_CONFIG_BUFFER, 40);
	BASS_3D_Mixdown = BASS_StreamCreate(44100, 2, BASS_SAMPLE_FLOAT, STREAMPROC_DEVICE_3D, NULL);
	BASS_ChannelPlay(BASS_3D_Mixdown, false);

	// Reset buffer back to normal value.
	BASS_SetConfig(BASS_CONFIG_BUFFER, 300);

	if (Sound_CheckBASSError("Starting 3D mixdown", true))
		return;

	// Initialize channels and tracks array
	ZeroMemory(SoundSlot, (sizeof(SoundEffectSlot) * SOUND_MAX_CHANNELS));

	// Attach reverb effect to 3D channel
 	BASS_FXHandler[(int)SoundFilter::Reverb] = BASS_ChannelSetFX(BASS_3D_Mixdown, BASS_FX_BFX_FREEVERB, 0);
	BASS_FXSetParameters(BASS_FXHandler[(int)SoundFilter::Reverb], &BASS_ReverbTypes[(int)ReverbType::Outside]);

	if (Sound_CheckBASSError("Attaching environmental FX", true))
		return;

	// Apply slight compression to 3D channel
	BASS_FXHandler[(int)SoundFilter::Compressor] = BASS_ChannelSetFX(BASS_3D_Mixdown, BASS_FX_BFX_COMPRESSOR2, 1);
	auto comp = BASS_BFX_COMPRESSOR2{ 4.0f, -18.0f, 1.5f, 10.0f, 100.0f, -1 };
	BASS_FXSetParameters(BASS_FXHandler[(int)SoundFilter::Compressor], &comp);

	if (Sound_CheckBASSError("Attaching compressor", true))
		return;

	return;
}

// Stop all sounds and streams, if any, unplug all channels from the mixer and unload BASS engine.
// Must be called on engine quit.
void Sound_DeInit()
{
	if (!g_Configuration.EnableSound)
		return;

	TENLog("Shutting down BASS...", LogLevel::Info);
	BASS_Free();

	// HACK: Manually unload previously loaded ADPCM codec.
	if (ADPCMLibrary != NULL)
		FreeLibrary(ADPCMLibrary);
}

bool Sound_CheckBASSError(const char* message, bool verbose, ...)
{
	va_list argptr;
	static char data[4096];

	int bassError = BASS_ErrorGetCode();
	if (verbose || bassError)
	{
		va_start(argptr, verbose);
		int written = vsprintf(data, (char*)message, argptr);	// @TODO: replace with debug/console/message output later...
		va_end(argptr);
		snprintf(data + written, sizeof(data) - written, bassError ? ": error #%d" : ": success", bassError);
		TENLog(data, bassError ? LogLevel::Error : LogLevel::Info);
	}
	return bassError != 0;
}

void SayNo()
{
	SoundEffect(SFX_TR4_LARA_NO_ENGLISH, nullptr, SoundEnvironment::Always);
}

void PlaySecretTrack()
{
	if (SoundTracks.find(SecretSoundIndex) == SoundTracks.end())
	{
		TENLog("No secret soundtrack index was found!", LogLevel::Warning);
		return;
	}

	// Secret soundtrack should be always last one on a list.	
	PlaySoundTrack(SoundTracks.at(SecretSoundIndex).Name, SoundTrackType::OneShot);
}

int GetShatterSound(int shatterID)
{
	auto fxID = Statics[shatterID].shatterSound;
	if (fxID != NO_VALUE && fxID < g_Level.SoundMap.size())
		return fxID;

	return SFX_TR4_SMASH_ROCK;
}

void PlaySoundSources()
{
	for (const auto& soundSource : g_Level.SoundSources)
	{
		int group = soundSource.Flags & 0x1FFF;

		if (group >= MAX_FLIPMAP)
			continue;

		if ((!FlipStats[group] && (soundSource.Flags & SS_FLAG_PLAY_FLIP_ROOM)) ||
			(FlipStats[group] && (soundSource.Flags & SS_FLAG_PLAY_BASE_ROOM)))
		{
			continue;
		}

		SoundEffect(soundSource.SoundID, (Pose*)&soundSource.Position);
	}
}
